#include "ApplicationController.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // Set application metadata for proper data directory creation
    QCoreApplication::setOrganizationName("AllesSpitze");
    QCoreApplication::setApplicationName("AllesSpitzeQt");

    // Ensure QML engine doesn't crash on missing resources
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    ApplicationController controller;
    controller.initialize();

    if (!controller.start()) {
        qCritical() << "Failed to start ApplicationController - no root objects loaded";
        return -1;
    }

    return QGuiApplication::exec();
}