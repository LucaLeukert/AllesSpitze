#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVariantMap>

#ifdef Q_OS_LINUX
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

class SerialWorker : public QObject {
    Q_OBJECT

public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker() override;

    enum class Command {
        Unknown,
        PowerOn,
        PowerOff,
        SetBalance,
        SetProbabilities,
        GetStatus
    };

public slots:
    void initialize();
    void cleanup();
    void openPort(const QString &portName = QString());
    void closePort();
    void sendStatus();
    void sendResponse(const QString &response);

signals:
    void portOpened(bool success, const QString &message);
    void portClosed();
    void commandReceived(Command cmd, const QVariantMap &params);
    void errorOccurred(const QString &error);

private slots:
#ifdef Q_OS_LINUX
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError error);
#endif

private:
    void processCommand(const QString &line);
    QString findSerialPort();

#ifdef Q_OS_LINUX
    QSerialPort *m_serial_port = nullptr;
#endif
    QByteArray m_read_buffer;
    bool m_is_open = false;

    static constexpr int BAUD_RATE = 115200;
};
