#pragma once

#include <QObject>
#include <QString>
#include <QFile>

class DebugLogger : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)

public:
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };
    Q_ENUM(LogLevel)

    static DebugLogger& instance();
    QString logText() const { return m_log_text; }

    Q_INVOKABLE void debug(const QString& message);
    Q_INVOKABLE void info(const QString& message);
    Q_INVOKABLE void warning(const QString& message);
    Q_INVOKABLE void error(const QString& message);
    Q_INVOKABLE void critical(const QString& message);
    Q_INVOKABLE void log(const QString& message); // Legacy support, maps to debug

signals:
    void logTextChanged();

private:
    explicit DebugLogger(QObject *parent = nullptr);
    ~DebugLogger() override;

    QString m_log_text;
    QFile m_logFile;

    void openLogFile();
    void writeToLogFile(const QString& message);
    void logMessage(const QString& message, LogLevel level);
    static QString levelToString(LogLevel level);
    static QColor levelToColor(LogLevel level);
};
