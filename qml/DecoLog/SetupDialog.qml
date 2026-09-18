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
    // Larga quanto serve alla pagina piu' fitta: in italiano le etichette sono
    // piu' lunghe, e una pagina stretta finiva sopra la colonna di sinistra.
    width: Math.min(960, parent ? parent.width - 40 : 960)

    readonly property var pages: [qsTr("General"), qsTr("Theme & density"), qsTr("Decodium link"),
                                  qsTr("Sync & Cloud"), qsTr("QSL services"), qsTr("Callbook"),
                                  qsTr("Rotor"), qsTr("Backup")]

    onOpened: {
        portField.text = decolog.udpPort
        groupField.text = decolog.multicastGroup
        serverField.text = decolog.cloud.server
        backupDirField.text = decolog.backupDir
        backupTimeField.text = decolog.backupTime
        keepField.text = decolog.backupKeep
    }

    function apply() {
        const port = parseInt(portField.text)
        if (!isNaN(port))
            decolog.udpPort = port
        decolog.multicastGroup = groupField.text.trim()
        decolog.cloud.server = serverField.text.trim()
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

    FileDialog {
        id: tqslDialog
        title: qsTr("TQSL program")
        nameFilters: [qsTr("Programs (*.exe)"), qsTr("All files (*)")]
        onAccepted: decolog.qsl.tqslPath = decolog.localPath(selectedFile)
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
    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── Navigazione ─────────────────────────────────────────────────
            ColumnLayout {
                Layout.preferredWidth: 190
                Layout.minimumWidth: 190
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
                Layout.minimumWidth: 0
                clip: true
                currentIndex: root.page

                // ── General ─────────────────────────────────────────────────
                ColumnLayout {
                    spacing: 12
                    SectionTitle { text: qsTr("This copy of DecoLog") }
                    Note {
                        text: qsTr("DecoLog %1 · Qt %2 · %3").arg(decolog.version).arg(decolog.qtVersion).arg(decolog.buildInfo)
                    }

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
                    SectionTitle { text: qsTr("Language") }
                    RowLayout {
                        spacing: 12
                        LabeledField {
                            label: qsTr("Interface")
                            StyledComboBox {
                                Layout.preferredWidth: 220
                                readonly property var codes: ["auto", "it", "en"]
                                model: [qsTr("Like the system"), "Italiano", "English"]
                                currentIndex: Math.max(0, codes.indexOf(decolog.uiLanguage))
                                onActivated: decolog.uiLanguage = codes[currentIndex]
                            }
                        }
                    }
                    Note { text: qsTr("The new language shows up the next time DecoLog starts.") }

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
                            // Il nome del messaggio non si abbrevia bene: e' quello che
                            // l'operatore cerca nelle impostazioni di Decodium.
                            Layout.horizontalStretchFactor: 3
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
                            Layout.horizontalStretchFactor: 2
                            label: qsTr("Multicast group")
                            StyledTextField { id: groupField; Layout.fillWidth: true; placeholderText: qsTr("empty = unicast") }
                        }
                    }
                    Note { text: qsTr("In Decodium set the UDP server to this address and port. A multicast group (e.g. 239.255.0.1) shares the stream with GridTracker or JTAlert. Duplicate windows are in General.") }

                    SectionTitle { text: qsTr("DecoLink · log towards Decodium") }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        ToggleSwitch {
                            text: qsTr("Share the log with Decodium")
                            checked: decolog.decoLinkEnabled
                            onToggled: decolog.decoLinkEnabled = checked
                        }
                        LabeledField {
                            label: qsTr("Port (127.0.0.1)")
                            StyledTextField {
                                Layout.preferredWidth: 100
                                text: decolog.decoLinkPort
                                validator: IntValidator { bottom: 1; top: 65535 }
                                onEditingFinished: decolog.decoLinkPort = parseInt(text)
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Pill {
                            text: !decolog.decoLinkEnabled ? qsTr("off")
                                : !decolog.decoLinkListening ? qsTr("error")
                                : decolog.decoLinkClients.length ? qsTr("%n client(s)", "", decolog.decoLinkClients.length)
                                : qsTr("waiting")
                            tone: !decolog.decoLinkEnabled ? Theme.textSecondary
                                : !decolog.decoLinkListening ? Theme.errorColor
                                : decolog.decoLinkClients.length ? Theme.accentColor : Theme.textSecondary
                        }
                    }
                    Repeater {
                        model: decolog.decoLinkClients
                        Text {
                            required property var modelData
                            text: "● " + [modelData.app, modelData.version, modelData.station].filter(s => s).join(" · ")
                            color: Theme.accentColor
                            font.family: Theme.monoFamily
                            font.pixelSize: 12
                        }
                    }
                    Note {
                        text: decolog.decoLinkError.length && decolog.decoLinkEnabled
                              ? decolog.decoLinkError
                              : qsTr("Decodium receives the worked calls, confirmations and FT2 Award status from this log, and a confirmation for every QSO written. Only local connections are accepted. Protocol: docs/DECOLINK.md.")
                        color: decolog.decoLinkError.length && decolog.decoLinkEnabled ? Theme.errorColor : Theme.textSecondary
                    }
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
                            Led {
                                size: 10
                                color: decolog.cloud.linked ? Theme.accentColor
                                     : decolog.cloud.server.length ? Theme.warningColor : Theme.textSecondary
                            }
                            Column {
                                Layout.fillWidth: true
                                Text {
                                    text: decolog.cloud.linked
                                          ? qsTr("DecoLog Cloud · %1").arg(decolog.cloud.callsign)
                                          : qsTr("DecoLog Cloud · not linked")
                                    color: Theme.textPrimary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                Text {
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    text: decolog.cloud.status.length ? decolog.cloud.status
                                        : decolog.cloud.linked
                                          ? qsTr("%1 QSO on the server · queue %2")
                                                .arg(decolog.cloud.remote.qsos !== undefined ? decolog.cloud.remote.qsos : "—")
                                                .arg(decolog.cloud.queued)
                                          : qsTr("The log stays yours and works offline: the Cloud is where your devices pass each other the changes.")
                                    color: Theme.textSecondary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 11
                                }
                            }
                            GlassButton {
                                text: decolog.cloud.busy ? qsTr("syncing…") : qsTr("Sync now")
                                tone: Theme.primaryColor
                                filled: true
                                enabled: decolog.cloud.linked && !decolog.cloud.busy
                                onClicked: decolog.cloud.syncNow()
                            }
                            GlassButton {
                                text: qsTr("Unlink")
                                visible: decolog.cloud.linked
                                onClicked: decolog.cloud.logout()
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Tile { label: qsTr("Last sync"); value: decolog.cloud.lastSync || qsTr("never") }
                        Tile {
                            label: qsTr("On the server")
                            value: decolog.cloud.remote.qsos !== undefined ? decolog.cloud.remote.qsos : "—"
                        }
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
                            StyledTextField {
                                id: serverField
                                Layout.fillWidth: true
                                mono: false
                                placeholderText: "http://127.0.0.1:8787"
                            }
                        }
                        LabeledField {
                            Layout.fillWidth: true
                            label: qsTr("Auto sync")
                            StyledComboBox {
                                Layout.fillWidth: true
                                readonly property var values: ["qso", "timer", "manual"]
                                model: [qsTr("After every QSO + every 5 min"), qsTr("Every 5 min"), qsTr("Manual only")]
                                currentIndex: Math.max(0, values.indexOf(decolog.cloud.autoMode))
                                onActivated: decolog.cloud.autoMode = values[currentIndex]
                            }
                        }
                    }

                    // ── Accesso ─────────────────────────────────────────────
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        visible: !decolog.cloud.linked
                        SectionTitle { text: qsTr("Sign in") }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            LabeledField {
                                label: qsTr("Callsign")
                                StyledTextField {
                                    id: cloudCall
                                    Layout.preferredWidth: 150
                                    uppercase: true
                                    text: decolog.cloud.callsign
                                }
                            }
                            LabeledField {
                                label: qsTr("Password")
                                StyledTextField {
                                    id: cloudPassword
                                    Layout.preferredWidth: 220
                                    mono: false
                                    echoMode: TextInput.Password
                                    // Invio fa quello che fa il tasto: stesso
                                    // indirizzo, stessa pulizia dopo.
                                    Keys.onReturnPressed: {
                                        decolog.cloud.server = serverField.text
                                        decolog.cloud.login(cloudCall.text, cloudPassword.text)
                                        cloudPassword.text = ""
                                    }
                                }
                            }
                            GlassButton {
                                Layout.alignment: Qt.AlignBottom
                                Layout.bottomMargin: 2
                                text: qsTr("Sign in")
                                tone: Theme.primaryColor
                                filled: true
                                enabled: !decolog.cloud.busy && cloudCall.text.trim().length >= 3
                                         && cloudPassword.text.length >= 8
                                onClicked: {
                                    decolog.cloud.server = serverField.text
                                    decolog.cloud.login(cloudCall.text, cloudPassword.text)
                                    cloudPassword.text = ""
                                }
                            }
                            GlassButton {
                                Layout.alignment: Qt.AlignBottom
                                Layout.bottomMargin: 2
                                text: qsTr("Create account")
                                enabled: !decolog.cloud.busy && cloudCall.text.trim().length >= 3
                                         && cloudPassword.text.length >= 8
                                onClicked: {
                                    decolog.cloud.server = serverField.text
                                    decolog.cloud.signup(cloudCall.text, cloudPassword.text)
                                    cloudPassword.text = ""
                                }
                            }
                        }
                        Note {
                            text: qsTr("The password travels once and is not kept: DecoLog stores only the token the "
                                       + "server gives back, in the system keystore. On the network use HTTPS; at home, "
                                       + "on your own LAN, plain HTTP is fine.")
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
                        ToggleSwitch {
                            Layout.topMargin: 4
                            Layout.bottomMargin: 2
                            enabled: decolog.cloud.vaultAvailable
                            text: qsTr("Carry the service passwords to the other devices too")
                            checked: decolog.cloud.syncSecrets && decolog.cloud.vaultAvailable
                            onToggled: decolog.cloud.syncSecrets = checked
                        }
                        // Chi si e' collegato prima che la cassaforte esistesse
                        // ha la chiave mancante: si fa qui, senza scollegarsi.
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: decolog.cloud.linked && decolog.cloud.vaultAvailable
                                     && decolog.cloud.syncSecrets && !decolog.cloud.vaultReady
                            LabeledField {
                                label: qsTr("Cloud password, to open the vault on this device")
                                StyledTextField {
                                    id: vaultPassword
                                    Layout.preferredWidth: 220
                                    mono: false
                                    echoMode: TextInput.Password
                                    Keys.onReturnPressed: {
                                        decolog.cloud.unlockVault(vaultPassword.text)
                                        vaultPassword.text = ""
                                    }
                                }
                            }
                            GlassButton {
                                Layout.alignment: Qt.AlignBottom
                                Layout.bottomMargin: 2
                                text: qsTr("Open the vault")
                                tone: Theme.primaryColor
                                enabled: vaultPassword.text.length >= 8
                                onClicked: {
                                    decolog.cloud.unlockVault(vaultPassword.text)
                                    vaultPassword.text = ""
                                }
                            }
                        }
                        Note {
                            text: decolog.cloud.vaultAvailable
                                  ? qsTr("They travel sealed: DecoLog closes them on this computer with AES-256-GCM "
                                         + "and a key made from your Cloud password, which the server only knows as "
                                         + "an Argon2 fingerprint. What reaches the server is a block of bytes that "
                                         + "does not open without that password. Sign in on the other device with "
                                         + "the same password and the services are ready there too.")
                                  : qsTr("This build has no OpenSSL: the service passwords cannot be sealed, so they "
                                         + "stay on this computer.")
                        }
                        CredentialsList {
                            Layout.fillWidth: true
                            serviceIds: ["cloud", "qrz", "qrzlogbook", "lotw", "clublog", "eqsl", "hamqth"]
                        }
                    }
                    Item { Layout.fillHeight: true }
                }

                // ── QSL services ────────────────────────────────────────────
                ColumnLayout {
                    spacing: 12
                    SectionTitle { text: qsTr("LoTW confirmations") }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Tile { label: qsTr("Last sync"); value: decolog.lotwLastSync || qsTr("never") }
                        Tile { label: qsTr("Confirmations since"); value: decolog.lotwCursor || qsTr("all") }
                        Tile {
                            label: qsTr("Confirmed in log")
                            value: (decolog.qslSummary.find(r => r.service === "lotw") || {}).confirmed || 0
                        }
                    }
                    RowLayout {
                        spacing: 8
                        GlassButton {
                            text: decolog.lotwBusy ? qsTr("Downloading…") : qsTr("Sync now")
                            tone: Theme.accentColor
                            filled: true
                            enabled: !decolog.lotwBusy
                            onClicked: decolog.syncLotw(false)
                        }
                        GlassButton {
                            text: qsTr("Download everything again")
                            enabled: !decolog.lotwBusy
                            onClicked: decolog.syncLotw(true)
                        }
                        GlassButton {
                            visible: decolog.lotwBusy
                            text: qsTr("Cancel")
                            onClicked: decolog.cancelLotw()
                        }
                    }
                    RowLayout {
                        spacing: 8
                        Text { text: qsTr("Automatic sync"); color: Theme.textSecondary; font.pixelSize: 12 }
                        StyledComboBox {
                            Layout.preferredWidth: 150
                            readonly property var hours: [0, 6, 12, 24]
                            model: [qsTr("Off"), qsTr("Every 6 hours"), qsTr("Every 12 hours"), qsTr("Once a day")]
                            currentIndex: Math.max(0, hours.indexOf(decolog.lotwAutoHours))
                            onActivated: decolog.lotwAutoHours = hours[currentIndex]
                        }
                    }
                    Note {
                        visible: decolog.lotwStatus.length > 0
                        text: decolog.lotwStatus
                        color: decolog.lotwStatus.indexOf("LoTW: ") === 0 && /incorrect|error|not available|cancel|HTTP|unexpected|web page/i.test(decolog.lotwStatus)
                               ? Theme.errorColor : Theme.textPrimary
                    }
                    Note {
                        text: qsTr("Confirmations are matched by call, band, mode group (data, CW, phone) and time within 30 minutes, as LoTW does. "
                                   + "A confirmed QSO becomes a new revision; grid, zones, state and county from LoTW fill only empty fields. "
                                   + "Uploading to LoTW still goes through TQSL. QRZ Logbook, Club Log and eQSL arrive later.")
                    }
                    SectionTitle { text: qsTr("Sending to LoTW (TQSL)") }
                    Note { text: decolog.qsl.tqslStatus }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        LabeledField {
                            Layout.fillWidth: true
                            label: qsTr("TQSL program")
                            StyledTextField {
                                id: tqslField
                                Layout.fillWidth: true
                                mono: false
                                text: decolog.qsl.tqslPath
                                placeholderText: "C:/Program Files (x86)/TrustedQSL/tqsl.exe"
                                onEditingFinished: decolog.qsl.tqslPath = text
                            }
                        }
                        GlassButton { text: qsTr("Browse…"); Layout.alignment: Qt.AlignBottom; onClicked: tqslDialog.open() }
                        LabeledField {
                            label: qsTr("Station location")
                            StyledComboBox {
                                Layout.preferredWidth: 200
                                readonly property var names: [""].concat(decolog.qsl.tqslLocations)
                                model: [qsTr("From the station profile")].concat(decolog.qsl.tqslLocations)
                                currentIndex: Math.max(0, names.indexOf(decolog.qsl.tqslLocation))
                                onActivated: decolog.qsl.tqslLocation = names[currentIndex]
                            }
                        }
                    }
                    Note {
                        text: qsTr("The certificate stays in TQSL: DecoLog writes a temporary ADIF, TQSL signs it and sends it. "
                                   + "Duplicates are not an error, LoTW simply keeps the one it already has. Sending, automatic sending "
                                   + "and the counters are in the QSL tab at the bottom.")
                    }
                    SectionTitle { text: "Club Log" }
                    RowLayout {
                        spacing: 12
                        LabeledField {
                            label: qsTr("API key")
                            StyledTextField {
                                Layout.preferredWidth: 280
                                mono: false
                                text: decolog.qsl.clubLogApiKey
                                placeholderText: qsTr("the key Club Log gave you")
                                onEditingFinished: decolog.qsl.clubLogApiKey = text
                            }
                        }
                    }
                    Note {
                        text: qsTr("Club Log wants three things: the email and password of the account (below), the callsign of the "
                                   + "station profile, and an API key. The key is free and personal, and is asked for at "
                                   + "clublog.org/need_api.php — it identifies the program, not you. A single QSO leaves as soon as "
                                   + "it is logged, a backlog leaves as one ADIF file.")
                    }
                    CredentialsList {
                        Layout.fillWidth: true
                        serviceIds: ["lotw", "qrzlogbook", "clublog", "eqsl"]
                    }
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
                    RowLayout {
                        spacing: 12
                        LabeledField {
                            label: qsTr("Lookup service")
                            StyledComboBox {
                                Layout.preferredWidth: 260
                                readonly property var ids: ["off", "qrz", "hamqth"]
                                model: [qsTr("Off (log and cty.csv only)"), "QRZ.com (XML)", "HamQTH"]
                                currentIndex: Math.max(0, ids.indexOf(decolog.callbookProvider))
                                onActivated: decolog.callbookProvider = ids[currentIndex]
                            }
                        }
                        ToggleSwitch {
                            Layout.alignment: Qt.AlignBottom
                            Layout.bottomMargin: 4
                            text: qsTr("Fill empty name, QTH and grid in New QSO")
                            checked: decolog.callbookAutofill
                            onToggled: decolog.callbookAutofill = checked
                        }
                    }
                    CredentialsList {
                        Layout.fillWidth: true
                        serviceIds: ["qrz", "hamqth"]
                    }
                    SectionTitle { text: qsTr("Try a lookup") }
                    RowLayout {
                        spacing: 8
                        StyledTextField {
                            id: callbookTest
                            Layout.preferredWidth: 160
                            uppercase: true
                            placeholderText: "IU8LMC"
                            Keys.onReturnPressed: testButton.clicked()
                        }
                        GlassButton {
                            id: testButton
                            text: qsTr("Look up")
                            tone: Theme.primaryColor
                            filled: true
                            enabled: decolog.callbookProvider !== "off" && callbookTest.text.trim().length >= 3
                            onClicked: decolog.lookupCall = callbookTest.text
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        readonly property var info: decolog.callInfo
                        readonly property bool mine: callbookTest.text.trim().toUpperCase() === (info.call || "")
                        text: decolog.callbookBusy ? qsTr("Looking up…")
                            : mine && info.callbook ? [info.callbook.call, info.callbook.name, info.callbook.qth,
                                                        info.callbook.grid, info.callbook.country].filter(s => s).join(" · ")
                            : mine && info.callbookError ? info.callbookError
                            : decolog.callbookStatus
                        color: mine && info.callbookError ? Theme.warningColor : Theme.textPrimary
                        font.family: Theme.monoFamily
                        font.pixelSize: 12
                    }
                    Note { text: qsTr("QRZ.com needs an XML data subscription; HamQTH is free. Results are kept in memory for a day, so moving through the log does not use up lookups.") }
                    Item { Layout.fillHeight: true }
                }

                // ── Rotore ──────────────────────────────────────────────────
                ColumnLayout {
                    spacing: 12
                    ToggleSwitch {
                        text: qsTr("Antenna rotor")
                        checked: decolog.rotor.enabled
                        onToggled: decolog.rotor.enabled = checked
                    }
                    RowLayout {
                        spacing: 12
                        LabeledField {
                            label: qsTr("Talks to")
                            StyledComboBox {
                                Layout.preferredWidth: 300
                                readonly property var ids: ["decorotor", "rotctld"]
                                model: [qsTr("DecoRotor (WebSocket)"), qsTr("rotctld (Hamlib) — any program")]
                                currentIndex: Math.max(0, ids.indexOf(decolog.rotor.backend))
                                onActivated: decolog.rotor.backend = ids[currentIndex]
                            }
                        }
                        LabeledField {
                            label: qsTr("Host")
                            StyledTextField {
                                Layout.preferredWidth: 180
                                text: decolog.rotor.host
                                placeholderText: "127.0.0.1"
                                onEditingFinished: decolog.rotor.host = text
                            }
                        }
                        LabeledField {
                            label: qsTr("Port")
                            StyledTextField {
                                Layout.preferredWidth: 100
                                text: decolog.rotor.port
                                onEditingFinished: decolog.rotor.port = parseInt(text) || 0
                            }
                        }
                        LabeledField {
                            label: qsTr("Beamwidth")
                            StyledComboBox {
                                Layout.preferredWidth: 120
                                readonly property var values: [20, 30, 45, 60, 90, 120]
                                model: values.map(v => v + "°")
                                currentIndex: Math.max(0, values.indexOf(decolog.rotor.beamwidth))
                                onActivated: decolog.rotor.beamwidth = values[currentIndex]
                            }
                        }
                    }
                    ToggleSwitch {
                        text: qsTr("Follow the call Decodium is working")
                        checked: decolog.rotor.followDx
                        onToggled: decolog.rotor.followDx = checked
                    }
                    RowLayout {
                        spacing: 8
                        GlassButton {
                            text: qsTr("Connect again")
                            enabled: decolog.rotor.enabled
                            onClicked: decolog.rotor.reconnect()
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: decolog.rotor.status
                            color: decolog.rotor.connected ? Theme.accentColor : Theme.warningColor
                            font.family: Theme.monoFamily
                            font.pixelSize: 12
                        }
                    }
                    Note {
                        text: qsTr("DecoRotor is the gateway of the family: it reads the Prosistel control box on the "
                                   + "serial port and publishes it on the network (WebSocket 8765). With rotctld any "
                                   + "other rotor program works too — DecoRotor itself answers on 4532. DecoLog never "
                                   + "touches the serial port: it only says where to point, and the control box keeps "
                                   + "its own limits.")
                    }
                    Note {
                        text: qsTr("Where a bearing is known — a cluster spot, the call being worked, a QSO with a "
                                   + "grid — the rotor menu points there. The panel is in the right column, with the "
                                   + "compass and the STOP.")
                    }
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
