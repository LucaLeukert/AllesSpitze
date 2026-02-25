#include "HardwarePanelBackendFactory.h"

#include <QDebug>

#include "HardwarePanelBackend.h"
#include "I2CHardwarePanelBackend.h"
#include "StubSimulatedHardwarePanelBackend.h"

std::unique_ptr<HardwarePanelBackend> createHardwarePanelBackend() {
    const QByteArray backend = qgetenv("ALLESSPITZE_PANEL_BACKEND").trimmed().toLower();
    if (backend == "sim") {
        qInfo() << "Using hardware panel backend: stub simulator";
        return std::make_unique<StubSimulatedHardwarePanelBackend>();
    }

    if (!backend.isEmpty() && backend != "i2c") {
        qWarning() << "Unknown ALLESSPITZE_PANEL_BACKEND=" << backend << "- falling back to i2c";
    }
    qInfo() << "Using hardware panel backend: i2c";
    return std::make_unique<I2CHardwarePanelBackend>();
}
