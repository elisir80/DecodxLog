// DecoDXLog — la frequenza in cima, che si gira come una manopola.
//
// Le cifre non sono solo da guardare: la rotellina del mouse muove **la cifra
// sotto il puntatore**, come la manopola di una radio dove ogni tacca vale
// quanto la cifra che stai guardando. Un clic apre la casella e la frequenza si
// scrive: in MHz (14.074) o in kHz (14074), come viene comodo.
import QtQuick
import QtQuick.Controls
import Decodium.UI

Item {
    id: root

    // Quella da mostrare, in MHz. Zero vuol dire "non si sa".
    property real mhz: 0
    // C'e' qualcuno che la puo' ricevere: la radio o Decodium. Se no, le cifre
    // restano da guardare e basta.
    property bool tunable: false
    property color textColor: Theme.textSecondary
    property int pixelSize: 20

    // Chiede di andare su questa frequenza (MHz).
    signal tuned(real mhz)

    readonly property string shown: mhz > 0 ? mhz.toFixed(6) : "--.------"
    readonly property bool editing: editor.visible

    implicitWidth: Math.max(fullMetrics.width, editor.implicitWidth)
    implicitHeight: Math.max(fullMetrics.height, 26)

    TextMetrics { id: fullMetrics; font: label.font; text: root.shown }
    // Con un carattere a spaziatura fissa una cifra vale l'altra: da qui si
    // ricava quale cifra sta sotto il puntatore.
    TextMetrics { id: oneDigit; font: label.font; text: "0" }

    // La cifra sotto il puntatore, come indice nella stringa; -1 se fuori.
    property int hotDigit: -1

    // Quanto vale un giro di rotellina su quella cifra, in Hz.
    function stepOf(index) {
        const dot = shown.indexOf(".")
        if (index < 0 || index >= shown.length || index === dot)
            return 0
        if (index < dot)
            return Math.pow(10, 6 + (dot - index - 1))
        return Math.pow(10, 6 - (index - dot))
    }

    Text {
        id: label
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        visible: !editor.visible
        text: root.shown
        color: root.textColor
        font.family: Theme.monoFamily
        font.pixelSize: root.pixelSize
        font.bold: true
    }

    // La sottolineatura dice quale cifra sta per muoversi: senza, la rotellina
    // e' una sorpresa.
    Rectangle {
        visible: root.tunable && !editor.visible && root.hotDigit >= 0 && root.stepOf(root.hotDigit) > 0
        x: label.x + root.hotDigit * oneDigit.advanceWidth
        y: label.y + label.height - 2
        width: oneDigit.advanceWidth
        height: 2
        color: Theme.accentColor
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        visible: !editor.visible
        hoverEnabled: true
        enabled: root.tunable
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        acceptedButtons: Qt.LeftButton

        onPositionChanged: (m) => {
            root.hotDigit = oneDigit.advanceWidth > 0
                          ? Math.floor((m.x - label.x) / oneDigit.advanceWidth) : -1
        }
        onExited: root.hotDigit = -1

        onWheel: (w) => {
            const step = root.stepOf(root.hotDigit >= 0 ? root.hotDigit : root.shown.indexOf(".") + 3)
            if (step <= 0 || root.mhz <= 0)
                return
            const hz = Math.round(root.mhz * 1e6) + (w.angleDelta.y > 0 ? step : -step)
            if (hz > 0)
                root.tuned(hz / 1e6)
        }

        onClicked: {
            editor.text = root.mhz > 0 ? root.mhz.toFixed(6) : ""
            editor.visible = true
            editor.forceActiveFocus()
            editor.selectAll()
        }

        ToolTip.visible: containsMouse && !editor.visible
        ToolTip.delay: 700
        ToolTip.text: qsTr("Wheel: the digit under the pointer. Click: write the frequency.")
    }

    TextField {
        id: editor
        visible: false
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(fullMetrics.width + 16, 140)
        height: root.height
        padding: 2
        color: Theme.accentColor
        font.family: Theme.monoFamily
        font.pixelSize: root.pixelSize
        font.bold: true
        selectByMouse: true
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        background: Rectangle {
            color: Theme.bgDeep
            border.width: 1
            border.color: Theme.accentColor
            radius: 4
        }

        // Si accetta sia 14.074 (MHz) sia 14074 (kHz): chi opera scrive come
        // gli viene, e il numero grande non puo' essere altro che kHz.
        function apply() {
            const raw = parseFloat(text.replace(",", "."))
            editor.visible = false
            if (!isFinite(raw) || raw <= 0)
                return
            const mhz = raw > 1000 ? raw / 1000 : raw
            root.tuned(mhz)
        }

        onAccepted: apply()
        Keys.onEscapePressed: editor.visible = false
        onActiveFocusChanged: if (!activeFocus) editor.visible = false
    }
}
