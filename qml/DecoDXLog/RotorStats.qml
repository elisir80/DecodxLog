// DecoDXLog — i contatori dell'esercizio del gateway.
// Copia di `desktop/qml/DecoRotor/StatsPanel.qml`.
import QtQuick
import QtQuick.Layouts

RotorGlass {
    id: panel

    title: qsTr("IN SERVICE")

    RotorPalette { id: rt }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Repeater {
            model: [
                { caption: qsTr("Frames sent"), value: String(panel.st.txFrames || 0), tint: rt.warning },
                { caption: qsTr("Frames received"), value: String(panel.st.rxFrames || 0), tint: rt.accent },
                { caption: qsTr("Answers lost"), value: String(panel.st.errorCount || 0),
                  tint: (panel.st.errorCount || 0) > 0 ? rt.danger : rt.textSecondary },
                { caption: qsTr("Reconnections"), value: String(panel.st.reconnects || 0),
                  tint: (panel.st.reconnects || 0) > 0 ? rt.warning : rt.textSecondary },
                { caption: qsTr("Running since"), value: panel.rotor.uptimeText, tint: rt.primary },
                { caption: qsTr("Clients connected"), value: String(panel.st.clients || 0), tint: rt.primary }
            ]

            RowLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: modelData.caption
                    color: rt.textSecondary
                    font.pixelSize: rt.fontBody
                    elide: Text.ElideRight
                }

                Text {
                    text: modelData.value
                    color: modelData.tint
                    font.pixelSize: 18
                    font.family: rt.monoFamily
                    font.bold: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: rt.borderSoft
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: qsTr("Control box")
                color: rt.textSecondary
                font.pixelSize: rt.fontBody
            }

            Text {
                text: panel.st.modelLabel || ""
                color: rt.textPrimary
                font.pixelSize: rt.fontSmall
                elide: Text.ElideRight
                Layout.maximumWidth: 220
            }
        }

        Item { Layout.fillHeight: true }
    }
}
