#pragma once

#include <QObject>
#include <QThread>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QVariantList>
#include "I2CWorker.h"
#include "SlotMachine.h"
#include "SerialWorker.h"

class ApplicationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool poweredOn READ poweredOn NOTIFY poweredOnChanged)

public:
    explicit ApplicationController(QObject *parent = nullptr);

    ~ApplicationController() override;

    void initialize();

    [[nodiscard]] bool start() const;

    // Debug interface - callable from QML
    Q_INVOKABLE void sendRawI2CCommand(int command, const QVariantList &data) const;

    // Power control
    Q_INVOKABLE void setPowerOn(bool on);
    [[nodiscard]] bool poweredOn() const { return m_powered_on; }

signals:
    // Signal to forward response to QML
    void i2cCommandResponse(int command, bool success, const QVariantList &response);
    void poweredOnChanged();

private:
    void setupQmlEngine() const;

    void setupI2CWorker();

    void setupSerialWorker();

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

    // Serial command handling
    void handleSerialCommand(SerialWorker::Command cmd, const QVariantMap &params);
    void sendSerialStatus() const;

    // Power state management
    void applyPowerState();

private:
    QScopedPointer<QQmlApplicationEngine> m_engine;
    QScopedPointer<QThread> m_workerThread;
    QScopedPointer<I2CWorker> m_worker;
    QScopedPointer<QThread> m_serialThread;
    QScopedPointer<SerialWorker> m_serialWorker;
    QScopedPointer<SlotMachine> m_slotMachine;
    QScopedPointer<QTimer> m_healthcheckTimer;
    int m_consecutiveFailures{0};
    bool m_powered_on{true};  // Default to powered on
    static constexpr int MAX_CONSECUTIVE_FAILURES = 3;
};