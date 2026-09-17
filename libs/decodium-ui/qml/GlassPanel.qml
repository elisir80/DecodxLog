// decodium-ui — la superficie su cui poggia ogni pannello, con la sua testata.
import QtQuick
import Decodium.UI

Rectangle {
    id: root

    property string title: ""
    // Il pallino colorato accanto al titolo: dice a colpo d'occhio che pannello e'.
    property color dotColor: Theme.accentColor
    property bool showDot: true
    // Contenuto a destra della testata: contatori, pulsanti piccoli.
    property alias headerTools: header.tools
    property alias headerLeading: header.leading
    default property alias content: body.data
    property int padding: 10

    color: Theme.panelColor
    border.color: Theme.glassBorder
    border.width: 1
    radius: 6
    clip: true

    PanelHeader {
        id: header
        visible: root.title.length > 0 || root.headerLeading.length > 0
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 }
        text: root.title
        dotColor: root.dotColor
        showDot: root.showDot
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
