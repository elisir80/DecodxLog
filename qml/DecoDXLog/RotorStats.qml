// DecoDXLog — i contatori dell'esercizio del gateway.
// Copia di `desktop/qml/DecoRotor/StatsPanel.qml`.
import QtQuick
import QtQuick.Layouts

RotorGlass {
    id: panel

    title: qsTr("ESERCIZIO")

    RotorPalette { id: rt }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Repeater {
            model: [
                { caption: qsTr("Frame inviati"), value: String(panel.st.txFrames || 0), tint: rt.warning },
                { caption: qsTr("Frame ricevuti"), value: String(panel.st.rxFrames || 0), tint: rt.accent },
                { caption: qsTr("Risposte perse"), value: String(panel.st.errorCount || 0),
                  tint: (panel.st.errorCount || 0) > 0 ? rt.danger : rt.textSecondary },
                { caption: qsTr("Riconnessioni"), value: String(panel.st.reconnects || 0),
                  tint: (panel.st.reconnects || 0) > 0 ? rt.warning : rt.textSecondary },
                { caption: qsTr("In servizio da"), value: panel.rotor.uptimeText, tint: rt.primary },
                { caption: qsTr("Client collegati"), value: String(panel.st.clients || 0), tint: rt.primary }
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
