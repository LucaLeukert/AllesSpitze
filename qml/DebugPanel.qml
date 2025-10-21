// DebugPanel.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import DebugTools 1.0

Rectangle {
    id: root
    width: 700
    height: 700
    color: "#2b2b2b"
    border.color: "#555"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: "Debug Log"
                color: "white"
                font.bold: true
                font.pixelSize: 16
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: "Clear"
                onClicked: DebugLogger.clearLog()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: "Verbosity:"
                color: "white"
            }
            
            ComboBox {
                id: verbosityCombo
                model: ["Normal", "Verbose"]
                currentIndex: 0
                
                onCurrentIndexChanged: {
                    if (currentIndex === 0) {
                        DebugLogger.verbosity = DebugLogger.LogVerbosity.Normal
                    } else {
                        DebugLogger.verbosity = DebugLogger.LogVerbosity.Verbose
                    }
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                id: logTextArea
                readOnly: true
                wrapMode: Text.NoWrap
                text: DebugLogger.logText
                color: "#00ff00"
                font.family: "Courier"
                font.pixelSize: 15
                background: Rectangle {
                    color: "#1a1a1a"
                }
                
                // Auto-scroll to bottom
                onTextChanged: {
                    logTextArea.cursorPosition = logTextArea.length
                }
            }
        }
    }
}