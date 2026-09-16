// decodium-ui — la fascia che titola un pannello: maiuscoletto, alta quanto la
// densita' scelta.
import QtQuick
import Decodium.UI

Rectangle {
    id: root

    property alias text: label.text

    height: Theme.panelHeight
    color: Theme.panelHeader
    radius: 10

    // Solo gli angoli in alto arrotondati: sotto la fascia prosegue il pannello.
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: parent.radius
        color: parent.color
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.borderSoft
    }

    Text {
        id: label
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: Theme.fontSize - 1
        font.letterSpacing: 1.4
        font.bold: true
        font.capitalization: Font.AllUppercase
        color: Theme.textSecondary
    }
}
