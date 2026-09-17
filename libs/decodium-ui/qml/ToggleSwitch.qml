// decodium-ui — interruttore acceso/spento con la sua etichetta.
import QtQuick
import QtQuick.Controls
import Decodium.UI

AbstractButton {
    id: root

    checkable: true
    implicitHeight: 22
    implicitWidth: track.width + 8 + caption.implicitWidth

    contentItem: Item {
        Rectangle {
            id: track
            width: 34
            height: 20
            radius: 10
            anchors.verticalCenter: parent.verticalCenter
            color: root.checked ? Theme.accentColor : Theme.bgMedium
            border.width: root.checked ? 0 : 1
            border.color: Theme.glassBorder
            Rectangle {
                width: 16
                height: 16
                radius: 8
                y: 2
                x: root.checked ? parent.width - width - 2 : 2
                color: root.checked ? Theme.panelColor : Theme.textSecondary
                Behavior on x { NumberAnimation { duration: 120 } }
            }
        }
        Text {
            id: caption
            anchors.left: track.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.enabled ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: 12
        }
    }
}
