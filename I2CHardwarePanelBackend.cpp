#include "I2CHardwarePanelBackend.h"

#include <QMetaObject>

#include "DebugLogger.h"

I2CHardwarePanelBackend::I2CHardwarePanelBackend(QObject *parent)
    : HardwarePanelBackend(parent)
    , m_thread(new QThread)
    , m_worker(new I2CWorker) {
    m_worker->moveToThread(m_thread.data());

    connect(m_thread.data(), &QThread::started,
            m_worker.data(), &I2CWorker::initialize);

    connect(this, &I2CHardwarePanelBackend::requestInitialize,
            m_worker.data(), &I2CWorker::initialize, Qt::QueuedConnection);
    connect(this, &I2CHardwarePanelBackend::requestCleanup,
            m_worker.data(), &I2CWorker::cleanup, Qt::QueuedConnection);
    connect(this, &I2CHardwarePanelBackend::requestOpenDevice,
            m_worker.data(), &I2CWorker::openDevice, Qt::QueuedConnection);
    connect(this, &I2CHardwarePanelBackend::requestStartPolling,
            m_worker.data(), &I2CWorker::startPolling, Qt::QueuedConnection);
    connect(this, &I2CHardwarePanelBackend::requestStopPolling,
            m_worker.data(), &I2CWorker::stopPolling, Qt::QueuedConnection);
    connect(this, &I2CHardwarePanelBackend::requestSendHealthCheck,
            m_worker.data(), &I2CWorker::sendHealthCheck, Qt::QueuedConnection);
    connect(this, &I2CHardwarePanelBackend::requestHighlightButton,
            m_worker.data(), &I2CWorker::highlightButton, Qt::QueuedConnection);
    connect(this, &I2CHardwarePanelBackend::requestHighlightTower,
            m_worker.data(), &I2CWorker::highlightTower, Qt::QueuedConnection);
    connect(this, &I2CHardwarePanelBackend::requestUpdateUserBalance,
            m_worker.data(), &I2CWorker::updateUserBalance, Qt::QueuedConnection);
    connect(this, &I2CHardwarePanelBackend::requestSendRawCommand,
            m_worker.data(), &I2CWorker::sendRawCommand, Qt::QueuedConnection);

    connectWorkerSignals();
}

I2CHardwarePanelBackend::~I2CHardwarePanelBackend() {
    shutdownBackend();
}

void I2CHardwarePanelBackend::connectWorkerSignals() {
    connect(m_worker.data(), &I2CWorker::deviceOpened,
            this, &I2CHardwarePanelBackend::panelOpened);
    connect(m_worker.data(), &I2CWorker::healthCheckComplete,
            this, &I2CHardwarePanelBackend::healthCheckComplete);
    connect(m_worker.data(), &I2CWorker::buttonEventsReceived,
            this, &I2CHardwarePanelBackend::buttonEventsReceived);
    connect(m_worker.data(), &I2CWorker::rawCommandResponse,
            this, &I2CHardwarePanelBackend::rawCommandResponse);
    connect(m_worker.data(), &I2CWorker::operationError,
            this, &I2CHardwarePanelBackend::backendError);
    connect(m_worker.data(), &I2CWorker::initComplete,
            this, [this](bool success, uint8_t status) {
                if (success && status == 0x00) {
                    emit backendReady();
                } else {
                    emit backendError(QString("Panel init failed (status=0x%1)")
                        .arg(status, 2, 16, QChar('0')));
                }
            });
}

void I2CHardwarePanelBackend::initializeBackend() {
    if (m_started) {
        emit requestInitialize();
        return;
    }
    m_started = true;
    m_thread->start();
}

void I2CHardwarePanelBackend::shutdownBackend() {
    if (!m_thread) {
        return;
    }

    if (m_thread->isRunning()) {
        QMetaObject::invokeMethod(m_worker.data(), "stopPolling", Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(m_worker.data(), "cleanup", Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
    }
    m_started = false;
}

void I2CHardwarePanelBackend::openPanel() {
    emit requestOpenDevice(m_deviceAddress);
}

void I2CHardwarePanelBackend::startMonitoring() {
    emit requestStartPolling(200);
}

void I2CHardwarePanelBackend::stopMonitoring() {
    emit requestStopPolling();
}

void I2CHardwarePanelBackend::sendHealthCheck() {
    emit requestSendHealthCheck();
}

void I2CHardwarePanelBackend::setButtonHighlight(uint8_t buttonId, bool state) {
    emit requestHighlightButton(buttonId, state);
}

void I2CHardwarePanelBackend::setTowerLevel(uint8_t towerId, uint8_t row) {
    emit requestHighlightTower(towerId, row);
}

void I2CHardwarePanelBackend::setDisplayedBalance(double balance) {
    emit requestUpdateUserBalance(balance);
}

void I2CHardwarePanelBackend::sendRawPanelCommand(uint8_t command, const QVariantList &data) {
    emit requestSendRawCommand(command, data);
}
