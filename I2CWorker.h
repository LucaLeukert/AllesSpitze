#pragma once

#include <QThread>

class I2CWorker : public QObject {
    Q_OBJECT

public:
    explicit I2CWorker(QObject *parent = nullptr);
    ~I2CWorker() override;

public slots:
    void initialize();
    void cleanup();

signals:
    void initialization_complete();
    void error_occurred(const QString &error);

private:
    bool m_isInitialized{false};
};
