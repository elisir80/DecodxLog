// decodium-ui — pulsante piatto su vetro, con uno stato "armato" ben distinto.
import QtQuick
import QtQuick.Controls
import Decodium.UI

Button {
    id: root

    // Un comando che sta facendo qualcosa (in ascolto, collegato, in invio) si
    // deve riconoscere dall'altra parte della stanza.
    property bool  armed: false
    property color tone: Theme.primaryColor
    property int   minimumWidth: 64

    implicitHeight: Math.max(26, Theme.rowHeight + 4)
    implicitWidth: Math.max(minimumWidth, contentText.implicitWidth + 20)
    font.pixelSize: Theme.fontSize

    contentItem: Text {
        id: contentText
        text: root.text
        font: root.font
        color: root.enabled ? (root.armed ? Theme.bgDeep : Theme.textPrimary)
                            : Theme.textSecondary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 7
        color: {
            if (!root.enabled)
                return Qt.rgba(Theme.bgLight.r, Theme.bgLight.g, Theme.bgLight.b, 0.35)
            if (root.armed)
                return root.tone
            if (root.pressed)
                return Qt.rgba(root.tone.r, root.tone.g, root.tone.b, 0.45)
            if (root.hovered)
                return Qt.rgba(root.tone.r, root.tone.g, root.tone.b, 0.22)
            return Theme.glassOverlay
        }
        border.color: root.armed ? root.tone : Theme.glassBorder
        border.width: 1
        Behavior on color { ColorAnimation { duration: 140 } }
    }
}
