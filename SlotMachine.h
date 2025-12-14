#pragma once

#include <QObject>
#include <QVector>
#include <QPointer>
#include <QVariantList>
#include <QTimer>
#include <random>
#include "Tower.h"
#include "SlotReel.h"
#include "Symbol.h"
#include "I2CWorker.h"

class SlotMachine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList towers READ towers NOTIFY towersChanged)
    Q_PROPERTY(bool canSpin READ canSpin NOTIFY canSpinChanged)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY lastResultChanged)
    Q_PROPERTY(double balance READ balance NOTIFY balanceChanged)
    Q_PROPERTY(double bet READ bet WRITE setBet NOTIFY betChanged)
    Q_PROPERTY(double currentPrize READ currentPrize NOTIFY currentPrizeChanged)
    Q_PROPERTY(QVariantList towerPrizes READ towerPrizes NOTIFY currentPrizeChanged)
    Q_PROPERTY(bool sessionActive READ sessionActive NOTIFY sessionActiveChanged)
    Q_PROPERTY(bool canChangeBet READ canChangeBet NOTIFY canChangeBetChanged)

    // Risk ladder properties
    Q_PROPERTY(bool riskModeActive READ riskModeActive NOTIFY riskModeChanged)
    Q_PROPERTY(double riskPrize READ riskPrize NOTIFY riskPrizeChanged)
    Q_PROPERTY(int riskLevel READ riskLevel NOTIFY riskLevelChanged)
    Q_PROPERTY(QVariantList riskLadderSteps READ riskLadderSteps CONSTANT)
    Q_PROPERTY(bool riskAnimating READ riskAnimating NOTIFY riskAnimatingChanged)
    Q_PROPERTY(int riskAnimationPosition READ riskAnimationPosition NOTIFY riskAnimationPositionChanged)

public:
    explicit SlotMachine(QObject *parent = nullptr);
    ~SlotMachine() override = default;

    [[nodiscard]] QVariantList towers() const;
    [[nodiscard]] bool canSpin() const { return m_can_spin && m_balance >= m_bet && !m_risk_mode_active; }
    [[nodiscard]] QString lastResult() const { return m_last_result; }
    [[nodiscard]] double balance() const { return m_balance; }
    [[nodiscard]] double bet() const { return m_bet; }
    [[nodiscard]] double currentPrize() const;
    [[nodiscard]] QVariantList towerPrizes() const;
    [[nodiscard]] bool sessionActive() const { return m_session_active; }
    [[nodiscard]] bool canChangeBet() const { return !m_session_active && !m_risk_mode_active; }
    [[nodiscard]] bool isSpinning() const { return m_reel && m_reel->spinning(); }

    // Risk ladder getters
    [[nodiscard]] bool riskModeActive() const { return m_risk_mode_active; }
    [[nodiscard]] double riskPrize() const { return m_risk_prize; }
    [[nodiscard]] int riskLevel() const { return m_risk_level; }
    [[nodiscard]] QVariantList riskLadderSteps() const;
    [[nodiscard]] bool riskAnimating() const { return m_risk_animating; }
    [[nodiscard]] int riskAnimationPosition() const { return m_risk_animation_position; }

    Q_INVOKABLE void spin();
    Q_INVOKABLE void resetAllTowers();
    Q_INVOKABLE void setReel(SlotReel *reel);
    Q_INVOKABLE void addBalance(double amount);
    Q_INVOKABLE void setBet(double bet);
    Q_INVOKABLE void increaseBet();
    Q_INVOKABLE void decreaseBet();
    Q_INVOKABLE void cashout();
    [[nodiscard]] Q_INVOKABLE double getPrizeForTower(int towerId) const;
    [[nodiscard]] Q_INVOKABLE double getMultiplierForTower(int towerId, int level) const;

    // Risk ladder actions
    Q_INVOKABLE void startRiskMode();
    Q_INVOKABLE void riskHigher();      // Try to go higher on ladder
    Q_INVOKABLE void collectRiskPrize(); // Take current risk prize

    void setBalance(double balance);
    void setI2CWorker(I2CWorker *worker) { m_i2c_worker = worker; }

    // Balance persistence - accessible for ApplicationController
    static QString balanceFilePath();
    void saveBalance();

signals:
    void towersChanged();
    void canSpinChanged();
    void lastResultChanged();
    void balanceChanged();
    void betChanged();
    void currentPrizeChanged();
    void sessionActiveChanged();
    void canChangeBetChanged();
    void spinComplete(const QString &result);
    void jackpotWon();
    void cashedOut(double amount);

    // Risk ladder signals
    void riskModeChanged();
    void riskPrizeChanged();
    void riskLevelChanged();
    void riskAnimatingChanged();
    void riskAnimationPositionChanged();
    void riskWon(double newPrize);
    void riskLost();
    void riskCollected(double amount);

private slots:
    void onSpinFinished();
    void onRiskAnimationStep();

private:
    void processResult(Symbol::Type symbolType, bool isMiss);
    void updatePhysicalTower(int towerId);
    void updatePrize();
    void updateSessionState();
    void finishRiskAttempt(bool won);

    // Prize multiplier tables [tower][level] - level 0 means no prize
    inline static constexpr double MARIENKAEFER_MULTIPLIERS[6] = {0, 1, 2, 4, 7, 10};
    inline static constexpr double KLEEBLATT_MULTIPLIERS[6] = {0, 3, 8, 16, 29, 50};
    inline static constexpr double COIN_MULTIPLIERS[6] = {0, 10, 40, 100, 200, 350};

    // Risk ladder multipliers (each step doubles)
    inline static constexpr int RISK_LADDER_STEPS = 8;
    inline static constexpr int RISK_CHECKPOINT_LEVEL = 5; // "Ausspielung" checkpoint
    inline static constexpr double RISK_MULTIPLIERS[RISK_LADDER_STEPS] = {1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0};

    QVector<Tower*> m_towers;
    QPointer<SlotReel> m_reel;
    QPointer<I2CWorker> m_i2c_worker;
    bool m_can_spin = true;
    bool m_session_active = false;
    QString m_last_result;
    double m_balance = 0.0;
    double m_bet = 1.0;

    // Risk ladder state
    bool m_risk_mode_active = false;
    double m_risk_prize = 0.0;        // Current prize in risk mode
    double m_risk_base_prize = 0.0;   // Original prize before risk
    int m_risk_level = 0;             // Current step on ladder (0-7)
    bool m_risk_animating = false;
    int m_risk_animation_position = 0;
    int m_risk_target_position = 0;
    QTimer *m_risk_animation_timer = nullptr;
    std::mt19937 m_rng;

    inline static constexpr double MIN_BET = 0.10;
    inline static constexpr double MAX_BET = 100.0;
    inline static constexpr double BET_STEP = 0.10;
};