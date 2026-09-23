// DecoDXLog — l'inserimento veloce del contest, in un pannello suo.
//
// In gara questa e' la finestra dove stanno le mani: nominativo, rapporti,
// scambio, Invio. Sta in una finestra a sé perche' durante un contest ognuno
// la mette dove vuole — di solito in mezzo, davanti a tutto — e le altre
// (cluster, log, mappa) si dispongono attorno.
//
// Enter registra, Esc pulisce, Tab passa al campo dopo. Lo spazio dopo il
// nominativo salta allo scambio: e' il gesto che si fa senza pensarci.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    property int revision: 0
    readonly property var act: decolog.activation
    readonly property var session: { revision; return act.state }
    readonly property var scoring: { revision; return act.score() }
    readonly property bool running: act.active

    property string band: ""
    property string mode: ""

    Connections {
        target: decolog
        function onLogChanged() { root.revision++ }
    }
    Connections {
        target: decolog.activation
        function onChanged() { root.revision++ }
    }

    // Un clic su uno spot del cluster: il nominativo, la banda e il modo
    // vengono qui, la finestra passa davanti e il cursore va sullo scambio —
    // la stazione si chiama, si scrive quello che manda, Invio.
    function pickSpot(call, band, mode) {
        if (!call || call.length === 0)
            return
        callField.text = call.toUpperCase()
        const b = bandBox.bands.indexOf(band)
        if (b >= 0) {
            root.band = band
            bandBox.currentIndex = b
        }
        const m = mode === "LSB" || mode === "USB" || mode === "AM" || mode === "FM" ? "SSB" : mode
        const mi = modeBox.modes.indexOf(m)
        if (mi >= 0 && m !== root.mode) {
            root.mode = m
            modeBox.currentIndex = mi
            sentRst.text = m === "SSB" ? "59" : "599"
            rcvdRst.text = sentRst.text
        }
        message.text = ""
        const w = root.Window.window
        if (w) {
            w.raise()
            w.requestActivate()
        }
        rcvdNr.forceActiveFocus()
    }
    Connections {
        target: decolog.cluster
        function onSpotPicked(call, band, mode, freqKhz) { root.pickSpot(call, band, mode) }
    }

    Component.onCompleted: {
        if (band.length === 0)
            band = session.band || "20m"
        if (mode.length === 0)
            mode = session.mode || "CW"
        callField.forceActiveFocus()
    }

    // Quello che non va nello scambio, mentre lo si scrive: finche' il campo e'
    // vuoto non si dice niente, perche' avvisare prima che si scriva e' rumore.
    readonly property string exchangeProblem: {
        revision
        if (!running || rcvdNr.text.trim().length === 0)
            return ""
        return act.checkExchange(rcvdNr.text)
    }
    readonly property bool duplicate: callField.text.trim().length > 0
                                      && act.wouldDuplicate(callField.text, root.band, root.mode)

    function clearEntry() {
        callField.text = ""
        rcvdRst.text = root.mode === "SSB" ? "59" : "599"
        rcvdNr.text = ""
        message.text = ""
        callField.forceActiveFocus()
    }

    function logQso() {
        const call = callField.text.trim()
        if (call.length < 3) {
            message.text = qsTr("The callsign is too short")
            return
        }
        const now = decolog.utcNow()
        const error = decolog.logManualQso({
            call: call, date: now.date, time: now.time,
            band: root.band, mode: root.mode,
            rst_sent: sentRst.text, rst_rcvd: rcvdRst.text,
            srx: rcvdNr.text
        })
        if (error.length > 0) {
            message.text = error
            return
        }
        const problem = root.exchangeProblem
        root.clearEntry()
        // Lo scambio storto non ferma il QSO — la stazione e' gia' passata — ma
        // si dice, cosi' si corregge adesso invece che a spoglio fatto.
        if (problem.length > 0)
            message.text = problem
    }

    title: root.running ? qsTr("%1 · next %2").arg(root.session.title || qsTr("Contest"))
                                              .arg(root.session.nextSerial || 1)
                        : qsTr("Contest entry · no session")
    dotColor: root.running ? Theme.accentColor : Theme.textSecondary

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            LabeledField {
                label: qsTr("Band")
                StyledComboBox {
                    id: bandBox
                    Layout.preferredWidth: 96
                    readonly property var bands: ["160m", "80m", "40m", "30m", "20m", "17m",
                                                  "15m", "12m", "10m", "6m", "2m"]
                    model: bands
                    currentIndex: Math.max(0, bands.indexOf(root.band))
                    onActivated: root.band = bands[currentIndex]
                }
            }
            LabeledField {
                label: qsTr("Mode")
                StyledComboBox {
                    id: modeBox
                    Layout.preferredWidth: 96
                    readonly property var modes: ["CW", "SSB", "RTTY", "FT2", "FT8", "FT4", "PSK31"]
                    model: modes
                    currentIndex: Math.max(0, modes.indexOf(root.mode))
                    onActivated: {
                        root.mode = modes[currentIndex]
                        sentRst.text = root.mode === "SSB" ? "59" : "599"
                        rcvdRst.text = sentRst.text
                    }
                }
            }
            Item { Layout.fillWidth: true }
            Pill {
                visible: root.duplicate
                text: qsTr("already worked")
                tone: Theme.errorColor
            }
        }

        // Quando la finestra e' stretta i campi vanno a capo invece di
        // schiacciare il nominativo fino a farlo sparire: in una finestra da
        // mettere dove si vuole, "stretta" succede.
        GridLayout {
            Layout.fillWidth: true
            columns: root.width < 660 ? 3 : 6
            rowSpacing: 6
            columnSpacing: 8
            LabeledField {
                Layout.fillWidth: true
                Layout.minimumWidth: 150
                Layout.columnSpan: parent.columns === 3 ? 3 : 1
                label: qsTr("Callsign")
                StyledTextField {
                    id: callField
                    Layout.fillWidth: true
                    uppercase: true
                    fieldHeight: 40
                    font.pixelSize: 22
                    accentBorder: root.duplicate ? Theme.errorColor : Theme.primaryColor
                    onTextChanged: decolog.lookupCall = text
                    Keys.onSpacePressed: rcvdNr.forceActiveFocus()
                    Keys.onReturnPressed: root.logQso()
                    Keys.onEnterPressed: root.logQso()
                    Keys.onEscapePressed: root.clearEntry()
                }
            }
            LabeledField {
                label: qsTr("RST s")
                StyledTextField { id: sentRst; Layout.preferredWidth: 72; fieldHeight: 40; text: "599" }
            }
            LabeledField {
                label: qsTr("Nr s")
                StyledTextField {
                    Layout.preferredWidth: 72
                    fieldHeight: 40
                    readOnly: true
                    text: root.session.serialEnabled ? String(root.session.nextSerial || 1) : "—"
                }
            }
            LabeledField {
                label: qsTr("RST r")
                StyledTextField { id: rcvdRst; Layout.preferredWidth: 72; fieldHeight: 40; text: "599" }
            }
            LabeledField {
                // Il nome del campo lo decide il contest: zona, provincia,
                // sezione o numero.
                label: root.scoring.exchangeLabel || qsTr("Nr r")
                StyledTextField {
                    id: rcvdNr
                    Layout.preferredWidth: 130
                    fieldHeight: 40
                    uppercase: true
                    accentBorder: root.exchangeProblem.length > 0 ? Theme.warningColor
                                                                  : Theme.primaryColor
                    Keys.onReturnPressed: root.logQso()
                    Keys.onEnterPressed: root.logQso()
                    Keys.onEscapePressed: root.clearEntry()
                }
            }
            GlassButton {
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: 2
                Layout.columnSpan: parent.columns === 3 ? 3 : 1
                Layout.fillWidth: parent.columns === 3
                text: qsTr("Log")
                tone: Theme.accentColor
                filled: true
                buttonHeight: 40
                enabled: root.running && callField.text.trim().length > 0
                onClicked: root.logQso()
            }
        }

        Text {
            id: message
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.warningColor
            font.pixelSize: 12
            text: root.running ? "" : qsTr("Open a session from Contest, then the QSOs come in here.")
        }
        Item { Layout.fillHeight: true }
    }
}
