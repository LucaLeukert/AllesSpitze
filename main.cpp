#include <QGuiApplication>
#include <QtQml>

#include "SlotReel.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Register the custom C++ component
    qmlRegisterType<SlotReel>("SlotMachine", 1, 0, "SlotReel");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
