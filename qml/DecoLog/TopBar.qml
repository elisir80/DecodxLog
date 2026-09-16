// DecoLog — barra superiore: stazione, stato del collegamento con Decodium, sync.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Rectangle {
    id: root

    signal settingsRequested()

    implicitHeight: Theme.panelHeight + 18
    color: Theme.bgMedium

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.glassBorder
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        spacing: 18

        Row {
            spacing: 10
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "DecoLog"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSize + 5
                font.bold: true
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: decolog.deCall.length ? decolog.deCall : qsTr("no station")
                color: decolog.deCall.length ? Theme.accentColor : Theme.textSecondary
                font.pixelSize: Theme.fontSize + 2
                font.family: Theme.monoFamily
                font.bold: true
            }
        }

        Rectangle { implicitWidth: 1; Layout.fillHeight: true; Layout.topMargin: 10; Layout.bottomMargin: 10; color: Theme.borderSoft }

        // Chi sta mandando i QSO, e cosa sta facendo.
        Row {
            spacing: 8
            Rectangle {
                id: led
                anchors.verticalCenter: parent.verticalCenter
                width: 10; height: 10; radius: 5
                color: !decolog.listening ? Theme.errorColor
                     : decolog.transmitting ? Theme.warningColor
                     : decolog.clientConnected ? Theme.accentColor : Theme.textSecondary
                opacity: decolog.transmitting ? blink : 1.0
                property real blink: 1.0
                SequentialAnimation on blink {
                    running: decolog.transmitting
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.35; duration: 450 }
                    NumberAnimation { to: 1.0; duration: 450 }
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: !decolog.listening ? qsTr("UDP %1 closed").arg(decolog.udpPort)
                     : decolog.clientConnected ? decolog.clientName
                                                 + (decolog.clientVersion.length ? " " + decolog.clientVersion : "")
                     : qsTr("waiting for Decodium on UDP %1").arg(decolog.udpPort)
                color: decolog.listening ? Theme.textPrimary : Theme.errorColor
            }
        }

        StatusItem {
            visible: decolog.clientConnected && decolog.dialFrequency.length > 0
            label: "DIAL"
            value: decolog.dialFrequency + " MHz"
        }
        StatusItem {
            visible: decolog.clientConnected && decolog.currentMode.length > 0
            label: "MODE"
            value: decolog.currentMode
            valueColor: Theme.secondaryColor
        }
        StatusItem {
            visible: decolog.clientConnected && decolog.dxCall.length > 0
            label: "DX"
            value: decolog.dxCall
            valueColor: Theme.accentColor
        }

        Item { Layout.fillWidth: true }

        StatusItem {
            label: "CLOUD"
            value: qsTr("not configured")
            valueColor: Theme.textSecondary
        }

        GlassButton {
            text: qsTr("Settings")
            onClicked: root.settingsRequested()
        }
    }
}
