import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import SlotMachine 1.0

ApplicationWindow {
    id: window
    visible: true
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
            }
        }

        // Custom button to avoid styling issues
        Rectangle {
            id: spinButton
            width: 60
            height: 60
            anchors.horizontalCenter: parent.horizontalCenter

            color: mouseArea.pressed ? "#555" : "#333"
            radius: width / 2
            border.color: reel.spinning ? "#666" : "#999"
            border.width: 2

            Rectangle {
                anchors.centerIn: parent
                width: 20
                height: 20
                color: reel.spinning ? "#666" : "#fff"
                radius: width / 2
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                enabled: !reel.spinning

                onClicked: {
                    reel.spin();
                }
            }
        }

        // Debug info
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: reel.spinning ? "SPINNING..." : "READY"
            color: "white"
            font.pointSize: 12
        }
    }
}