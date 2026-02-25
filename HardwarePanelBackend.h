#pragma once

#include <QObject>
#include <QByteArray>
#include <QVariantList>
#include <QVector>
#include <cstdint>

class HardwarePanelBackend : public QObject {
    Q_OBJECT

public:
    explicit HardwarePanelBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~HardwarePanelBackend() override = default;

public slots:
    virtual void initializeBackend() = 0;
    virtual void shutdownBackend() = 0;
    virtual void openPanel() = 0;
    virtual void startMonitoring() = 0;
    virtual void stopMonitoring() = 0;
    virtual void sendHealthCheck() = 0;
    virtual void setButtonHighlight(uint8_t buttonId, bool state) = 0;
    virtual void setTowerLevel(uint8_t towerId, uint8_t row) = 0;
    virtual void setDisplayedBalance(double balance) = 0;
    virtual void sendRawPanelCommand(uint8_t command, const QVariantList &data) = 0;

signals:
    void backendReady();
    void panelOpened(bool success, const QString &message);
    void healthCheckComplete(bool success, uint8_t status);
    void buttonEventsReceived(const QVector<uint8_t> &buttonIds);
    void rawCommandResponse(uint8_t command, bool success, const QByteArray &response);
    void backendError(const QString &error);
};
