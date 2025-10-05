#include <QGuiApplication>
#include <QtQml>
#include <QThread>
#include "SlotReel.h"
#include "I2CWorker.h"
#include "DebugLogger.h"

int main(int argc, char *argv[])
{
    const QGuiApplication app(argc, argv);

    qDebug() << "Main/UI Thread ID:" << QThread::currentThreadId();

#ifdef QT_DEBUG
    qDebug() << "Debug build detected.";
    // Registriere den Debug-Logger als Singleton
    qmlRegisterSingletonInstance("DebugTools", 1, 0, "DebugLogger", &DebugLogger::instance());
#endif

    // Erstelle und starte den I2C-Thread
    QThread i2cThread;
    I2CWorker i2cWorker;
    i2cWorker.moveToThread(&i2cThread);

    // Verbinde Thread-Signale
    QObject::connect(&i2cThread, &QThread::started, &i2cWorker, &I2CWorker::initialize);
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &i2cWorker, &I2CWorker::cleanup);
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &i2cThread, &QThread::quit);
    QObject::connect(&i2cThread, &QThread::finished, &i2cThread, &QThread::deleteLater);

    // Starte den I2C-Thread
    i2cThread.start();

    // Register the custom C++ component
    qmlRegisterType<SlotReel>("SlotMachine", 1, 0, "SlotReel");

    QQmlApplicationEngine engine;

#ifdef QT_DEBUG
    // Aktiviere Debug-Modus in QML
    engine.rootContext()->setContextProperty("isDebugBuild", true);
#else
    engine.rootContext()->setContextProperty("isDebugBuild", false);
#endif

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
