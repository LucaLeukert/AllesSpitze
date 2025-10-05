#include "I2CWorker.h"
#include <QDebug>

I2CWorker::I2CWorker(QObject *parent)
    : QObject(parent) {
}

I2CWorker::~I2CWorker() {
    cleanup();
}

void I2CWorker::initialize() {
    if (m_isInitialized) {
        return;
    }

    qDebug() << "I2C Thread ID:" << QThread::currentThreadId();

    // Hier würde die tatsächliche I2C-Initialisierung stattfinden
    m_isInitialized = true;
    emit initialization_complete();
}

void I2CWorker::cleanup() {
    if (!m_isInitialized) {
        return;
    }

    // Hier würde die I2C-Cleanup stattfinden
    m_isInitialized = false;
}
