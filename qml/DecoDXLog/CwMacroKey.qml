// DecoDXLog — un tasto delle macro CW.
//
// La scritta e' quella della macro, tutta: anche "F1" si cambia o si toglie.
// La misura resta quella della griglia — tutti i tasti uguali — e una scritta
// lunga si accorcia con i puntini invece di allargare il pannello; per intero
// si legge passandoci sopra. Clic: la macro parte. Tasto destro: la si
// modifica li', anche con la radio spenta.
import QtQuick
import QtQuick.Controls
import Decodium.UI

Item {
    id: root

    // Quale macro manda il tasto. Non si chiama "index" apposta: il Repeater
    // mette nel delegato un suo "index", e un secondo "index" dichiarato li'
    // copriva questo — che restava 0 per tutti i tasti, e ogni tasto mandava
    // la prima macro (il CQ) e apriva la prima da modificare.
    property int macroIndex: 0
    property var macro: ({})
    property color tone: "transparent"
    property int keyHeight: 28
    property int fontPixelSize: 12
    property bool sendEnabled: true

    signal sendRequested(int index)
    signal editRequested(int index)

    readonly property string caption: (root.macro.label || "").trim().length > 0
                                      ? root.macro.label : "F" + (root.macroIndex + 1)
    readonly property bool toned: tone.a > 0

    implicitWidth: 60
    implicitHeight: keyHeight

    Button {
        id: key
        anchors.fill: parent
        enabled: root.sendEnabled
        focusPolicy: Qt.TabFocus
        opacity: enabled ? 1.0 : 0.45
        onClicked: root.sendRequested(root.macroIndex)
        contentItem: Text {
            text: root.caption
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.family: Theme.monoFamily
            font.pixelSize: root.fontPixelSize
            font.bold: true
            color: !key.enabled ? Theme.textSecondary : (root.toned ? root.tone : Theme.textPrimary)
        }
        background: Rectangle {
            radius: 5
            color: {
                const base = root.toned ? root.tone : Theme.primaryColor
                if (key.pressed)
                    return Qt.rgba(base.r, base.g, base.b, 0.30)
                if (key.hovered)
                    return Qt.rgba(base.r, base.g, base.b, 0.14)
                return "transparent"
            }
            border.width: 1
            border.color: key.activeFocus ? Theme.primaryColor : (root.toned ? root.tone : Theme.glassBorder)
        }
    }

    // Fuori dal pulsante: un tasto spento (radio non collegata) si modifica lo stesso.
    MouseArea {
        id: area
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        hoverEnabled: true
        onClicked: root.editRequested(root.macroIndex)
    }

    ToolTip.visible: area.containsMouse
    ToolTip.delay: 600
    ToolTip.text: qsTr("%1 · %2\nRight click: change it").arg(root.caption).arg(root.macro.text || "")
}
