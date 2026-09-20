// DecoDXLog — il CW: le macro che vanno al manipolatore della radio e il decoder
// che legge quello che arriva.
//
// Sta in piedi da solo: non serve aprire il contest. Il testo esce dal
// manipolatore della radio (Hamlib), e quello che entra lo legge DecoDXLog
// dall'audio, senza chiedere niente alla radio.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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

        ScrollView {
            id: decodedScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
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
