#include "DebugLogger.h"
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QColor>

DebugLogger& DebugLogger::instance() {
    static DebugLogger instance;
    return instance;
}

DebugLogger::DebugLogger(QObject *parent) : QObject(parent) {
    openLogFile();
}

DebugLogger::~DebugLogger() {
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void DebugLogger::setVerbosity(LogVerbosity verbosity) {
    if (m_verbosity != verbosity) {
        m_verbosity = verbosity;

        QString msg = QString("Logging verbosity changed to: %1")
            .arg(verbosity == LogVerbosity::Verbose ? "VERBOSE" : "NORMAL");
        info(msg);

        emit verbosityChanged();
    }
}

void DebugLogger::openLogFile() {
    QString logDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation
    );

    QDir().mkpath(logDir);

    QString timestamp = QDateTime::currentDateTime()
        .toString("yyyy-MM-dd_HH-mm-ss");
    QString logFileName = QString("%1/debug_%2.log")
        .arg(logDir)
        .arg(timestamp);

    m_logFile.setFileName(logFileName);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString openMsg = QString("Log file opened: %1").arg(logFileName);
        logMessage(openMsg, LogLevel::Debug);
        qDebug() << openMsg;
    } else {
        qWarning() << "Failed to open log file:" << logFileName;
    }
}

void DebugLogger::writeToLogFile(const QString& message) {
    if (m_logFile.isOpen()) {
        QString timestamp = QDateTime::currentDateTime()
            .toString("yyyy-MM-dd HH:mm:ss.zzz");
        QString logEntry = QString("[%1] %2\n")
            .arg(timestamp)
            .arg(message);
        m_logFile.write(logEntry.toUtf8());
        m_logFile.flush();
    }
}

bool DebugLogger::shouldLog(LogLevel level, bool verboseOnly) const {
    // Verbose messages only shown in Verbose mode
    if (verboseOnly && m_verbosity != LogVerbosity::Verbose) {
        return false;
    }

    // In Normal mode, skip Debug messages
    if (m_verbosity == LogVerbosity::Normal && level == LogLevel::Debug) {
        return false;
    }

    return true;
}

void DebugLogger::logMessage(const QString& message, LogLevel level,
                              bool verboseOnly) {
    if (!shouldLog(level, verboseOnly)) {
        return; // Skip this message
    }

    QString timestamp = QDateTime::currentDateTime()
        .toString("HH:mm:ss.zzz");
    QString levelStr = levelToString(level);

    QString formattedMessage = QString("[%1] [%2] %3")
        .arg(timestamp)
        .arg(levelStr)
        .arg(message);

    // Add to in-memory log
    m_log_text += formattedMessage + "\n";

    // Keep only last 10000 characters in memory
    if (m_log_text.length() > 10000) {
        m_log_text = m_log_text.right(10000);
    }

    // Write to file (always write everything to file)
    writeToLogFile(formattedMessage);

    emit logTextChanged();

    // Also output to console
    qDebug().noquote() << formattedMessage;
}

QString DebugLogger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO ";
        case LogLevel::Warning:  return "WARN ";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRIT ";
        default:                 return "UNKNOWN";
    }
}

QColor DebugLogger::levelToColor(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:    return {128, 128, 128};
        case LogLevel::Info:     return {0, 128, 255};
        case LogLevel::Warning:  return {255, 165, 0};
        case LogLevel::Error:    return {255, 0, 0};
        case LogLevel::Critical: return {139, 0, 0};
        default:                 return {0, 0, 0};
    }
}

void DebugLogger::debug(const QString& message) {
    logMessage(message, LogLevel::Debug);
}

void DebugLogger::info(const QString& message) {
    logMessage(message, LogLevel::Info);
}

void DebugLogger::warning(const QString& message) {
    logMessage(message, LogLevel::Warning);
}

void DebugLogger::error(const QString& message) {
    logMessage(message, LogLevel::Error);
}

void DebugLogger::critical(const QString& message) {
    logMessage(message, LogLevel::Critical);
}

void DebugLogger::verbose(const QString& message) {
    logMessage(message, LogLevel::Debug, true); // verboseOnly = true
}

void DebugLogger::log(const QString& message) {
    info(message); // Map legacy log() to info()
}

void DebugLogger::clearLog() {
    m_log_text.clear();
    emit logTextChanged();
}

QString DebugLogger::formatHexDump(const QByteArray& data) {
    return formatHexDump(reinterpret_cast<const uint8_t*>(data.data()),
                         data.size());
}

QString DebugLogger::formatHexDump(const uint8_t* data, int length) {
    QString result;
    for (int i = 0; i < length; ++i) {
        result += QString("0x%1").arg(data[i], 2, 16, QChar('0'));
        if (i < length - 1) {
            result += " ";
        }
    }
    return result;
}