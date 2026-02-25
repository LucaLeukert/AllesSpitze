#pragma once

#include <QScopedPointer>
#include <QThread>
#include <QTimer>
#include "HardwarePanelBackend.h"
#include "I2CWorker.h"

class I2CHardwarePanelBackend : public HardwarePanelBackend {
    Q_OBJECT

public:
    explicit I2CHardwarePanelBackend(QObject *parent = nullptr);
    ~I2CHardwarePanelBackend() override;

public slots:
    void initializeBackend() override;
    void shutdownBackend() override;
    void openPanel() override;
    void startMonitoring() override;
    void stopMonitoring() override;
    void sendHealthCheck() override;
    void setButtonHighlight(uint8_t buttonId, bool state) override;
    void setTowerLevel(uint8_t towerId, uint8_t row) override;
    void setDisplayedBalance(double balance) override;
    void sendRawPanelCommand(uint8_t command, const QVariantList &data) override;

signals:
    void requestInitialize();
    void requestCleanup();
    void requestOpenDevice(uint8_t address);
    void requestStartPolling(int intervalMs);
    void requestStopPolling();
    void requestSendHealthCheck();
    void requestHighlightButton(uint8_t buttonId, bool state);
    void requestHighlightTower(uint8_t towerId, uint8_t row);
    void requestUpdateUserBalance(double balance);
    void requestSendRawCommand(uint8_t command, QVariantList data);

private:
    void connectWorkerSignals();

    QScopedPointer<QThread> m_thread;
    QScopedPointer<I2CWorker> m_worker;
    bool m_started{false};
    uint8_t m_deviceAddress{0x42};
};
