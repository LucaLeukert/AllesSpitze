import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import SlotMachine 1.0
import DebugTools 1.0

ApplicationWindow {
    id: window
    visibility: "FullScreen"
    width: 1024
    height: 600
    title: "Slot Machine Reel"
    color: "black"

    Column {
        anchors.centerIn: parent
        spacing: 30

        // C++ rendered reel with images
        SlotReel {
            id: reel
            anchors.horizontalCenter: parent.horizontalCenter

            Component.onCompleted: {
                // Set custom probabilities
                set_probabilities({
                    "coin": 30,
                    "kleeblatt": 25,
                    "marienkaefer": 20,
                    "sonne": 15,
                    "teufel": 10
                });

                set_miss_probability(0.5);
                DebugLogger.log("SlotReel initialized");
            }
        }
    }

    // Debug Panel
    Loader {
        active: isDebugBuild
        sourceComponent: DebugPanel {
            id: debugPanel
        }
    }
}