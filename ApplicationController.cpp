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
      , m_serialThread(new QThread)
      , m_serialWorker(new SerialWorker)
      , m_slotMachine(new SlotMachine)
      , m_healthcheckTimer(new QTimer(this)) {
    m_healthcheckTimer->setInterval(1000);
}

ApplicationController::~ApplicationController() {
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
    if (m_serialThread) {
        m_serialThread->quit();
        m_serialThread->wait();
    }
}

void ApplicationController::initialize() {
    qDebug() << "Main/UI Thread ID:" << QThread::currentThreadId();

    setupQmlEngine();
    setupI2CWorker();
    setupSerialWorker();
    setupSlotMachine();
    setupConnections();
    setupCleanup();

    QTimer::singleShot(200, this, [this]() {
        QMetaObject::invokeMethod(m_worker.data(), "openDevice",
                                  Q_ARG(uint8_t, 0x42));
    });

    QTimer::singleShot(500, this, [this]() {
        applyPowerState();
        startHealthcheck();
    });

    // Open serial port after a delay
    QTimer::singleShot(1000, this, [this]() {
        QMetaObject::invokeMethod(m_serialWorker.data(), "openPort",
                                  Qt::QueuedConnection);
    });
}

bool ApplicationController::start() const {
    if (!m_engine) {
        qCritical() << "QML Engine is null!";
        return false;
    }

    m_engine->load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (m_engine->rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML - no root objects created";
        qCritical() << "QML Errors:" << m_engine->hasError();
        return false;
    }

    return true;
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

void ApplicationController::setupSerialWorker() {
    m_serialWorker->moveToThread(m_serialThread.data());
    connect(m_serialThread.data(), &QThread::started,
            m_serialWorker.data(), &SerialWorker::initialize);
    m_serialThread->start();
}

void ApplicationController::setupSlotMachine() const {
    loadBalance();
    m_slotMachine->setI2CWorker(m_worker.data());
}

void ApplicationController::setupConnections() {
    // Connect button events with proper cross-thread invocation
    connect(m_worker.data(), &I2CWorker::buttonEventsReceived,
            this, [this](const QVector<uint8_t> &buttons) {
                if (!m_powered_on) {
                    return;  // Ignore button events when powered off
                }
                for (const uint8_t buttonId : buttons) {
                    handleButtonPress(buttonId);
                }
            });

    //send balance on init complete
    connect(m_worker.data(), &I2CWorker::initialization_complete,
            this, [this]() {
                QMetaObject::invokeMethod(m_worker.data(), "updateUserBalance",
                                          Qt::QueuedConnection,
                                          Q_ARG(double, m_slotMachine->balance()));
                // Initialize button states
                updateButtonStates();
            });

    // Update button highlights when risk mode changes
    connect(m_slotMachine.data(), &SlotMachine::riskModeChanged,
            this, [this]() {
                updateButtonStates();
            });

    // Update button highlights when can spin changes
    connect(m_slotMachine.data(), &SlotMachine::canSpinChanged,
            this, [this]() {
                updateButtonStates();
            });

    // Update button highlights when session active changes
    connect(m_slotMachine.data(), &SlotMachine::sessionActiveChanged,
            this, [this]() {
                updateButtonStates();
            });

    // Update button highlights when prize changes
    connect(m_slotMachine.data(), &SlotMachine::currentPrizeChanged,
            this, [this]() {
                updateButtonStates();
            });

    // Update button highlights when risk animating changes
    connect(m_slotMachine.data(), &SlotMachine::riskAnimatingChanged,
            this, [this]() {
                updateButtonStates();
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

    // Serial worker connections
    connect(m_serialWorker.data(), &SerialWorker::commandReceived,
            this, &ApplicationController::handleSerialCommand);

    connect(m_serialWorker.data(), &SerialWorker::portOpened,
            this, [](bool success, const QString &message) {
                if (success) {
                    DebugLogger::instance().info("Serial: " + message);
                } else {
                    DebugLogger::instance().warning("Serial: " + message);
                }
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

        QMetaObject::invokeMethod(m_serialWorker.data(), "cleanup",
                                  Qt::BlockingQueuedConnection);

        m_workerThread->quit();
        m_workerThread->wait();

        m_serialThread->quit();
        m_serialThread->wait();
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

void ApplicationController::handleButtonPress(uint8_t buttonId) {
    if (!m_powered_on) {
        return;  // Ignore button presses when powered off
    }

    DebugLogger::instance().info(QString("Button %1 pressed").arg(buttonId));

    if (m_slotMachine->riskModeActive()) {
        // Risk mode active
        if (buttonId == 0) {
            // Button 0: Risk Higher (if not animating)
            if (!m_slotMachine->riskAnimating()) {
                m_slotMachine->riskHigher();
                DebugLogger::instance().info("Risk Higher triggered by button 0");
            }
        } else if (buttonId == 1) {
            // Button 1: Collect Prize (if not animating)
            if (!m_slotMachine->riskAnimating()) {
                m_slotMachine->collectRiskPrize();
                DebugLogger::instance().info("Collect Prize triggered by button 1");
            }
        }
    } else {
        // Normal slot machine mode
        if (buttonId == 0) {
            // Button 0: Spin
            if (m_slotMachine->canSpin()) {
                m_slotMachine->spin();
                DebugLogger::instance().info("Spin triggered by button 0");
            }
        } else if (buttonId == 1) {
            // Button 1: Cashout
            if (m_slotMachine->currentPrize() > 0) {
                m_slotMachine->cashout();
                DebugLogger::instance().info("Cashout triggered by button 1");
            }
        }
    }

    // Update button states after action
    QTimer::singleShot(100, this, [this]() {
        updateButtonStates();
    });
}

void ApplicationController::updateButtonStates() const {
    if (!m_powered_on) {
        // Power off - disable all buttons
        QMetaObject::invokeMethod(m_worker.data(), "highlightButton",
                                  Qt::QueuedConnection,
                                  Q_ARG(uint8_t, 0),
                                  Q_ARG(bool, false));
        QMetaObject::invokeMethod(m_worker.data(), "highlightButton",
                                  Qt::QueuedConnection,
                                  Q_ARG(uint8_t, 1),
                                  Q_ARG(bool, false));
        return;
    }

    if (m_slotMachine->riskModeActive()) {
        // Risk mode - update button highlights
        const bool canRisk = !m_slotMachine->riskAnimating() && m_slotMachine->riskLevel() < 7;
        const bool canCollect = !m_slotMachine->riskAnimating();

        // Button 0: Risk Higher (orange/active when can risk)
        QMetaObject::invokeMethod(m_worker.data(), "highlightButton",
                                  Qt::QueuedConnection,
                                  Q_ARG(uint8_t, 0),
                                  Q_ARG(bool, canRisk));

        // Button 1: Collect Prize (green/active when can collect)
        QMetaObject::invokeMethod(m_worker.data(), "highlightButton",
                                  Qt::QueuedConnection,
                                  Q_ARG(uint8_t, 1),
                                  Q_ARG(bool, canCollect));

        DebugLogger::instance().verbose(QString("Risk mode buttons updated: Risk=%1, Collect=%2")
            .arg(canRisk).arg(canCollect));
    } else {
        // Slot machine mode - update button highlights
        const bool canSpin = m_slotMachine->canSpin() && !m_slotMachine->isSpinning();
        const bool canCashout = m_slotMachine->currentPrize() > 0 && !m_slotMachine->isSpinning() && m_slotMachine->canSpin();

        // Button 0: Spin (active when can spin)
        QMetaObject::invokeMethod(m_worker.data(), "highlightButton",
                                  Qt::QueuedConnection,
                                  Q_ARG(uint8_t, 0),
                                  Q_ARG(bool, canSpin));

        // Button 1: Cashout (active when has prize)
        QMetaObject::invokeMethod(m_worker.data(), "highlightButton",
                                  Qt::QueuedConnection,
                                  Q_ARG(uint8_t, 1),
                                  Q_ARG(bool, canCashout));

        DebugLogger::instance().verbose(QString("Slot mode buttons updated: Spin=%1, Cashout=%2")
            .arg(canSpin).arg(canCashout));
    }
}

void ApplicationController::setPowerOn(bool on) {
    if (m_powered_on == on) {
        return;
    }

    m_powered_on = on;
    DebugLogger::instance().info(QString("Power state changed to: %1").arg(on ? "ON" : "OFF"));

    applyPowerState();
    emit poweredOnChanged();
}

void ApplicationController::applyPowerState() {
    if (m_powered_on) {
        // Power ON
        DebugLogger::instance().info("Applying POWER ON state");

        // Turn off all tower LEDs first
        for (int t = 0; t < 3; t++) {
            QMetaObject::invokeMethod(m_worker.data(), "highlightTower",
                                      Qt::QueuedConnection,
                                      Q_ARG(uint8_t, t),
                                      Q_ARG(uint8_t, 0));
        }

        // Update button states based on current game state
        updateButtonStates();

        // Update balance on display
        QMetaObject::invokeMethod(m_worker.data(), "updateUserBalance",
                                  Qt::QueuedConnection,
                                  Q_ARG(double, m_slotMachine->balance()));
    } else {
        // Power OFF
        DebugLogger::instance().info("Applying POWER OFF state");

        // Turn off all buttons
        QMetaObject::invokeMethod(m_worker.data(), "highlightButton",
                                  Qt::QueuedConnection,
                                  Q_ARG(uint8_t, 0),
                                  Q_ARG(bool, false));
        QMetaObject::invokeMethod(m_worker.data(), "highlightButton",
                                  Qt::QueuedConnection,
                                  Q_ARG(uint8_t, 1),
                                  Q_ARG(bool, false));

        // Turn off all tower LEDs
        for (int t = 0; t < 3; t++) {
            QMetaObject::invokeMethod(m_worker.data(), "highlightTower",
                                      Qt::QueuedConnection,
                                      Q_ARG(uint8_t, t),
                                      Q_ARG(uint8_t, 0));
        }

        // Clear display (set balance to 0 or send blank)
        QMetaObject::invokeMethod(m_worker.data(), "updateUserBalance",
                                  Qt::QueuedConnection,
                                  Q_ARG(double, 0.0));
    }
}

void ApplicationController::handleSerialCommand(SerialWorker::Command cmd, const QVariantMap &params) {
    switch (cmd) {
        case SerialWorker::Command::PowerOn:
            DebugLogger::instance().info("Serial: POWER_ON command received");
            setPowerOn(true);
            break;

        case SerialWorker::Command::PowerOff:
            DebugLogger::instance().info("Serial: POWER_OFF command received");
            setPowerOn(false);
            break;

        case SerialWorker::Command::SetBalance:
            if (params.contains("balance")) {
                const double newBalance = params["balance"].toDouble();
                DebugLogger::instance().info(QString("Serial: SET_BALANCE command received: %1").arg(newBalance));
                m_slotMachine->setBalance(newBalance);
                m_slotMachine->saveBalance();
            }
            break;

        case SerialWorker::Command::SetProbabilities:
            if (params.contains("probabilities")) {
                const QVariantMap probMap = params["probabilities"].toMap();
                DebugLogger::instance().info(QString("Serial: SET_PROBABILITIES command received"));

                // Find the reel in QML and update probabilities
                if (m_engine && !m_engine->rootObjects().isEmpty()) {
                    QObject *root = m_engine->rootObjects().first();
                    QObject *reel = root->findChild<QObject*>("mainReel");
                    if (reel) {
                        QMetaObject::invokeMethod(reel, "set_probabilities",
                                                  Q_ARG(QVariantMap, probMap));
                        DebugLogger::instance().info("Probabilities updated on reel");
                    } else {
                        DebugLogger::instance().warning("Could not find reel object");
                    }
                }
            }
            break;

        case SerialWorker::Command::GetStatus:
            DebugLogger::instance().verbose("Serial: STATUS command received");
            sendSerialStatus();
            break;

        default:
            DebugLogger::instance().warning("Serial: Unknown command received");
            break;
    }
}

void ApplicationController::sendSerialStatus() const {
    QString status = QString(
        "=== AllesSpitze Status ===\n"
        "Power: %1\n"
        "Balance: %2\n"
        "Bet: %3\n"
        "Current Prize: %4\n"
        "Session Active: %5\n"
        "Risk Mode: %6\n"
        "Risk Level: %7\n"
        "Risk Prize: %8\n"
        "==========================\n"
    ).arg(m_powered_on ? "ON" : "OFF")
     .arg(m_slotMachine->balance())
     .arg(m_slotMachine->bet())
     .arg(m_slotMachine->currentPrize())
     .arg(m_slotMachine->sessionActive() ? "YES" : "NO")
     .arg(m_slotMachine->riskModeActive() ? "YES" : "NO")
     .arg(m_slotMachine->riskLevel())
     .arg(m_slotMachine->riskPrize());

    // Send via serial worker - use a direct call with the captured status
    QMetaObject::invokeMethod(m_serialWorker.data(), "sendResponse",
                              Qt::QueuedConnection,
                              Q_ARG(QString, status));
}
