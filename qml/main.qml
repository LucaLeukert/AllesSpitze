import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import SlotMachine 1.0
import DebugTools 1.0

ApplicationWindow {
    id: window
    visibility: "FullScreen"
    width: 1920
    height: 1080
    title: "Slot Machine with Towers"
    color: "black"

    // Reel in the center of the screen
    SlotReel {
        id: reel
        objectName: "mainReel"
        anchors.centerIn: parent
        width: 600
        height: 1080
        visible: !slotMachine.riskModeActive

        Component.onCompleted: {
            DebugLogger.log("SlotReel initialized h");

            // Connect to slot machine
            slotMachine.setReel(reel);
        }
    }

    // Risk Ladder - shown when risk mode is active (centered)
    RiskLadder {
        id: riskLadder
        anchors.centerIn: parent
        width: 450
        height: 700
        visible: slotMachine.riskModeActive

        active: slotMachine.riskModeActive
        currentPrize: slotMachine.riskPrize
        basePrize: slotMachine.riskPrize / Math.pow(2, slotMachine.riskLevel) // Calculate base prize
        currentLevel: slotMachine.riskLevel
        animating: slotMachine.riskAnimating
        animationPosition: slotMachine.riskAnimationPosition

        onRiskHigher: slotMachine.riskHigher()
        onCollectPrize: slotMachine.collectRiskPrize()
    }

    // Bet Panel - bottom left (always visible, touch-friendly)
    BetPanel {
        id: betPanel
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: 280
        height: 420
        visible: !slotMachine.riskModeActive

        currentBet: slotMachine.bet
        balance: slotMachine.balance
        canChangeBet: slotMachine.canChangeBet
        minBet: 0.10
        maxBet: 100.0

        onBetIncreased: slotMachine.increaseBet()
        onBetDecreased: slotMachine.decreaseBet()
        onBetChanged: function(newBet) {
            slotMachine.setBet(newBet)
        }
    }

    // Cashout Panel - bottom right (hidden during risk mode)
    CashoutPanel {
        id: cashoutPanel
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: 300
        height: 420
        visible: !slotMachine.riskModeActive

        currentPrize: slotMachine.currentPrize
        towerPrizes: slotMachine.towerPrizes
        currentBet: slotMachine.bet

        onCashoutRequested: slotMachine.cashout()
        onRiskModeRequested: slotMachine.startRiskMode()
    }

    // Balance display during risk mode (top right)
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20
        width: 200
        height: 80
        color: "#2a2a2a"
        radius: 10
        border.color: "#FFD700"
        border.width: 2
        visible: slotMachine.riskModeActive

        Column {
            anchors.centerIn: parent
            spacing: 4

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "GUTHABEN"
                font.pixelSize: 14
                color: "#888"
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: slotMachine.balance.toFixed(2) + " €"
                font.pixelSize: 24
                font.bold: true
                color: "#FFD700"
            }
        }
    }

    Loader {
        active: isDebugBuild
        anchors.right: parent.right
        anchors.top: parent.top
        sourceComponent: TowerDebugPanel {
            width: 500
            height: 500
        }
    }

    Loader {
        active: isDebugBuild
        anchors.left: parent.left
        anchors.top: parent.top
        sourceComponent: DebugPanel {
            width: 500
            height: 500
        }
    }

    // I2C Debug Panel - positioned bottom center (moved to avoid overlap)
    Loader {
        id: i2cDebugLoader
        active: isDebugBuild
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.borrom
        sourceComponent: I2CDebugPanel {
            width: 400
            height: 500

            onSendRawCommand: function(command, dataBytes) {
                // Send via appController (thread-safe proxy)
                appController.sendRawI2CCommand(command, dataBytes)
            }

            // Connect response handler
            Connections {
                target: appController
                function onI2cCommandResponse(command, success, response) {
                    // Forward to the panel
                    i2cDebugLoader.item.onRawCommandResponse(command, success, response)
                }
            }
        }
    }

    // Risk mode result notifications
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
}