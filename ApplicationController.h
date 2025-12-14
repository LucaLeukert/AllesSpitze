#pragma once

#include <QObject>
#include <QThread>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QVariantList>
#include "I2CWorker.h"
#include "SlotMachine.h"

class ApplicationController : public QObject {
    Q_OBJECT

public:
    explicit ApplicationController(QObject *parent = nullptr);

    ~ApplicationController() override;

    void initialize();

    [[nodiscard]] bool start() const;

    // Debug interface - callable from QML
    Q_INVOKABLE void sendRawI2CCommand(int command, const QVariantList &data) const;

signals:
    // Signal to forward response to QML
    void i2cCommandResponse(int command, bool success, const QVariantList &response);

private:
    void setupQmlEngine() const;

    void setupI2CWorker();

    void setupSlotMachine() const;

    void setupConnections();

    void setupCleanup();

    void startHealthcheck();

    void loadBalance() const;

    void handleHealthcheckResponse(bool success, uint8_t status);

    void handleRawCommandResponse(uint8_t command, bool success, const QByteArray &response);

    // Button handling
    void handleButtonPress(uint8_t buttonId);
    void updateButtonStates() const;

private:
    QScopedPointer<QQmlApplicationEngine> m_engine;
    QScopedPointer<QThread> m_workerThread;
    QScopedPointer<I2CWorker> m_worker;
    QScopedPointer<SlotMachine> m_slotMachine;
    QScopedPointer<QTimer> m_healthcheckTimer;
    int m_consecutiveFailures{0};
    static constexpr int MAX_CONSECUTIVE_FAILURES = 3;
};