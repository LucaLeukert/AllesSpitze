#pragma once

#include <QObject>
#include <QThread>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QVariantList>
#include <cstdint>
#include <optional>
#include <array>
#include "SlotMachine.h"
#include "SerialWorker.h"
#include "AudioEngine.h"
class HardwarePanelBackend;

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
    [[nodiscard]] AudioEngine *audioEngine() const { return m_audioEngine.data(); }

signals:
    // Signal to forward response to QML
    void i2cCommandResponse(int command, bool success, const QVariantList &response);
    void poweredOnChanged();
    void requestSerialOpenPort(const QString &portName = QString());
    void requestSerialCleanup();
    void requestSerialSendResponse(const QString &response);

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
    void handleButtonPress(uint8_t buttonId) const;
    void updateButtonStates() const;

    // Serial command handling
    void handleSerialCommand(SerialWorker::Command cmd, const QVariantMap &params);
    void sendSerialStatus();

    // Power state management
    void applyPowerState() const;
    void beginPanelStartupSequence();
    void beginPanelRecoverySequence();
    void onPanelReady();
    void invalidateHardwareStateCache() const;
    void sendButtonHighlightIfChanged(uint8_t buttonId, bool state) const;
    void sendTowerLevelIfChanged(uint8_t towerId, uint8_t row) const;
    void sendBalanceIfChanged(double balance) const;

private:
    enum class ControllerState {
        Starting,
        Ready,
        Recovering,
        ShuttingDown
    };

    QScopedPointer<QQmlApplicationEngine> m_engine;
    QScopedPointer<HardwarePanelBackend> m_panelBackend;
    QScopedPointer<QThread> m_serialThread;
    QScopedPointer<SerialWorker> m_serialWorker;
    QScopedPointer<SlotMachine> m_slotMachine;
    QScopedPointer<AudioEngine> m_audioEngine;
    QScopedPointer<QTimer> m_healthcheckTimer;
    mutable std::array<std::optional<bool>, 2> m_lastButtonStates{};
    mutable std::array<std::optional<uint8_t>, 3> m_lastTowerLevels{};
    mutable std::optional<int64_t> m_lastDisplayedBalanceCents;
    int m_consecutiveFailures{0};
    bool m_powered_on{true};
    ControllerState m_state{ControllerState::Starting};
    static constexpr int MAX_CONSECUTIVE_FAILURES = 3;
};
