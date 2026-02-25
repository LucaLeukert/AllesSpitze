#include "SlotMachine.h"
#include "DebugLogger.h"
#include "AudioEngine.h"
#include <QPointer>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QVariantMap>
#include <algorithm>
#include <cmath>
#include <chrono>

SlotMachine::SlotMachine(QObject *parent) : QObject(parent) {
    // Initialize random number generator
    const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
    m_rng.seed(static_cast<unsigned int>(seed));

    // Initialize risk animation timer
    m_risk_animation_timer = new QTimer(this);
    m_risk_animation_timer->setInterval(220); // Default ladder blink animation
    connect(m_risk_animation_timer, &QTimer::timeout, this, &SlotMachine::onRiskAnimationStep);

    // Create 3 towers with Qt parent ownership
    // Order: Coin (0), Kleeblatt (1), Marienkaefer (2)
    auto *tower1 = new Tower(Symbol::Type::Coin, 0, this);
    auto *tower2 = new Tower(Symbol::Type::Kleeblatt, 1, this);
    auto *tower3 = new Tower(Symbol::Type::Marienkaefer, 2, this);

    m_towers = {tower1, tower2, tower3};

    for (const auto *tower: m_towers) {
        connect(tower, &Tower::levelChanged, this, &SlotMachine::towersChanged);
        connect(tower, &Tower::levelChanged, this, &SlotMachine::updatePrize);
        connect(tower, &Tower::towerFull, this, [this]() {
            bool allFull = true;
            for (const auto *t: m_towers) {
                if (!t->isFull()) {
                    allFull = false;
                    break;
                }
            }
            if (allFull) {
                DebugLogger::instance().info("🎰 JACKPOT! All towers full!");
                if (m_audio_engine) {
                    m_audio_engine->playSfx("ASPI_GewinnTop1");
                }
                emit jackpotWon();
                cashout(); // Auto-cashout on jackpot
            }
        });
    }
}

QVariantList SlotMachine::towers() const {
    QVariantList list;
    for (const auto *tower: m_towers) {
        QVariantMap towerData;
        towerData["level"] = tower->level();
        towerData["symbolType"] = Symbol::typeToString(tower->symbolTypeEnum());
        towerData["towerId"] = tower->towerId();
        towerData["isFull"] = tower->isFull();
        list.append(towerData);
    }
    return list;
}

void SlotMachine::setReel(SlotReel *reel) {
    if (m_reel == reel) return;

    if (m_reel) {
        disconnect(m_reel, nullptr, this, nullptr);
    }

    m_reel = reel;

    if (m_reel) {
        connect(m_reel, &SlotReel::spinning_changed,
                this, &SlotMachine::onSpinFinished);
        applyDynamicRtpOdds();
    }
}

void SlotMachine::spin() {
    if (!canSpin() || !m_reel) {
        DebugLogger::instance().warning("Cannot spin - insufficient balance or no reel");
        return;
    }

    // Deduct bet amount for the spin
    m_balance -= m_bet;
    m_total_bets += m_bet;
    saveBalance();
    emit balanceChanged();

    m_can_spin = false;
    emit canSpinChanged();

    if (m_audio_engine) {
        const QString spinSound =
            (m_spins_since_last_clear < 3) ? QStringLiteral("ASPI_WLauf6s_01")
            : (m_spins_since_last_clear < 7) ? QStringLiteral("ASPI_WLauf6s_02")
                                             : QStringLiteral("ASPI_WLauf6s_03");
        m_audio_engine->playSfx(spinSound);
    }

    DebugLogger::instance().info(QString("Starting slot machine spin... (Bet: %1, Balance: %2)").arg(m_bet).arg(m_balance));
    m_reel->spin();
    ++m_spins_since_last_clear;
}

void SlotMachine::onSpinFinished() {
    if (!m_reel || m_reel->spinning()) {
        return;
    }

    const auto symbolType = m_reel->currentSymbolType();
    const bool isMiss = m_reel->isMiss();

    if (m_audio_engine) {
        m_audio_engine->fadeOutSfxByPrefix("ASPI_WLauf6s_", 120);
        m_audio_engine->playSfx("SFX_ReelStop");
    }

    processResult(symbolType, isMiss);
    applyDynamicRtpOdds();

    m_can_spin = true;
    emit canSpinChanged();

    const QString result = isMiss ? "miss" : Symbol::typeToString(symbolType);
    emit spinComplete(result);
}

void SlotMachine::processResult(Symbol::Type symbolType, bool isMiss) {
    if (isMiss) {
        m_last_result = "miss";
        DebugLogger::instance().info("Result: MISS - no tower update");
        emit lastResultChanged();
        return;
    }

    m_last_result = Symbol::typeToString(symbolType);
    emit lastResultChanged();

    if (symbolType == Symbol::Type::Sonne) {
        DebugLogger::instance().info("Result: SUN - increasing all towers");
        if (m_audio_engine) {
            m_audio_engine->playSfx("ASPI_GewLeiter_HiVal5");
        }
        for (auto *tower: m_towers) {
            tower->increase();
            emitTowerLevelForHardware(tower->towerId());
        }
        updateSessionState();
        return;
    }

    if (symbolType == Symbol::Type::Teufel) {
        const bool hadAnyTowerLevel = std::any_of(m_towers.cbegin(), m_towers.cend(), [](const Tower *tower) {
            return tower && tower->level() > 0;
        });
        if (hadAnyTowerLevel && m_audio_engine) {
            m_audio_engine->playSfx("ASPI_GewLeiterNegativ");
        }
        DebugLogger::instance().info("Result: DEVIL - resetting all towers");
        resetAllTowers();
        return;
    }

    // Find matching tower
    for (auto *tower: m_towers) {
        if (tower->symbolTypeEnum() == symbolType) {
            if (tower->increase() && m_audio_engine) {
                const int level = tower->level();
                if (level >= 1 && level <= 5) {
                    QString soundName;
                    switch (symbolType) {
                        case Symbol::Type::Coin:
                            soundName = QStringLiteral("ASPI_GewLeiter_HiVal%1").arg(level);
                            break;
                        case Symbol::Type::Kleeblatt:
                            soundName = QStringLiteral("ASPI_GewLeiter_MidVaL%1").arg(level);
                            break;
                        case Symbol::Type::Marienkaefer:
                            soundName = QStringLiteral("ASPI_GewLeiter_LowVal%1").arg(level);
                            break;
                        default:
                            break;
                    }
                    if (!soundName.isEmpty()) {
                        m_audio_engine->playSfx(soundName);
                    }
                }
            }
            emitTowerLevelForHardware(tower->towerId());
            updateSessionState();
            break;
        }
    }
}

void SlotMachine::emitTowerLevelForHardware(const int towerId) {
    const int level = m_towers[towerId]->level();
    emit towerLevelChangedForHardware(towerId, level);
}

void SlotMachine::resetAllTowers() {
    DebugLogger::instance().info("Resetting all towers");
    for (auto *tower: m_towers) {
        tower->reset();
        emitTowerLevelForHardware(tower->towerId());
    }
    updatePrize();
    updateSessionState();
    m_spins_since_last_clear = 0;
}

void SlotMachine::addBalance(double amount) {
    if (amount <= 0) return;

    m_balance += amount;
    saveBalance();
    emit balanceChanged();
    emit canSpinChanged();
    DebugLogger::instance().info(QString("Added %1 to balance. New balance: %2").arg(amount).arg(m_balance));
}

void SlotMachine::setBalance(double balance) {
    if (qFuzzyCompare(m_balance, balance)) return;

    m_balance = balance;
    emit balanceChanged();
    emit canSpinChanged();
    DebugLogger::instance().info(QString("Balance set to: %1 units").arg(m_balance));
}

void SlotMachine::setBet(double bet) {
    // Don't allow bet changes during active session
    if (m_session_active) {
        DebugLogger::instance().warning("Cannot change bet during active session");
        return;
    }

    // Round to 2 decimal places
    bet = std::round(bet * 100.0) / 100.0;

    if (bet < MIN_BET) bet = MIN_BET;
    if (bet > MAX_BET) bet = MAX_BET;

    if (qFuzzyCompare(m_bet, bet)) return;

    m_bet = bet;
    emit betChanged();
    emit canSpinChanged();
    emit currentPrizeChanged(); // Prize depends on bet
    DebugLogger::instance().info(QString("Bet set to: %1 units").arg(m_bet));
}

void SlotMachine::increaseBet() {
    if (!m_session_active) {
        setBet(m_bet + BET_STEP);
    }
}

void SlotMachine::decreaseBet() {
    if (!m_session_active) {
        setBet(m_bet - BET_STEP);
    }
}

double SlotMachine::getMultiplierForTower(int towerId, int level) const {
    if (level < 0 || level > 5) return 0;

    if (towerId >= 0 && towerId < m_towers.size()) {
        const Symbol::Type type = m_towers[towerId]->symbolTypeEnum();
        switch (type) {
            case Symbol::Type::Coin:
                return COIN_MULTIPLIERS[level];
            case Symbol::Type::Kleeblatt:
                return KLEEBLATT_MULTIPLIERS[level];
            case Symbol::Type::Marienkaefer:
                return MARIENKAEFER_MULTIPLIERS[level];
            default:
                return 0;
        }
    }
    return 0;
}

double SlotMachine::getPrizeForTower(int towerId) const {
    if (towerId < 0 || towerId >= m_towers.size()) return 0;

    const int level = m_towers[towerId]->level();
    const double multiplier = getMultiplierForTower(towerId, level);
    return m_bet * multiplier;
}

double SlotMachine::currentPrize() const {
    double total = 0;
    for (int i = 0; i < m_towers.size(); ++i) {
        total += getPrizeForTower(i);
    }
    return total;
}

QVariantList SlotMachine::towerPrizes() const {
    QVariantList list;
    for (int i = 0; i < m_towers.size(); ++i) {
        QVariantMap prizeData;
        const int level = m_towers[i]->level();
        const double multiplier = getMultiplierForTower(i, level);
        prizeData["towerId"] = i;
        prizeData["symbolType"] = Symbol::typeToString(m_towers[i]->symbolTypeEnum());
        prizeData["level"] = level;
        prizeData["multiplier"] = multiplier;
        prizeData["prize"] = getPrizeForTower(i);
        list.append(prizeData);
    }
    return list;
}

void SlotMachine::updatePrize() {
    emit currentPrizeChanged();
}

void SlotMachine::updateSessionState() {
    const bool wasActive = m_session_active;

    // Session is active if any tower has level > 0
    m_session_active = false;
    for (const auto *tower : m_towers) {
        if (tower->level() > 0) {
            m_session_active = true;
            break;
        }
    }

    if (wasActive != m_session_active) {
        emit sessionActiveChanged();
        emit canChangeBetChanged();

        if (m_session_active) {
            DebugLogger::instance().info("Session started - bet locked");
        } else {
            DebugLogger::instance().info("Session ended - bet unlocked");
        }
    }
}

void SlotMachine::applyDynamicRtpOdds() {
    if (!m_reel) {
        return;
    }

    const double expectedPayout = m_total_bets * TARGET_RTP;
    const double payoutGap = expectedPayout - m_total_payouts;
    const double controlRange = std::max(20.0, m_total_bets * 0.15);
    const double control = std::clamp(payoutGap / controlRange, -1.0, 1.0);

    const double missProbability = std::clamp(0.60 - (control * 0.15), 0.45, 0.78);
    m_reel->set_miss_probability(missProbability);

    auto clampWeight = [](const int value, const int minValue, const int maxValue) {
        return std::clamp(value, minValue, maxValue);
    };

    const int coin = clampWeight(static_cast<int>(std::lround(3 + (2.5 * control))), 1, 6);
    const int kleeblatt = clampWeight(static_cast<int>(std::lround(9 + (3.0 * control))), 5, 13);
    const int marienkaefer = clampWeight(static_cast<int>(std::lround(18 + (2.0 * control))), 12, 22);
    const int sonne = clampWeight(static_cast<int>(std::lround(2 + (2.0 * control))), 1, 4);
    const int teufel = clampWeight(static_cast<int>(std::lround(16 - (6.0 * control))), 8, 22);

    QVariantMap probabilities;
    probabilities.insert("coin", coin);
    probabilities.insert("kleeblatt", kleeblatt);
    probabilities.insert("marienkaefer", marienkaefer);
    probabilities.insert("sonne", sonne);
    probabilities.insert("teufel", teufel);
    m_reel->set_probabilities(probabilities);
}

void SlotMachine::recordPayout(const double amount) {
    if (amount <= 0) {
        return;
    }
    m_total_payouts += amount;
}

void SlotMachine::cashout() {
    const double prize = currentPrize();

    if (prize <= 0) {
        DebugLogger::instance().info("Cashout: No prize to collect");
        return;
    }

    DebugLogger::instance().info(QString("💰 CASHOUT! Prize: %1 units").arg(prize));

    // Add prize to balance
    m_balance += prize;
    recordPayout(prize);
    saveBalance();
    emit balanceChanged();

    // Reset all towers (this will also update session state)
    resetAllTowers();

    // Emit cashout signal
    emit cashedOut(prize);

    DebugLogger::instance().info(QString("New balance after cashout: %1 units").arg(m_balance));
}

void SlotMachine::acceptPrize() {
    const double prize = currentPrize();
    if (prize <= 0) {
        DebugLogger::instance().info("AcceptPrize: No prize to accept");
        return;
    }

    m_price_accepted = true;
    DebugLogger::instance().info(QString("✅ Prize accepted: %1 units").arg(prize));

    m_accepted_prize = prize;
    emit acceptedPrizeChanged();
    if (m_audio_engine) {
        m_audio_engine->stopSfxByPrefix("SFX_GambleBgLoop");
        m_audio_engine->playLoopSfx("SFX_ChooseGambleBgLoop");
    }

    // Reset towers so player can keep spinning
    resetAllTowers();
}

void SlotMachine::payoutAccepted() {
    if (m_accepted_prize <= 0) {
        DebugLogger::instance().info("PayoutAccepted: No accepted prize to pay out");
        return;
    }

    const double prize = m_accepted_prize;
    m_price_accepted = false;
    DebugLogger::instance().info(QString("💰 Paying out accepted prize: %1 units").arg(prize));

    m_balance += prize;
    recordPayout(prize);
    saveBalance();
    emit balanceChanged();

    m_accepted_prize = 0.0;
    emit acceptedPrizeChanged();
    if (m_audio_engine) {
        m_audio_engine->fadeOutSfxByPrefix("SFX_ChooseGambleBgLoop", 120);
        m_audio_engine->playSfx("SFX_Collect");
    }

    emit cashedOut(prize);
    DebugLogger::instance().info(QString("New balance after payout: %1 units").arg(m_balance));
}

QString SlotMachine::balanceFilePath() {
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (const QDir dir(dataPath); !dir.exists()) {
        (void) dir.mkpath(".");
    }
    return dataPath + "/balance.txt";
}

void SlotMachine::saveBalance() const {
    QFile file(balanceFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << QString::number(m_balance, 'f', 2);
        file.close();
    } else {
        DebugLogger::instance().error("Could not save balance to file");
    }
}

// ===== RISK LADDER FUNCTIONS =====

QVariantList SlotMachine::riskLadderSteps() const {
    QVariantList steps;
    for (int i = 0; i < RISK_LADDER_STEPS; ++i) {
        QVariantMap step;
        step["level"] = i;
        step["multiplier"] = RISK_MULTIPLIERS[i];
        step["prize"] = std::round((m_bet * RISK_MULTIPLIERS[i]) * 100.0) / 100.0;
        steps.append(step);
    }

    QVariantMap loseStep;
    loseStep["level"] = -1;
    loseStep["multiplier"] = 0.0;
    loseStep["prize"] = 0.0;
    steps.prepend(loseStep);
    return steps;
}

void SlotMachine::startRiskMode() {
    const double prize = m_accepted_prize > 0 ? m_accepted_prize : currentPrize();

    if (prize <= 0) {
        DebugLogger::instance().warning("Cannot start risk mode without a prize");
        return;
    }

    if (m_risk_mode_active) {
        DebugLogger::instance().warning("Risk mode already active");
        return;
    }

    m_price_accepted = false;
    DebugLogger::instance().info(QString("Starting risk mode with prize: %1").arg(prize));
    if (m_audio_engine) {
        m_audio_engine->fadeOutSfxByPrefix("SFX_ChooseGambleBgLoop", 120);
        m_audio_engine->playLoopSfx("SFX_GambleBgLoop");
    }

    int startLevel = 0;
    for (int i = 0; i < RISK_LADDER_STEPS; ++i) {
        const double ladderPrize = m_bet * RISK_MULTIPLIERS[i];
        if (ladderPrize <= prize) {
            startLevel = i;
            continue;
        }
        break;
    }
    const double checkpointPrize = m_bet * RISK_MULTIPLIERS[RISK_CHECKPOINT_LEVEL];
    if (startLevel >= RISK_CHECKPOINT_LEVEL && prize <= checkpointPrize) {
        startLevel = RISK_CHECKPOINT_LEVEL - 1;
    }

    m_risk_level = startLevel;
    m_risk_prize = std::round(prize * 100.0) / 100.0;
    m_risk_base_prize = m_risk_prize;
    m_risk_mode_active = true;
    m_risk_animating = false;
    m_risk_animation_position = m_risk_level;
    m_risk_animation_mode = 0;
    m_ausspielung_started = m_risk_level > RISK_CHECKPOINT_LEVEL;
    m_risk_scan_position = m_risk_level + 1;

    // Clear accepted prize since it's now in the risk ladder
    if (m_accepted_prize > 0) {
        m_accepted_prize = 0.0;
        emit acceptedPrizeChanged();
    }

    // Reset towers without adding to balance
    for (auto *tower: m_towers) {
        tower->reset();
        emitTowerLevelForHardware(tower->towerId());
    }
    updatePrize();
    updateSessionState();

    emit riskModeChanged();
    emit riskPrizeChanged();
    emit riskLevelChanged();
    emit riskAnimationPositionChanged();
    emit riskAusspielungStartedChanged();
    emit canSpinChanged();
    emit canChangeBetChanged();
}

void SlotMachine::riskHigher() {
    if (!m_risk_mode_active) {
        return;
    }
    if (m_risk_animating && m_risk_animation_mode != 1) {
        return;
    }
    if (m_risk_prize <= 0 || m_risk_level < 0) {
        DebugLogger::instance().info("Risk attempt ignored: no active prize");
        return;
    }

    if (m_risk_level >= RISK_LADDER_STEPS - 1 && m_risk_animation_mode != 1) {
        DebugLogger::instance().info("Already at top of risk ladder");
        return;
    }

    // During Ausspielung scan, pressing 1:1 starts random 50/50 target selection
    // from first level above checkpoint and continues animation until that target.
    if (m_risk_animation_mode == 1) {
        int targetLevel = RISK_CHECKPOINT_LEVEL + 1;
        std::uniform_int_distribution dist(0, 1);
        while (targetLevel < RISK_LADDER_STEPS - 1 && dist(m_rng) == 1) {
            ++targetLevel;
        }

        m_risk_target_position = targetLevel;
        m_risk_animation_mode = 2;
        return;
    }

    DebugLogger::instance().info(
        QString("Attempting risk: level %1 (prize %2)")
        .arg(m_risk_level)
        .arg(m_risk_prize, 0, 'f', 2));
    const double checkpointPrize = std::round((m_bet * RISK_MULTIPLIERS[RISK_CHECKPOINT_LEVEL]) * 100.0) / 100.0;

    // First action at the checkpoint starts looping Ausspielung scan.
    if (m_risk_level == RISK_CHECKPOINT_LEVEL && !m_ausspielung_started && m_risk_prize <= checkpointPrize) {
        if (m_audio_engine) {
            m_audio_engine->fadeSfxVolumeByPrefix("SFX_GambleBgLoop", 0, 120);
            m_audio_engine->stopSfxByPrefix("SFX_GamblePlayoff");
            m_audio_engine->playSfx("SFX_GamblePlayoff");
            QTimer::singleShot(1000, this, [this]() {
                if (m_audio_engine && m_risk_mode_active) {
                    m_audio_engine->fadeSfxVolumeByPrefix("SFX_GambleBgLoop", 1.0, 220);
                }
            });
        }
        m_risk_animating = true;
        m_risk_animation_mode = 1;
        m_risk_scan_position = RISK_CHECKPOINT_LEVEL + 1;
        m_risk_animation_position = m_risk_scan_position;
        m_risk_animation_timer->setInterval(130);

        emit riskAnimatingChanged();
        emit riskAnimationPositionChanged();
        m_risk_animation_timer->start();
        return;
    }

    // 50% chance to win
    std::uniform_int_distribution dist(0, 1);
    const bool willWin = dist(m_rng) == 1;

    // Resolve instantly to lose target or win target:
    // before Ausspielung-start: lose-all vs next higher ladder step
    // after Ausspielung-start/above checkpoint: one-down vs next level
    if (!m_ausspielung_started) {
        m_risk_lose_position = -1;
        m_risk_win_position = RISK_LADDER_STEPS - 1;
        for (int i = 0; i < RISK_LADDER_STEPS; ++i) {
            const double ladderPrize = std::round((m_bet * RISK_MULTIPLIERS[i]) * 100.0) / 100.0;
            if (ladderPrize > m_risk_prize) {
                m_risk_win_position = i;
                break;
            }
        }
    } else {
        m_risk_lose_position =
            ((m_risk_level > RISK_CHECKPOINT_LEVEL) || (m_risk_level == RISK_CHECKPOINT_LEVEL && m_ausspielung_started))
            ? (m_risk_level - 1)
            : -1;
        m_risk_win_position = qMin(m_risk_level + 1, RISK_LADDER_STEPS - 1);
    }
    m_risk_target_position = willWin ? m_risk_win_position : m_risk_lose_position;
    m_risk_animation_position = m_risk_target_position;

    emit riskAnimationPositionChanged();
    m_risk_animation_timer->stop();
    finishRiskAttempt(willWin);
}

void SlotMachine::onRiskAnimationStep() {
    if (m_risk_animation_mode == 1) {
        if (m_risk_scan_position > RISK_LADDER_STEPS - 1) {
            m_audio_engine->playSfx("SFX_BlinkDown1");
            m_risk_scan_position = RISK_CHECKPOINT_LEVEL + 1;
        } else {
            m_audio_engine->playSfx("SFX_BlinkUp1");
        }
        m_risk_animation_position = m_risk_scan_position;
        ++m_risk_scan_position;
        emit riskAnimationPositionChanged();
        return;
    }

    if (m_risk_animation_mode == 2) {
        if (m_risk_scan_position > RISK_LADDER_STEPS - 1) {
            m_audio_engine->playSfx("SFX_BlinkDown1");
            m_risk_scan_position = RISK_CHECKPOINT_LEVEL + 1;
        } else {
            m_audio_engine->playSfx("SFX_BlinkUp1");
        }
        m_risk_animation_position = m_risk_scan_position;
        ++m_risk_scan_position;
        emit riskAnimationPositionChanged();

        if (m_risk_animation_position != m_risk_target_position) {
            return;
        }

        m_risk_animation_timer->stop();
        m_risk_animation_mode = 0;
        m_risk_animating = false;
        m_ausspielung_started = true;
        m_risk_level = m_risk_target_position;
        m_risk_prize = std::round((m_bet * RISK_MULTIPLIERS[m_risk_level]) * 100.0) / 100.0;
        m_risk_scan_position = m_risk_level + 1;

        emit riskAnimatingChanged();
        emit riskLevelChanged();
        emit riskPrizeChanged();
        emit riskAnimationPositionChanged();
        emit riskAusspielungStartedChanged();
        emit riskWon(m_risk_prize);
        return;
    }
}

void SlotMachine::finishRiskAttempt(bool won) {
    if (won) {
        if (m_audio_engine) {
            m_audio_engine->stopSfxByPrefix("SFX_Ladder");
            m_audio_engine->playSfx("SFX_LadderUp");
        }
        m_risk_animating = true;
        emit riskAnimatingChanged();

        m_risk_level++;
        m_risk_prize = std::round((m_bet * RISK_MULTIPLIERS[m_risk_level]) * 100.0) / 100.0;
        m_risk_animation_position = m_risk_level;

        DebugLogger::instance().info(QString("🎉 Risk won! New level: %1, Prize: %2").arg(m_risk_level).arg(m_risk_prize));

        emit riskLevelChanged();
        emit riskPrizeChanged();
        emit riskAnimationPositionChanged();
        emit riskWon(m_risk_prize);

        // Hold the won level for 1 second before next input.
        QTimer::singleShot(1000, this, [this]() {
            if (!m_risk_mode_active || !m_risk_animating) {
                return;
            }
            m_risk_animating = false;
            emit riskAnimatingChanged();

            if (m_risk_level == RISK_CHECKPOINT_LEVEL && !m_ausspielung_started) {
                riskHigher();
                return;
            }

            if (m_risk_level >= RISK_LADDER_STEPS - 1) {
                DebugLogger::instance().info("🏆 Reached top of risk ladder! Auto-collecting.");
                collectRiskPrize();
            }
        });
    } else {
        m_risk_animating = false;
        emit riskAnimatingChanged();

        if (m_risk_target_position >= 0) {
            if (m_audio_engine) {
                m_audio_engine->stopSfxByPrefix("SFX_Ladder");
                m_audio_engine->playSfx("SFX_LadderDown");
            }
            DebugLogger::instance().info(QString("↘ Risk lost above Ausspielung, dropping to level %1").arg(m_risk_target_position));

            m_risk_level = m_risk_target_position;
            m_risk_prize = std::round((m_bet * RISK_MULTIPLIERS[m_risk_level]) * 100.0) / 100.0;
            m_risk_animation_position = m_risk_level;

            emit riskLevelChanged();
            emit riskPrizeChanged();
            emit riskAnimationPositionChanged();
        } else {
            // At or below checkpoint - lose everything
            if (m_audio_engine) {
                m_audio_engine->fadeOutSfxByPrefix("SFX_GambleBgLoop", 120);
                m_audio_engine->playSfx("SFX_GambleLoose");
            }
            DebugLogger::instance().info("💀 Risk lost! Prize forfeited.");

            m_risk_animation_position = -1;
            emit riskAnimationPositionChanged();

            // Lost everything, but keep ladder open to show the lose state.
            m_risk_prize = 0;
            m_risk_level = -1;
            m_risk_base_prize = 0;

            emit riskPrizeChanged();
            emit riskLevelChanged();
            emit riskLost();

            // Keep the lose state visible briefly, then close ladder automatically.
            QTimer::singleShot(3000, this, [this]() {
                if (!m_risk_mode_active || m_risk_animating) {
                    return;
                }
                if (m_risk_level != -1 || m_risk_prize > 0.0) {
                    return;
                }

                m_risk_mode_active = false;
                m_risk_level = 0;
                m_risk_animation_position = 0;
                m_ausspielung_started = false;

                emit riskModeChanged();
                emit riskLevelChanged();
                emit riskAnimationPositionChanged();
                emit riskAusspielungStartedChanged();
                emit canSpinChanged();
                emit canChangeBetChanged();
            });
        }
    }
}

void SlotMachine::collectRiskPrize() {
    if (!m_risk_mode_active) {
        return;
    }

    const double prize = m_risk_prize;

    DebugLogger::instance().info(QString("💰 Collecting risk prize: %1").arg(prize));
    if (m_audio_engine) {
        m_audio_engine->fadeOutSfxByPrefix("SFX_GambleBgLoop", 120);
        m_audio_engine->playSfx("SFX_GambleCollect");
    }

    // Add to balance
    m_balance += prize;
    recordPayout(prize);
    saveBalance();
    emit balanceChanged();

    // Reset risk state
    m_risk_mode_active = false;
    m_risk_prize = 0;
    m_risk_base_prize = 0;
    m_risk_level = 0;
    m_risk_animation_position = 0;
    m_ausspielung_started = false;

    emit riskModeChanged();
    emit riskPrizeChanged();
    emit riskLevelChanged();
    emit riskAnimationPositionChanged();
    emit riskAusspielungStartedChanged();
    emit riskCollected(prize);
    emit canSpinChanged();
    emit canChangeBetChanged();
}

void SlotMachine::collectRiskOneToOnePrize() {
    if (!m_risk_mode_active) {
        return;
    }

    const double prize = m_risk_base_prize;
    DebugLogger::instance().info(QString("💰 Collecting 1:1 risk prize: %1").arg(prize));
    if (m_audio_engine) {
        m_audio_engine->fadeOutSfxByPrefix("SFX_GambleBgLoop", 120);
        m_audio_engine->fadeOutSfxByPrefix("SFX_ChooseGambleBgLoop", 120);
        m_audio_engine->playSfx("SFX_GambleCollect");
    }

    m_balance += prize;
    recordPayout(prize);
    saveBalance();
    emit balanceChanged();

    m_risk_mode_active = false;
    m_risk_prize = 0;
    m_risk_base_prize = 0;
    m_risk_level = 0;
    m_risk_animation_position = 0;
    m_risk_animating = false;
    m_risk_blink_steps = 0;
    m_ausspielung_started = false;

    emit riskModeChanged();
    emit riskPrizeChanged();
    emit riskLevelChanged();
    emit riskAnimatingChanged();
    emit riskAnimationPositionChanged();
    emit riskAusspielungStartedChanged();
    emit riskCollected(prize);
    emit canSpinChanged();
    emit canChangeBetChanged();
}
bool SlotMachine::applyReelProbabilities(const QVariantMap &probabilities) {
    if (!m_reel) {
        DebugLogger::instance().warning("Cannot apply reel probabilities - reel not set");
        return false;
    }
    m_reel->set_probabilities(probabilities);
    return true;
}
