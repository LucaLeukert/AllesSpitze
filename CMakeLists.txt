cmake_minimum_required(VERSION 3.16)
project(AllesSpitzeQt)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Quick
)

qt_add_executable(AllesSpitzeQt
        main.cpp
        SlotReel.h
        SlotReel.cpp
        qml.qrc
        SlotReel.cpp
        SlotReel.h
)

target_link_libraries(AllesSpitzeQt PRIVATE
        Qt6::Core
        Qt6::Quick
)