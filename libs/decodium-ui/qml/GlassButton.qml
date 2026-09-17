// decodium-ui — pulsante a contorno, testo a spaziatura fissa.
//
// `tone` colora bordo e testo; `filled` aggiunge il vetro dietro, per l'azione
// principale di un gruppo o per il comando selezionato. Un pulsante neutro ha
// il bordo del tema e il testo normale.
import QtQuick
import QtQuick.Controls
import Decodium.UI

Button {
    id: root

    property color tone: "transparent"
    readonly property bool toned: tone.a > 0
    property bool filled: false
    property int minimumWidth: 0
    property int buttonHeight: 28
    property int fontPixelSize: 12
    // Testo piccolo a destra, per la scorciatoia da tastiera.
    property string hint: ""

    implicitHeight: buttonHeight
    implicitWidth: Math.max(minimumWidth, contentRow.implicitWidth + 24)
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1.0 : 0.45

    contentItem: Item {
        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: 8
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                font.family: Theme.monoFamily
                font.pixelSize: root.fontPixelSize
                font.bold: true
                color: !root.enabled ? Theme.textSecondary : (root.toned ? root.tone : Theme.textPrimary)
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.hint.length > 0
                text: root.hint
                font.family: Theme.monoFamily
                font.pixelSize: 10
                color: Theme.textSecondary
            }
        }
    }

    background: Rectangle {
        radius: 5
        color: {
            const base = root.toned ? root.tone : Theme.primaryColor
            if (root.pressed)
                return Qt.rgba(base.r, base.g, base.b, 0.30)
            if (root.hovered)
                return Qt.rgba(base.r, base.g, base.b, 0.14)
            return root.filled ? Theme.glassOverlay : "transparent"
        }
        border.width: 1
        border.color: root.activeFocus ? Theme.primaryColor : (root.toned ? root.tone : Theme.glassBorder)
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
