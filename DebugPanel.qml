import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DebugTools 1.0

Rectangle {
    id: debugPanel
    color: "#80000000"
    radius: 10
    enabled: debugButton.checked
    visible: debugButton.checked

    width: 100
    height: 120
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    anchors.margins: 10

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        ScrollView {
            clip: true

            TextArea {
                id: logArea
                readOnly: true
                textFormat: TextEdit.RichText
                wrapMode: TextArea.Wrap
                font.family: "Consolas"
                font.pixelSize: 12
                text: DebugLogger.logText
                background: null

                // Automatisches Scrollen zum Ende
                onTextChanged: {
                    cursorPosition = length
                    cursorVisible = false
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 10
            columnSpacing: 10

            Label {
                text: "Miss Probability:"
                color: "white"
            }

            SpinBox {
                id: missProbabilitySpinner
                from: 0
                to: 100
                value: 50
                onValueChanged: reel.set_miss_probability(value / 100.0)
            }

            Label {
                text: "Test Logging:"
                color: "white"
            }

            ComboBox {
                id: logLevelCombo
                model: ["Debug", "Info", "Warning", "Error", "Critical"]
            }

            Button {
                text: "Test Log"
                Layout.columnSpan: 1
                onClicked: {
                    switch(logLevelCombo.currentText) {
                        case "Debug":
                            DebugLogger.debug("Test Debug Message");
                            break;
                        case "Info":
                            DebugLogger.info("Test Info Message");
                            break;
                        case "Warning":
                            DebugLogger.warning("Test Warning Message");
                            break;
                        case "Error":
                            DebugLogger.error("Test Error Message");
                            break;
                        case "Critical":
                            DebugLogger.critical("Test Critical Message");
                            break;
                    }
                }
            }

            Button {
                text: "Clear Log"
                Layout.columnSpan: 1
                onClicked: logArea.clear()
            }
        }

        Button {
            text: "I2C Test"
            Layout.fillWidth: true
            onClicked: i2cWorker.writeByte(0x00, 0x42)
        }
    }
}
