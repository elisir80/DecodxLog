// DecoLog — i comandi che servono con l'antenna in movimento: passi a destra e
// a sinistra, STOP al centro sotto il pollice, riposo e memorie.
// Copia di `desktop/qml/DecoRotor/CommandBar.qml`.
import QtQuick
import QtQuick.Layouts

RotorGlass {
    id: bar

    signal presetsRequested()

    RotorPalette { id: rt }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: [-10, -1]

                RotorButton {
                    required property int modelData

                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 48
                    text: (modelData === -10 ? "◀◀ " : "◀ ") + modelData + "°"
                    enabled: bar.st.connected === true
                    onClicked: bar.rotor.nudge(modelData)
                }
            }

            RotorButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: qsTr("STOP")
                kind: 2
                font.pixelSize: 20
                onClicked: bar.rotor.stopNow(false)
            }

            Repeater {
                model: [1, 10]

                RotorButton {
                    required property int modelData

                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 48
                    text: "+" + modelData + (modelData === 10 ? " ▶▶" : " ▶")
                    enabled: bar.st.connected === true
                    onClicked: bar.rotor.nudge(modelData)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            RotorButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                text: qsTr("PARK %1°").arg((bar.st.parkAz || 0).toFixed(0))
                onClicked: bar.rotor.park()
            }

            RotorButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                text: qsTr("MEMORIE…")
                onClicked: bar.presetsRequested()
            }
        }
    }
}
