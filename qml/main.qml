import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import SlotMachine 1.0
import DebugTools 1.0

ApplicationWindow {
    id: window
    visibility: "FullScreen"
    width: 1920
    height: 1080
    title: "Slot Machine with Towers"

    property real u: Math.min(width / 1280, height / 720)
    function px(v) { return Math.round(v * u) }

    // Fire-themed background inspired by the reference design.
    Rectangle {
        id: bg
        anchors.fill: parent

        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            source: "qrc:/images/background_texture.png"
        }
    }

    // Power Off Overlay - blocks everything when powered off
    Rectangle {
        id: powerOffOverlay
        anchors.fill: parent
        color: "black"
        visible: !(appController && appController.poweredOn)
        z: 1000

        Text {
            anchors.centerIn: parent
            text: "POWERED OFF"
            font.pixelSize: 48
            font.bold: true
            color: "#333"
        }
    }

    Item {
        id: mainLayout
        anchors.fill: parent
        anchors.margins: px(24)
        visible: (appController && appController.poweredOn)

        TitleText {
            id: title
            text: "Alles Spitze"
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: px(86)
        }

        Row {
            id: topRow
            spacing: px(58)
            anchors.top: title.bottom
            anchors.topMargin: px(12)
            anchors.horizontalCenter: parent.horizontalCenter

            GamePanel {
                id: leftPanel
                width: px(280)
                height: px(200)
                title: "Joker"

                Item {
                    anchors.fill: leftPanel.contentArea
                    anchors.margins: px(14)

                    Rectangle {
                        anchors.fill: parent
                        radius: px(10)
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#540000" }
                            GradientStop { position: 0.45; color: "#c30707" }
                            GradientStop { position: 1; color: "#2f0000" }
                        }
                        border.color: "#ffb000"
                        border.width: px(2)
                    }

                    Image {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -px(4)
                        width: parent.width * 0.62
                        height: width
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/images/sonne.png"
                    }

                    Row {
                        spacing: px(10)
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: px(8)
                        anchors.horizontalCenter: parent.horizontalCenter

                        MiniIcon { source: "qrc:/images/marienkaefer.png" }
                        Text { text: "+"; color: "#ffd25c"; font.pixelSize: px(22); font.bold: true }
                        MiniIcon { source: "qrc:/images/coin.png" }
                        Text { text: "+"; color: "#ffd25c"; font.pixelSize: px(22); font.bold: true }
                        MiniIcon { source: "qrc:/images/kleeblatt.png" }
                    }
                }
            }

            GamePanel {
                id: centerPanel
                width: px(320)
                height: px(230)
                title: ""

                Item {
                    anchors.fill: centerPanel.contentArea

                    Rectangle {
                        id: reelFrame
                        anchors.fill: parent
                        radius: px(10)
                        color: "#0b0b0b"
                        border.color: "#ffce32"
                        border.width: px(3)
                        z: 0
                    }

                    Item {
                        id: reelViewport
                        anchors.fill: parent
                        anchors.margins: px(10)
                        clip: true
                        z: 1

                        SlotReel {
                            id: reel
                            objectName: "mainReel"
                            anchors.fill: parent
                            visible: true
                            z: 1

                            Component.onCompleted: {
                                if (slotMachine) {
                                    slotMachine.setReel(reel)
                                }
                            }
                        }
                    }
                }
            }

            GamePanel {
                id: rightPanel
                width: px(280)
                height: px(200)
                title: ""

                Item {
                    anchors.fill: rightPanel.contentArea
                    anchors.margins: px(14)

                    Rectangle {
                        anchors.fill: parent
                        radius: px(10)
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#540000" }
                            GradientStop { position: 0.45; color: "#c30707" }
                            GradientStop { position: 1; color: "#2f0000" }
                        }
                        border.color: "#ffb000"
                        border.width: px(2)
                    }

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: px(8)
                        width: parent.width * 0.84
                        height: parent.height * 0.66
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/images/teufel.png"
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: px(8)
                        anchors.rightMargin: px(8)
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: px(8)
                        text: "Cashpot auf " + (slotMachine ? slotMachine.currentPrize.toFixed(0) : "0")
                        font.pixelSize: px(24)
                        font.bold: true
                        font.family: "Georgia"
                        color: "#ff5f45"
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.NoWrap
                        minimumPixelSize: px(14)
                        fontSizeMode: Text.Fit
                    }
                }
            }
        }

        TitleText {
            id: cashpotTitle
            text: "CASHPOT"
            anchors.top: topRow.bottom
            anchors.topMargin: px(12)
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: px(54)
        }

        Rectangle {
            id: cashpotBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: bottomBar.top
            anchors.bottomMargin: px(4)
            height: px(82)
            radius: px(14)

            gradient: Gradient {
                GradientStop { position: 0; color: "#2a0000" }
                GradientStop { position: 0.5; color: "#7a0b0b" }
                GradientStop { position: 1; color: "#2a0000" }
            }
            border.color: "#ffb000"
            border.width: px(3)

            RowLayout {
                anchors.fill: parent
                anchors.margins: px(8)
                spacing: px(8)

                DigitBox {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    valueText: slotMachine ? slotMachine.currentPrize.toFixed(0) : "0"
                    big: true
                }

                // Annahme – enabled when there is a spieler cashpot
                Button {
                    id: annahmeButton
                    Layout.preferredWidth: px(175)
                    Layout.fillHeight: true
                    text: "Annahme"
                    font.pixelSize: px(20)
                    font.bold: true
                    enabled: slotMachine && slotMachine.currentPrize > 0
                    onClicked: if (slotMachine) slotMachine.acceptPrize()
                    contentItem: Text {
                        text: parent.text; color: "#fff"
                        font.pixelSize: parent.font.pixelSize; font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: px(10)
                        gradient: Gradient {
                            GradientStop { position: 0; color: annahmeButton.enabled ? "#2db84d" : "#8d8d8d" }
                            GradientStop { position: 1; color: annahmeButton.enabled ? "#1a7a34" : "#3d3d3d" }
                        }
                        border.color: annahmeButton.enabled ? "#6dff99" : "#bfbfbf"
                        border.width: px(2)
                    }
                }

                // Leiter – enabled when there is an angenommener cashpot
                Button {
                    id: leiterButton
                    Layout.preferredWidth: px(175)
                    Layout.fillHeight: true
                    text: "Leiter"
                    font.pixelSize: px(20)
                    font.bold: true
                    enabled: slotMachine && slotMachine.acceptedPrize > 0
                    onClicked: if (slotMachine) slotMachine.startRiskMode()
                    contentItem: Text {
                        text: parent.text; color: "#fff"
                        font.pixelSize: parent.font.pixelSize; font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: px(10)
                        gradient: Gradient {
                            GradientStop { position: 0; color: leiterButton.enabled ? "#1e8a3c" : "#2a2a2a" }
                            GradientStop { position: 1; color: leiterButton.enabled ? "#0d5022" : "#1a1a1a" }
                        }
                        border.color: leiterButton.enabled ? "#4cdd7a" : "#555"
                        border.width: px(2)
                    }
                }

                // Auszahlen – enabled when there is an angenommener cashpot
                Button {
                    id: auszahlenButton
                    Layout.preferredWidth: px(175)
                    Layout.fillHeight: true
                    text: "Auszahlen"
                    font.pixelSize: px(20)
                    font.bold: true
                    enabled: slotMachine && slotMachine.acceptedPrize > 0
                    onClicked: if (slotMachine) slotMachine.payoutAccepted()
                    contentItem: Text {
                        text: parent.text; color: "#fff"
                        font.pixelSize: parent.font.pixelSize; font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: px(10)
                        gradient: Gradient {
                            GradientStop { position: 0; color: auszahlenButton.enabled ? "#2db84d" : "#8d8d8d" }
                            GradientStop { position: 1; color: auszahlenButton.enabled ? "#1a7a34" : "#3d3d3d" }
                        }
                        border.color: auszahlenButton.enabled ? "#6dff99" : "#bfbfbf"
                        border.width: px(2)
                    }
                }
            }
        }

        Rectangle {
            id: bottomBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: px(148)
            radius: px(14)
            clip: true

            gradient: Gradient {
                GradientStop { position: 0; color: "#3a0000" }
                GradientStop { position: 0.45; color: "#b80707" }
                GradientStop { position: 1; color: "#2f0000" }
            }
            border.color: "#c71c0c"
            border.width: px(2)

            Column {
                anchors.fill: parent
                anchors.margins: px(10)
                spacing: px(10)

                RowLayout {
                    width: parent.width
                    height: px(54)
                    spacing: px(8)

                    Rectangle {
                        Layout.preferredWidth: px(44)
                        Layout.preferredHeight: px(44)
                        radius: px(6)
                        color: "#7f0000"
                        border.color: "#ffb000"
                        border.width: px(2)
                    }

                    MetricBox { title: "GELDSPEICHER (EURO)"; value: slotMachine ? slotMachine.balance.toFixed(2) : "0.00"; accent: "#ff3b30"; Layout.fillWidth: true; widthOverride: 0 }

                    Rectangle {
                        Layout.preferredWidth: px(44)
                        Layout.preferredHeight: px(44)
                        radius: Layout.preferredWidth / 2
                        color: "#161616"
                        border.color: "#d7d7d7"
                        border.width: px(2)
                    }

                    MetricBox { title: "BANK-BITS"; value: slotMachine ? slotMachine.currentPrize.toFixed(2) : "0.00"; accent: "#ffd25c"; Layout.fillWidth: true; widthOverride: 0 }

                    Image {
                        Layout.preferredWidth: px(44)
                        Layout.preferredHeight: px(44)
                        fillMode: Image.PreserveAspectFit
                        source: "qrc:/images/sonne.png"
                    }

                    MetricBox { title: "ERFOLG-BITS"; value: "0"; accent: "#ffd25c"; Layout.fillWidth: true; widthOverride: 0 }
                    MetricBox { title: "LEVEL-BITS"; value: slotMachine ? Math.max(0, Math.round(slotMachine.bet * 200)).toString() : "0"; accent: "#ffd25c"; Layout.fillWidth: true; widthOverride: 0 }
                }

                Row {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: px(18)

                    Text {
                        text: "EURO   \u2190   BITS"
                        color: "#ff5a3d"
                        font.pixelSize: px(22)
                        font.bold: true
                    }

                    Item { width: px(18) }

                    Text {
                        text: "UNTERHALTUNG MIT BITS"
                        color: "#ffd25c"
                        font.pixelSize: px(24)
                        font.bold: true
                    }
                }
            }
        }
    }

    Component {
        id: debugPanelComponent
        TowerDebugPanel {}
    }

    Loader {
        id: debugPanelLoader
        sourceComponent: debugPanelComponent
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: px(24)
        visible: true
    }

    Rectangle {
        id: riskModeDimmer
        anchors.fill: parent
        visible: (slotMachine && slotMachine.riskModeActive) && (appController && appController.poweredOn)
        color: "#70000000"
        z: 10
    }

    // Risk Ladder - shown when risk mode is active (centered)
    RiskLadder {
        id: riskLadder
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: px(520)
        z: 20
        visible: (slotMachine && slotMachine.riskModeActive) && (appController && appController.poweredOn)

        active: slotMachine && slotMachine.riskModeActive
        currentPrize: slotMachine ? slotMachine.riskPrize : 0
        basePrize: slotMachine ? slotMachine.riskBasePrize : 0
        currentLevel: slotMachine ? slotMachine.riskLevel : 0
        ausspielungStarted: slotMachine && slotMachine.riskAusspielungStarted
        animating: slotMachine && slotMachine.riskAnimating
        animationPosition: slotMachine ? slotMachine.riskAnimationPosition : 0
        debugControls: debugPanelLoader.visible

        onRiskHigher: if (slotMachine) slotMachine.riskHigher()
        onCollectPrize: if (slotMachine) slotMachine.collectRiskPrize()
        onCollectOneToOnePrize: if (slotMachine) slotMachine.collectRiskOneToOnePrize()
    }

    Connections {
        target: slotMachine

        function onRiskWon(newPrize) {
            console.log("Risk won! New prize: " + newPrize)
        }

        function onRiskLost() {
            console.log("Risk lost! Prize forfeited.")
        }

        function onRiskCollected(amount) {
            console.log("Risk prize collected: " + amount)
        }
    }

    component TitleText: Item {
        property alias text: t.text
        property alias font: t.font

        width: t.implicitWidth
        height: t.implicitHeight

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: px(3)
            text: t.text
            font.family: "Impact"
            font.bold: true
            font.pixelSize: t.font.pixelSize
            color: "#6c0500"
            style: Text.Outline
            styleColor: "#2d0000"
        }

        Text {
            id: t
            text: ""
            font.family: "Impact"
            font.bold: true
            color: "#ffe45f"
            style: Text.Outline
            styleColor: "#7a0900"
        }
    }

    component GamePanel: Item {
        id: root
        property string title: ""
        property alias contentArea: content

        clip: true

        readonly property int borderW: px(3)
        readonly property int innerPad: px(14)
        readonly property int titlePad: title.length > 0 ? px(42) : 0

        Rectangle {
            id: frame
            anchors.fill: parent
            radius: px(14)
            gradient: Gradient {
                GradientStop { position: 0; color: "#120202" }
                GradientStop { position: 1; color: "#2b0303" }
            }
            border.color: "#ffb000"
            border.width: borderW
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: px(8)
            radius: px(12)
            color: "#00000000"
            border.color: "#ff3b00"
            border.width: px(2)
        }

        Text {
            id: header
            visible: title.length > 0
            text: title
            anchors.left: parent.left
            anchors.leftMargin: px(16)
            anchors.top: parent.top
            anchors.topMargin: px(10)
            color: "#ff3b30"
            font.pixelSize: px(28)
            font.bold: true
            font.family: "Georgia"
            z: 2
        }

        Item {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: parent.top
            anchors.leftMargin: innerPad
            anchors.rightMargin: innerPad
            anchors.bottomMargin: innerPad
            anchors.topMargin: innerPad + titlePad
            clip: true
            z: 1
        }
    }

    component MiniIcon: Rectangle {
        property string source: ""
        width: px(28)
        height: px(28)
        radius: px(6)
        color: "#111"
        border.color: "#ffb000"
        border.width: px(1)

        Image {
            anchors.centerIn: parent
            width: parent.width * 0.7
            height: width
            fillMode: Image.PreserveAspectFit
            source: parent.source
        }
    }

    component DigitBox: Rectangle {
        property string valueText: "0"
        property bool big: false
        radius: px(12)
        color: "#0f0f0f"
        border.color: "#ffb000"
        border.width: px(2)

        Text {
            anchors.centerIn: parent
            text: valueText
            color: "#ffd25c"
            font.bold: true
            font.pixelSize: big ? px(52) : px(44)
            font.family: "Impact"
        }
    }

    component ArrowButton: Rectangle {
        property string direction: "up"
        property bool enabled: true
        signal clicked()
        radius: px(10)
        color: enabled ? "#1a1a1a" : "#0f0f0f"
        border.color: "#ffb000"
        border.width: px(2)

        Text {
            anchors.centerIn: parent
            text: direction === "up" ? "▲" : "▼"
            color: enabled ? "#4fd36a" : "#555"
            font.bold: true
            font.pixelSize: px(22)
        }

        MouseArea {
            anchors.fill: parent
            enabled: parent.enabled
            onClicked: parent.clicked()
        }
    }

    component MetricBox: Item {
        property string title: ""
        property string value: ""
        property string accent: "#ffd25c"
        property bool narrow: false
        property int widthOverride: 0

        width: widthOverride > 0 ? widthOverride : (narrow ? px(170) : px(300))
        height: px(54)

        Rectangle {
            anchors.fill: parent
            radius: px(12)
            color: "#0f0f0f"
            border.color: "#ffb000"
            border.width: px(2)
        }

        Text {
            text: title
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: px(12)
            anchors.rightMargin: px(12)
            anchors.top: parent.top
            anchors.topMargin: px(8)
            color: "#cfcfcf"
            font.pixelSize: px(13)
            font.bold: true
            elide: Text.ElideRight
        }

        Text {
            text: value
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: px(14)
            anchors.rightMargin: px(14)
            anchors.verticalCenter: parent.verticalCenter
            color: accent
            font.pixelSize: px(27)
            font.bold: true
            font.family: "Impact"
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideLeft
        }
    }
}
