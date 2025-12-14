// I2CDebugPanel.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    width: 400
    height: 500
    color: "#1e1e1e"
    border.color: "#444"
    border.width: 1
    radius: 8

    // Signal to send command - connect this from outside
    signal sendRawCommand(int command, var dataBytes)

    property string lastResponse: ""
    property bool lastSuccess: false

    // Helper function to pad hex strings (padStart not available in QML)
    function padHex(num, length) {
        var str = num.toString(16).toUpperCase()
        while (str.length < length) {
            str = "0" + str
        }
        return str
    }

    // Function to receive response from C++
    function onRawCommandResponse(command, success, response) {
        lastSuccess = success
        if (success) {
            var hexStr = ""
            for (var i = 0; i < response.length; i++) {
                var byteVal = response[i]
                if (byteVal < 0) byteVal = byteVal + 256
                hexStr += padHex(byteVal, 2) + " "
            }
            lastResponse = hexStr.trim()
            responseHistory.append({
                "cmd": "0x" + padHex(command, 2),
                "success": true,
                "response": hexStr.trim()
            })
        } else {
            lastResponse = "FAILED"
            responseHistory.append({
                "cmd": "0x" + padHex(command, 2),
                "success": false,
                "response": "Command failed"
            })
        }
    }

    ListModel {
        id: responseHistory
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // Header
        Label {
            text: "🔧 I2C Debug Interface"
            color: "#00d4ff"
            font.bold: true
            font.pixelSize: 16
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#444"
        }

        // Command Input
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 8

            Label {
                text: "Command (hex):"
                color: "#ccc"
                font.pixelSize: 12
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 5

                Label {
                    text: "0x"
                    color: "#888"
                    font.family: "Courier"
                    font.pixelSize: 14
                }

                TextField {
                    id: commandInput
                    Layout.preferredWidth: 60
                    placeholderText: "01"
                    font.family: "Courier"
                    font.pixelSize: 14
                    maximumLength: 2
                    validator: RegularExpressionValidator { regularExpression: /[0-9A-Fa-f]{0,2}/ }
                    background: Rectangle {
                        color: "#2a2a2a"
                        border.color: commandInput.focus ? "#00d4ff" : "#555"
                        radius: 4
                    }
                    color: "#fff"
                }

                // Quick command buttons
                ComboBox {
                    id: presetCommands
                    Layout.fillWidth: true
                    model: [
                        "-- Presets --",
                        "01 INIT",
                        "02 HEALTHCHECK",
                        "03 POLL_BUTTONS",
                        "04 HIGHLIGHT_BTN",
                        "05 HIGHLIGHT_TOWER",
                        "06 UPDATE_NAME",
                        "07 UPDATE_BALANCE"
                    ]
                    onCurrentIndexChanged: {
                        if (currentIndex > 0) {
                            var cmd = model[currentIndex].split(" ")[0]
                            commandInput.text = cmd
                            currentIndex = 0
                        }
                    }
                    background: Rectangle {
                        color: "#2a2a2a"
                        border.color: "#555"
                        radius: 4
                    }
                    contentItem: Text {
                        text: presetCommands.displayText
                        color: "#aaa"
                        font.pixelSize: 11
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }
                }
            }

            Label {
                text: "Data  (hex bytes):"
                color: "#ccc"
                font.pixelSize: 12
            }

            TextField {
                id: dataInput
                Layout.fillWidth: true
                placeholderText: "00 01 FF (space separated)"
                font.family: "Courier"
                font.pixelSize: 12
                validator: RegularExpressionValidator { regularExpression: /[0-9A-Fa-f\s]*/ }
                background: Rectangle {
                    color: "#2a2a2a"
                    border.color: dataInput.focus ? "#00d4ff" : "#555"
                    radius: 4
                }
                color: "#fff"
            }
        }

        // Quick data presets for common commands
        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Label {
                text: "Quick:"
                color: "#888"
                font.pixelSize: 10
            }

            Button {
                text: "Btn 0 ON"
                font.pixelSize: 10
                implicitHeight: 24
                onClicked: {
                    commandInput.text = "04"
                    dataInput.text = "00 01"
                }
                background: Rectangle {
                    color: parent.pressed ? "#444" : "#333"
                    border.color: "#555"
                    radius: 3
                }
                contentItem: Text {
                    text: parent.text
                    color: "#aaa"
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: "Btn 0 OFF"
                font.pixelSize: 10
                implicitHeight: 24
                onClicked: {
                    commandInput.text = "04"
                    dataInput.text = "00 00"
                }
                background: Rectangle {
                    color: parent.pressed ? "#444" : "#333"
                    border.color: "#555"
                    radius: 3
                }
                contentItem: Text {
                    text: parent.text
                    color: "#aaa"
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: "Tower 0 R1"
                font.pixelSize: 10
                implicitHeight: 24
                onClicked: {
                    commandInput.text = "05"
                    dataInput.text = "00 01"
                }
                background: Rectangle {
                    color: parent.pressed ? "#444" : "#333"
                    border.color: "#555"
                    radius: 3
                }
                contentItem: Text {
                    text: parent.text
                    color: "#aaa"
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Send Button
        Button {
            id: sendButton
            Layout.fillWidth: true
            text: "📤 Send Command"
            font.bold: true
            implicitHeight: 40

            background: Rectangle {
                color: sendButton.pressed ? "#005580" : (sendButton.hovered ? "#0088cc" : "#006699")
                radius: 6
            }

            contentItem: Text {
                text: sendButton.text
                color: "#fff"
                font.bold: true
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: {
                var cmdHex = commandInput.text.trim()
                if (cmdHex.length === 0) {
                    return
                }

                var command = parseInt(cmdHex, 16)

                // Parse data bytes
                var dataBytes = []
                var dataStr = dataInput.text.trim()
                if (dataStr.length > 0) {
                    var parts = dataStr.split(" ")
                    for (var i = 0; i < parts.length; i++) {
                        var part = parts[i].trim()
                        if (part.length > 0) {
                            dataBytes.push(parseInt(part, 16))
                        }
                    }
                }

                root.sendRawCommand(command, dataBytes)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#444"
        }

        // Response History
        Label {
            text: "📋 Response History"
            color: "#aaa"
            font.pixelSize: 12
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#151515"
            border.color: "#333"
            radius: 4

            ListView {
                id: historyList
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                model: responseHistory
                spacing: 4

                delegate: Rectangle {
                    width: historyList.width
                    height: responseColumn.height + 8
                    color: model.success ? "#1a2e1a" : "#2e1a1a"
                    radius: 3
                    border.color: model.success ? "#2a4a2a" : "#4a2a2a"

                    ColumnLayout {
                        id: responseColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 4
                        spacing: 2

                        RowLayout {
                            Label {
                                text: model.success ? "✓" : "✗"
                                color: model.success ? "#4caf50" : "#f44336"
                                font.pixelSize: 12
                            }
                            Label {
                                text: "CMD: " + model.cmd
                                color: "#00d4ff"
                                font.family: "Courier"
                                font.pixelSize: 11
                            }
                        }

                        Label {
                            text: model.response
                            color: "#aaa"
                            font.family: "Courier"
                            font.pixelSize: 10
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                    }
                }

                // Auto scroll to bottom
                onCountChanged: {
                    positionViewAtEnd()
                }
            }
        }

        // Clear button
        Button {
            Layout.fillWidth: true
            text: "🗑 Clear History"
            implicitHeight: 30

            background: Rectangle {
                color: parent.pressed ? "#333" : "#2a2a2a"
                border.color: "#555"
                radius: 4
            }

            contentItem: Text {
                text: parent.text
                color: "#888"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: responseHistory.clear()
        }
    }
}
