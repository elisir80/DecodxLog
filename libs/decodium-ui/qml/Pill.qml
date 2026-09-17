// decodium-ui — pillola a contorno: stati (LIVE, dirty), sorgenti, contatori.
import QtQuick
import Decodium.UI

Rectangle {
    id: root

    property alias text: label.text
    property color tone: Theme.textSecondary
    property color textColor: tone
    property string detail: ""
    property int pillHeight: 22
    property int fontPixelSize: 11
    property bool rounded: true
    // Cliccabile solo se chiesto: le pillole di stato non devono sembrare pulsanti.
    property bool interactive: false
    signal clicked()

    implicitHeight: pillHeight
    implicitWidth: row.implicitWidth + 20
    radius: rounded ? height / 2 : 4
    color: mouse.containsMouse && mouse.enabled ? Qt.rgba(tone.r, tone.g, tone.b, 0.12) : "transparent"
    border.width: 1
    border.color: tone

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6
        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            color: root.textColor
            font.family: Theme.monoFamily
            font.pixelSize: root.fontPixelSize
            font.bold: true
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.detail.length > 0
            text: root.detail
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: root.fontPixelSize
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.interactive
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
