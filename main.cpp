#include <QCoreApplication>
#include <QtQml>
#include <QThread>
#include "SlotReel.h"
#include "I2CWorker.h"
#include "DebugLogger.h"

int main(int argc, char *argv[]) {
    const QGuiApplication app(argc, argv);

    qDebug() << "Main/UI Thread ID: " << QThread::currentThreadId();

#ifdef QT_DEBUG
    qDebug() << "Debug build detected.";
#endif

    qmlRegisterSingletonInstance("DebugTools", 1, 0, "DebugLogger",
                                 &DebugLogger::instance());

    const auto worker = new I2CWorker();
    const auto thread = new QThread();
    worker->moveToThread(thread);

    QObject::connect(thread, &QThread::started, worker,
                     &I2CWorker::initialize);

    QObject::connect(
        worker,
        &I2CWorker::initComplete,
        [](const bool success, const uint8_t status) {
            DebugLogger::instance().info(QString("Init: %1, Status: 0x%2")
                                         .arg(success ? "SUCCESS" : "FAILURE")
                                         .arg(status, 2, 16, QChar('0')));
        }
    );

    // Clean shutdown
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [worker]() {
        QMetaObject::invokeMethod(worker, "stopPolling");
    });

    thread->start();

    // Open device after thread starts
    QTimer::singleShot(200, [worker]() {
        QMetaObject::invokeMethod(
            worker,
            "openDevice",
            Q_ARG(uint8_t, 0x42)
        );
    });


    qmlRegisterType<SlotReel>("SlotMachine", 1, 0, "SlotReel");


    QQmlApplicationEngine engine;

#ifdef QT_DEBUG
    engine.rootContext()->setContextProperty("isDebugBuild", true);
#else
    engine.rootContext()->setContextProperty("isDebugBuild", false);
#endif

    engine.rootContext()->setContextProperty("i2cWorker", worker);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    // Get root object to access reels
    QObject *rootObject = engine.rootObjects().first();

    // Connect button events to spin reels
    QObject::connect(
        worker,
        &I2CWorker::buttonEventsReceived,
        rootObject,
        [rootObject](const QVector<uint8_t> &buttons) {
            if (buttons.isEmpty()) {
                return; // No buttons pressed
            }

            // Log button presses
            for (const uint8_t btnId: buttons) {
                DebugLogger::instance().info(
                    QString("Button %1 pressed").arg(btnId)
                );
            }

            // Find all SlotReel objects and spin them
            for (QList<SlotReel *> reels = rootObject->findChildren<SlotReel *>(); SlotReel *reel: reels) {
                if (reel && !reel->spinning()) {
                    reel->spin();
                }
            }
        }
    );

    return QGuiApplication::exec();
}
