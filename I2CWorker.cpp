#include "I2CWorker.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <QDebug>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include "DebugLogger.h"

I2CWorker::I2CWorker(QObject *parent) : QObject(parent) {
}

I2CWorker::~I2CWorker() {
    cleanup();
}

void I2CWorker::initialize() {
    if (m_is_initialized) {
        return;
    }

    qDebug() << "I2C Thread ID:" << QThread::currentThreadId();
    DebugLogger::instance().info(
        QString("I2C Worker initialized on thread: %1 (using Linux I2C + "
            "Protocol)")
        .arg(reinterpret_cast<qulonglong>(QThread::currentThreadId()))
    );

    m_is_initialized = true;
    emit initialization_complete();
}

void I2CWorker::cleanup() {
    if (!m_is_initialized) {
        return;
    }

    if (m_i2c_fd >= 0) {
        close(m_i2c_fd);
        m_i2c_fd = -1;
        DebugLogger::instance().info("I2C device released");
    }

    m_is_initialized = false;
}

void I2CWorker::openDevice(const uint8_t deviceAddress) {
    auto i2cDevice = "/dev/i2c-1";

    m_i2c_fd = open(i2cDevice, O_RDWR);
    if (m_i2c_fd < 0) {
        const QString error = QString("Failed to open %1: %2")
                .arg(i2cDevice, strerror(errno));
        DebugLogger::instance().error(error);
        emit deviceOpened(false, error);
        return;
    }

    if (ioctl(m_i2c_fd, I2C_SLAVE, deviceAddress) < 0) {
        const QString error =
                QString("Failed to set I2C slave address 0x%1: %2")
                .arg(deviceAddress, 2, 16, QChar('0'))
                .arg(strerror(errno));
        DebugLogger::instance().error(error);
        close(m_i2c_fd);
        m_i2c_fd = -1;
        emit deviceOpened(false, error);
        return;
    }

    m_device_address = deviceAddress;
    const QString success =
            QString("I2C device opened successfully at address: 0x%1")
            .arg(deviceAddress, 2, 16, QChar('0'));
    DebugLogger::instance().info(success);
    emit deviceOpened(true, success);

    // Wait longer before INIT to let Arduino stabilize
    QTimer::singleShot(500, this, &I2CWorker::sendInit);
}

// Protocol Implementation

void I2CWorker::sendInit() {
    if (!checkInitialized()) return;

    QMutexLocker locker(&m_i2c_mutex);

    DebugLogger::instance().info("Sending INIT command...");

    QByteArray response;

    if (const bool success = sendCommandWithRetry(CMD_INIT, QByteArray(), response);
        success && response.size() >= 4) {
        const auto status = static_cast<uint8_t>(response[2]);
        DebugLogger::instance().info(
            QString("INIT complete with status: 0x%1")
            .arg(status, 2, 16, QChar('0'))
        );
        m_is_ready = true;
        emit initComplete(status == 0x00, status);

        startPolling(200);
    } else {
        DebugLogger::instance().error("INIT failed - retrying in 2 seconds");
        m_is_ready = false;
        emit initComplete(false, 0xFF);

        QTimer::singleShot(2000, this, &I2CWorker::sendInit);
    }
}

void I2CWorker::sendHealthCheck() {
    if (!checkInitialized() || !m_is_ready) return;

    QMutexLocker locker(&m_i2c_mutex);

    QByteArray response;

    if (const bool success = sendCommandWithRetry(CMD_HEALTHCHECK, QByteArray(), response);
        success && response.size() >= 4) {
        const auto status = static_cast<uint8_t>(response[2]);
        emit healthCheckComplete(status == 0x00, status);
    } else {
        DebugLogger::instance().error("HEALTHCHECK failed");
        emit healthCheckComplete(false, 0xFF);
    }
}

void I2CWorker::pollButtonEvents() {
    if (!checkInitialized() || !m_is_ready) {
        return;
    }

    QMutexLocker locker(&m_i2c_mutex);

    QByteArray response;
    const bool success = sendCommandWithRetry(
        CMD_POLL_BUTTON_EVENTS,
        QByteArray(),
        response
    );

    if (success && response.size() >= 4) {
        const auto count = static_cast<uint8_t>(response[2]);
        QVector<uint8_t> buttonIds;

        for (int i = 0; i < count && (3 + i) < response.size() - 1; ++i) {
            buttonIds.append(static_cast<uint8_t>(response[3 + i]));
        }

        // Only log when buttons are actually pressed
        if (count > 0) {
            DebugLogger::instance().info(
                QString("Button events: %1 button(s) pressed").arg(count)
            );
        }

        m_consecutive_errors = 0;
        emit buttonEventsReceived(buttonIds);
    } else {
        m_consecutive_errors++;

        // Only log errors occasionally
        if (m_consecutive_errors % 10 == 1) {
            DebugLogger::instance().warning(
                QString("Button polling failed (consecutive errors: %1)")
                .arg(m_consecutive_errors)
            );
        }

        if (m_consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
            DebugLogger::instance().error(
                "Too many consecutive errors, attempting reinit..."
            );
            m_is_ready = false;
            if (reinitializeI2C()) {
                m_consecutive_errors = 0;
                QTimer::singleShot(500, this, &I2CWorker::sendInit);
            }
        }

        emit buttonEventsReceived(QVector<uint8_t>());
    }
}

void I2CWorker::highlightButton(const uint8_t buttonId, const bool state) {
    if (!checkInitialized() || !m_is_ready) return;

    QMutexLocker locker(&m_i2c_mutex);

    QByteArray data;
    data.append(static_cast<char>(buttonId));
    data.append(static_cast<char>(state ? 0x01 : 0x00));

    QByteArray response;

    if (const bool success = sendCommandWithRetry(CMD_HIGHLIGHT_BUTTON, data, response);
        success && response.size() >= 4) {
        const auto status = static_cast<uint8_t>(response[2]);
        DebugLogger::instance().debug(
            QString("HIGHLIGHT_BUTTON (ID: 0x%1, State: %2) status: 0x%3")
            .arg(buttonId, 2, 16, QChar('0'))
            .arg(state)
            .arg(status, 2, 16, QChar('0'))
        );
        emit highlightButtonComplete(status == 0x00, status);
    } else {
        DebugLogger::instance().error("HIGHLIGHT_BUTTON failed");
        emit highlightButtonComplete(false, 0xFF);
    }
}

void I2CWorker::highlightTower(const uint8_t towerId, const uint8_t row) {
    if (!checkInitialized() || !m_is_ready) return;

    QMutexLocker locker(&m_i2c_mutex);

    QByteArray data;
    data.append(static_cast<char>(towerId));
    data.append(static_cast<char>(row));

    QByteArray response;

    if (const bool success = sendCommandWithRetry(CMD_HIGHLIGHT_TOWER, data, response);
        success && response.size() >= 4) {
        const auto status = static_cast<uint8_t>(response[2]);
        DebugLogger::instance().debug(
            QString("HIGHLIGHT_TOWER (ID: 0x%1, Row: %2) status: 0x%3")
            .arg(towerId, 2, 16, QChar('0'))
            .arg(row)
            .arg(status, 2, 16, QChar('0'))
        );
        emit highlightTowerComplete(status == 0x00, status);
    } else {
        DebugLogger::instance().error("HIGHLIGHT_TOWER failed");
        emit highlightTowerComplete(false, 0xFF);
    }
}

void I2CWorker::updateUserName(const QString &username) {
    if (!checkInitialized() || !m_is_ready) return;

    QMutexLocker locker(&m_i2c_mutex);

    const QByteArray data = username.toUtf8();
    if (data.size() > 255) {
        DebugLogger::instance().error(
            "Username too long (max 255 bytes)"
        );
        emit userNameUpdated(false, 0xFF);
        return;
    }

    QByteArray response;

    if (const bool success = sendCommandWithRetry(CMD_UPDATE_USER_NAME, data, response);
        success && response.size() >= 4) {
        const auto status = static_cast<uint8_t>(response[2]);
        DebugLogger::instance().info(
            QString("UPDATE_USER_NAME (%1) status: 0x%2")
            .arg(username)
            .arg(status, 2, 16, QChar('0'))
        );
        emit userNameUpdated(status == 0x00, status);
    } else {
        DebugLogger::instance().error("UPDATE_USER_NAME failed");
        emit userNameUpdated(false, 0xFF);
    }
}

void I2CWorker::updateUserBalance(double balance) {
    if (!checkInitialized() || !m_is_ready) return;

    QMutexLocker locker(&m_i2c_mutex);

    // Convert to cents (multiply by 100) for transmission as int32
    int32_t balanceCents = static_cast<int32_t>(balance * 100.0);

    QByteArray data;
    data.append(static_cast<char>(balanceCents & 0xFF));
    data.append(static_cast<char>((balanceCents >> 8) & 0xFF));
    data.append(static_cast<char>((balanceCents >> 16) & 0xFF));
    data.append(static_cast<char>((balanceCents >> 24) & 0xFF));

    QByteArray response;
    const bool success =
            sendCommandWithRetry(CMD_UPDATE_USER_BALANCE, data, response);

    if (success && response.size() >= 4) {
        const auto status = static_cast<uint8_t>(response[2]);
        DebugLogger::instance().info(
            QString("UPDATE_USER_BALANCE (%1) status: 0x%2")
            .arg(balance, 0, 'f', 2)
            .arg(status, 2, 16, QChar('0'))
        );
        emit userBalanceUpdated(status == 0x00, status);
    } else {
        DebugLogger::instance().error("UPDATE_USER_BALANCE failed");
        emit userBalanceUpdated(false, 0xFF);
    }
}

// Protocol Helper Methods

void I2CWorker::flushI2CBuffers() const {
    if (m_i2c_fd < 0) return;

    // Simple flush - try to read any pending data
    uint8_t dummy[256];
    fcntl(m_i2c_fd, F_SETFL, O_NONBLOCK);
    while (read(m_i2c_fd, dummy, sizeof(dummy)) > 0) {
        // Discard all pending data
    }
    fcntl(m_i2c_fd, F_SETFL, 0); // Back to blocking
}

bool I2CWorker::reinitializeI2C() {
    DebugLogger::instance().warning("Attempting to reinitialize I2C...");

    if (m_i2c_fd >= 0) {
        close(m_i2c_fd);
        m_i2c_fd = -1;
    }

    QThread::msleep(1000); // Longer wait

    const auto i2cDevice = "/dev/i2c-1";
    m_i2c_fd = open(i2cDevice, O_RDWR);
    if (m_i2c_fd < 0) {
        DebugLogger::instance().error("Failed to reopen I2C device");
        return false;
    }

    if (ioctl(m_i2c_fd, I2C_SLAVE, m_device_address) < 0) {
        DebugLogger::instance().error("Failed to reset I2C slave address");
        close(m_i2c_fd);
        m_i2c_fd = -1;
        return false;
    }

    flushI2CBuffers();
    DebugLogger::instance().info("I2C reinitialized successfully");
    return true;
}

QByteArray I2CWorker::buildPacket(const uint8_t command, const QByteArray &data) {
    QByteArray packet;
    packet.append(static_cast<char>(command));
    packet.append(static_cast<char>(data.size()));
    packet.append(data);

    const uint8_t checksum = calculateChecksum(packet);
    packet.append(static_cast<char>(checksum));

    return packet;
}

bool I2CWorker::sendPacket(const QByteArray &packet) const {
    if (const ssize_t written = write(m_i2c_fd, packet.data(), packet.size());
        written != packet.size()) {
        DebugLogger::instance().error(
            QString("Failed to write packet: %1 (wrote %2/%3 bytes)")
            .arg(strerror(errno))
            .arg(written)
            .arg(packet.size())
        );
        return false;
    }

    return true;
}

QByteArray I2CWorker::receivePacket() const {
    QThread::msleep(150);

    uint8_t buffer[MAX_PACKET_SIZE];

    ssize_t bytesRead = read(m_i2c_fd, buffer, MAX_PACKET_SIZE);

    if (bytesRead < 0) {
        DebugLogger::instance().error(
            QString("Read failed: %1").arg(strerror(errno))
        );
        return {};
    }

    if (bytesRead < 4) {
        DebugLogger::instance().verbose(
            QString("Response too short: %1 bytes").arg(bytesRead)
        );
        return {};
    }

    const uint8_t dataLength = buffer[1];

    if (const int expectedLength = 3 + dataLength;
        bytesRead >= expectedLength) {
        bytesRead = expectedLength;
    } else {
        DebugLogger::instance().warning(
            QString("Incomplete packet: expected %1, got %2 bytes")
            .arg(expectedLength)
            .arg(bytesRead)
        );
    }

    return {reinterpret_cast<char *>(buffer), bytesRead};
}

bool I2CWorker::validateChecksum(const QByteArray &packet) {
    if (packet.size() < 3) return false;

    const QByteArray dataForChecksum = packet.left(packet.size() - 1);
    const uint8_t expectedChecksum = calculateChecksum(dataForChecksum);
    const auto receivedChecksum =
            static_cast<uint8_t>(packet[packet.size() - 1]);

    const bool valid = (expectedChecksum == receivedChecksum);

    if (!valid) {
        DebugLogger::instance().error(
            QString("Checksum mismatch: expected 0x%1, got 0x%2")
            .arg(expectedChecksum, 2, 16, QChar('0'))
            .arg(receivedChecksum, 2, 16, QChar('0'))
        );
    }

    return valid;
}

uint8_t I2CWorker::calculateChecksum(const QByteArray &data) {
    uint8_t checksum = 0;
    for (const char byte: data) {
        checksum ^= static_cast<uint8_t>(byte);
    }
    return checksum;
}

bool I2CWorker::sendCommandWithRetry(
    const uint8_t command,
    const QByteArray &data,
    QByteArray &response
) const {
    const QByteArray packet = buildPacket(command, data);

    // VERBOSE: Log packet being sent
    /*DebugLogger::instance().verbose(
        QString("TX (%1 bytes): %2")
            .arg(packet.size())
            .arg(DebugLogger::formatHexDump(packet))
    );*/

    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        if (attempt > 0) {
            DebugLogger::instance().warning(
                QString("Retry %1/%2 for command 0x%3")
                .arg(attempt + 1)
                .arg(MAX_RETRIES)
                .arg(command, 2, 16, QChar('0'))
            );
            QThread::msleep(500);
        }

        if (!sendPacket(packet)) {
            continue;
        }

        response = receivePacket();
        if (response.isEmpty()) {
            DebugLogger::instance().warning("No response received");
            continue;
        }

        // VERBOSE: Log received packet
        /*DebugLogger::instance().verbose(
            QString("RX (%1 bytes): %2")
                .arg(response.size())
                .arg(DebugLogger::formatHexDump(response))
        );*/

        if (!validateChecksum(response)) {
            continue;
        }

        const uint8_t expectedRsp = command | 0x80;

        if (const auto receivedCmd = static_cast<uint8_t>(response[0]); receivedCmd != expectedRsp) {
            DebugLogger::instance().error(
                QString("Response mismatch. Expected 0x%1, got 0x%2")
                .arg(expectedRsp, 2, 16, QChar('0'))
                .arg(receivedCmd, 2, 16, QChar('0'))
            );
            continue;
        }

        return true;
    }

    const QString error =
            QString("Command 0x%1 failed after %2 retries")
            .arg(command, 2, 16, QChar('0'))
            .arg(MAX_RETRIES);
    DebugLogger::instance().error(error);
    return false;
}

bool I2CWorker::checkInitialized() const {
    if (m_i2c_fd < 0) {
        return false;
    }
    return true;
}

void I2CWorker::startPolling(int intervalMs) {
    if (!m_poll_timer) {
        m_poll_timer = new QTimer(this);
        connect(m_poll_timer, &QTimer::timeout,
                this, &I2CWorker::pollButtonEvents);
    }

    m_poll_timer->setInterval(intervalMs);
    m_poll_timer->start();

    DebugLogger::instance().info(
        QString("Button polling started (%1ms interval)").arg(intervalMs)
    );
}

void I2CWorker::stopPolling() const {
    if (m_poll_timer) {
        m_poll_timer->stop();
        DebugLogger::instance().info("Button polling stopped");
    }
}

void I2CWorker::sendRawCommand(uint8_t command, const QVariantList &data) {
    if (!checkInitialized()) {
        DebugLogger::instance().error("Cannot send raw command - I2C not initialized");
        emit rawCommandResponse(command, false, QByteArray());
        return;
    }

    QMutexLocker locker(&m_i2c_mutex);

    // Convert QVariantList to QByteArray
    QByteArray byteData;
    for (const QVariant &v : data) {
        byteData.append(static_cast<char>(v.toInt() & 0xFF));
    }

    DebugLogger::instance().info(
        QString("Sending raw command 0x%1 with %2 bytes data")
            .arg(command, 2, 16, QChar('0'))
            .arg(byteData.size())
    );

    // Log the data being sent
    if (!byteData.isEmpty()) {
        QString hexData;
        for (int i = 0; i < byteData.size(); ++i) {
            hexData += QString("%1 ").arg(static_cast<uint8_t>(byteData[i]), 2, 16, QChar('0'));
        }
        DebugLogger::instance().info(QString("Data: %1").arg(hexData.trimmed()));
    }

    QByteArray response;
    const bool success = sendCommandWithRetry(command, byteData, response);

    if (success) {
        QString hexResponse;
        for (int i = 0; i < response.size(); ++i) {
            hexResponse += QString("%1 ").arg(static_cast<uint8_t>(response[i]), 2, 16, QChar('0'));
        }
        DebugLogger::instance().info(
            QString("Raw command response (%1 bytes): %2")
                .arg(response.size())
                .arg(hexResponse.trimmed())
        );
    } else {
        DebugLogger::instance().error("Raw command failed");
    }

    emit rawCommandResponse(command, success, response);
}
