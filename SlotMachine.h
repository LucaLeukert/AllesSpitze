#pragma once

#include <QObject>
#include <QVector>
#include <QPointer>
#include <QVariantList>
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

public:
    explicit SlotMachine(QObject *parent = nullptr);
    ~SlotMachine() override = default;

    [[nodiscard]] QVariantList towers() const;
    [[nodiscard]] bool canSpin() const { return m_can_spin && m_balance >= m_bet; }
    [[nodiscard]] QString lastResult() const { return m_last_result; }
    [[nodiscard]] double balance() const { return m_balance; }
    [[nodiscard]] double bet() const { return m_bet; }
    [[nodiscard]] double currentPrize() const;
    [[nodiscard]] QVariantList towerPrizes() const;
    [[nodiscard]] bool sessionActive() const { return m_session_active; }
    [[nodiscard]] bool canChangeBet() const { return !m_session_active; }

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

private slots:
    void onSpinFinished();

private:
    void processResult(Symbol::Type symbolType, bool isMiss);
    void updatePhysicalTower(int towerId);
    void updatePrize();
    void updateSessionState();

    // Prize multiplier tables [tower][level] - level 0 means no prize
    static constexpr double MARIENKAEFER_MULTIPLIERS[6] = {0, 1, 2, 4, 7, 10};
    static constexpr double KLEEBLATT_MULTIPLIERS[6] = {0, 3, 8, 16, 29, 50};
    static constexpr double COIN_MULTIPLIERS[6] = {0, 10, 40, 100, 200, 350};

    QVector<Tower*> m_towers;
    QPointer<SlotReel> m_reel;
    QPointer<I2CWorker> m_i2c_worker;
    bool m_can_spin = true;
    bool m_session_active = false;
    QString m_last_result;
    double m_balance = 0.0;
    double m_bet = 1.0;

    static constexpr double MIN_BET = 0.10;
    static constexpr double MAX_BET = 100.0;
    static constexpr double BET_STEP = 0.10;
};