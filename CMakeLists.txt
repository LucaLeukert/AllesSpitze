cmake_minimum_required(VERSION 3.16)
project(AllesSpitzeQt)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Quick
)

# Erstelle eine QML Resource File
qt_add_executable(AllesSpitzeQt
        main.cpp
        SlotReel.h
        SlotReel.cpp
        Symbol.h
        Symbol.cpp
        I2CWorker.h
        I2CWorker.cpp
        DebugLogger.h
        DebugLogger.cpp
        qml.qrc
)

# Füge QML Ressourcen direkt zum Target hinzu
qt_add_qml_module(AllesSpitzeQt
    URI SlotMachine
    VERSION 1.0
    QML_FILES
        main.qml
        DebugPanel.qml
)

target_link_libraries(AllesSpitzeQt PRIVATE
        Qt6::Core
        Qt6::Quick
)