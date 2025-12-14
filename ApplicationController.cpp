#include "ApplicationController.h"
#include <QtQml>
#include <QFile>
#include <QTextStream>
#include "DebugLogger.h"

ApplicationController::ApplicationController(QObject *parent)
    : QObject(parent)
      , m_engine(new QQmlApplicationEngine)
      , m_workerThread(new QThread)
      , m_worker(new I2CWorker)
      , m_slotMachine(new SlotMachine)
      , m_healthcheckTimer(new QTimer(this)) {
    m_healthcheckTimer->setInterval(1000);
}

ApplicationController::~ApplicationController() {
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void ApplicationController::initialize() {
    qDebug() << "Main/UI Thread ID:" << QThread::currentThreadId();

    setupQmlEngine();
    setupI2CWorker();
    setupSlotMachine();
    setupConnections();
    setupCleanup();

    QTimer::singleShot(200, this, [this]() {
        QMetaObject::invokeMethod(m_worker.data(), "openDevice",
                                  Q_ARG(uint8_t, 0x42));
    });

    QTimer::singleShot(500, this, [this]() {
        QMetaObject::invokeMethod(m_worker.data(), "highlightButton",
                                  Q_ARG(uint8_t, 0),
                                  Q_ARG(bool, true));
        startHealthcheck();
    });
}

bool ApplicationController::start() const {
    m_engine->load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    return !m_engine->rootObjects().isEmpty();
}

void ApplicationController::setupQmlEngine() const {
    qmlRegisterSingletonInstance("DebugTools", 1, 0, "DebugLogger",
                                 &DebugLogger::instance());
    qmlRegisterType<SlotReel>("SlotMachine", 1, 0, "SlotReel");
    qmlRegisterType<Tower>("SlotMachine", 1, 0, "Tower");

#ifdef QT_DEBUG
    m_engine->rootContext()->setContextProperty("isDebugBuild", true);
#else
    m_engine->rootContext()->setContextProperty("isDebugBuild", false);
#endif

    m_engine->rootContext()->setContextProperty("appController", const_cast<ApplicationController *>(this));
    m_engine->rootContext()->setContextProperty("slotMachine", m_slotMachine.data());
}

void ApplicationController::setupI2CWorker() {
    m_worker->moveToThread(m_workerThread.data());
    connect(m_workerThread.data(), &QThread::started,
            m_worker.data(), &I2CWorker::initialize);
    m_workerThread->start();
}

void ApplicationController::setupSlotMachine() const {
    loadBalance();
    m_slotMachine->setI2CWorker(m_worker.data());
}

void ApplicationController::setupConnections() {
    // Connect button events with proper cross-thread invocation
    connect(m_worker.data(), &I2CWorker::buttonEventsReceived,
            this, [this](const QVector<uint8_t> &buttons) {
                if (!buttons.isEmpty() && m_slotMachine->canSpin()) {
                    QMetaObject::invokeMethod(m_worker.data(), "highlightButton",
                                              Q_ARG(uint8_t, 0),
                                              Q_ARG(bool, false));
                    m_slotMachine->spin();
                }
            });

    //send balance on init complete
    connect(m_worker.data(), &I2CWorker::initialization_complete,
            this, [this]() {
                QMetaObject::invokeMethod(m_worker.data(), "updateUserBalance",
                                          Qt::QueuedConnection,
                                          Q_ARG(double, m_slotMachine->balance()));
            });

    // Re-enable button after spin
    connect(m_slotMachine.data(), &SlotMachine::canSpinChanged,
            this, [this]() {
                if (m_slotMachine->canSpin()) {
                    QMetaObject::invokeMethod(m_worker.data(), "highlightButton",
                                              Q_ARG(uint8_t, 0),
                                              Q_ARG(bool, true));
                }
            });

    // Connect healthcheck response
    connect(m_worker.data(), &I2CWorker::healthCheckComplete,
            this, &ApplicationController::handleHealthcheckResponse);

    // Connect raw command response for debug interface
    connect(m_worker.data(), &I2CWorker::rawCommandResponse,
            this, &ApplicationController::handleRawCommandResponse);

    connect(m_slotMachine.data(), &SlotMachine::balanceChanged,
            this, [this]() {
                QMetaObject::invokeMethod(m_worker.data(), "updateUserBalance",
                                          Qt::QueuedConnection,
                                          Q_ARG(double, m_slotMachine->balance()));
            });
}

void ApplicationController::startHealthcheck() {
    // Use QMetaObject::invokeMethod for cross-thread call
    connect(m_healthcheckTimer.data(), &QTimer::timeout,
            this, [this]() {
                QMetaObject::invokeMethod(m_worker.data(), "sendHealthCheck",
                                          Qt::QueuedConnection);
            });
    m_healthcheckTimer->start();
}

void ApplicationController::handleHealthcheckResponse(const bool success, const uint8_t status) {
    DebugLogger::instance().verbose(QString("Healthcheck response received. Success: %1, Status: 0x%2")
        .arg(success)
        .arg(status, 2, 16, QChar('0')));

    if (success && status == 0) {
        m_consecutiveFailures = 0;
        return;
    }

    m_consecutiveFailures++;
    DebugLogger::instance().warning(
        QString("I2C Healthcheck failed. Status: 0x%1, Consecutive failures: %2")
        .arg(status, 2, 16, QChar('0'))
        .arg(m_consecutiveFailures)
    );

    if (m_consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
        DebugLogger::instance().error(
            "Too many consecutive I2C failures. Attempting recovery..."
        );
        m_healthcheckTimer->stop();

        QMetaObject::invokeMethod(m_worker.data(), "cleanup",
                                  Qt::QueuedConnection);

        // Attempt reinitialization after delay
        QTimer::singleShot(2000, this, [this]() {
            QMetaObject::invokeMethod(m_worker.data(), "initialize",
                                      Qt::QueuedConnection);

            QTimer::singleShot(500, this, [this]() {
                QMetaObject::invokeMethod(m_worker.data(), "openDevice",
                                          Qt::QueuedConnection,
                                          Q_ARG(uint8_t, 0x42));
                m_consecutiveFailures = 0;
                m_healthcheckTimer->start();
            });
        });
    }
}

void ApplicationController::setupCleanup() {
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        m_healthcheckTimer->stop();

        QMetaObject::invokeMethod(m_worker.data(), "stopPolling",
                                  Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(m_worker.data(), "cleanup",
                                  Qt::BlockingQueuedConnection);

        m_workerThread->quit();
        m_workerThread->wait();
    });
}

void ApplicationController::sendRawI2CCommand(const int command, const QVariantList &data) const {
    QMetaObject::invokeMethod(m_worker.data(), "sendRawCommand",
                              Qt::QueuedConnection,
                              Q_ARG(uint8_t, static_cast<uint8_t>(command)),
                              Q_ARG(QVariantList, data));
}

void ApplicationController::handleRawCommandResponse(const uint8_t command, const bool success, const QByteArray &response) {
    // Convert QByteArray to QVariantList for QML
    QVariantList responseList;
    for (const char i : response) {
        responseList.append(static_cast<uint8_t>(i));
    }
    emit i2cCommandResponse(command, success, responseList);
}

void ApplicationController::loadBalance() const {
    QFile file(SlotMachine::balanceFilePath());
    DebugLogger::instance().critical(SlotMachine::balanceFilePath());

    if (!file.exists()) {
        m_slotMachine->setBalance(100);  // Start with 100 units
        m_slotMachine->saveBalance();
        DebugLogger::instance().info(QString("No balance file found. Starting with 100 units"));
        return;
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        bool ok;
        const double balance = in.readLine().toDouble(&ok);
        if (ok) {
            m_slotMachine->setBalance(balance);
            DebugLogger::instance().info(QString("Balance loaded: %1 units").arg(balance));
        } else {
            m_slotMachine->setBalance(100);
            DebugLogger::instance().warning("Invalid balance file, resetting to 100");
        }
        file.close();
    } else {
        m_slotMachine->setBalance(100);
        DebugLogger::instance().error("Could not open balance file");
    }
}
