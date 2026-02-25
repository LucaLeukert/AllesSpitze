#include "ApplicationController.h"
#include <QCoreApplication>
#include <QGuiApplication>

/**
 *
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
    if (qEnvironmentVariableIsEmpty("DISPLAY") &&
        qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") &&
        qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("QT_QUICK_BACKEND", "software");
    }

    // Ensure QML engine doesn't crash on missing resources
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QGuiApplication app(argc, argv);

    // Set application metadata for proper data directory creation
    QCoreApplication::setOrganizationName("AllesSpitze");
    QCoreApplication::setApplicationName("AllesSpitzeQt");

    ApplicationController controller;
    controller.initialize();

    if (!controller.start()) {
        qCritical() << "Failed to start ApplicationController - no root objects loaded";
        return -1;
    }

    return QGuiApplication::exec();
}
