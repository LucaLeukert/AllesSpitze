#pragma once

#include <QThread>
#include <QString>

class I2CWorker : public QObject {
    Q_OBJECT

public:
    explicit I2CWorker(QObject *parent = nullptr);

    ~I2CWorker() override;

public slots:
    void initialize();

    void cleanup();

    void openBus(const QString &busPath, uint8_t slaveAddress);

    Q_INVOKABLE void writeByte(uint8_t reg, uint8_t value);

    void readByte(uint8_t reg);

    void writeBlock(uint8_t reg, const QByteArray &data);

    void readBlock(uint8_t reg, int length);

signals:
    void initialization_complete();

    void busOpened(bool success, const QString &message);

    void byteRead(uint8_t value);

    void blockRead(const QByteArray &data);

    void operationError(const QString &error);

private:
    bool m_is_initialized = false;
    int m_i2c_fd = -1;
    uint8_t m_slave_address = 0;

    bool writeRegister(uint8_t reg, const uint8_t *data, size_t length);

    bool readRegister(uint8_t reg, uint8_t *data, size_t length);
};
