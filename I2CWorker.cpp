#include "I2CWorker.h"
#include "DebugLogger.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cerrno>
#include <cstring>
#include <wiringPiI2C.h>

#define DEVICE_ID 0x08


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
    openBus(QString("/dev/i2c-0"), DEVICE_ID); // Beispiel: I2C Bus 1, Slave-Adresse 0x50
    DebugLogger::instance().info(
        QString("I2C Worker initialized on thread: %1")
        .arg(reinterpret_cast<qlonglong>(QThread::currentThreadId()))
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
        DebugLogger::instance().info("I2C bus closed");
    }

    m_is_initialized = false;
}

void I2CWorker::openBus(const QString &busPath, const uint8_t slaveAddress) {
    if (m_i2c_fd >= 0) {
        close(m_i2c_fd);
    }

    m_i2c_fd = wiringPiI2CSetup(DEVICE_ID);
    if (m_i2c_fd < 0) {
        const QString error = QString("Failed to open I2C bus %1: %2")
                .arg(busPath, strerror(errno));
        DebugLogger::instance().error(error);
        emit busOpened(false, error);
        return;
    }

    m_slave_address = slaveAddress;
    const QString success = QString("I2C bus %1 opened, slave address: 0x%2")
            .arg(busPath)
            .arg(slaveAddress, 2, 16, QChar('0'));
    DebugLogger::instance().info(success);
    emit busOpened(true, success);
}

void I2CWorker::writeByte(const uint8_t reg, const uint8_t value) {
    if (writeRegister(reg, &value, 1)) {
        DebugLogger::instance().debug(
            QString("Wrote 0x%1 to register 0x%2")
            .arg(value, 2, 16, QChar('0'))
            .arg(reg, 2, 16, QChar('0'))
        );
    }
}

void I2CWorker::readByte(const uint8_t reg) {
    uint8_t value = 0;
    if (readRegister(reg, &value, 1)) {
        DebugLogger::instance().debug(
            QString("Read 0x%1 from register 0x%2")
            .arg(value, 2, 16, QChar('0'))
            .arg(reg, 2, 16, QChar('0'))
        );
        emit byteRead(value);
    }
}

void I2CWorker::writeBlock(const uint8_t reg, const QByteArray &data) {
    if (writeRegister(
        reg,
        reinterpret_cast<const uint8_t *>(data.data()),
        data.size()
    )) {
        DebugLogger::instance().debug(
            QString("Wrote %1 bytes to register 0x%2")
            .arg(data.size())
            .arg(reg, 2, 16, QChar('0'))
        );
    }
}

void I2CWorker::readBlock(const uint8_t reg, const int length) {
    QByteArray data(length, 0);
    if (readRegister(
        reg,
        reinterpret_cast<uint8_t *>(data.data()),
        length
    )) {
        DebugLogger::instance().debug(
            QString("Read %1 bytes from register 0x%2")
            .arg(length)
            .arg(reg, 2, 16, QChar('0'))
        );
        emit blockRead(data);
    }
}

bool I2CWorker::writeRegister(
    const uint8_t reg,
    const uint8_t *data,
    const size_t length
) {
    if (m_i2c_fd < 0) {
        const QString error = "I2C bus not open";
        DebugLogger::instance().error(error);
        emit operationError(error);
        return false;
    }

    uint8_t buffer[256];
    buffer[0] = reg;
    memcpy(buffer + 1, data, length);

    if (write(m_i2c_fd, buffer, length + 1) != static_cast<ssize_t>(length + 1)) {
        const QString error =
                QString("I2C write failed: %1").arg(strerror(errno));
        DebugLogger::instance().error(error);
        emit operationError(error);
        return false;
    }

    return true;
}

bool I2CWorker::readRegister(
    const uint8_t reg,
    uint8_t *data,
    const size_t length
) {
    if (m_i2c_fd < 0) {
        const QString error = "I2C bus not open";
        DebugLogger::instance().error(error);
        emit operationError(error);
        return false;
    }

    if (write(m_i2c_fd, &reg, 1) != 1) {
        const QString error = QString("I2C write register failed: %1")
                .arg(strerror(errno));
        DebugLogger::instance().error(error);
        emit operationError(error);
        return false;
    }

    if (read(m_i2c_fd, data, length) != static_cast<ssize_t>(length)) {
        const QString error =
                QString("I2C read failed: %1").arg(strerror(errno));
        DebugLogger::instance().error(error);
        emit operationError(error);
        return false;
    }

    return true;
}
