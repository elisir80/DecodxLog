// decodium-ui — scheda a riquadro: contorno neutro, contorno primario quando attiva.
import QtQuick
import QtQuick.Controls
import Decodium.UI

AbstractButton {
    id: root

    property bool active: false
    property string badge: ""
    property int chipHeight: 26

    implicitHeight: chipHeight
    implicitWidth: row.implicitWidth + 24
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1.0 : 0.5

    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 6
            Text {
                text: root.text
                color: root.active ? Theme.primaryColor : Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: 12
                font.bold: true
            }
            Text {
                visible: root.badge.length > 0
                text: root.badge
                color: Theme.secondaryColor
                font.family: Theme.monoFamily
                font.pixelSize: 12
                font.bold: true
            }
        }
    }

    background: Rectangle {
        radius: 4
        color: root.active ? Theme.glassOverlay
             : root.hovered ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.08)
             : "transparent"
        border.width: 1
        border.color: root.active || root.activeFocus ? Theme.primaryColor : Theme.glassBorder
    }
}
