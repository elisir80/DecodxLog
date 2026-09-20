// DecoDXLog — i frame che passano sulla seriale del control box.
// Copia di `desktop/qml/DecoRotor/TrafficPanel.qml`.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RotorGlass {
    id: panel

    title: qsTr("TRAFFICO SERIALE")

    RotorPalette { id: rt }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: qsTr("%1 @ %2 8N1").arg(panel.st.port || "—").arg(9600)
                color: rt.textSecondary
                font.pixelSize: rt.fontSmall
                font.family: rt.monoFamily
            }

            Item { Layout.fillWidth: true }

            RotorLed {
                colour: rt.accent
                label: qsTr("%1 frame").arg(panel.rotor.traffic.length)
            }
        }

        ListView {
            id: log

            Layout.fillWidth: true
            Layout.fillHeight: true
            model: panel.rotor.traffic
            clip: true
            spacing: 1
            reuseItems: true
            onCountChanged: positionViewAtEnd()

            delegate: Row {
                id: entry

                required property var modelData

                readonly property bool outgoing: modelData.dir === "tx"

                spacing: 10

                Text {
                    text: entry.outgoing ? "TX" : "RX"
                    color: entry.outgoing ? rt.warning : rt.accent
                    font.pixelSize: rt.fontSmall
                    font.family: rt.monoFamily
                    font.bold: true
                }

                Text {
                    text: entry.modelData.frame
                    color: rt.textPrimary
                    font.pixelSize: rt.fontSmall
                    font.family: rt.monoFamily
                }

                Text {
                    text: entry.modelData.hex
                    color: rt.textDim
                    font.pixelSize: rt.fontSmall
                    font.family: rt.monoFamily
                }
            }

            Text {
                anchors.centerIn: parent
                visible: panel.rotor.traffic.length === 0
                text: qsTr("nessun frame: il gateway li manda a richiesta")
                color: rt.textDim
                font.pixelSize: rt.fontSmall
            }
        }
    }
}
