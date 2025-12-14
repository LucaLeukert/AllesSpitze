import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: cashoutPanel
    color: "#2a2a2a"
    radius: 15
    border.color: "#4CAF50"
    border.width: 2

    property real currentPrize: 0.0
    property var towerPrizes: []
    property real currentBet: 1.0
    property bool riskModeAvailable: currentPrize > 0

    signal cashoutRequested()
    signal riskModeRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "💰 GEWINN"
            font.pixelSize: 24
            font.bold: true
            color: "#4CAF50"
        }

        // Total prize display
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            color: currentPrize > 0 ? "#1a3a1a" : "#1a1a1a"
            radius: 10
            border.color: currentPrize > 0 ? "#4CAF50" : "#444"
            border.width: 2

            Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "GESAMT"
                    font.pixelSize: 12
                    color: "#888"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: currentPrize.toFixed(2) + " €"
                    font.pixelSize: 36
                    font.bold: true
                    color: currentPrize > 0 ? "#4CAF50" : "#666"
                }
            }
        }

        // Tower breakdown
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Tower-Gewinne (Einsatz: " + currentBet.toFixed(2) + "€)"
            font.pixelSize: 14
            color: "#888"
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1a1a1a"
            radius: 8

            ListView {
                id: towerList
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6
                clip: true
                model: towerPrizes

                delegate: Rectangle {
                    width: towerList.width
                    height: 65
                    color: modelData.prize > 0 ? "#2a3a2a" : "#222"
                    radius: 6
                    border.color: modelData.prize > 0 ? "#4CAF50" : "#333"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 10

                        // Symbol indicator
                        Rectangle {
                            Layout.preferredWidth: 40
                            Layout.preferredHeight: 40
                            radius: 20
                            color: getSymbolColor(modelData.symbolType)

                            Text {
                                anchors.centerIn: parent
                                text: getSymbolEmoji(modelData.symbolType)
                                font.pixelSize: 20
                            }
                        }

                        // Tower info
                        Column {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: modelData.symbolType
                                font.pixelSize: 14
                                font.bold: true
                                color: "white"
                            }

                            Text {
                                text: "Level " + modelData.level + " × " + modelData.multiplier.toFixed(0)
                                font.pixelSize: 12
                                color: "#888"
                            }
                        }

                        // Prize amount
                        Text {
                            text: modelData.prize.toFixed(2) + "€"
                            font.pixelSize: 18
                            font.bold: true
                            color: modelData.prize > 0 ? "#4CAF50" : "#666"
                        }
                    }
                }
            }
        }

        // Action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            // Cashout button
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: currentPrize > 0 ? (cashoutArea.pressed ? "#2E7D32" : "#4CAF50") : "#444"
                radius: 10

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: currentPrize > 0 ? "💰 AUSZAHLEN" : "Kein Gewinn"
                        font.pixelSize: 16
                        font.bold: true
                        color: currentPrize > 0 ? "white" : "#666"
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: currentPrize > 0 ? currentPrize.toFixed(2) + "€" : ""
                        font.pixelSize: 12
                        color: "#C8E6C9"
                        visible: currentPrize > 0
                    }
                }

                MouseArea {
                    id: cashoutArea
                    anchors.fill: parent
                    enabled: currentPrize > 0
                    onClicked: {
                        cashoutPanel.cashoutRequested()
                    }
                }
            }

            // Risk button
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: riskModeAvailable ? (riskArea.pressed ? "#CC5500" : "#FF6600") : "#444"
                radius: 10
                visible: true

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "🎲 RISIKO"
                        font.pixelSize: 16
                        font.bold: true
                        color: riskModeAvailable ? "white" : "#666"
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: riskModeAvailable ? "Verdoppeln!" : "---"
                        font.pixelSize: 12
                        color: riskModeAvailable ? "#FFE0B2" : "#666"
                    }
                }

                MouseArea {
                    id: riskArea
                    anchors.fill: parent
                    enabled: riskModeAvailable
                    onClicked: {
                        cashoutPanel.riskModeRequested()
                    }
                }
            }
        }
    }

    function getSymbolColor(symbolType) {
        switch(symbolType) {
            case "coin": return "#FFD700"
            case "kleeblatt": return "#228B22"
            case "marienkaefer": return "#B22222"
            default: return "#666"
        }
    }

    function getSymbolEmoji(symbolType) {
        switch(symbolType) {
            case "coin": return "🪙"
            case "kleeblatt": return "🍀"
            case "marienkaefer": return "🐞"
            default: return "?"
        }
    }
}
