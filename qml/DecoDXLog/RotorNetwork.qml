// DecoDXLog — gli indirizzi del gateway da usare su telefono e software di
// stazione.
// Copia di `desktop/qml/DecoRotor/NetworkPanel.qml`.
import QtQuick
import QtQuick.Layouts

RotorGlass {
    id: panel

    title: qsTr("NETWORK LINKS")

    RotorPalette { id: rt }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: qsTr("Addresses to use on the phone and in station software:")
            color: rt.textSecondary
            font.pixelSize: rt.fontSmall
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: panel.rotor.endpoints

            RowLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing: 10

                RotorLed {
                    colour: modelData.active ? rt.accent : rt.danger
                    label: modelData.role
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: modelData.address
                    color: rt.textPrimary
                    font.pixelSize: rt.fontBody
                    font.family: rt.monoFamily
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: rt.borderSoft
        }

        Text {
            Layout.fillWidth: true
            text: panel.st.tokenRequired
                  ? qsTr("Access is closed by a token: the clients have to show it.")
                  : qsTr("Open access on the local network. From outside the house go through a VPN, do not open ports on the router.")
            color: panel.st.tokenRequired ? rt.accent : rt.warning
            font.pixelSize: rt.fontSmall
            wrapMode: Text.WordWrap
        }

        Item { Layout.fillHeight: true }
    }
}
