// decodium-ui — la superficie su cui poggia ogni pannello, con la sua testata.
import QtQuick
import Decodium.UI

Rectangle {
    id: root

    property string title: ""
    // Contenuto a destra della testata: contatori, pulsanti piccoli.
    property alias headerTools: toolsRow.data
    default property alias content: body.data
    property int padding: 10

    color: Theme.panelColor
    border.color: Theme.glassBorder
    border.width: 1
    radius: 10
    clip: true

    PanelHeader {
        id: header
        visible: root.title.length > 0
        anchors { left: parent.left; right: parent.right; top: parent.top }
        text: root.title

        Row {
            id: toolsRow
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
        }
    }

    Item {
        id: body
        anchors {
            left: parent.left; right: parent.right; bottom: parent.bottom
            top: header.visible ? header.bottom : parent.top
            margins: root.padding
        }
    }
}
