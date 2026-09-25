// DecoDXLog — il CW: le macro che vanno al manipolatore della radio e il decoder
// che legge quello che arriva.
//
// Sta in piedi da solo: non serve aprire il contest. Il testo esce dal
// manipolatore della radio (Hamlib), e quello che entra lo legge DecoDXLog
// dall'audio, senza chiedere niente alla radio.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Decodium.UI

GlassPanel {
    id: root

    // Con chi si sta parlando, per riempire {CALL} nelle macro. Chi ospita il
    // pannello lo dice; da solo, guarda il nominativo che il log sta cercando.
    property string callsign: decolog.lookupCall
    property string rstSent: "599"
    property string serial: ""
    property string exchange: ""
    property bool compact: false

    readonly property var rig: decolog.rig

    // Per le schermate di prova: apre la tendina dell'ingresso audio.
    function showCombo() { audioBox.popup.open() }

    function cwContext() {
        return {
            call: root.callsign.trim(),
            rst: root.rstSent.trim(),
            nr: root.serial.trim(),
            exch: root.exchange.trim()
        }
    }

    title: qsTr("CW")
    dotColor: root.rig.connected ? Theme.accentColor : Theme.textSecondary
    headerTools: [
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.rig.connected
                  ? "%1 %2".arg(root.rig.frequencyLabel).arg(root.rig.mode)
                  : root.rig.status
            color: root.rig.connected ? Theme.textSecondary : Theme.warningColor
            font.family: Theme.monoFamily
            font.pixelSize: 11
            elide: Text.ElideRight
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Stop")
            tone: Theme.errorColor
            buttonHeight: 22
            fontPixelSize: 11
            enabled: root.rig.connected
            onClicked: root.rig.stop()
        },
        // La puntina: staccato, il CW sta davanti alle altre finestre. Si vede
        // solo quando c'e' una finestra da tenere davanti.
        GlassButton {
            id: onTopButton
            readonly property var hostWindow: root.Window.window
            anchors.verticalCenter: parent.verticalCenter
            // In gara no: le finestre staccate dalla lavagna stanno gia' sopra
            // la principale, come tutte le altre, e la CW non fa eccezione.
            visible: root.detached && hostWindow !== null
                     && hostWindow.alwaysOnTop !== undefined && !hostWindow.contestMode
            text: qsTr("On top")
            tone: Theme.primaryColor
            filled: visible && hostWindow.alwaysOnTop
            buttonHeight: 22
            fontPixelSize: 11
            onClicked: hostWindow.alwaysOnTop = !hostWindow.alwaysOnTop
        }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // Il collegamento c'e' ma il CW non passa: lo si dice, invece di
        // lasciare i tasti che non fanno niente.
        Text {
            Layout.fillWidth: true
            visible: root.rig.connected && !root.rig.canKeyCw
            wrapMode: Text.Wrap
            color: Theme.errorColor
            font.pixelSize: 12
            text: qsTr("This CAT link does not key CW: it reads the radio but it cannot send. "
                       + "Either connect rigctld to the radio itself, or — with Decodium holding "
                       + "the CAT — set up the keyer on a serial port of its own: "
                       + "Setup → Radio (CAT) → Keying on a serial port.")
        }

        // La radio spenta non si nasconde: si dice dov'e' l'interruttore.
        Text {
            Layout.fillWidth: true
            visible: !root.rig.enabled
            wrapMode: Text.Wrap
            color: Theme.warningColor
            font.pixelSize: 12
            text: qsTr("The radio is off: Setup → Radio (CAT) to turn it on. The decoder works anyway, "
                       + "it only needs the audio coming out of the radio.")
        }

        // ── Le macro ────────────────────────────────────────────────────────
        GridLayout {
            Layout.fillWidth: true
            columns: root.compact ? 8 : 4
            columnSpacing: 6
            rowSpacing: 6
            Repeater {
                model: root.rig.macros
                GlassButton {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    buttonHeight: 28
                    fontPixelSize: 12
                    enabled: root.rig.connected && root.rig.canKeyCw
                    text: "F%1 %2".arg(index + 1).arg(modelData.label)
                    tone: index === 0 ? Theme.accentColor : "transparent"
                    onClicked: root.rig.sendMacro(index, root.cwContext())
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Speed")
                color: Theme.textSecondary
                font.pixelSize: 11
            }
            Slider {
                Layout.fillWidth: true
                from: 10
                to: 45
                stepSize: 1
                value: root.rig.wpm
                enabled: root.rig.connected
                onMoved: root.rig.wpm = Math.round(value)
            }
            Text {
                text: qsTr("%1 wpm").arg(root.rig.wpm)
                color: Theme.textPrimary
                font.family: Theme.monoFamily
                font.pixelSize: 12
                font.bold: true
            }
        }

        // ── Scrivere a mano quello che non sta in una macro ─────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            StyledTextField {
                id: freeText
                Layout.fillWidth: true
                uppercase: true
                placeholderText: qsTr("write here and press Enter: it goes out in CW")
                enabled: root.rig.connected
                onAccepted: {
                    root.rig.sendText(text, root.cwContext())
                    text = ""
                }
            }
            GlassButton {
                text: qsTr("Send")
                tone: Theme.accentColor
                buttonHeight: 28
                fontPixelSize: 11
                enabled: root.rig.connected && freeText.text.trim().length > 0
                onClicked: {
                    root.rig.sendText(freeText.text, root.cwContext())
                    freeText.text = ""
                }
            }
        }

        // ── Il decoder ──────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ToggleSwitch {
                text: qsTr("Decoder")
                checked: root.rig.decoderOn
                onToggled: root.rig.decoderOn = checked
            }
            StyledComboBox {
                id: audioBox
                Layout.fillWidth: true
                mono: false
                fieldHeight: 26
                model: root.rig.audioInputs
                currentIndex: Math.max(0, root.rig.audioInputs.indexOf(root.rig.audioInput))
                onActivated: root.rig.audioInput = currentText
            }
            Text {
                visible: root.rig.decoderOn
                text: root.rig.decoderWpm > 0
                      ? qsTr("%1 wpm · %2 Hz").arg(root.rig.decoderWpm).arg(root.rig.decoderTone)
                      : qsTr("listening…")
                color: Theme.secondaryColor
                font.family: Theme.monoFamily
                font.pixelSize: 11
            }
            GlassButton {
                text: qsTr("Clear")
                buttonHeight: 24
                fontPixelSize: 11
                onClicked: root.rig.clearDecoder()
            }
        }

        // ── Il grafico, come quello di ggmorse ────────────────────────────
        // Sopra, in verde, i segni come li sta leggendo: una barra per ogni
        // tono sopra la soglia. Sotto, in arancio, il segnale filtrato sul
        // tono negli ultimi tre secondi, con la soglia tratteggiata. Scorre da
        // destra a sinistra: a destra c'e' quello che si sente adesso.
        Rectangle {
            id: scope
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            visible: root.rig.decoderOn
            color: Theme.bgDeep
            border.color: Theme.borderSoft
            radius: 4
            clip: true

            readonly property var info: root.rig.decoderScope
            readonly property var trace: info && info.signal ? info.signal : []
            readonly property real level: info && info.level !== undefined ? info.level : 0
            readonly property bool reading: info ? info.reading === true : false
            readonly property color keyColor: Theme.successColor
            readonly property color traceColor: "#f28c28"
            onInfoChanged: plot.requestPaint()

            Canvas {
                id: plot
                anchors.fill: parent
                anchors.margins: 4
                anchors.bottomMargin: 18
                renderStrategy: Canvas.Cooperative
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const sig = scope.trace
                    const n = sig.length
                    if (n < 2 || width <= 0)
                        return
                    const keyH = 12
                    const top = keyH + 6
                    const h = height - top
                    const dx = width / (n - 1)

                    // I segni: acceso dove il segnale passa la soglia. Pieno
                    // quando sta leggendo, sbiadito quando e' solo rumore.
                    ctx.fillStyle = scope.keyColor
                    ctx.globalAlpha = scope.reading ? 0.9 : 0.25
                    let start = -1
                    for (let i = 0; i <= n; ++i) {
                        const on = i < n && sig[i] > scope.level
                        if (on && start < 0)
                            start = i
                        else if (!on && start >= 0) {
                            ctx.fillRect(start * dx, 1, Math.max(1.5, (i - start) * dx), keyH - 2)
                            start = -1
                        }
                    }
                    ctx.globalAlpha = 1

                    // Il segnale, pieno sotto la linea.
                    ctx.beginPath()
                    ctx.moveTo(0, top + h)
                    for (let i = 0; i < n; ++i)
                        ctx.lineTo(i * dx, top + h - sig[i] * h)
                    ctx.lineTo(width, top + h)
                    ctx.closePath()
                    ctx.fillStyle = Qt.alpha(scope.traceColor, 0.25)
                    ctx.fill()
                    ctx.beginPath()
                    for (let i = 0; i < n; ++i) {
                        const y = top + h - sig[i] * h
                        if (i === 0)
                            ctx.moveTo(0, y)
                        else
                            ctx.lineTo(i * dx, y)
                    }
                    ctx.strokeStyle = scope.traceColor
                    ctx.lineWidth = 1.2
                    ctx.stroke()

                    // La soglia.
                    const ly = Math.round(top + h - scope.level * h) + 0.5
                    ctx.setLineDash([4, 3])
                    ctx.strokeStyle = Theme.textSecondary
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    ctx.moveTo(0, ly)
                    ctx.lineTo(width, ly)
                    ctx.stroke()
                }
            }

            Text {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 6
                anchors.bottomMargin: 3
                text: scope.trace.length === 0
                      ? qsTr("waiting for audio…")
                      : qsTr("F: %1 Hz · S: %2 WPM · C: %3")
                        .arg(Number(scope.info.pitch || 0).toFixed(1))
                        .arg(Math.round(scope.info.wpm || 0))
                        .arg(Number(scope.info.cost || 0).toFixed(3))
                color: scope.reading ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: 10
            }
            Text {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 6
                anchors.bottomMargin: 3
                visible: scope.trace.length > 0
                text: scope.reading ? qsTr("reading") : qsTr("noise")
                color: scope.reading ? scope.keyColor : Theme.textSecondary
                font.pixelSize: 10
                font.bold: scope.reading
            }
        }

        ScrollView {
            id: decodedScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
            ScrollBar.vertical: PanelScrollBar {}
            clip: true
            contentWidth: availableWidth

            TextArea {
                id: decoded
                width: decodedScroll.availableWidth
                readOnly: true
                wrapMode: TextArea.WrapAnywhere
                text: root.rig.decoderText
                color: Theme.textPrimary
                font.family: Theme.monoFamily
                font.pixelSize: 13
                background: Rectangle { color: Theme.bgMedium; border.color: Theme.borderSoft; radius: 4 }
                // Si guarda sempre l'ultima riga, come in una telescrivente.
                onTextChanged: cursorPosition = length
            }
        }
    }

    // I tasti funzione, quando il pannello ha il fuoco.
    Repeater {
        model: 8
        Item {
            required property int index
            Shortcut {
                sequence: "F" + (index + 1)
                enabled: root.visible && root.rig.connected
                onActivated: root.rig.sendMacro(index, root.cwContext())
            }
        }
    }
}
