cmake_minimum_required(VERSION 3.16)
project(AllesSpitzeQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(QT_IMPORT_QML_PLUGINS OFF)

# -----------------------------------------------------------------------------
# Local macOS Qt installation path (for CLion code completion only)
# -----------------------------------------------------------------------------
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/Users/LucaLeukert/Qt/6.9.2/macos/lib/cmake")
endif()

if(UNIX AND NOT APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/usr/lib/aarch64-linux-gnu/cmake/")
endif()

# -----------------------------------------------------------------------------
# Find and link required Qt modules
# -----------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Quick)

qt_policy(SET QTP0001 NEW)

qt_add_executable(AllesSpitzeQt
        main.cpp
        SlotReel.h SlotReel.cpp
        Symbol.h Symbol.cpp
        I2CWorker.h I2CWorker.cpp
        DebugLogger.h DebugLogger.cpp
        qml.qrc
)

qt_add_qml_module(AllesSpitzeQt
        URI SlotMachine
        VERSION 1.0
        QML_FILES
        qml/main.qml
        qml/DebugPanel.qml
)

target_link_libraries(AllesSpitzeQt
        PRIVATE Qt6::Core Qt6::Quick
)