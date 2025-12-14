import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: riskLadder
    color: "#1a1a1a"
    radius: 15
    border.color: "#FF6600"
    border.width: 3

    property bool active: false
    property double currentPrize: 0.0
    property double basePrize: 0.0
    property int currentLevel: 0
    property bool animating: false
    property int animationPosition: 0

    // Checkpoint level (Ausspielung)
    readonly property int checkpointLevel: 5

    // Ladder multipliers
    readonly property var ladderMultipliers: [1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0]

    signal riskHigher()
    signal collectPrize()

    // Background gradient
    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#2a1a0a" }
            GradientStop { position: 1.0; color: "#1a1a1a" }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 8

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "🎲 RISIKOLEITER"
            font.pixelSize: 28
            font.bold: true
            color: "#FF6600"
        }

        // Current prize display
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "#2a2a1a"
            radius: 10
            border.color: "#FFD700"
            border.width: 2

            Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "AKTUELLER GEWINN"
                    font.pixelSize: 12
                    color: "#888"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: currentPrize.toFixed(2) + " €"
                    font.pixelSize: 32
                    font.bold: true
                    color: "#FFD700"
                }
            }
        }

        // Ladder visualization
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0a0a0a"
            radius: 8
            border.color: "#333"

            ListView {
                id: ladderView
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                clip: true
                verticalLayoutDirection: ListView.BottomToTop

                model: ladderMultipliers.length

                delegate: Rectangle {
                    width: ladderView.width
                    height: 45
                    radius: 6

                    property int stepLevel: index
                    property bool isCurrentLevel: stepLevel === currentLevel && !animating
                    property bool isAnimationPosition: stepLevel === animationPosition && animating
                    property bool isCheckpoint: stepLevel === checkpointLevel
                    property double stepPrize: basePrize * ladderMultipliers[stepLevel]

                    color: {
                        if (isAnimationPosition) return "#FF6600"
                        if (isCurrentLevel) return "#4CAF50"
                        if (isCheckpoint) return "#1a2a3a" // Special checkpoint color
                        if (stepLevel < currentLevel) return "#2a3a2a"
                        return "#1a1a1a"
                    }

                    border.color: {
                        if (isAnimationPosition) return "#FFAA00"
                        if (isCurrentLevel) return "#66BB6A"
                        if (isCheckpoint) return "#4488FF" // Blue border for checkpoint
                        return "#333"
                    }
                    border.width: (isCurrentLevel || isAnimationPosition || isCheckpoint) ? 3 : 1

                    // Pulsing animation for current position
                    SequentialAnimation on opacity {
                        running: isAnimationPosition
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.6; duration: 100 }
                        NumberAnimation { to: 1.0; duration: 100 }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 10

                        // Level indicator
                        Rectangle {
                            Layout.preferredWidth: 35
                            Layout.preferredHeight: 35
                            radius: 17
                            color: {
                                if (isCheckpoint) return "#4488FF"
                                if (stepLevel < currentLevel) return "#4CAF50"
                                return "#444"
                            }

                            Text {
                                anchors.centerIn: parent
                                text: isCheckpoint ? "▶" : (stepLevel + 1).toString()
                                font.pixelSize: 16
                                font.bold: true
                                color: "white"
                            }
                        }

                        // Multiplier or Ausspielung label
                        Text {
                            Layout.fillWidth: true
                            text: isCheckpoint ? "AUSSPIELUNG" : ("×" + ladderMultipliers[stepLevel].toFixed(0))
                            font.pixelSize: isCheckpoint ? 14 : 18
                            font.bold: true
                            color: {
                                if (isCheckpoint) return "#4488FF"
                                if (isCurrentLevel) return "#FFD700"
                                return "#AAA"
                            }
                        }

                        // Prize at this level
                        Text {
                            text: stepPrize.toFixed(2) + "€"
                            font.pixelSize: 18
                            font.bold: true
                            color: {
                                if (isCurrentLevel) return "#FFD700"
                                if (isCheckpoint) return "#4488FF"
                                if (stepLevel < currentLevel) return "#4CAF50"
                                return "#666"
                            }
                        }
                    }
                }
            }
        }

        // Checkpoint info text
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: currentLevel > checkpointLevel ?
                  "📍 Checkpoint aktiv - Bei Verlust zurück zu AUSSPIELUNG" :
                  "⬆️ Erreiche AUSSPIELUNG für Sicherheit!"
            font.pixelSize: 11
            color: currentLevel > checkpointLevel ? "#4488FF" : "#888"
        }

        // Action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            // Collect button
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: collectArea.pressed ? "#2E7D32" : "#4CAF50"
                radius: 10
                opacity: animating ? 0.5 : 1.0

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "💰 AUSZAHLEN"
                        font.pixelSize: 16
                        font.bold: true
                        color: "white"
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: currentPrize.toFixed(2) + "€"
                        font.pixelSize: 14
                        color: "#C8E6C9"
                    }
                }

                MouseArea {
                    id: collectArea
                    anchors.fill: parent
                    enabled: !animating
                    onClicked: riskLadder.collectPrize()
                }
            }

            // Risk higher button
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: {
                    if (currentLevel >= 7) return "#444"
                    return riskArea.pressed ? "#CC5500" : "#FF6600"
                }
                radius: 10
                opacity: animating ? 0.5 : 1.0

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: currentLevel >= 7 ? "🏆 MAX" : "⬆️ HÖHER"
                        font.pixelSize: 16
                        font.bold: true
                        color: "white"
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: currentLevel >= 7 ? "Ausgezahlt!" : (basePrize * ladderMultipliers[Math.min(currentLevel + 1, 7)]).toFixed(2) + "€"
                        font.pixelSize: 14
                        color: "#FFE0B2"
                    }
                }

                MouseArea {
                    id: riskArea
                    anchors.fill: parent
                    enabled: !animating && currentLevel < 7
                    onClicked: riskLadder.riskHigher()
                }
            }
        }

        // Warning text
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: currentLevel <= checkpointLevel ?
                  "⚠️ Bei Verlust ist der gesamte Gewinn weg!" :
                  "⚠️ Bei Verlust zurück zu AUSSPIELUNG!"
            font.pixelSize: 12
            color: "#FF6666"
        }
    }
}
