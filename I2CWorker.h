#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QVector>
#include <QMutex>
#include <QVariantList>

class I2CWorker : public QObject {
    Q_OBJECT

public:
    explicit I2CWorker(QObject *parent = nullptr);

    ~I2CWorker() override;

    enum Command : uint8_t {
        CMD_INIT = 0x01,
        CMD_HEALTHCHECK = 0x02,
        CMD_POLL_BUTTON_EVENTS = 0x03,
        CMD_HIGHLIGHT_BUTTON = 0x04,
        CMD_HIGHLIGHT_TOWER = 0x05,
        CMD_UPDATE_USER_NAME = 0x06,
        CMD_UPDATE_USER_BALANCE = 0x07
    };

    enum Response : uint8_t {
        RSP_INIT = 0x81,
        RSP_HEALTHCHECK = 0x82,
        RSP_POLL_BUTTON_EVENTS = 0x83,
        RSP_HIGHLIGHT_BUTTON = 0x84,
        RSP_HIGHLIGHT_TOWER = 0x85,
        RSP_UPDATE_USER_NAME = 0x86,
        RSP_UPDATE_USER_BALANCE = 0x87
    };

public slots:
    void initialize();

    void cleanup();

    void openDevice(uint8_t deviceAddress);

    void sendInit();

    Q_INVOKABLE void sendHealthCheck();

    void pollButtonEvents();

    void highlightButton(uint8_t buttonId, bool state);

    void highlightTower(uint8_t towerId, uint8_t row);

    void updateUserName(const QString &username);

    void updateUserBalance(double balance);

    void startPolling(int intervalMs = 250); // NEW
    void stopPolling() const; // NEW

    // Debug interface - send any command with raw data
    Q_INVOKABLE void sendRawCommand(uint8_t command, const QVariantList &data);

signals:
    void initialization_complete();

    void deviceOpened(bool success, const QString &message);

    void operationError(const QString &error);

    void initComplete(bool success, uint8_t status);

    void healthCheckComplete(bool success, uint8_t status);

    void buttonEventsReceived(const QVector<uint8_t> &buttonIds);

    void highlightButtonComplete(bool success, uint8_t status);

    void highlightTowerComplete(bool success, uint8_t status);

    void userNameUpdated(bool success, uint8_t status);

    void userBalanceUpdated(bool success, uint8_t status);

    // Debug signal for raw command responses
    void rawCommandResponse(uint8_t command, bool success, const QByteArray &response);

private:
    bool m_is_initialized = false;
    bool m_is_ready = false;
    int m_i2c_fd = -1;
    uint8_t m_device_address = 0;
    QMutex m_i2c_mutex;
    int m_consecutive_errors = 0;
    QTimer *m_poll_timer = nullptr;

    static constexpr int MAX_RETRIES = 3;
    static constexpr int RESPONSE_TIMEOUT_MS = 150;
    static constexpr int MAX_PACKET_SIZE = 256;
    static constexpr int MAX_CONSECUTIVE_ERRORS = 10;

    [[nodiscard]] bool checkInitialized() const;

    void flushI2CBuffers() const;

    bool reinitializeI2C();

    static QByteArray buildPacket(
        uint8_t command,
        const QByteArray &data = QByteArray()
    );

    [[nodiscard]] bool sendPacket(const QByteArray &packet) const;

    [[nodiscard]] QByteArray receivePacket() const;

    static bool validateChecksum(const QByteArray &packet);

    static uint8_t calculateChecksum(const QByteArray &data);

    bool sendCommandWithRetry(
        uint8_t command,
        const QByteArray &data,
        QByteArray &response
    ) const;
};
