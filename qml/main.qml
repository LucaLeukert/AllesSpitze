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
        anchors.centerIn: parent  // Center in the window
        width: 600
        height: 1080

        Component.onCompleted: {
            DebugLogger.log("SlotReel initialized h");

            // Connect to slot machine
            slotMachine.setReel(reel);
        }
    }

    // Bet Panel - bottom left (always visible, touch-friendly)
    BetPanel {
        id: betPanel
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: 280
        height: 420

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

    // Cashout Panel - bottom right (always visible)
    CashoutPanel {
        id: cashoutPanel
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: 300
        height: 420

        currentPrize: slotMachine.currentPrize
        towerPrizes: slotMachine.towerPrizes
        currentBet: slotMachine.bet

        onCashoutRequested: slotMachine.cashout()
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
        anchors.bottom: parent.bottom
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
}