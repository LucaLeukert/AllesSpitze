import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    width: 400
    height: 500
    color: "#2b2b2b"
    border.color: "#555"
    border.width: 2

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 15

        Label {
            text: "Tower Status"
            color: "white"
            font.bold: true
            font.pixelSize: 20
        }

        Repeater {
            model: slotMachine ? slotMachine.towers : []

            delegate: Rectangle {
                id: towerDelegate
                Layout.fillWidth: true
                height: 80
                color: "#3a3a3a"
                radius: 5
                border.color: modelData.isFull ? "#00ff00" : "#555"
                border.width: 2

                // Store tower data as properties to access in nested Repeater
                property int towerLevel: modelData.level
                property bool towerIsFull: modelData.isFull
                property string towerSymbol: modelData.symbolType

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Label {
                        text: towerDelegate.towerSymbol.toUpperCase()
                        color: "white"
                        font.bold: true
                        font.pixelSize: 16
                        Layout.preferredWidth: 120
                    }

                    Row {
                        spacing: 5
                        Repeater {
                            model: 5
                            Rectangle {
                                width: 30
                                height: 30
                                // Use towerDelegate.towerLevel instead of modelData.level
                                color: index < towerDelegate.towerLevel ? "#00ff00" : "#1a1a1a"
                                border.color: "#555"
                                border.width: 1
                                radius: 3

                                Label {
                                    anchors.centerIn: parent
                                    text: index + 1
                                    // Use towerDelegate.towerLevel instead of modelData.level
                                    color: index < towerDelegate.towerLevel ? "black" : "#555"
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: towerDelegate.towerIsFull ? "FULL!" : towerDelegate.towerLevel + "/5"
                        color: towerDelegate.towerIsFull ? "#00ff00" : "white"
                        font.bold: towerDelegate.towerIsFull
                        font.pixelSize: 14
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 60
            color: "#3a3a3a"
            radius: 5

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 5

                Label {
                    text: "Last Result:"
                    color: "#888"
                    font.pixelSize: 12
                }

                Label {
                    text: slotMachine ? slotMachine.lastResult.toUpperCase() : "---"
                    color: "yellow"
                    font.bold: true
                    font.pixelSize: 18
                }
            }
        }

        Button {
            Layout.fillWidth: true
            text: slotMachine && slotMachine.canSpin ? "SPIN" : "Spinning..."
            enabled: slotMachine ? slotMachine.canSpin : false
            height: 50

            onClicked: {
                if (slotMachine) {
                    slotMachine.spin();
                }
            }

            background: Rectangle {
                color: parent.enabled ? "#4CAF50" : "#555"
                radius: 5
            }

            contentItem: Text {
                text: parent.text
                color: "white"
                font.bold: true
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Button {
            Layout.fillWidth: true
            text: "Reset All Towers"
            height: 40

            onClicked: {
                if (slotMachine) {
                    slotMachine.resetAllTowers();
                }
            }

            background: Rectangle {
                color: "#f44336"
                radius: 5
            }

            contentItem: Text {
                text: parent.text
                color: "white"
                font.bold: true
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}