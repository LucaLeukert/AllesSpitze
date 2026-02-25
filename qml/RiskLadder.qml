import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: riskLadder

    property bool active: false
    property double currentPrize: 0.0
    property double basePrize: 0.0
    property int currentLevel: 0
    property bool ausspielungStarted: false
    property bool animating: false
    property int animationPosition: 0
    property bool debugControls: false
    property int idleAnimationPosition: -1
    readonly property bool ausspielungLoopActive: animating && !ausspielungStarted && currentLevel === 6 && animationPosition > 6

    signal riskHigher()
    signal collectPrize()
    signal collectOneToOnePrize()

    readonly property var ladderRows: [
        { level: 13, isCheckpoint: false },
        { level: 12, isCheckpoint: false },
        { level: 11, isCheckpoint: false },
        { level: 10, isCheckpoint: false },
        { level: 9, isCheckpoint: false },
        { level: 8, isCheckpoint: false },
        { level: 7, isCheckpoint: false },
        { level: 6, isCheckpoint: true },
        { level: 5, isCheckpoint: false },
        { level: 4, isCheckpoint: false },
        { level: 3, isCheckpoint: false },
        { level: 2, isCheckpoint: false },
        { level: 1, isCheckpoint: false },
        { level: 0, isCheckpoint: false }
    ]

    readonly property var levelMultipliers: ({
        "0": 0.0,
        "1": 1.5,
        "2": 3.0,
        "3": 6.0,
        "4": 12.0,
        "5": 24.0,
        "6": 40.0,
        "7": 80.0,
        "8": 120.0,
        "9": 200.0,
        "10": 320.0,
        "11": 520.0,
        "12": 840.0,
        "13": 1400.0
    })

    function scrollToLevel(level) {
        var row = ladderLevelToRow(level)
        if (row < 0 || !ladderView.width || !ladderView.height) return

        var top = 0
        for (var i = 0; i < row; ++i) {
            top += rowHeightForIndex(i) + ladderView.spacing
        }

        var rowCenter = top + rowHeightForIndex(row) / 2
        var target = rowCenter - (ladderView.height / 2)
        var maxY = Math.max(0, ladderView.contentHeight - ladderView.height)
        ladderView.contentY = Math.max(0, Math.min(target, maxY))
    }

    function idleLoseLevel() {
        if (currentLevel > 6) return currentLevel - 1
        if (currentLevel === 6 && ausspielungStarted) return 5
        return 0
    }

    function idleWinLevel() {
        return Math.min(currentLevel + 1, 13)
    }

    onCurrentLevelChanged: scrollToLevel(currentLevel)
    onActiveChanged: if (active) scrollToLevel(currentLevel)

    Component.onCompleted: Qt.callLater(function() { scrollToLevel(currentLevel) })

    function rowWidthForIndex(i) {
        var t = i / Math.max(1, ladderRows.length - 1)
        return ladderView.width * (1.0 - (0.38 * t))
    }

    function rowHeightForIndex(i) {
        return Math.round(rowWidthForIndex(i) * 60 / 176)
    }

    function ladderLevelToRow(level) {
        if (level < 0) level = 0
        for (var i = 0; i < ladderRows.length; ++i) {
            if (ladderRows[i].level === level) return i
        }
        return -1
    }

    function rowHighlighted(row) {
        var rowLevel = ladderRows[row].level
        if (animating) return animationPosition < 0 ? rowLevel === 0 : rowLevel === animationPosition
        if (idleBlinkTimer.running) return rowLevel === idleAnimationPosition
        if (currentLevel < 0) return rowLevel === 0
        return rowLevel === currentLevel
    }

    function payoutForLevel(level) {
        if (level < 0) return 0
        var mult = levelMultipliers[level.toString()]
        if (mult === undefined) return 0
        return mult
    }

    function formatPayout(value) {
        return Number(value).toFixed(2)
    }

    function rowLabel(modelData) {
        if (modelData.isCheckpoint) return "AUSSPIELUNG\n" + formatPayout(payoutForLevel(modelData.level))
        return formatPayout(payoutForLevel(modelData.level))
    }

    Timer {
        id: idleBlinkTimer
        interval: 220
        repeat: true
        running: active && !animating && currentPrize > 0 && currentLevel >= 0
        onRunningChanged: {
            if (running) {
                idleAnimationPosition = idleLoseLevel()
            } else {
                idleAnimationPosition = -99
            }
        }
        onTriggered: {
            var loseLevel = idleLoseLevel()
            var winLevel = idleWinLevel()
            idleAnimationPosition = (idleAnimationPosition === loseLevel) ? winLevel : loseLevel
        }
    }

    Item {
        anchors.fill: parent

        ListView {
            id: ladderView
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 0
            anchors.bottomMargin: 0
            width: Math.min(parent.width * 0.60, 300)
            clip: true
            interactive: false
            spacing: 4
            model: ladderRows
            onHeightChanged: Qt.callLater(function() { scrollToLevel(currentLevel) })
            onWidthChanged: Qt.callLater(function() { scrollToLevel(currentLevel) })

            Behavior on contentY {
                NumberAnimation {
                    duration: 520
                    easing.type: Easing.InOutQuad
                }
            }

            delegate: Item {
                readonly property real t: index / Math.max(1, ladderRows.length - 1)
                width: rowWidthForIndex(index)
                height: rowHeightForIndex(index)
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined

                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    source: rowHighlighted(index)
                            ? "qrc:/images/riskladder_highlight.png"
                            : "qrc:/images/riskladder_idle.png"
                    smooth: true
                }

                Text {
                    anchors.centerIn: parent
                    text: rowLabel(modelData)
                    color: rowHighlighted(index) ? "#1b1b1b" : (modelData.isCheckpoint ? "#1b1b1b" : "#ffffff")
                    font.pixelSize: modelData.isCheckpoint ? 17 : 20
                    font.bold: true
                    font.family: "Impact"
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        Rectangle {
            width: 6
            radius: 3
            color: "#55ffffff"
            anchors.top: ladderView.top
            anchors.bottom: ladderView.bottom
            anchors.left: ladderView.left
            anchors.leftMargin: 8
            z: -1
        }

        Rectangle {
            width: 6
            radius: 3
            color: "#55ffffff"
            anchors.top: ladderView.top
            anchors.bottom: ladderView.bottom
            anchors.right: ladderView.right
            anchors.rightMargin: 8
            z: -1
        }

        Column {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 8
            spacing: 16

            Rectangle {
                width: 88
                height: 88
                radius: 44
                color: "#22000000"
                border.color: "#d7d7d7"
                border.width: 2
                opacity: animating ? 0.6 : 1.0

                Text {
                    anchors.centerIn: parent
                    text: "$"
                    color: "#ffffff"
                    font.pixelSize: 44
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: !animating && active
                    onClicked: riskLadder.collectPrize()
                }
            }

            Rectangle {
                width: 88
                height: 88
                radius: 44
                color: "#22000000"
                border.color: "#d7d7d7"
                border.width: 2
                opacity: animating ? 0.6 : 1.0

                Text {
                    anchors.centerIn: parent
                    text: "1:1"
                    color: "#ffffff"
                    font.pixelSize: 28
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: active && currentPrize > 0 && (!animating || ausspielungLoopActive)
                    onClicked: riskLadder.riskHigher()
                }
            }
        }

        Rectangle {
            visible: debugControls
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            radius: 8
            color: "#99000000"
            border.color: "#808080"
            border.width: 1
            width: debugRow.implicitWidth + 18
            height: debugRow.implicitHeight + 12

            Row {
                id: debugRow
                anchors.centerIn: parent
                spacing: 8

                Button {
                    text: "1:1"
                    enabled: active && currentPrize > 0 && (!animating || ausspielungLoopActive)
                    onClicked: riskLadder.riskHigher()
                }
                Button {
                    text: "$"
                    enabled: active && !animating
                    onClicked: riskLadder.collectPrize()
                }
            }
        }
    }
}
