#pragma once

#include <array>
#include "HardwarePanelBackend.h"

class StubSimulatedHardwarePanelBackend : public HardwarePanelBackend {
    Q_OBJECT

public:
    explicit StubSimulatedHardwarePanelBackend(QObject *parent = nullptr);

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

private:
    bool m_initialized{false};
    bool m_open{false};
    std::array<bool, 2> m_buttons{false, false};
    std::array<uint8_t, 3> m_towers{0, 0, 0};
    double m_balance{0.0};
};
