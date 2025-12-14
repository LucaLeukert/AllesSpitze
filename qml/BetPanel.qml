import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: betPanel
    color: "#2a2a2a"
    radius: 15
    border.color: canChangeBet ? "#FFD700" : "#666"
    border.width: 2

    property real currentBet: 1.0
    property real minBet: 0.10
    property real maxBet: 100.0
    property real balance: 0.0
    property bool canChangeBet: true
    property bool canDecrease: canChangeBet && currentBet > minBet
    property bool canIncrease: canChangeBet && currentBet < maxBet

    signal betIncreased()
    signal betDecreased()
    signal betChanged(real newBet)

    // Locked overlay
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: canChangeBet ? 0 : 0.4
        radius: parent.radius
        z: 100
        visible: !canChangeBet

        Text {
            anchors.centerIn: parent
            text: "🔒 GESPERRT"
            font.pixelSize: 24
            font.bold: true
            color: "#FFD700"
        }

        MouseArea {
            anchors.fill: parent
            // Block all clicks when locked
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "EINSATZ"
            font.pixelSize: 24
            font.bold: true
            color: canChangeBet ? "#FFD700" : "#888"
        }

        // Current bet display
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "#1a1a1a"
            radius: 10
            border.color: canChangeBet ? "#444" : "#333"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: currentBet.toFixed(2) + " €"
                font.pixelSize: 32
                font.bold: true
                color: canChangeBet ? "white" : "#888"
            }
        }

        // Bet adjustment buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            // Decrease button
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                color: canDecrease ? (decreaseArea.pressed ? "#8B0000" : "#B22222") : "#444"
                radius: 10

                Text {
                    anchors.centerIn: parent
                    text: "−"
                    font.pixelSize: 48
                    font.bold: true
                    color: canDecrease ? "white" : "#666"
                }

                MouseArea {
                    id: decreaseArea
                    anchors.fill: parent
                    enabled: canDecrease
                    onClicked: {
                        betPanel.betDecreased()
                    }
                }
            }

            // Increase button
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                color: canIncrease ? (increaseArea.pressed ? "#006400" : "#228B22") : "#444"
                radius: 10

                Text {
                    anchors.centerIn: parent
                    text: "+"
                    font.pixelSize: 48
                    font.bold: true
                    color: canIncrease ? "white" : "#666"
                }

                MouseArea {
                    id: increaseArea
                    anchors.fill: parent
                    enabled: canIncrease
                    onClicked: {
                        betPanel.betIncreased()
                    }
                }
            }
        }

        // Quick bet buttons
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Schnellwahl"
            font.pixelSize: 14
            color: "#888"
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            rowSpacing: 8
            columnSpacing: 8

            Repeater {
                model: [0.10, 0.50, 1.00, 2.00, 5.00, 10.00]

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 45
                    color: {
                        if (!canChangeBet) return "#333"
                        if (Math.abs(currentBet - modelData) < 0.001) return "#FFD700"
                        return quickBetArea.pressed ? "#555" : "#3a3a3a"
                    }
                    radius: 8
                    border.color: "#555"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: modelData.toFixed(2) + "€"
                        font.pixelSize: 16
                        font.bold: Math.abs(currentBet - modelData) < 0.001
                        color: {
                            if (!canChangeBet) return "#666"
                            if (Math.abs(currentBet - modelData) < 0.001) return "black"
                            return "white"
                        }
                    }

                    MouseArea {
                        id: quickBetArea
                        anchors.fill: parent
                        enabled: canChangeBet
                        onClicked: {
                            betPanel.betChanged(modelData)
                        }
                    }
                }
            }
        }

        // Balance display
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: "#1a1a1a"
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10

                Text {
                    text: "Guthaben:"
                    font.pixelSize: 16
                    color: "#888"
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: balance.toFixed(2) + " €"
                    font.pixelSize: 20
                    font.bold: true
                    color: balance > 0 ? "#4CAF50" : "#F44336"
                }
            }
        }
    }
}
