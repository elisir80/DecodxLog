// DecoDXLog — la scheda IMPOSTAZIONI del posto di comando: stazione, sicurezza,
// rete e collegamento al control box.
// Copia di `desktop/qml/DecoRotor/SettingsPage.qml`.
import QtQuick
import QtQuick.Layouts

RowLayout {
    id: page

    RotorPalette { id: rt }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state

    spacing: rt.spacing

    RotorSettingsPanel {
        Layout.fillHeight: true
        Layout.preferredWidth: 620
        Layout.maximumWidth: 760
        Layout.minimumWidth: 420
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: 320
        spacing: rt.spacing

        RotorNetwork {
            Layout.fillWidth: true
            Layout.preferredHeight: 260
        }

        RotorGlass {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: qsTr("LINK TO THE CONTROL BOX")

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Repeater {
                    model: [
                        { caption: qsTr("Serial port"), value: page.st.port || "—" },
                        { caption: qsTr("Speed"), value: qsTr("%1 baud, 8N1").arg(9600) },
                        { caption: qsTr("Model"), value: page.st.modelLabel || "—" },
                        { caption: qsTr("Status"), value: page.st.connected ? qsTr("connected") : qsTr("not there") }
                    ]

                    RowLayout {
                        required property var modelData

                        Layout.fillWidth: true
                        spacing: 10

                        Text {
                            Layout.fillWidth: true
                            text: modelData.caption
                            color: rt.textSecondary
                            font.pixelSize: rt.fontBody
                        }

                        Text {
                            text: modelData.value
                            color: rt.textPrimary
                            font.pixelSize: rt.fontBody
                            font.family: rt.monoFamily
                            elide: Text.ElideRight
                            Layout.maximumWidth: 260
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Port and model are changed at startup, with --port and --model.")
                    color: rt.textDim
                    font.pixelSize: rt.fontSmall
                    wrapMode: Text.WordWrap
                }

                Item { Layout.fillHeight: true }
            }
        }
    }
}
