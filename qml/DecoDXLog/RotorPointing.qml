// DecoDXLog — il puntamento a mano: gradi, elevazione e le otto direzioni.
// Copia di `desktop/qml/DecoRotor/PointingPanel.qml`.
import QtQuick
import QtQuick.Layouts

RotorGlass {
    id: panel

    RotorPalette { id: rt }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state

    function send() {
        const az = parseFloat(azField.text.replace(",", "."))
        const el = parseFloat(elField.text.replace(",", "."))
        panel.rotor.gotoPosition(isNaN(az) ? -1 : az, isNaN(el) ? -1 : el)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            RotorField {
                id: azField

                Layout.fillWidth: true
                placeholderText: qsTr("Azimut °")
                validator: DoubleValidator { bottom: 0; top: 360; decimals: 1 }
                onAccepted: panel.send()
            }

            RotorField {
                id: elField

                Layout.fillWidth: true
                visible: panel.st.hasEl === true
                placeholderText: qsTr("Elevazione °")
                validator: DoubleValidator { bottom: 0; top: 180; decimals: 1 }
                onAccepted: panel.send()
            }

            RotorButton {
                Layout.preferredWidth: 92
                text: qsTr("PUNTA")
                kind: 1
                onClicked: panel.send()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 8
            columnSpacing: 6

            Repeater {
                model: [
                    { name: qsTr("N"), deg: 0 }, { name: qsTr("NE"), deg: 45 },
                    { name: qsTr("E"), deg: 90 }, { name: qsTr("SE"), deg: 135 },
                    { name: qsTr("S"), deg: 180 }, { name: qsTr("SO"), deg: 225 },
                    { name: qsTr("O"), deg: 270 }, { name: qsTr("NO"), deg: 315 }
                ]

                RotorButton {
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    text: modelData.name
                    onClicked: panel.rotor.pointTo(modelData.deg, modelData.name)
                }
            }
        }
    }
}
