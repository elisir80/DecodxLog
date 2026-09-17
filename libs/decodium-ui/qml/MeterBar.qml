// decodium-ui — barra di avanzamento sottile, con etichetta e conteggio sopra.
import QtQuick
import Decodium.UI

Column {
    id: root

    property string label: ""
    property string valueText: ""
    property real fraction: 0
    property color barColor: Theme.accentColor

    spacing: 4

    Item {
        width: root.width
        height: caption.implicitHeight
        Text {
            id: caption
            text: root.label
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
        }
        Text {
            anchors.right: parent.right
            text: root.valueText
            color: Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: 11
            font.bold: true
        }
    }
    Rectangle {
        width: root.width
        height: 6
        radius: 3
        color: Theme.bgMedium
        clip: true
        Rectangle {
            width: parent.width * Math.max(0, Math.min(1, root.fraction))
            height: parent.height
            radius: 3
            color: root.barColor
            Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        }
    }
}
