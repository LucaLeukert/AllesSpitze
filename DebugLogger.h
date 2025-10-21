#pragma once

#include <QObject>
#include <QString>
#include <QFile>

class DebugLogger : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)
    Q_PROPERTY(LogVerbosity verbosity READ verbosity WRITE setVerbosity
               NOTIFY verbosityChanged)

public:
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };
    Q_ENUM(LogLevel)

    enum class LogVerbosity {
        Normal,   // Info, Warning, Error, Critical only
        Verbose   // All messages including Debug (byte-level)
    };
    Q_ENUM(LogVerbosity)

    static DebugLogger& instance();
    QString logText() const { return m_log_text; }
    LogVerbosity verbosity() const { return m_verbosity; }
    void setVerbosity(LogVerbosity verbosity);

    // Regular logging methods
    Q_INVOKABLE void debug(const QString& message);
    Q_INVOKABLE void info(const QString& message);
    Q_INVOKABLE void warning(const QString& message);
    Q_INVOKABLE void error(const QString& message);
    Q_INVOKABLE void critical(const QString& message);

    // Verbose-only logging (always Debug level, only shown in Verbose mode)
    void verbose(const QString& message);

    Q_INVOKABLE void log(const QString& message); // Legacy, maps to info
    Q_INVOKABLE void clearLog();

    // Helper for formatting hex dumps
    static QString formatHexDump(const QByteArray& data);
    static QString formatHexDump(const uint8_t* data, int length);

signals:
    void logTextChanged();
    void verbosityChanged();

private:
    explicit DebugLogger(QObject *parent = nullptr);
    ~DebugLogger() override;

    QString m_log_text;
    QFile m_logFile;
    LogVerbosity m_verbosity = LogVerbosity::Normal;

    void openLogFile();
    void writeToLogFile(const QString& message);
    void logMessage(const QString& message, LogLevel level,
                    bool verboseOnly = false);
    static QString levelToString(LogLevel level);
    static QColor levelToColor(LogLevel level);
    bool shouldLog(LogLevel level, bool verboseOnly) const;
};