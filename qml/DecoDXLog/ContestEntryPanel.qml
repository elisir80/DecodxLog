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
        root.goToExchange()
    }
    Connections {
        target: decolog.cluster
        function onSpotPicked(call, band, mode, freqKhz) { root.pickSpot(call, band, mode) }
    }

    // Il modo di partenza: quello della sessione, o quello che dice il nome
    // della gara (CQ-WW-SSB e' in fonia), o quello della radio. Prima partiva
    // sempre in CW, anche in una gara in SSB, con il 599 al posto del 59.
    function startingMode() {
        if (root.session.mode)
            return root.session.mode
        const id = String(root.session.contestId || "").toUpperCase()
        if (id.endsWith("-SSB") || id.endsWith("-PH") || id.endsWith("-PHONE"))
            return "SSB"
        if (id.endsWith("-RTTY"))
            return "RTTY"
        if (id.endsWith("-CW"))
            return "CW"
        const rig = String(decolog.shownMode || "").toUpperCase()
        if (rig === "LSB" || rig === "USB" || rig === "SSB" || rig === "AM" || rig === "FM")
            return "SSB"
        if (rig === "CW" || rig === "CW-R" || rig === "RTTY" || rig === "JTTY" || rig === "FT8" || rig === "FT4" || rig === "FT2")
            return rig === "CW-R" ? "CW" : rig
        return "CW"
    }

    Component.onCompleted: {
        if (band.length === 0)
            band = session.band || "20m"
        if (mode.length === 0)
            mode = root.startingMode()
        sentRst.text = mode === "SSB" ? "59" : "599"
        rcvdRst.text = sentRst.text
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

    // Lo scambio che la stazione mandera', scritto da solo mentre si batte il
    // nominativo: la zona dal paese, o quello che ha mandato l'ultima volta
    // (la provincia, la sezione, la zona di una HQ). Si sostituisce solo un
    // campo vuoto o gia' riempito da qui: quello scritto a mano non si tocca.
    // Il progressivo non si puo' sapere prima, e resta da scrivere.
    property string autoFrom: ""
    property bool settingAuto: false
    function suggestExchange() {
        if (!root.running)
            return
        if (rcvdNr.text.length > 0 && root.autoFrom === "")
            return
        const s = decolog.activation.suggestExchange(callField.text.trim())
        root.settingAuto = true
        rcvdNr.text = s.value || ""
        root.settingAuto = false
        root.autoFrom = rcvdNr.text.length > 0 ? s.from : ""
    }
    Timer { id: suggestTimer; interval: 200; onTriggered: root.suggestExchange() }
    // Il nome del campo dice da dove viene quello che c'e' dentro.
    function exchangeLabelText() {
        const base = root.scoring.exchangeLabel || qsTr("Nr r")
        return root.autoFrom === "log" ? qsTr("%1 · log").arg(base)
             : root.autoFrom === "cty" ? qsTr("%1 · country").arg(base)
             : base
    }
    // Si passa allo scambio con tutto selezionato: se il suggerimento va bene
    // si preme Invio, se no si scrive sopra.
    function goToExchange() {
        suggestTimer.stop()
        root.suggestExchange()
        rcvdNr.forceActiveFocus()
        rcvdNr.selectAll()
    }

    // La radio va dove si sceglie: cambiando banda, sulla frequenza di quella
    // banda (l'ultima usata li', o l'inizio del segmento del modo); cambiando
    // modo, il modo — e per i digitali anche la frequenza di chiamata. E al
    // contrario, se si gira la manopola della radio l'inserimento la segue.
    property var bandMemory: ({})
    // Subito dopo un cambio chiesto da qui la radio ci mette un attimo: nel
    // frattempo non si segue quello che dice, se no il menu tornerebbe indietro.
    property double lastQsy: 0
    // La memoria e' per banda e modo, come il tasto band stack delle radio:
    // tornando sui 20 metri in SSB si ritrova la frequenza lasciata li' in
    // SSB, non quella del CW.
    function memoryKey(band, mode) {
        const m = ["LSB", "USB", "AM", "FM"].indexOf(mode) >= 0 ? "SSB" : mode
        return band + "|" + m
    }
    function qsyTo(band, mode) {
        root.lastQsy = Date.now()
        const here = parseFloat(decolog.shownFrequency || "0")
        const hereBand = here > 0 ? decolog.bandForFrequency(String(here)) : ""
        const hereMode = String(decolog.shownMode || "").toUpperCase()
        if (hereBand.length > 0 && hereMode.length > 0)
            root.bandMemory[root.memoryKey(hereBand, hereMode)] = here
        let mhz = root.bandMemory[root.memoryKey(band, mode)] || decolog.bandFrequency(band, mode)
        if (mhz <= 0)
            mhz = here
        decolog.tuneTo(mhz, mode)
    }
    Connections {
        target: decolog
        function onTuningChanged() {
            if (Date.now() - root.lastQsy < 2500)
                return
            const f = parseFloat(decolog.shownFrequency || "0")
            const b = f > 0 ? decolog.bandForFrequency(String(f)) : ""
            if (b.length > 0 && b !== root.band && bandBox.bands.indexOf(b) >= 0) {
                root.band = b
                bandBox.currentIndex = bandBox.bands.indexOf(b)
            }
            const raw = String(decolog.shownMode || "").toUpperCase()
            const m = raw === "LSB" || raw === "USB" || raw === "AM" || raw === "FM" ? "SSB"
                    : raw === "CW-R" ? "CW" : raw
            const i = modeBox.modes.indexOf(m)
            if (i >= 0 && m !== root.mode) {
                root.mode = m
                modeBox.currentIndex = i
                sentRst.text = m === "SSB" ? "59" : "599"
                rcvdRst.text = sentRst.text
            }
        }
    }

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

    // Quanto serve per vedere tutti i campi e Registra: chi la ospita non la
    // stringe sotto questa misura, anche quando i campi vanno a capo.
    implicitHeight: entryColumn.implicitHeight + 20 + padding * 2 + Theme.panelHeight + 2

    ColumnLayout {
        id: entryColumn
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
                    onActivated: {
                        root.band = bands[currentIndex]
                        root.qsyTo(root.band, root.mode)
                    }
                }
            }
            LabeledField {
                label: qsTr("Mode")
                StyledComboBox {
                    id: modeBox
                    Layout.preferredWidth: 96
                    readonly property var modes: ["CW", "SSB", "RTTY", "JTTY", "FT2", "FT8", "FT4", "PSK31"]
                    model: modes
                    currentIndex: Math.max(0, modes.indexOf(root.mode))
                    onActivated: {
                        root.mode = modes[currentIndex]
                        sentRst.text = root.mode === "SSB" ? "59" : "599"
                        rcvdRst.text = sentRst.text
                        root.qsyTo(root.band, root.mode)
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
            columns: root.width < 660 ? 4 : 6
            rowSpacing: 6
            columnSpacing: 8
            LabeledField {
                Layout.fillWidth: true
                Layout.minimumWidth: 150
                Layout.columnSpan: parent.columns === 4 ? 4 : 1
                label: qsTr("Callsign")
                StyledTextField {
                    id: callField
                    Layout.fillWidth: true
                    uppercase: true
                    fieldHeight: 40
                    font.pixelSize: 22
                    accentBorder: root.duplicate ? Theme.errorColor : Theme.primaryColor
                    onTextChanged: {
                        decolog.lookupCall = text
                        suggestTimer.restart()
                    }
                    Keys.onSpacePressed: root.goToExchange()
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
                label: root.exchangeLabelText()
                StyledTextField {
                    id: rcvdNr
                    Layout.preferredWidth: 130
                    onTextChanged: if (!root.settingAuto) root.autoFrom = ""
                    color: root.autoFrom.length > 0 ? Theme.accentColor : Theme.textPrimary
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
                Layout.columnSpan: parent.columns === 4 ? 4 : 1
                Layout.fillWidth: parent.columns === 4
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
