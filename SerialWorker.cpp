#include "SerialWorker.h"
#include "DebugLogger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

SerialWorker::SerialWorker(QObject *parent)
    : QObject(parent)
#ifdef Q_OS_LINUX
      , m_serial_port(new QSerialPort(this))
#endif
{
}

SerialWorker::~SerialWorker() {
    cleanup();
}

void SerialWorker::initialize() {
    DebugLogger::instance().info("SerialWorker initialized on thread: " +
                                 QString::number(reinterpret_cast<quint64>(QThread::currentThreadId())));
#ifndef Q_OS_LINUX
    DebugLogger::instance().warning("SerialWorker: Serial port support is disabled on this platform (macOS)");
#endif
}

void SerialWorker::cleanup() {
    closePort();
}

QString SerialWorker::findSerialPort() {
#ifdef Q_OS_LINUX
    // Look for USB serial ports (common patterns for USB-to-Serial adapters)
    const auto ports = QSerialPortInfo::availablePorts();

    for (const auto &port : ports) {
        const QString portName = port.portName();
        const QString description = port.description().toLower();
        const QString manufacturer = port.manufacturer().toLower();

        // Common USB serial adapter patterns
        if (portName.contains("ttyUSB") ||      // Linux USB serial
            portName.contains("ttyACM") ||      // Linux ACM devices
            portName.contains("cu.usbserial") || // macOS USB serial
            portName.contains("cu.usbmodem") ||  // macOS USB modem
            description.contains("usb") ||
            description.contains("serial") ||
            manufacturer.contains("ftdi") ||
            manufacturer.contains("prolific")) {

            DebugLogger::instance().info(QString("Found serial port: %1 (%2 - %3)")
                .arg(port.portName())
                .arg(port.description())
                .arg(port.manufacturer()));

            return port.systemLocation();
        }
    }

    // Fallback: return first available port
    if (!ports.isEmpty()) {
        DebugLogger::instance().warning("No USB serial port found, using first available port");
        return ports.first().systemLocation();
    }
#endif

    return QString();
}

void SerialWorker::openPort(const QString &portName) {
#ifdef Q_OS_LINUX
    if (m_is_open) {
        DebugLogger::instance().warning("Serial port already open");
        return;
    }

    QString selectedPort = portName;
    if (selectedPort.isEmpty()) {
        selectedPort = findSerialPort();
    }

    if (selectedPort.isEmpty()) {
        const QString errorMsg = "No serial ports found";
        DebugLogger::instance().error(errorMsg);
        emit portOpened(false, errorMsg);
        return;
    }

    m_serial_port->setPortName(selectedPort);
    m_serial_port->setBaudRate(BAUD_RATE);
    m_serial_port->setDataBits(QSerialPort::Data8);
    m_serial_port->setParity(QSerialPort::NoParity);
    m_serial_port->setStopBits(QSerialPort::OneStop);
    m_serial_port->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serial_port->open(QIODevice::ReadWrite)) {
        m_is_open = true;

        connect(m_serial_port, &QSerialPort::readyRead,
                this, &SerialWorker::handleReadyRead);
        connect(m_serial_port, &QSerialPort::errorOccurred,
                this, &SerialWorker::handleError);

        const QString successMsg = QString("Serial port opened: %1 at %2 baud")
            .arg(selectedPort).arg(BAUD_RATE);
        DebugLogger::instance().info(successMsg);
        emit portOpened(true, successMsg);

        // Send welcome message
        sendResponse("# AllesSpitze Serial Interface Ready\n");
        sendResponse("# Commands: POWER_ON, POWER_OFF, SET_BALANCE <value>, SET_PROB <json>, STATUS\n");
    } else {
        const QString errorMsg = QString("Failed to open serial port %1: %2")
            .arg(selectedPort).arg(m_serial_port->errorString());
        DebugLogger::instance().error(errorMsg);
        emit portOpened(false, errorMsg);
    }
#else
    Q_UNUSED(portName);
    DebugLogger::instance().info("Serial port support disabled on macOS - development mode");
    emit portOpened(false, "Serial port not available on macOS");
#endif
}

void SerialWorker::closePort() {
#ifdef Q_OS_LINUX
    if (m_serial_port && m_is_open) {
        m_serial_port->close();
        m_is_open = false;
        DebugLogger::instance().info("Serial port closed");
        emit portClosed();
    }
#endif
}

#ifdef Q_OS_LINUX
void SerialWorker::handleReadyRead() {
    m_read_buffer.append(m_serial_port->readAll());

    // Process complete lines (terminated by \n or \r\n)
    while (m_read_buffer.contains('\n')) {
        const int newlinePos = m_read_buffer.indexOf('\n');
        QByteArray line = m_read_buffer.left(newlinePos);
        m_read_buffer.remove(0, newlinePos + 1);

        // Remove trailing \r if present
        if (line.endsWith('\r')) {
            line.chop(1);
        }

        if (!line.isEmpty()) {
            const QString command = QString::fromUtf8(line).trimmed();
            DebugLogger::instance().verbose("Serial RX: " + command);
            processCommand(command);
        }
    }
}

void SerialWorker::handleError(QSerialPort::SerialPortError error) {
    if (error != QSerialPort::NoError && error != QSerialPort::TimeoutError) {
        const QString errorMsg = QString("Serial port error: %1").arg(m_serial_port->errorString());
        DebugLogger::instance().error(errorMsg);
        emit errorOccurred(errorMsg);
    }
}
#endif

void SerialWorker::processCommand(const QString &line) {
    const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return;
    }

    const QString cmd = parts[0].toUpper();
    QVariantMap params;

    if (cmd == "POWER_ON" || cmd == "ON") {
        sendResponse("OK: Powering on\n");
        emit commandReceived(Command::PowerOn, params);

    } else if (cmd == "POWER_OFF" || cmd == "OFF") {
        sendResponse("OK: Powering off\n");
        emit commandReceived(Command::PowerOff, params);

    } else if (cmd == "SET_BALANCE" || cmd == "BALANCE") {
        if (parts.size() < 2) {
            sendResponse("ERROR: SET_BALANCE requires value (e.g., SET_BALANCE 100.5)\n");
            return;
        }

        bool ok;
        const double balance = parts[1].toDouble(&ok);
        if (!ok || balance < 0) {
            sendResponse("ERROR: Invalid balance value\n");
            return;
        }

        params["balance"] = balance;
        sendResponse(QString("OK: Balance set to %1\n").arg(balance));
        emit commandReceived(Command::SetBalance, params);

    } else if (cmd == "SET_PROB" || cmd == "PROBABILITIES") {
        if (parts.size() < 2) {
            sendResponse("ERROR: SET_PROB requires JSON (e.g., SET_PROB {\"coin\":10,\"kleeblatt\":15})\n");
            return;
        }

        // Reconstruct JSON from remaining parts
        QString jsonStr = line.mid(cmd.length()).trimmed();
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());

        if (!doc.isObject()) {
            sendResponse("ERROR: Invalid JSON format\n");
            return;
        }

        const QJsonObject obj = doc.object();
        QVariantMap probMap;

        // Expected keys: coin, kleeblatt, marienkaefer, sonne, teufel
        const QStringList validKeys = {"coin", "kleeblatt", "marienkaefer", "sonne", "teufel"};
        for (const QString &key : validKeys) {
            if (obj.contains(key)) {
                probMap[key] = obj[key].toInt();
            }
        }

        if (probMap.isEmpty()) {
            sendResponse("ERROR: No valid probabilities found\n");
            return;
        }

        params["probabilities"] = probMap;
        sendResponse("OK: Probabilities updated\n");
        emit commandReceived(Command::SetProbabilities, params);

    } else if (cmd == "STATUS" || cmd == "?") {
        sendStatus();

    } else {
        sendResponse("ERROR: Unknown command. Available: POWER_ON, POWER_OFF, SET_BALANCE, SET_PROB, STATUS\n");
    }
}

void SerialWorker::sendStatus() {
    // Status will be filled by ApplicationController
    emit commandReceived(Command::GetStatus, QVariantMap());
}

void SerialWorker::sendResponse(const QString &response) {
#ifdef Q_OS_LINUX
    if (m_is_open && m_serial_port) {
        m_serial_port->write(response.toUtf8());
        m_serial_port->flush();
        DebugLogger::instance().verbose("Serial TX: " + response.trimmed());
    }
#else
    Q_UNUSED(response);
    // On macOS, just log it
    // DebugLogger::instance().verbose("Serial TX (simulated): " + response.trimmed());
#endif
}
