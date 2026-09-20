// DecoDXLog — l'indicatore di rotazione di DecoRotor: i segmenti scorrono nel
// verso in cui il rotore sta davvero girando, le frecce dicono CCW o CW e il
// cerchietto si accende quando l'antenna e' ferma.
// Copia di `desktop/qml/DecoRotor/RotationBar.qml`.
import QtQuick
import QtQuick.Layouts

Item {
    id: bar

    RotorPalette { id: rt }

    property int sense: 0             // -1 antiorario, 0 fermo, +1 orario
    property bool moving: false

    readonly property int segments: 7

    implicitHeight: 22
    implicitWidth: row.implicitWidth

    QtObject {
        id: internal
        property int step: 0
    }

    Timer {
        interval: 110
        repeat: true
        running: bar.moving && bar.visible
        onTriggered: internal.step = (internal.step + 1) % bar.segments
    }

    RowLayout {
        id: row

        anchors.fill: parent
        spacing: 12

        Row {
            spacing: 3

            Repeater {
                model: bar.segments

                Rectangle {
                    required property int index

                    readonly property int slot: bar.sense < 0
                                                ? (index + internal.step) % bar.segments
                                                : (bar.segments + index - internal.step) % bar.segments

                    width: 5
                    height: 18
                    radius: 1
                    color: bar.moving && slot < 3 ? rt.warning : rt.textDim
                    opacity: bar.moving ? (slot < 3 ? 1.0 : 0.30) : 0.30
                }
            }
        }

        Text {
            text: "◀ CCW"
            color: bar.moving && bar.sense < 0 ? rt.warning : rt.textDim
            font.pixelSize: rt.fontSmall
            font.bold: true
            font.letterSpacing: 1.0
        }

        Text {
            text: "CW ▶"
            color: bar.moving && bar.sense > 0 ? rt.warning : rt.textDim
            font.pixelSize: rt.fontSmall
            font.bold: true
            font.letterSpacing: 1.0
        }

        Text {
            text: "(●)"
            color: bar.moving ? rt.textDim : rt.accent
            font.pixelSize: rt.fontSmall
            font.bold: true
        }

        Item { Layout.fillWidth: true }
    }
}
