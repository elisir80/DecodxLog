// DecoLog — il contest, con le mani sulla tastiera.
//
// In un contest non si usa il mouse: si scrive il nominativo, si va avanti con
// Tab o con la barra, si registra con Invio, si pulisce con Esc. Tutto quello che
// serve sapere sta a schermo mentre si scrive: se e' un duplicato, che ritmo si
// tiene, quanti moltiplicatori sono entrati, e gli ultimi QSO fatti.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtCore
import Decodium.UI

ApplicationWindow {
    id: root

    property int revision: 0
    readonly property var session: { revision; return decolog.activation.state }
    readonly property var recent: { revision; return decolog.activation.recentQsos(14) }
    readonly property var rate: { revision; return decolog.activation.rate() }
    readonly property bool running: decolog.activation.active

    property string band: ""
    property string mode: ""

    width: 1080
    height: 660
    visible: true
    title: qsTr("DecoLog — Contest")
    color: Theme.bgDeep

    Settings {
        category: "contestWindow"
        property alias width: root.width
        property alias height: root.height
        property alias band: root.band
        property alias mode: root.mode
    }

    Connections {
        target: decolog
        function onLogChanged() { root.revision++ }
    }
    Connections {
        target: decolog.activation
        function onChanged() { root.revision++ }
    }
    Timer {
        running: root.running
        interval: 15000
        repeat: true
        onTriggered: root.revision++
    }

    Component.onCompleted: {
        if (root.band.length === 0)
            root.band = root.session.band || "20m"
        if (root.mode.length === 0)
            root.mode = root.session.mode || "CW"
        callField.forceActiveFocus()
    }

    readonly property bool duplicate: callField.text.trim().length >= 3
                                      && decolog.activation.wouldDuplicate(callField.text, root.band, root.mode)

    function openCabrillo() { cabrilloDialog.openDialog() }

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
        root.clearEntry()
    }

    component Tile: Rectangle {
        property string label: ""
        property string value: ""
        property color tone: Theme.textPrimary
        implicitWidth: 120
        implicitHeight: 52
        radius: 5
        color: Theme.bgMedium
        border.width: 1
        border.color: Theme.borderSoft
        Column {
            anchors.centerIn: parent
            spacing: 2
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: parent.parent.label
                color: Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: 10
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: parent.parent.value
                color: parent.parent.tone
                font.family: Theme.monoFamily
                font.pixelSize: 18
                font.bold: true
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // ── Sessione ────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Text {
                text: root.running ? root.session.title || qsTr("Session") : qsTr("No session open")
                color: root.running ? Theme.textPrimary : Theme.warningColor
                font.family: Theme.monoFamily
                font.pixelSize: 15
                font.bold: true
            }
            Text {
                visible: root.running
                text: "· " + (root.session.elapsed || "") + " · " + qsTr("next number %1").arg(root.session.nextSerial || 1)
                color: Theme.textSecondary
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            GlassButton {
                visible: !root.running
                text: qsTr("Open a session…")
                tone: Theme.primaryColor
                filled: true
                buttonHeight: 26
                fontPixelSize: 12
                onClicked: window.openActivation()
            }
        }

        // ── Contatori ───────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Tile { label: qsTr("QSO"); value: String(root.session.qsoCount || 0); tone: Theme.primaryColor }
            Tile { label: qsTr("Calls"); value: String(root.session.uniqueCalls || 0) }
            Tile { label: qsTr("DXCC"); value: String(root.rate.dxcc || 0); tone: Theme.accentColor }
            Tile { label: qsTr("Grids"); value: String(root.rate.grids || 0) }
            Tile { label: qsTr("Last 10 min"); value: String(root.rate.last10 || 0) }
            Tile {
                label: qsTr("QSO/h")
                value: String(root.rate.perHour10 || 0)
                tone: (root.rate.perHour10 || 0) >= 60 ? Theme.accentColor : Theme.textPrimary
            }
            Tile { label: qsTr("Last hour"); value: String(root.rate.last60 || 0) }
            Item { Layout.fillWidth: true }
        }

        // ── Riga di battitura ───────────────────────────────────────────────
        GlassPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            title: qsTr("New QSO · Enter logs · Esc clears · Tab moves")
            dotColor: root.duplicate ? Theme.errorColor : Theme.accentColor

            RowLayout {
                anchors.fill: parent
                spacing: 12

                LabeledField {
                    label: qsTr("Band")
                    StyledComboBox {
                        Layout.preferredWidth: 110
                        readonly property var bands: ["160m", "80m", "60m", "40m", "30m", "20m", "17m",
                                                      "15m", "12m", "10m", "6m", "2m", "70cm"]
                        model: bands
                        currentIndex: Math.max(0, bands.indexOf(root.band))
                        onActivated: root.band = bands[currentIndex]
                    }
                }
                LabeledField {
                    label: qsTr("Mode")
                    StyledComboBox {
                        Layout.preferredWidth: 110
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
                LabeledField {
                    label: qsTr("Callsign")
                    StyledTextField {
                        id: callField
                        Layout.preferredWidth: 190
                        uppercase: true
                        fieldHeight: 38
                        font.pixelSize: 20
                        accentBorder: root.duplicate ? Theme.errorColor : Theme.primaryColor
                        onTextChanged: decolog.lookupCall = text
                        Keys.onSpacePressed: rcvdRst.forceActiveFocus()
                    }
                }
                LabeledField {
                    label: qsTr("RST s")
                    StyledTextField {
                        id: sentRst
                        Layout.preferredWidth: 80
                        fieldHeight: 38
                        text: "599"
                    }
                }
                LabeledField {
                    label: qsTr("Nr s")
                    StyledTextField {
                        Layout.preferredWidth: 90
                        fieldHeight: 38
                        readOnly: true
                        text: root.session.serialEnabled ? String(root.session.nextSerial || 1) : "—"
                    }
                }
                LabeledField {
                    label: qsTr("RST r")
                    StyledTextField {
                        id: rcvdRst
                        Layout.preferredWidth: 80
                        fieldHeight: 38
                        text: "599"
                    }
                }
                LabeledField {
                    label: qsTr("Nr r")
                    StyledTextField {
                        id: rcvdNr
                        Layout.preferredWidth: 110
                        fieldHeight: 38
                        uppercase: true
                    }
                }
                GlassButton {
                    Layout.alignment: Qt.AlignBottom
                    Layout.bottomMargin: 2
                    text: qsTr("Log")
                    tone: Theme.accentColor
                    filled: true
                    buttonHeight: 38
                    enabled: root.running && callField.text.trim().length >= 3
                    onClicked: root.logQso()
                }
                Item { Layout.fillWidth: true }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Rectangle {
                visible: root.duplicate
                implicitWidth: 90
                implicitHeight: 24
                radius: 4
                color: Qt.alpha(Theme.errorColor, 0.2)
                border.width: 1
                border.color: Theme.errorColor
                Text {
                    anchors.centerIn: parent
                    text: qsTr("DUPE")
                    color: Theme.errorColor
                    font.family: Theme.monoFamily
                    font.pixelSize: 13
                    font.bold: true
                }
            }
            Text {
                readonly property var info: decolog.callInfo
                text: info.call === callField.text.trim().toUpperCase()
                      ? [info.country,
                         (info.count || 0) > 0 ? qsTr("worked %1×").arg(info.count) : qsTr("new station"),
                         info.distanceKm ? qsTr("%1 km · %2°").arg(Math.round(info.distanceKm)).arg(Math.round(info.bearing || 0)) : ""
                        ].filter(s => s).join(" · ")
                      : ""
                color: (decolog.callInfo.count || 0) > 0 ? Theme.textSecondary : Theme.accentColor
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Text {
                id: message
                color: Theme.errorColor
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.maximumWidth: 380
            }
        }

        // ── Ultimi QSO ──────────────────────────────────────────────────────
        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: qsTr("Last QSOs of the session")

            ListView {
                anchors.fill: parent
                clip: true
                model: root.recent
                ScrollBar.vertical: ScrollBar {}
                delegate: RowLayout {
                    required property var modelData
                    width: ListView.view.width
                    spacing: 0
                    Repeater {
                        model: [{ v: modelData.time, w: 70 }, { v: modelData.call, w: 130, bold: true },
                                { v: modelData.band, w: 70 }, { v: modelData.mode, w: 80 },
                                { v: modelData.rstSent, w: 60 }, { v: modelData.rstRcvd, w: 60 }]
                        Text {
                            required property var modelData
                            Layout.preferredWidth: modelData.w
                            text: modelData.v || "—"
                            color: modelData.bold ? Theme.textPrimary : Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 13
                            font.bold: modelData.bold === true
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.country || ""
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: root.recent.length === 0
                    text: qsTr("No QSO in this session yet.")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            GlassButton {
                text: qsTr("Cabrillo…")
                tone: Theme.primaryColor
                filled: true
                buttonHeight: 26
                fontPixelSize: 12
                enabled: root.running && (root.session.qsoCount || 0) > 0
                onClicked: cabrilloDialog.openDialog()
            }
            GlassButton {
                text: qsTr("ADIF of the session…")
                buttonHeight: 26
                fontPixelSize: 12
                enabled: root.running && (root.session.qsoCount || 0) > 0
                onClicked: adifDialog.open()
            }
            Item { Layout.fillWidth: true }
            Text {
                text: qsTr("Enter logs · Esc clears · space moves to the report")
                color: Theme.textSecondary
                font.pixelSize: 11
            }
        }
    }

    // Invio e Esc valgono ovunque nella finestra.
    Shortcut { sequence: "Return"; onActivated: root.logQso() }
    Shortcut { sequence: "Enter"; onActivated: root.logQso() }
    Shortcut { sequence: "Esc"; onActivated: root.clearEntry() }

    FileDialog {
        id: adifDialog
        title: qsTr("ADIF of the session")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "adi"
        nameFilters: [qsTr("ADIF files (*.adi)")]
        onAccepted: decolog.activation.exportAdif(selectedFile)
    }

    FileDialog {
        id: cabrilloFile
        title: qsTr("Cabrillo log")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "log"
        nameFilters: [qsTr("Cabrillo files (*.log *.cbr)"), qsTr("All files (*)")]
        onAccepted: {
            const error = decolog.activation.exportCabrillo(selectedFile, cabrilloDialog.values())
            cabrilloDialog.error = error
            if (error.length === 0)
                cabrilloDialog.close()
        }
    }

    // ── Testata Cabrillo ────────────────────────────────────────────────────
    DialogFrame {
        id: cabrilloDialog
        property string error: ""
        width: Math.min(680, root.width - 60)
        title: qsTr("Cabrillo header")
        dotColor: Theme.primaryColor
        leftPadding: 14
        rightPadding: 14
        bottomPadding: 14

        function openDialog() {
            const d = decolog.activation.cabrilloDefaults()
            contestField.text = d.contest || ""
            callField2.text = d.callsign || ""
            gridField.text = d.gridLocator || ""
            operatorsField.text = d.operators || ""
            locationField.text = d.location || ""
            error = ""
            open()
        }
        function values() {
            return {
                contest: contestField.text, callsign: callField2.text,
                gridLocator: gridField.text, location: locationField.text,
                operators: operatorsField.text, club: clubField.text,
                categoryOperator: opBox.currentText, categoryPower: powerBox.currentText,
                categoryMode: modeBox2.currentText, categoryAssisted: assistedBox.currentText,
                claimedScore: scoreField.text, soapbox: soapboxField.text
            }
        }

        contentItem: ColumnLayout {
            anchors.margins: 14
            spacing: 10

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 8

                LabeledField {
                    label: qsTr("Contest (CONTEST)")
                    StyledTextField {
                        id: contestField
                        Layout.preferredWidth: 260
                        uppercase: true
                        placeholderText: "CQ-WW-CW"
                    }
                }
                LabeledField {
                    label: qsTr("Callsign")
                    StyledTextField { id: callField2; Layout.preferredWidth: 260; uppercase: true }
                }
                LabeledField {
                    label: qsTr("Grid")
                    StyledTextField { id: gridField; Layout.preferredWidth: 260; uppercase: true }
                }
                LabeledField {
                    label: qsTr("Location (section, zone)")
                    StyledTextField { id: locationField; Layout.preferredWidth: 260; uppercase: true }
                }
                LabeledField {
                    label: qsTr("Operators")
                    StyledTextField { id: operatorsField; Layout.preferredWidth: 260; uppercase: true }
                }
                LabeledField {
                    label: qsTr("Club")
                    StyledTextField { id: clubField; Layout.preferredWidth: 260; mono: false }
                }
                LabeledField {
                    label: qsTr("Category")
                    StyledComboBox {
                        id: opBox
                        Layout.preferredWidth: 260
                        model: ["SINGLE-OP", "MULTI-OP", "CHECKLOG"]
                    }
                }
                LabeledField {
                    label: qsTr("Power")
                    StyledComboBox {
                        id: powerBox
                        Layout.preferredWidth: 260
                        model: ["LOW", "HIGH", "QRP"]
                    }
                }
                LabeledField {
                    label: qsTr("Mode")
                    StyledComboBox {
                        id: modeBox2
                        Layout.preferredWidth: 260
                        model: ["MIXED", "CW", "SSB", "RTTY", "DIGI", "FM"]
                    }
                }
                LabeledField {
                    label: qsTr("Assisted")
                    StyledComboBox {
                        id: assistedBox
                        Layout.preferredWidth: 260
                        model: ["NON-ASSISTED", "ASSISTED"]
                    }
                }
                LabeledField {
                    label: qsTr("Claimed score")
                    StyledTextField { id: scoreField; Layout.preferredWidth: 260 }
                }
                LabeledField {
                    label: qsTr("Soapbox")
                    StyledTextField { id: soapboxField; Layout.preferredWidth: 260; mono: false }
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: cabrilloDialog.error.length > 0
                      ? cabrilloDialog.error
                      : qsTr("The exchange sent is the serial number of the session; the received one is what "
                             + "was typed in “Nr r”. Frequencies go out in kHz, VHF and up as the band number.")
                color: cabrilloDialog.error.length > 0 ? Theme.errorColor : Theme.textSecondary
                font.pixelSize: 11
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                GlassButton {
                    text: qsTr("Close")
                    buttonHeight: 28
                    onClicked: cabrilloDialog.close()
                }
                GlassButton {
                    text: qsTr("Write the file…")
                    tone: Theme.primaryColor
                    filled: true
                    buttonHeight: 28
                    enabled: contestField.text.trim().length > 0 && callField2.text.trim().length > 0
                    onClicked: cabrilloFile.open()
                }
            }
        }
    }
}
