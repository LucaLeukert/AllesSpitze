#include "SlotMachine.h"
#include "I2CWorker.h"
#include "DebugLogger.h"
#include <QPointer>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <cmath>
#include <chrono>

SlotMachine::SlotMachine(QObject *parent) : QObject(parent) {
    // Initialize random number generator
    auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
    m_rng.seed(static_cast<unsigned int>(seed));

    // Initialize risk animation timer
    m_risk_animation_timer = new QTimer(this);
    m_risk_animation_timer->setInterval(80); // Fast animation
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
    }
}

void SlotMachine::spin() {
    if (!canSpin() || !m_reel) {
        DebugLogger::instance().warning("Cannot spin - insufficient balance or no reel");
        return;
    }

    // Deduct bet amount for the spin
    m_balance -= m_bet;
    saveBalance();
    emit balanceChanged();

    m_can_spin = false;
    emit canSpinChanged();

    DebugLogger::instance().info(QString("Starting slot machine spin... (Bet: %1, Balance: %2)").arg(m_bet).arg(m_balance));
    m_reel->spin();
}

void SlotMachine::onSpinFinished() {
    if (!m_reel || m_reel->spinning()) {
        return;
    }

    const auto symbolType = m_reel->currentSymbolType();
    const bool isMiss = m_reel->isMiss();

    processResult(symbolType, isMiss);

    m_can_spin = true;
    emit canSpinChanged();

    QString result = isMiss ? "miss" : Symbol::typeToString(symbolType);
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
        for (auto *tower: m_towers) {
            tower->increase();
            updatePhysicalTower(tower->towerId());
        }
        updateSessionState();
        return;
    }

    if (symbolType == Symbol::Type::Teufel) {
        DebugLogger::instance().info("Result: DEVIL - resetting all towers");
        resetAllTowers();
        return;
    }

    // Find matching tower
    for (auto *tower: m_towers) {
        if (tower->symbolTypeEnum() == symbolType) {
            tower->increase();
            updatePhysicalTower(tower->towerId());
            updateSessionState();
            break;
        }
    }
}

void SlotMachine::updatePhysicalTower(int towerId) {
    if (!m_i2c_worker) return;

    int level = m_towers[towerId]->level();

    QMetaObject::invokeMethod(
        m_i2c_worker,
        "highlightTower",
        Q_ARG(uint8_t, static_cast<uint8_t>(towerId)),
        Q_ARG(uint8_t, static_cast<uint8_t>(level))
    );
}

void SlotMachine::resetAllTowers() {
    DebugLogger::instance().info("Resetting all towers");
    for (auto *tower: m_towers) {
        tower->reset();
        updatePhysicalTower(tower->towerId());
    }
    updatePrize();
    updateSessionState();
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
        Symbol::Type type = m_towers[towerId]->symbolTypeEnum();
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

    int level = m_towers[towerId]->level();
    double multiplier = getMultiplierForTower(towerId, level);
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
        int level = m_towers[i]->level();
        double multiplier = getMultiplierForTower(i, level);
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
    bool wasActive = m_session_active;

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

void SlotMachine::cashout() {
    double prize = currentPrize();

    if (prize <= 0) {
        DebugLogger::instance().info("Cashout: No prize to collect");
        return;
    }

    DebugLogger::instance().info(QString("💰 CASHOUT! Prize: %1 units").arg(prize));

    // Add prize to balance
    m_balance += prize;
    saveBalance();
    emit balanceChanged();

    // Reset all towers (this will also update session state)
    resetAllTowers();

    // Emit cashout signal
    emit cashedOut(prize);

    DebugLogger::instance().info(QString("New balance after cashout: %1 units").arg(m_balance));
}

QString SlotMachine::balanceFilePath() {
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dataPath + "/balance.txt";
}

void SlotMachine::saveBalance() {
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
        step["prize"] = m_risk_base_prize * RISK_MULTIPLIERS[i];
        steps.append(step);
    }
    return steps;
}

void SlotMachine::startRiskMode() {
    double prize = currentPrize();

    if (prize <= 0) {
        DebugLogger::instance().warning("Cannot start risk mode without a prize");
        return;
    }

    if (m_risk_mode_active) {
        DebugLogger::instance().warning("Risk mode already active");
        return;
    }

    DebugLogger::instance().info(QString("🎲 Starting risk mode with prize: %1").arg(prize));

    // Store the prize and reset towers (prize is now "at risk")
    m_risk_base_prize = prize;
    m_risk_prize = prize;
    m_risk_level = 0;
    m_risk_mode_active = true;
    m_risk_animating = false;
    m_risk_animation_position = 0;

    // Reset towers without adding to balance
    for (auto *tower: m_towers) {
        tower->reset();
        updatePhysicalTower(tower->towerId());
    }
    updatePrize();
    updateSessionState();

    emit riskModeChanged();
    emit riskPrizeChanged();
    emit riskLevelChanged();
    emit canSpinChanged();
    emit canChangeBetChanged();
}

void SlotMachine::riskHigher() {
    if (!m_risk_mode_active || m_risk_animating) {
        return;
    }

    if (m_risk_level >= RISK_LADDER_STEPS - 1) {
        DebugLogger::instance().info("Already at top of risk ladder");
        return;
    }

    DebugLogger::instance().info(QString("🎲 Attempting to climb risk ladder from level %1").arg(m_risk_level));

    // Start the animation
    m_risk_animating = true;
    m_risk_animation_position = m_risk_level;

    // 50% chance to win
    std::uniform_int_distribution<int> dist(0, 1);
    bool willWin = dist(m_rng) == 1;

    // Calculate target position
    if (willWin) {
        m_risk_target_position = m_risk_level + 1;
    } else {
        m_risk_target_position = -1; // Indicates loss
    }

    emit riskAnimatingChanged();
    emit riskAnimationPositionChanged();

    // Start animation timer
    m_risk_animation_timer->start();
}

void SlotMachine::onRiskAnimationStep() {
    // Animate the position bouncing up and down
    static int animationSteps = 0;
    static bool goingUp = true;

    animationSteps++;

    if (animationSteps < 15) {
        // Bounce animation
        if (goingUp) {
            m_risk_animation_position++;
            if (m_risk_animation_position >= RISK_LADDER_STEPS - 1) {
                goingUp = false;
            }
        } else {
            m_risk_animation_position--;
            if (m_risk_animation_position <= 0) {
                goingUp = true;
            }
        }
        emit riskAnimationPositionChanged();
    } else {
        // Animation finished, show result
        m_risk_animation_timer->stop();
        animationSteps = 0;
        goingUp = true;

        bool won = m_risk_target_position > m_risk_level;
        finishRiskAttempt(won);
    }
}

void SlotMachine::finishRiskAttempt(bool won) {
    m_risk_animating = false;
    emit riskAnimatingChanged();

    if (won) {
        m_risk_level++;
        m_risk_prize = m_risk_base_prize * RISK_MULTIPLIERS[m_risk_level];
        m_risk_animation_position = m_risk_level;

        DebugLogger::instance().info(QString("🎉 Risk won! New level: %1, Prize: %2").arg(m_risk_level).arg(m_risk_prize));

        emit riskLevelChanged();
        emit riskPrizeChanged();
        emit riskAnimationPositionChanged();
        emit riskWon(m_risk_prize);

        // Auto-collect at max level
        if (m_risk_level >= RISK_LADDER_STEPS - 1) {
            DebugLogger::instance().info("🏆 Reached top of risk ladder! Auto-collecting.");
            collectRiskPrize();
        }
    } else {
        // Check if we're above the checkpoint level (Ausspielung)
        if (m_risk_level > RISK_CHECKPOINT_LEVEL) {
            // Fall back to checkpoint level instead of losing everything
            DebugLogger::instance().info(QString("📍 Falling back to Ausspielung checkpoint (Level %1)").arg(RISK_CHECKPOINT_LEVEL));

            m_risk_level = RISK_CHECKPOINT_LEVEL;
            m_risk_prize = m_risk_base_prize * RISK_MULTIPLIERS[m_risk_level];
            m_risk_animation_position = m_risk_level;

            emit riskLevelChanged();
            emit riskPrizeChanged();
            emit riskAnimationPositionChanged();
            // Don't emit riskLost - player still has prize at checkpoint
        } else {
            // At or below checkpoint - lose everything
            DebugLogger::instance().info("💀 Risk lost! Prize forfeited.");

            m_risk_animation_position = -1;
            emit riskAnimationPositionChanged();

            // Lost everything
            m_risk_prize = 0;
            m_risk_level = 0;
            m_risk_base_prize = 0;
            m_risk_mode_active = false;

            emit riskPrizeChanged();
            emit riskLevelChanged();
            emit riskModeChanged();
            emit riskLost();
            emit canSpinChanged();
            emit canChangeBetChanged();
        }
    }
}

void SlotMachine::collectRiskPrize() {
    if (!m_risk_mode_active) {
        return;
    }

    double prize = m_risk_prize;

    DebugLogger::instance().info(QString("💰 Collecting risk prize: %1").arg(prize));

    // Add to balance
    m_balance += prize;
    saveBalance();
    emit balanceChanged();

    // Reset risk state
    m_risk_mode_active = false;
    m_risk_prize = 0;
    m_risk_base_prize = 0;
    m_risk_level = 0;
    m_risk_animation_position = 0;

    emit riskModeChanged();
    emit riskPrizeChanged();
    emit riskLevelChanged();
    emit riskAnimationPositionChanged();
    emit riskCollected(prize);
    emit canSpinChanged();
    emit canChangeBetChanged();
}
