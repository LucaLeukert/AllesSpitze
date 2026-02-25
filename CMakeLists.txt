cmake_minimum_required(VERSION 3.16)
project(AllesSpitzeQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(QT_IMPORT_QML_PLUGINS OFF)

# Add debug symbols even in Release builds for better crash debugging
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O2")
endif()

# -----------------------------------------------------------------------------
# Local macOS Qt installation path (for CLion code completion only)
# -----------------------------------------------------------------------------
if (APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/Users/LucaLeukert/Qt/6.9.2/macos/lib/cmake")
endif ()

if (UNIX AND NOT APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/usr/lib/aarch64-linux-gnu/cmake/")
endif ()

# -----------------------------------------------------------------------------
# Find and link required Qt modules
# -----------------------------------------------------------------------------
# SerialPort is only needed on Linux (Raspberry Pi)
if (UNIX AND NOT APPLE)
    find_package(Qt6 REQUIRED COMPONENTS Core Quick SerialPort Multimedia Test)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Quick Multimedia Test)
endif()

qt_policy(SET QTP0001 NEW)

qt_add_executable(AllesSpitzeQt
        main.cpp
        SlotReel.h SlotReel.cpp
        Symbol.h Symbol.cpp
        I2CWorker.h I2CWorker.cpp
        HardwarePanelBackend.h
        I2CHardwarePanelBackend.h I2CHardwarePanelBackend.cpp
        StubSimulatedHardwarePanelBackend.h StubSimulatedHardwarePanelBackend.cpp
        HardwarePanelBackendFactory.h HardwarePanelBackendFactory.cpp
        IPanelTransport.h
        LinuxI2CTransport.h LinuxI2CTransport.cpp
        SerialWorker.h SerialWorker.cpp
        DebugLogger.h DebugLogger.cpp
        qml.qrc
        Tower.cpp
        Tower.h
        SlotMachine.cpp
        SlotMachine.h
        ApplicationController.cpp
        ApplicationController.h
        AudioEngine.cpp
        AudioEngine.h
)

qt_add_qml_module(AllesSpitzeQt
        URI SlotMachine
        VERSION 1.0
        QML_FILES
        qml/main.qml
        qml/DebugPanel.qml
        qml/TowerDebugPanel.qml
        qml/I2CDebugPanel.qml
        qml/BetPanel.qml
        qml/CashoutPanel.qml
        qml/RiskLadder.qml
)

# Link libraries - conditionally link SerialPort only on Linux
if (UNIX AND NOT APPLE)
    target_link_libraries(AllesSpitzeQt
            PRIVATE Qt6::Core Qt6::Quick Qt6::Qml Qt6::SerialPort Qt6::Multimedia
    )
else()
    target_link_libraries(AllesSpitzeQt
            PRIVATE Qt6::Core Qt6::Quick Qt6::Qml Qt6::Multimedia
    )
endif()

include(CTest)

if (BUILD_TESTING)
    qt_add_executable(AllesSpitzeQtTests
            tests/test_slotmachine_backend.cpp
            SlotMachine.cpp SlotMachine.h
            SlotReel.cpp SlotReel.h
            Symbol.cpp Symbol.h
            Tower.cpp Tower.h
            DebugLogger.cpp DebugLogger.h
            AudioEngine.cpp AudioEngine.h
            I2CWorker.cpp I2CWorker.h
            HardwarePanelBackend.h
            I2CHardwarePanelBackend.cpp I2CHardwarePanelBackend.h
            StubSimulatedHardwarePanelBackend.cpp StubSimulatedHardwarePanelBackend.h
            HardwarePanelBackendFactory.cpp HardwarePanelBackendFactory.h
    )

    target_link_libraries(AllesSpitzeQtTests
            PRIVATE Qt6::Core Qt6::Quick Qt6::Qml Qt6::Multimedia Qt6::Test
    )
    target_include_directories(AllesSpitzeQtTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    add_test(NAME AllesSpitzeQtTests COMMAND AllesSpitzeQtTests)
endif()
