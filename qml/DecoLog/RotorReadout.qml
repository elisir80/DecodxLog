// DecoLog — la lettura d'angolo a caratteri grandi di DecoRotor: dall'altro
// capo dello shack si deve capire dov'e' puntata l'antenna senza avvicinarsi.
// Copia di `desktop/qml/DecoRotor/BigReadout.qml`.
import QtQuick

Item {
    id: readout

    RotorPalette { id: rt }

    property string label: ""
    property real value: 0
    property real target: -1
    property bool valid: false
    property bool moving: false
    property int digitSize: 78

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    Column {
        id: column

        anchors.right: parent.right
        spacing: 0

        Text {
            anchors.right: parent.right
            text: readout.label
            color: rt.textSecondary
            font.pixelSize: rt.fontSmall
            font.bold: true
            font.letterSpacing: 2.0
        }

        Row {
            anchors.right: parent.right
            spacing: 0

            Text {
                text: readout.valid ? Math.round(readout.value) : "---"
                color: readout.moving ? rt.warning : rt.textPrimary
                font.pixelSize: readout.digitSize
                font.family: rt.monoFamily
                font.bold: true
            }

            Text {
                anchors.top: parent.top
                anchors.topMargin: readout.digitSize * 0.06
                text: "°"
                color: readout.moving ? rt.warning : rt.textPrimary
                font.pixelSize: readout.digitSize * 0.62
                font.family: rt.monoFamily
                font.bold: true
            }
        }

        Text {
            anchors.right: parent.right
            text: readout.target >= 0
                  ? qsTr("verso %1°").arg(readout.target.toFixed(1))
                  : (readout.valid ? qsTr("posizione stabile") : qsTr("nessuna lettura"))
            color: readout.target >= 0 ? rt.primary : rt.textDim
            font.pixelSize: rt.fontBody
            font.family: rt.monoFamily
        }
    }
}
