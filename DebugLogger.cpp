#include "DebugLogger.h"
#include <QDateTime>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QColor>

DebugLogger::DebugLogger(QObject *parent) : QObject(parent) {
#ifdef QT_DEBUG
    openLogFile();
#endif
}

DebugLogger::~DebugLogger() {
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void DebugLogger::openLogFile() {
    const QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    (void) QDir().mkpath(logDir);

    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    const QString logPath = QString("%1/debug_%2.log").arg(logDir, timestamp);

    m_logFile.setFileName(logPath);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        qWarning() << "Failed to open log file:" << logPath;
        return;
    }

    log("Log file opened: " + logPath);
}

void DebugLogger::writeToLogFile(const QString &message) {
    if (!m_logFile.isOpen()) {
        return;
    }

    m_logFile.write(message.toUtf8());
    m_logFile.flush();
}

DebugLogger &DebugLogger::instance() {
    static DebugLogger instance;
    return instance;
}

QString DebugLogger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRIT";
        default: return "UNKNOWN";
    }
}

QColor DebugLogger::levelToColor(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return QColor("#808080");    // Grau
        case LogLevel::Info: return QColor("#FFFFFF");     // Weiß
        case LogLevel::Warning: return QColor("#FFA500");  // Orange
        case LogLevel::Error: return QColor("#FF0000");    // Rot
        case LogLevel::Critical: return QColor("#FF00FF"); // Magenta
        default: return QColor("#FFFFFF");
    }
}

void DebugLogger::logMessage(const QString& message, const LogLevel level) {
    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    const QString levelStr = levelToString(level);
    QString formattedMessage = QString("[%1] [%2] %3\n").arg(timestamp, levelStr, message);

    // Immer in die Datei schreiben
    writeToLogFile(formattedMessage);

    // Farbige Ausgabe für die UI
    const QString coloredMessage = QString("<font color='%1'>%2</font>")
        .arg(levelToColor(level).name(), formattedMessage.replace("\n", "<br>"));
    m_log_text.append(coloredMessage);
    emit logTextChanged();

    // Konsolenausgabe je nach Level
    switch (level) {
        case LogLevel::Debug:
            qDebug().noquote() << formattedMessage.trimmed();
            break;
        case LogLevel::Info:
            qInfo().noquote() << formattedMessage.trimmed();
            break;
        case LogLevel::Warning:
            qWarning().noquote() << formattedMessage.trimmed();
            break;
        case LogLevel::Error:
        case LogLevel::Critical:
            qCritical().noquote() << formattedMessage.trimmed();
            break;
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

void DebugLogger::log(const QString& message) {
    debug(message); // Legacy support
}
