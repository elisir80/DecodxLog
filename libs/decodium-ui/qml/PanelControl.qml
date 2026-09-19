// decodium-ui — un comando piccolo nella testata di un pannello: una lettera
// sola, che si accende quando ci passi sopra e dice cosa fa.
import QtQuick
import QtQuick.Controls
import Decodium.UI

Item {
    id: root

    property string glyph: ""
    property string hint: ""
    signal clicked()

    implicitWidth: 18
    implicitHeight: 18

    Text {
        anchors.centerIn: parent
        text: root.glyph
        color: area.containsMouse ? Theme.accentColor : Theme.textSecondary
        font.pixelSize: Theme.fontSize
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip.visible: area.containsMouse && root.hint.length > 0
    ToolTip.delay: 500
    ToolTip.text: root.hint
}
