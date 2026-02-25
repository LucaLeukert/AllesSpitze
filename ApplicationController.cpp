#include "ApplicationController.h"

#include <QtQml>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>

#include "DebugLogger.h"
#include "HardwarePanelBackend.h"
#include "HardwarePanelBackendFactory.h"

ApplicationController::ApplicationController(QObject *parent)
    : QObject(parent)
      , m_engine(new QQmlApplicationEngine)
      , m_panelBackend(createHardwarePanelBackend().release())
      , m_serialThread(new QThread)
      , m_serialWorker(new SerialWorker)
      , m_slotMachine(new SlotMachine)
      , m_audioEngine(new AudioEngine)
      , m_healthcheckTimer(new QTimer(this)) {
    m_healthcheckTimer->setInterval(1000);
}

ApplicationController::~ApplicationController() {
    if (m_panelBackend) {
        m_panelBackend->shutdownBackend();
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

    beginPanelStartupSequence();

    QTimer::singleShot(1000, this, [this]() {
        emit requestSerialOpenPort(QString());
    });
}

bool ApplicationController::start() const {
    if (!m_engine) {
        qCritical() << "QML Engine is null!";
        return false;
    }

    const bool noDisplay = qEnvironmentVariableIsEmpty("DISPLAY") &&
                           qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
    const bool offscreenPlatform = qEnvironmentVariable("QT_QPA_PLATFORM")
                                   .compare("offscreen", Qt::CaseInsensitive) == 0;
    if (noDisplay || offscreenPlatform) {
        DebugLogger::instance().warning("Headless mode detected: skipping QML UI load");
        return true;
    }

    m_engine->load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (m_engine->rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML - no root objects created";
        qCritical() << "QML Errors:" << m_engine->hasError();
        return false;
    }

    if (m_audioEngine) {
        m_audioEngine->playSfx("AllesSpitzeIntro");
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
    m_engine->rootContext()->setContextProperty("audioEngine", m_audioEngine.data());
}

void ApplicationController::setupI2CWorker() {
    if (!m_panelBackend) {
        qCritical() << "Hardware panel backend is null";
    }
}

void ApplicationController::setupSerialWorker() {
    m_serialWorker->moveToThread(m_serialThread.data());
    connect(m_serialThread.data(), &QThread::started,
            m_serialWorker.data(), &SerialWorker::initialize);
    connect(this, &ApplicationController::requestSerialOpenPort,
            m_serialWorker.data(), &SerialWorker::openPort, Qt::QueuedConnection);
    connect(this, &ApplicationController::requestSerialCleanup,
            m_serialWorker.data(), &SerialWorker::cleanup, Qt::QueuedConnection);
    connect(this, &ApplicationController::requestSerialSendResponse,
            m_serialWorker.data(), &SerialWorker::sendResponse, Qt::QueuedConnection);
    m_serialThread->start();
}

void ApplicationController::setupSlotMachine() const {
    loadBalance();
    m_slotMachine->setAudioEngine(m_audioEngine.data());
}

void ApplicationController::setupConnections() {
    if (m_panelBackend) {
        connect(m_panelBackend.data(), &HardwarePanelBackend::buttonEventsReceived,
                this, [this](const QVector<uint8_t> &buttons) {
                    if (!m_powered_on) {
                        return;
                    }
                    for (const uint8_t buttonId: buttons) {
                        handleButtonPress(buttonId);
                    }
                });

        connect(m_panelBackend.data(), &HardwarePanelBackend::healthCheckComplete,
                this, &ApplicationController::handleHealthcheckResponse);

        connect(m_panelBackend.data(), &HardwarePanelBackend::rawCommandResponse,
                this, &ApplicationController::handleRawCommandResponse);

        connect(m_panelBackend.data(), &HardwarePanelBackend::panelOpened,
                this, [](bool success, const QString &message) {
                    if (success) {
                        DebugLogger::instance().info("Panel: " + message);
                    } else {
                        DebugLogger::instance().warning("Panel: " + message);
                    }
                });

        connect(m_panelBackend.data(), &HardwarePanelBackend::backendReady,
                this, &ApplicationController::onPanelReady);

        connect(m_panelBackend.data(), &HardwarePanelBackend::backendError,
                this, [](const QString &error) {
                    DebugLogger::instance().warning("Panel backend: " + error);
                });
    }

    connect(m_slotMachine.data(), &SlotMachine::towerLevelChangedForHardware,
            this, [this](int towerId, int level) {
                sendTowerLevelIfChanged(static_cast<uint8_t>(towerId), static_cast<uint8_t>(level));
            });

    connect(m_slotMachine.data(), &SlotMachine::riskModeChanged,
            this, [this]() { updateButtonStates(); });
    connect(m_slotMachine.data(), &SlotMachine::canSpinChanged,
            this, [this]() { updateButtonStates(); });
    connect(m_slotMachine.data(), &SlotMachine::sessionActiveChanged,
            this, [this]() { updateButtonStates(); });
    connect(m_slotMachine.data(), &SlotMachine::currentPrizeChanged,
            this, [this]() { updateButtonStates(); });
    connect(m_slotMachine.data(), &SlotMachine::riskAnimatingChanged,
            this, [this]() { updateButtonStates(); });

    connect(m_slotMachine.data(), &SlotMachine::balanceChanged,
            this, [this]() {
                sendBalanceIfChanged(m_slotMachine->balance());
            });

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
    connect(m_healthcheckTimer.data(), &QTimer::timeout,
            this, [this]() {
                if (m_panelBackend) {
                    m_panelBackend->sendHealthCheck();
                }
            }, Qt::UniqueConnection);
    m_healthcheckTimer->start();
}

void ApplicationController::beginPanelStartupSequence() {
    if (!m_panelBackend) {
        return;
    }

    m_state = ControllerState::Starting;
    m_panelBackend->initializeBackend();
    m_panelBackend->openPanel();
}

void ApplicationController::beginPanelRecoverySequence() {
    if (!m_panelBackend || m_state == ControllerState::ShuttingDown) {
        return;
    }

    m_state = ControllerState::Recovering;
    m_healthcheckTimer->stop();
    m_panelBackend->shutdownBackend();

    QTimer::singleShot(2000, this, [this]() {
        if (m_state == ControllerState::ShuttingDown) {
            return;
        }
        beginPanelStartupSequence();
    });
}

void ApplicationController::onPanelReady() {
    m_state = ControllerState::Ready;
    m_consecutiveFailures = 0;
    invalidateHardwareStateCache();
    applyPowerState();
    startHealthcheck();
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
        beginPanelRecoverySequence();
    }
}

void ApplicationController::setupCleanup() {
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        m_state = ControllerState::ShuttingDown;
        m_healthcheckTimer->stop();

        if (m_panelBackend) {
            m_panelBackend->shutdownBackend();
        }

        emit requestSerialCleanup();

        m_serialThread->quit();
        m_serialThread->wait();
    });
}

void ApplicationController::sendRawI2CCommand(const int command, const QVariantList &data) const {
    if (!m_panelBackend) {
        return;
    }
    m_panelBackend->sendRawPanelCommand(static_cast<uint8_t>(command), data);
}

void ApplicationController::handleRawCommandResponse(const uint8_t command, const bool success,
                                                     const QByteArray &response) {
    QVariantList responseList;
    for (const char i: response) {
        responseList.append(static_cast<uint8_t>(i));
    }
    emit i2cCommandResponse(command, success, responseList);
}

void ApplicationController::loadBalance() const {
    QFile file(SlotMachine::balanceFilePath());
    DebugLogger::instance().critical(SlotMachine::balanceFilePath());

    if (!file.exists()) {
        m_slotMachine->setBalance(100);
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

void ApplicationController::handleButtonPress(uint8_t buttonId) const {
    if (!m_powered_on) {
        return;
    }

    DebugLogger::instance().info(QString("Button %1 pressed").arg(buttonId));

    if (m_slotMachine->riskModeActive()) {
        if (buttonId == 0) {
            if (!m_slotMachine->riskAnimating()) {
                m_slotMachine->riskHigher();
                DebugLogger::instance().info("Risk Higher triggered by button 0");
            }
        } else if (buttonId == 1) {
            if (!m_slotMachine->riskAnimating()) {
                m_slotMachine->collectRiskPrize();
                DebugLogger::instance().info("Collect Prize triggered by button 1");
            }
        }
    } else if (m_slotMachine->priceAccepted()) {
        if (buttonId == 0) {
            m_slotMachine->startRiskMode();
            DebugLogger::instance().info("Start Risk Mode triggered by button 0");
        } else if (buttonId == 1) {
            m_slotMachine->payoutAccepted();
            DebugLogger::instance().info("Payout accepted prize triggered by button 1");
        }
    } else {
        if (buttonId == 0) {
            if (m_slotMachine->canSpin()) {
                m_slotMachine->spin();
                DebugLogger::instance().info("Spin triggered by button 0");
            }
        } else if (buttonId == 1) {
            if (m_slotMachine->currentPrize() > 0) {
                m_slotMachine->acceptPrize();
                DebugLogger::instance().info("Cashout triggered by button 1");
            }
        }
    }

    QTimer::singleShot(100, this, [this]() {
        updateButtonStates();
    });
}

void ApplicationController::sendButtonHighlightIfChanged(const uint8_t buttonId, const bool state) const {
    if (!m_panelBackend || buttonId >= m_lastButtonStates.size()) {
        return;
    }
    if (m_lastButtonStates[buttonId].has_value() && m_lastButtonStates[buttonId].value() == state) {
        return;
    }
    m_lastButtonStates[buttonId] = state;
    m_panelBackend->setButtonHighlight(buttonId, state);
}

void ApplicationController::sendTowerLevelIfChanged(const uint8_t towerId, const uint8_t row) const {
    if (!m_panelBackend || towerId >= m_lastTowerLevels.size()) {
        return;
    }
    if (m_lastTowerLevels[towerId].has_value() && m_lastTowerLevels[towerId].value() == row) {
        return;
    }
    m_lastTowerLevels[towerId] = row;
    m_panelBackend->setTowerLevel(towerId, row);
}

void ApplicationController::sendBalanceIfChanged(const double balance) const {
    if (!m_panelBackend) {
        return;
    }
    const int64_t cents = qRound64(balance * 100.0);
    if (m_lastDisplayedBalanceCents.has_value() && m_lastDisplayedBalanceCents.value() == cents) {
        return;
    }
    m_lastDisplayedBalanceCents = cents;
    m_panelBackend->setDisplayedBalance(balance);
}

void ApplicationController::invalidateHardwareStateCache() const {
    for (auto &state : m_lastButtonStates) {
        state.reset();
    }
    for (auto &level : m_lastTowerLevels) {
        level.reset();
    }
    m_lastDisplayedBalanceCents.reset();
}

void ApplicationController::updateButtonStates() const {
    if (!m_powered_on) {
        sendButtonHighlightIfChanged(0, false);
        sendButtonHighlightIfChanged(1, false);
        return;
    }

    if (m_slotMachine->riskModeActive()) {
        const bool canRisk = !m_slotMachine->riskAnimating() && m_slotMachine->riskLevel() < 7;
        const bool canCollect = !m_slotMachine->riskAnimating();

        sendButtonHighlightIfChanged(0, canRisk);
        sendButtonHighlightIfChanged(1, canCollect);

        DebugLogger::instance().verbose(QString("Risk mode buttons updated: Risk=%1, Collect=%2")
            .arg(canRisk).arg(canCollect));
    } else if (m_slotMachine->priceAccepted()) {
        sendButtonHighlightIfChanged(0, true);
        sendButtonHighlightIfChanged(1, true);
    } else {
        const bool canSpin = m_slotMachine->canSpin() && !m_slotMachine->isSpinning();
        const bool canCashout = m_slotMachine->currentPrize() > 0 && !m_slotMachine->isSpinning() && m_slotMachine->
                                canSpin();

        sendButtonHighlightIfChanged(0, canSpin);
        sendButtonHighlightIfChanged(1, canCashout);

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

void ApplicationController::applyPowerState() const {
    if (m_powered_on) {
        DebugLogger::instance().info("Applying POWER ON state");

        for (int t = 0; t < 3; t++) {
            sendTowerLevelIfChanged(static_cast<uint8_t>(t), 0);
        }

        updateButtonStates();
        sendBalanceIfChanged(m_slotMachine->balance());
    } else {
        DebugLogger::instance().info("Applying POWER OFF state");

        sendButtonHighlightIfChanged(0, false);
        sendButtonHighlightIfChanged(1, false);

        for (int t = 0; t < 3; t++) {
            sendTowerLevelIfChanged(static_cast<uint8_t>(t), 0);
        }

        sendBalanceIfChanged(0.0);
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
                if (m_slotMachine->applyReelProbabilities(probMap)) {
                    DebugLogger::instance().info("Probabilities updated on reel");
                } else {
                    DebugLogger::instance().warning("Could not update reel probabilities");
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

void ApplicationController::sendSerialStatus() {
    const QString status = QString(
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

    emit requestSerialSendResponse(status);
}
