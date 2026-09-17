// DecoLog — Setup (mockup 1e): navigazione a sinistra, pagina a destra.
//
// Le pagine di cose che non esistono ancora (cloud, servizi QSL, callbook) lo
// dicono chiaramente e salvano solo le preferenze che avranno senso quando
// arriveranno: nessuno stato finto.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Decodium.UI

DialogFrame {
    id: root

    property int page: 3

    title: qsTr("Setup")
    dotColor: Theme.secondaryColor
    width: 820

    readonly property var pages: [qsTr("General"), qsTr("Theme & density"), qsTr("Decodium link"),
                                  qsTr("Sync & Cloud"), qsTr("QSL services"), qsTr("Callbook"), qsTr("Backup")]

    onOpened: {
        portField.text = decolog.udpPort
        groupField.text = decolog.multicastGroup
        serverField.text = decolog.cloudServer
        backupDirField.text = decolog.backupDir
        backupTimeField.text = decolog.backupTime
        keepField.text = decolog.backupKeep
    }

    function apply() {
        const port = parseInt(portField.text)
        if (!isNaN(port))
            decolog.udpPort = port
        decolog.multicastGroup = groupField.text.trim()
        decolog.cloudServer = serverField.text.trim()
        decolog.backupDir = backupDirField.text.trim()
        decolog.backupTime = backupTimeField.text.trim()
        const keep = parseInt(keepField.text)
        if (!isNaN(keep))
            decolog.backupKeep = keep
    }

    FileDialog {
        id: ctyDialog
        title: qsTr("cty.csv from country-files.com")
        nameFilters: [qsTr("cty.csv (*.csv)")]
        onAccepted: {
            const error = decolog.installCountries(selectedFile)
            ctyNote.text = error.length ? error : qsTr("cty.csv %1 in use.").arg(decolog.countriesVersion)
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Backup folder")
        onAccepted: backupDirField.text = decolog.localPath(selectedFolder)
    }

    component Tile: StatTile { Layout.fillWidth: true }
    component Note: Text {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Theme.textSecondary
        font.pixelSize: 12
    }
    component CredentialRow: RowLayout {
        property string service: ""
        property string account: "—"
        property string state: qsTr("not set")
        Layout.fillWidth: true
        Layout.preferredHeight: 30
        spacing: 10
        Text { Layout.preferredWidth: 120; text: parent.service; color: Theme.textPrimary; font.family: Theme.monoFamily; font.pixelSize: 12; font.bold: true }
        Text { Layout.fillWidth: true; text: parent.account; color: Theme.textSecondary; font.family: Theme.monoFamily; font.pixelSize: 12 }
        Text { Layout.preferredWidth: 90; text: parent.state; color: Theme.textSecondary; font.family: Theme.monoFamily; font.pixelSize: 12 }
        Text { Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight; text: qsTr("Add"); color: Theme.textSecondary; font.family: Theme.monoFamily; font.pixelSize: 12 }
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── Navigazione ─────────────────────────────────────────────────
            ColumnLayout {
                Layout.preferredWidth: 190
                Layout.fillHeight: true
                Layout.margins: 8
                spacing: 2
                Repeater {
                    model: root.pages
                    AbstractButton {
                        id: navItem
                        required property string modelData
                        required property int index
                        readonly property bool active: root.page === index
                        Layout.fillWidth: true
                        implicitHeight: 30
                        contentItem: Text {
                            leftPadding: 10
                            verticalAlignment: Text.AlignVCenter
                            text: navItem.modelData
                            color: navItem.active ? Theme.primaryColor : Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 12
                            font.bold: true
                        }
                        background: Rectangle {
                            radius: 4
                            color: navItem.active ? Theme.glassOverlay : navItem.hovered ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.08) : "transparent"
                            border.width: navItem.active ? 1 : 0
                            border.color: Theme.primaryColor
                        }
                        onClicked: root.page = index
                    }
                }
                Item { Layout.fillHeight: true }
            }
            Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: Theme.borderSoft }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 14
                Layout.minimumHeight: 470
                currentIndex: root.page

                // ── General ─────────────────────────────────────────────────
                ColumnLayout {
                    spacing: 12
                    SectionTitle { text: qsTr("Log file") }
                    RowLayout {
                        Layout.fillWidth: true
                        StyledTextField { Layout.fillWidth: true; readOnly: true; text: decolog.databasePath }
                        GlassButton { text: qsTr("Open folder"); onClicked: decolog.openDatabaseFolder() }
                    }
                    Note { text: qsTr("SQLite in WAL mode. To use another file start DecoLog with --db <path>.") }
                    SectionTitle { text: qsTr("DXCC entities") }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Tile { label: qsTr("cty.csv"); value: decolog.countriesVersion || "—" }
                        Tile { label: qsTr("Entities"); value: decolog.countriesEntities }
                        Tile {
                            label: qsTr("QSO without DXCC")
                            value: decolog.missingDxccCount
                            valueColor: decolog.missingDxccCount > 0 ? Theme.warningColor : Theme.textPrimary
                        }
                    }
                    RowLayout {
                        spacing: 8
                        GlassButton {
                            text: qsTr("Fill missing DXCC")
                            tone: Theme.primaryColor
                            filled: true
                            enabled: decolog.missingDxccCount > 0
                            onClicked: ctyNote.text = qsTr("%1 QSO completed, each kept as a new revision.").arg(decolog.fillMissingDxcc())
                        }
                        GlassButton { text: qsTr("Load newer cty.csv…"); onClicked: ctyDialog.open() }
                    }
                    Note {
                        id: ctyNote
                        text: qsTr("Source: %1. Updated files: country-files.com (AD1C). New QSOs from Decodium and manual entries get DXCC, country, zones and continent automatically; imported ADIF is kept as it is.").arg(decolog.countriesSource)
                    }
                    SectionTitle { text: qsTr("Call info") }
                    ToggleSwitch {
                        text: qsTr("Follow the DX call Decodium is working")
                        checked: decolog.followDxCall
                        onToggled: decolog.followDxCall = checked
                    }
                    SectionTitle { text: qsTr("Duplicates") }
                    RowLayout {
                        spacing: 10
                        LabeledField {
                            label: qsTr("Digital (min)")
                            StyledComboBox {
                                Layout.preferredWidth: 110
                                model: [1, 2, 5, 10]
                                currentIndex: Math.max(0, model.indexOf(decolog.dedupDigitalMinutes))
                                onActivated: decolog.dedupDigitalMinutes = model[currentIndex]
                            }
                        }
                        LabeledField {
                            label: qsTr("Manual (min)")
                            StyledComboBox {
                                Layout.preferredWidth: 110
                                model: [5, 10, 15, 30]
                                currentIndex: Math.max(0, model.indexOf(decolog.dedupManualMinutes))
                                onActivated: decolog.dedupManualMinutes = model[currentIndex]
                            }
                        }
                    }
                    Note { text: qsTr("Same call, band and mode/submode within this window counts as the same QSO.") }
                    Item { Layout.fillHeight: true }
                }

                // ── Theme & density ─────────────────────────────────────────
                ColumnLayout {
                    spacing: 12
                    GridLayout {
                        columns: 2
                        columnSpacing: 14
                        rowSpacing: 10
                        FieldLabel { text: qsTr("Theme") }
                        StyledComboBox {
                            Layout.preferredWidth: 220
                            model: Theme.availableThemes
                            currentIndex: Theme.availableThemes.indexOf(Theme.currentTheme)
                            onActivated: Theme.currentTheme = currentText
                        }
                        FieldLabel { text: qsTr("Accent (Darkcodium)") }
                        StyledComboBox {
                            Layout.preferredWidth: 220
                            enabled: Theme.currentTheme === "Darkcodium"
                            model: Theme.availableVariants
                            currentIndex: Theme.availableVariants.indexOf(Theme.accentVariant)
                            onActivated: Theme.accentVariant = currentText
                        }
                        FieldLabel { text: qsTr("Density") }
                        StyledComboBox {
                            Layout.preferredWidth: 220
                            model: Theme.availableDensities
                            currentIndex: Theme.availableDensities.indexOf(Theme.density)
                            onActivated: Theme.density = currentText
                        }
                        FieldLabel { text: qsTr("Custom colors") }
                        ToggleSwitch {
                            text: qsTr("Background and text over the theme")
                            checked: Theme.customColorsEnabled
                            onToggled: Theme.customColorsEnabled = checked
                        }
                        FieldLabel { text: qsTr("Background") }
                        StyledTextField {
                            Layout.preferredWidth: 220
                            enabled: Theme.customColorsEnabled
                            placeholderText: "#RRGGBB"
                            text: Theme.customBgColor
                            onEditingFinished: Theme.customBgColor = text
                        }
                        FieldLabel { text: qsTr("Text") }
                        StyledTextField {
                            Layout.preferredWidth: 220
                            enabled: Theme.customColorsEnabled
                            placeholderText: "#RRGGBB"
                            text: Theme.customTextColor
                            onEditingFinished: Theme.customTextColor = text
                        }
                    }
                    Note { text: qsTr("Same themes, accents and densities as Decodium: row %1 px · font %2 px · header %3 px.").arg(Theme.rowHeight).arg(Theme.fontSize).arg(Theme.panelHeight) }
                    Item { Layout.fillHeight: true }
                }

                // ── Decodium link ───────────────────────────────────────────
                ColumnLayout {
                    spacing: 12
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: Math.max(56, children[0].implicitHeight + 24)
                        radius: 5
                        color: decolog.clientConnected ? Theme.rowMatchBg : "transparent"
                        border.width: 1
                        border.color: decolog.clientConnected ? Theme.accentColor : Theme.glassBorder
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 12
                            Led { size: 10; glow: decolog.clientConnected; color: decolog.clientConnected ? Theme.accentColor : Theme.textSecondary }
                            Column {
                                Layout.fillWidth: true
                                Text {
                                    text: decolog.clientConnected ? qsTr("%1 · connected").arg(decolog.clientName)
                                                                  : qsTr("No client heard yet")
                                    color: Theme.textPrimary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                Text {
                                    text: decolog.udpError.length ? decolog.udpError
                                         : decolog.listening ? qsTr("listening on UDP %1").arg(decolog.udpPort) : qsTr("not listening")
                                    color: decolog.udpError.length ? Theme.errorColor : Theme.textSecondary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                    SectionTitle { text: qsTr("Decodium / WSJT-X UDP") }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 10
                        LabeledField {
                            Layout.preferredWidth: 100
                            label: qsTr("Port")
                            StyledTextField { id: portField; Layout.fillWidth: true; validator: IntValidator { bottom: 0; top: 65535 } }
                        }
                        LabeledField {
                            Layout.fillWidth: true
                            label: qsTr("Primary source")
                            StyledComboBox {
                                Layout.fillWidth: true
                                model: [qsTr("LoggedADIF (lossless)"), qsTr("QSOLogged (structured)")]
                                currentIndex: decolog.preferLoggedAdif ? 0 : 1
                                onActivated: decolog.preferLoggedAdif = currentIndex === 0
                            }
                        }
                        LabeledField {
                            Layout.fillWidth: true
                            label: qsTr("Multicast group")
                            StyledTextField { id: groupField; Layout.fillWidth: true; placeholderText: qsTr("empty = unicast") }
                        }
                    }
                    Note { text: qsTr("In Decodium set the UDP server to this address and port. A multicast group (e.g. 239.255.0.1) shares the stream with GridTracker or JTAlert. Duplicate windows are in General.") }
                    Item { Layout.fillHeight: true }
                }

                // ── Sync & Cloud ────────────────────────────────────────────
                ColumnLayout {
                    spacing: 14
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: Math.max(56, children[0].implicitHeight + 24)
                        radius: 5
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.glassBorder
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 12
                            Led { size: 10; color: Theme.textSecondary }
                            Column {
                                Layout.fillWidth: true
                                Text {
                                    text: qsTr("DecoLog Cloud · not connected")
                                    color: Theme.textPrimary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                Text {
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    text: qsTr("The sync service arrives in Phase 3. Every QSO is already tracked for it (uuid, revision, dirty).")
                                    color: Theme.textSecondary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 11
                                }
                            }
                            GlassButton { text: qsTr("Sign in"); enabled: false }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Tile { label: qsTr("Last push"); value: "—" }
                        Tile { label: qsTr("Last pull"); value: "—" }
                        Tile { label: qsTr("Queue (dirty)"); value: decolog.dirtyCount; valueColor: decolog.dirtyCount > 0 ? Theme.warningColor : Theme.textPrimary }
                        Tile { label: qsTr("Conflicts kept"); value: qsTr("%1 in history").arg(decolog.conflictCount) }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 10
                        LabeledField {
                            Layout.fillWidth: true
                            label: qsTr("Server")
                            StyledTextField { id: serverField; Layout.fillWidth: true; placeholderText: "https://…/v1" }
                        }
                        LabeledField {
                            Layout.fillWidth: true
                            label: qsTr("Auto sync")
                            StyledComboBox {
                                Layout.fillWidth: true
                                readonly property var values: ["qso+5min", "5min", "manual"]
                                model: [qsTr("After every QSO + every 5 min"), qsTr("Every 5 min"), qsTr("Manual only")]
                                currentIndex: Math.max(0, values.indexOf(decolog.autoSync))
                                onActivated: decolog.autoSync = values[currentIndex]
                            }
                        }
                    }
                    ColumnLayout {
                        spacing: 6
                        SectionTitle { text: qsTr("Conflicts") }
                        RowLayout {
                            spacing: 16
                            RadioChoice {
                                text: qsTr("Last edit wins, loser kept in history")
                                checked: decolog.conflictPolicy === "lastEdit"
                                onClicked: decolog.conflictPolicy = "lastEdit"
                            }
                            RadioChoice {
                                text: qsTr("Always ask")
                                checked: decolog.conflictPolicy === "ask"
                                onClicked: decolog.conflictPolicy = "ask"
                            }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        SectionTitle { text: qsTr("Credentials · system keystore") }
                        CredentialRow { service: "DecoLog Cloud" }
                        CredentialRow { service: "QRZ.com" }
                        CredentialRow { service: "LoTW"; account: qsTr("TQSL local") }
                        CredentialRow { service: "Club Log" }
                        CredentialRow { service: "eQSL" }
                        Note { text: qsTr("Credentials will live in the system keystore (qtkeychain), never in the settings file. The keystore is not linked in this build yet.") }
                    }
                    Item { Layout.fillHeight: true }
                }

                // ── QSL services ────────────────────────────────────────────
                ColumnLayout {
                    spacing: 12
                    SectionTitle { text: qsTr("QSL services") }
                    Note { text: qsTr("LoTW (through the local TQSL), QRZ Logbook, Club Log and eQSL: upload and download of confirmations arrive with the 1.x releases. The QSL state of every QSO is already stored per service and shown in the log, the QSO detail and the QSL Upload tab.") }
                    Repeater {
                        model: decolog.qslSummary
                        RowLayout {
                            required property var modelData
                            spacing: 10
                            Text { Layout.preferredWidth: 90; text: modelData.label; color: Theme.textPrimary; font.family: Theme.monoFamily; font.pixelSize: 12; font.bold: true }
                            Text {
                                text: qsTr("%1 sent · %2 queued · %3 confirmed").arg(modelData.sent || 0).arg(modelData.queued || 0).arg(modelData.confirmed || 0)
                                color: Theme.textSecondary
                                font.family: Theme.monoFamily
                                font.pixelSize: 12
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }

                // ── Callbook ────────────────────────────────────────────────
                ColumnLayout {
                    spacing: 12
                    SectionTitle { text: qsTr("Callbook") }
                    Note { text: qsTr("QRZ.com and HamQTH lookups arrive with the 1.x releases. Until then Call info and the New QSO form use what the log already knows about a callsign.") }
                    Item { Layout.fillHeight: true }
                }

                // ── Backup ──────────────────────────────────────────────────
                ColumnLayout {
                    spacing: 12
                    ToggleSwitch {
                        text: qsTr("Nightly backup")
                        checked: decolog.backupEnabled
                        onToggled: decolog.backupEnabled = checked
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 10
                        LabeledField {
                            Layout.columnSpan: 3
                            Layout.fillWidth: true
                            label: qsTr("Folder")
                            RowLayout {
                                Layout.fillWidth: true
                                StyledTextField { id: backupDirField; Layout.fillWidth: true }
                                GlassButton { text: qsTr("Browse…"); onClicked: folderDialog.open() }
                            }
                        }
                        LabeledField {
                            label: qsTr("Time (local)")
                            StyledTextField { id: backupTimeField; Layout.preferredWidth: 100; placeholderText: "02:00" }
                        }
                        LabeledField {
                            label: qsTr("Keep copies")
                            StyledTextField { id: keepField; Layout.preferredWidth: 100; validator: IntValidator { bottom: 1; top: 365 } }
                        }
                    }
                    RowLayout {
                        spacing: 8
                        Tile { label: qsTr("Last backup"); value: decolog.lastBackup.length ? decolog.lastBackup : qsTr("never") }
                        Tile { label: qsTr("File"); value: decolog.lastBackupInfo.length ? decolog.lastBackupInfo : "—" }
                    }
                    GlassButton {
                        text: qsTr("Back up now")
                        tone: Theme.primaryColor
                        filled: true
                        onClicked: { root.apply(); decolog.backupNow() }
                    }
                    Note { text: qsTr("A consistent copy made with SQLite VACUUM INTO, even while DecoLog is logging. If the PC is off at the chosen time, the copy is made as soon as DecoLog is open.") }
                    Item { Layout.fillHeight: true }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSoft }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 10
            Text {
                text: decolog.lastBackup.length
                      ? qsTr("Backup nightly %1 → %2 · last %3").arg(decolog.backupTime).arg(decolog.backupDir).arg(decolog.lastBackup)
                      : qsTr("Backup nightly %1 → %2").arg(decolog.backupTime).arg(decolog.backupDir)
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                color: Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: 11
            }
            GlassButton { text: qsTr("Cancel"); onClicked: root.reject() }
            GlassButton {
                text: qsTr("Apply")
                tone: Theme.accentColor
                filled: true
                onClicked: { root.apply(); root.accept() }
            }
        }
    }
}
