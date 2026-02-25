#include "StubSimulatedHardwarePanelBackend.h"

#include <QTimer>

#include "DebugLogger.h"

StubSimulatedHardwarePanelBackend::StubSimulatedHardwarePanelBackend(QObject *parent)
    : HardwarePanelBackend(parent) {}

void StubSimulatedHardwarePanelBackend::initializeBackend() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;
    QTimer::singleShot(0, this, [this]() {
        DebugLogger::instance().info("StubSim backend initialized");
    });
}

void StubSimulatedHardwarePanelBackend::shutdownBackend() {
    m_open = false;
    m_initialized = false;
}

void StubSimulatedHardwarePanelBackend::openPanel() {
    if (!m_initialized) {
        initializeBackend();
    }
    m_open = true;
    QTimer::singleShot(0, this, [this]() {
        emit panelOpened(true, QStringLiteral("Stub simulator panel opened"));
        emit backendReady();
    });
}

void StubSimulatedHardwarePanelBackend::startMonitoring() {}

void StubSimulatedHardwarePanelBackend::stopMonitoring() {}

void StubSimulatedHardwarePanelBackend::sendHealthCheck() {
    if (!m_open) {
        emit healthCheckComplete(false, 0xFF);
        return;
    }
    emit healthCheckComplete(true, 0x00);
}

void StubSimulatedHardwarePanelBackend::setButtonHighlight(uint8_t buttonId, bool state) {
    if (buttonId < m_buttons.size()) {
        m_buttons[buttonId] = state;
    }
}

void StubSimulatedHardwarePanelBackend::setTowerLevel(uint8_t towerId, uint8_t row) {
    if (towerId < m_towers.size()) {
        m_towers[towerId] = row;
    }
}

void StubSimulatedHardwarePanelBackend::setDisplayedBalance(double balance) {
    m_balance = balance;
    Q_UNUSED(m_balance);
}

void StubSimulatedHardwarePanelBackend::sendRawPanelCommand(uint8_t command, const QVariantList &data) {
    QByteArray response;
    response.append(static_cast<char>(command | 0x80));
    response.append(static_cast<char>(data.size() + 1));
    response.append(static_cast<char>(0x00));
    for (const QVariant &v : data) {
        response.append(static_cast<char>(v.toInt() & 0xFF));
    }
    emit rawCommandResponse(command, true, response);
}
