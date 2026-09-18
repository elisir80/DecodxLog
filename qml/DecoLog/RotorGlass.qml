// DecoLog — il riquadro di DecoRotor: testata piatta e corpo con il suo margine.
// Copia di `desktop/qml/DecoRotor/GlassPanel.qml`.
import QtQuick

Rectangle {
    id: panel

    property alias title: heading.text
    default property alias content: body.data

    RotorPalette { id: rt }

    color: rt.bgPanel
    border.color: rt.border
    border.width: 1
    radius: rt.radius

    Rectangle {
        id: header

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: heading.text.length > 0 ? 34 : 0
        visible: height > 0
        color: rt.bgHeader
        radius: rt.radius

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: rt.radius
            color: rt.bgHeader
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: rt.borderSoft
        }

        Text {
            id: heading

            anchors.left: parent.left
            anchors.leftMargin: rt.padding
            anchors.verticalCenter: parent.verticalCenter
            color: rt.textSecondary
            font.pixelSize: rt.fontSmall
            font.letterSpacing: 1.4
            font.bold: true
            text: ""
        }
    }

    Item {
        id: body

        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: rt.padding
    }
}
