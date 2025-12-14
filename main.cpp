#include "ApplicationController.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    ApplicationController controller;
    controller.initialize();

    if (!controller.start()) {
        return -1;
    }

    return QGuiApplication::exec();
}