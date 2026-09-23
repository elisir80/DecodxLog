// DecoDXLog — barra superiore: marchio, frequenza di Decodium, comandi, profilo
// stazione, stato del cloud, ricerca.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Rectangle {
    id: root

    signal setupRequested()
    signal logsRequested()
    signal importRequested()
    signal exportRequested()
    signal awardsRequested()
    signal clusterRequested()
    signal activationRequested()
    signal profilesRequested()
    signal panelsRequested()
    signal aboutRequested()
    // Per le prove: apre il menu del marchio senza mouse.
    function openMainMenu() { mainMenu.popup(brandBlock, 0, brandBlock.height + 4) }
    // Idem per l'elenco dei modi, che di solito si apre cliccando la pillola.
    function openModeMenu() { modeMenu.popup(modePill, 0, modePill.height + 6) }
    signal logFolderRequested()
    signal quitRequested()
    // Quanti pannelli sono chiusi adesso: lo dice il pulsante, cosi' un pannello
    // sparito non e' un pannello perso.
    property int closedPanels: 0

    // Contest Mode: tutto quello che serve in gara sta in un menu solo, qui in
    // alto: la sessione, entrare e uscire, le finestre, le disposizioni, il
    // Cabrillo e l'invio del log. Il comando va alla finestra principale, che
    // e' l'unica che sa dove sono le finestre.
    signal contestCommand(string what, string arg)
    property bool contestModeOn: false
    property var contestOpenPanels: []
    function openContestMenu() { contestMenu.popup(contestButton, 0, contestButton.height + 6) }

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    readonly property var profiles: decolog.stationProfiles

    // Sugli schermi piccoli la barra va a capo: i riquadri scendono sulla riga
    // sotto invece di uscire dalla finestra, e la barra si alza con loro.
    implicitHeight: bar.implicitHeight + 16
    color: Theme.bgMedium

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.borderSoft
    }

    component Block: Rectangle {
        id: blockRoot
        default property alias content: blockRow.data
        property color outline: Theme.glassBorder
        property int hPadding: 12
        property alias spacing: blockRow.spacing
        // Un riquadro che si puo' premere tutto intero: serve al marchio, che
        // apre il menu. Il MouseArea sta qui e non fra il contenuto, perche'
        // li' dentro c'e' un layout e gli anchors non ci vanno.
        property bool clickable: false
        signal clicked()
        implicitHeight: 48
        implicitWidth: blockRow.implicitWidth + hPadding * 2
        radius: 6
        color: Theme.panelColor
        border.width: 1
        border.color: outline
        MouseArea {
            anchors.fill: parent
            enabled: blockRoot.clickable
            hoverEnabled: blockRoot.clickable
            cursorShape: Qt.PointingHandCursor
            onClicked: blockRoot.clicked()
        }
        RowLayout {
            id: blockRow
            anchors.fill: parent
            anchors.leftMargin: parent.hPadding
            anchors.rightMargin: parent.hPadding
            spacing: 8
        }
    }

    // Un riquadro a larghezza fissa dentro la barra che va a capo: il Flow
    // guarda la larghezza vera, non quella che il riquadro vorrebbe.
    component FixedBlock: Block {
        width: implicitWidth
        height: implicitHeight
    }

    Flow {
        id: bar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 8
        spacing: 10

        // Quanto occupa tutto in fila: se ci sta su una riga, il cloud e la
        // ricerca restano spinti a destra come prima; se non ci sta, si va a
        // capo e il vuoto sparisce.
        readonly property real rowWidth: {
            let w = 0
            let n = 0
            for (let i = 0; i < children.length; ++i) {
                const c = children[i]
                if (c === spacer || !c.visible)
                    continue
                w += c.width
                ++n
            }
            return w + spacing * n
        }

        // Marchio e versione.
        FixedBlock {
            id: brandBlock
            hPadding: 14

            // Tutto il riquadro del marchio apre il menu: le tre righette da
            // sole sono un bersaglio piccolo, e la gente ci clicca sopra il nome.
            clickable: true
            onClicked: mainMenu.opened ? mainMenu.close()
                                       : mainMenu.popup(brandBlock, 0, brandBlock.height + 4)

            StyledMenu {
                id: mainMenu
                StyledMenuItem { text: qsTr("About DecoDXLog…"); onTriggered: root.aboutRequested() }
                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
                StyledMenuItem { text: qsTr("Settings…"); onTriggered: root.setupRequested() }
                StyledMenuItem { text: qsTr("Station profiles…"); onTriggered: root.profilesRequested() }
                StyledMenuItem { text: qsTr("Panels…"); onTriggered: root.panelsRequested() }
                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
                StyledMenuItem { text: qsTr("Import ADIF…"); onTriggered: root.importRequested() }
                StyledMenuItem { text: qsTr("Export ADIF…"); onTriggered: root.exportRequested() }
                StyledMenuItem { text: qsTr("Open the log folder"); onTriggered: root.logFolderRequested() }
                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
                StyledMenuItem { text: qsTr("Quit"); onTriggered: root.quitRequested() }
            }

            Text {
                text: "≡"
                color: Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: 22
            }
            Column {
                // Il marchio, con il DX acceso come sull'icona.
                Text {
                    textFormat: Text.StyledText
                    text: "DECO<font color=\"" + Theme.secondaryColor + "\">DX</font>LOG"
                    color: Theme.primaryColor
                    font.pixelSize: 15
                    font.weight: Font.ExtraBold
                    font.letterSpacing: 0.6
                }
                Text {
                    text: "v" + decolog.version
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
            }
        }

        // Frequenza e modo di Decodium: DecoDXLog non tocca il CAT, li legge dallo Status.
        FixedBlock {
            outline: decolog.clientConnected ? Theme.accentColor : Theme.glassBorder
            hPadding: 14
            Led {
                color: !decolog.listening ? Theme.errorColor
                     : decolog.transmitting ? Theme.warningColor
                     : decolog.clientConnected ? Theme.accentColor : Theme.textSecondary
                size: 10
                glow: decolog.clientConnected
                blinking: decolog.transmitting
            }
            VfoDisplay {
                id: vfo
                // La radio quando c'e', perche' e' lo stato vero; Decodium
                // quando la radio non c'e'. Chi clicca uno spot deve vedere il
                // display muoversi con il VFO.
                mhz: decolog.shownFrequency.length ? parseFloat(decolog.shownFrequency) : 0
                tunable: decolog.rig.connected || decolog.clientConnected
                textColor: decolog.clientConnected || decolog.rig.connected
                           ? Theme.accentColor : Theme.textSecondary
                onTuned: (freq) => decolog.tuneTo(freq, "")
            }
            Text {
                text: "MHz"
                color: Theme.textSecondary
                font.pixelSize: 12
            }
            // Il modo: si clicca e si sceglie, CW e fonia in cima, i digitali
            // sotto. Quello che si sceglie va alla radio e a Decodium.
            Pill {
                id: modePill
                readonly property string shown: decolog.shownMode
                visible: shown.length > 0 || vfo.tunable
                text: shown.length ? shown : "···"
                tone: Theme.primaryColor
                rounded: false
                pillHeight: 26
                fontPixelSize: 12
                interactive: vfo.tunable
                onClicked: modeMenu.opened ? modeMenu.close()
                                           : modeMenu.popup(modePill, 0, modePill.height + 6)

                StyledMenu {
                    id: modeMenu
                    // L'elenco non cambia mentre il programma e' aperto: si
                    // chiede una volta sola, se no l'Instantiator rifa' tutte
                    // le voci ogni volta che qualcuno lo guarda.
                    readonly property var entries: decolog.operatingModes()

                    Instantiator {
                        model: modeMenu.entries
                        delegate: StyledMenuItem {
                            required property var modelData
                            text: modelData.name
                            checkable: true
                            checked: modePill.shown === modelData.name
                            onTriggered: decolog.tuneTo(vfo.mhz, modelData.name)
                        }
                        onObjectAdded: (index, object) => modeMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => modeMenu.removeItem(object)
                    }
                }
            }
        }

        Rectangle {
            id: commands
            // In fila se c'e' posto, altrimenti i pulsanti vanno a capo dentro
            // il riquadro, che non esce mai dalla finestra.
            readonly property real rowWidth: {
                let w = 0
                for (let i = 0; i < commandFlow.children.length; ++i)
                    w += commandFlow.children[i].implicitWidth
                return w + commandFlow.spacing * Math.max(0, commandFlow.children.length - 1)
            }
            width: Math.min(rowWidth, bar.width - 20) + 20
            height: Math.max(48, commandFlow.implicitHeight + 18)
            radius: 6
            color: Theme.panelColor
            border.width: 1
            border.color: Theme.glassBorder

            Flow {
                id: commandFlow
                anchors.fill: parent
                anchors.margins: 10
                anchors.topMargin: 9
                spacing: 6
            GlassButton { text: qsTr("Setup"); tone: Theme.primaryColor; filled: true; onClicked: root.setupRequested() }
            // I log della stazione: quello di sempre e quelli dei contest.
            GlassButton { text: qsTr("Logs"); onClicked: root.logsRequested() }
            GlassButton { text: qsTr("Import"); onClicked: root.importRequested() }
            GlassButton { text: qsTr("Export"); onClicked: root.exportRequested() }
            GlassButton { text: qsTr("Awards"); onClicked: root.awardsRequested() }
            GlassButton {
                id: contestButton
                readonly property var act: decolog.activation
                // La sessione in corto: il nome della gara senza "Contest"
                // davanti, e i QSO — su quanti ne servono, per le attivazioni.
                readonly property string shortTitle: String(act.state.title || "").replace(/^Contest /, "")
                text: !act.active ? qsTr("Contest Mode ▾")
                      : act.requiredQsos > 0
                        ? qsTr("Contest Mode · %1 · %2/%3 ▾").arg(shortTitle).arg(act.qsoCount).arg(act.requiredQsos)
                        : qsTr("Contest Mode · %1 · %2 QSO ▾").arg(shortTitle).arg(act.qsoCount)
                tone: root.contestModeOn ? Theme.accentColor
                      : !act.active ? "transparent"
                      : act.requiredQsos > 0 && act.qsoCount < act.requiredQsos ? Theme.warningColor : Theme.primaryColor
                filled: root.contestModeOn
                onClicked: contestMenu.opened ? contestMenu.close() : root.openContestMenu()

                StyledMenu {
                    id: contestMenu
                    readonly property var act: decolog.activation
                    property var scoring: ({})
                    // Il conto si chiede quando il menu si apre, non a ogni QSO.
                    onAboutToShow: {
                        contestMenu.scoring = act.active ? act.score() : ({})
                        scoreLine.text = act.active
                            ? (contestMenu.scoring.valid
                               ? qsTr("%1 · %2 QSO · %3 points · %4 mult").arg(act.state.title || "")
                                     .arg(act.qsoCount).arg(contestMenu.scoring.points || 0)
                                     .arg(contestMenu.scoring.multipliers || 0)
                               : qsTr("%1 · %2 QSO").arg(act.state.title || "").arg(act.qsoCount))
                            : qsTr("No session open")
                    }

                    function isOpen(key) { return root.contestOpenPanels.indexOf(key) >= 0 }

                    // Com'e' andata finora, in una riga: non si clicca.
                    StyledMenuItem { id: scoreLine; enabled: false }
                    StyledMenuItem {
                        text: qsTr("Contest and activations…")
                        onTriggered: root.contestCommand("session", "")
                    }
                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
                    StyledMenuItem {
                        text: root.contestModeOn ? qsTr("Leave contest mode") : qsTr("Enter contest mode")
                        // Si entra con una sessione aperta: senza, le finestre
                        // della gara non avrebbero niente da mostrare.
                        enabled: root.contestModeOn || contestMenu.act.active
                        onTriggered: root.contestCommand(root.contestModeOn ? "exit" : "enter", "")
                    }
                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
                    // Le finestre della gara: spuntata vuol dire aperta.
                    Instantiator {
                        model: [
                            { key: "contest", label: qsTr("QSO entry") },
                            { key: "cluster", label: qsTr("Cluster") },
                            { key: "logbook", label: qsTr("Logbook") },
                            { key: "callinfo", label: qsTr("Callsign card") },
                            { key: "rate", label: qsTr("Rate") },
                            { key: "score", label: qsTr("Score") },
                            { key: "map", label: qsTr("Map") },
                            { key: "cw", label: qsTr("CW") }
                        ]
                        delegate: StyledMenuItem {
                            required property var modelData
                            text: modelData.label
                            checkable: true
                            checked: contestMenu.isOpen(modelData.key)
                            enabled: root.contestModeOn
                            onTriggered: root.contestCommand("toggle", modelData.key)
                        }
                        onObjectAdded: (index, object) => contestMenu.insertItem(5 + index, object)
                        onObjectRemoved: (index, object) => contestMenu.removeItem(object)
                    }
                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
                    StyledMenuItem {
                        // I pannelli e le misure di partenza, per chi ha
                        // trascinato troppo.
                        text: qsTr("Reset the layout")
                        enabled: root.contestModeOn
                        onTriggered: root.contestCommand("reset", "")
                    }
                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
                    StyledMenuItem {
                        text: qsTr("Cabrillo…")
                        onTriggered: root.contestCommand("export", "")
                    }
                    StyledMenuItem {
                        // Ogni contest ha il suo posto dove si manda il log: il
                        // programma ci porta, con il Cabrillo gia' scritto.
                        text: qsTr("Send the log…")
                        enabled: contestMenu.scoring.valid === true
                        onTriggered: root.contestCommand("submit", "")
                    }
                }
            }
            GlassButton {
                text: decolog.cluster.onlineCount > 0 ? qsTr("Cluster ●") : qsTr("Cluster")
                tone: decolog.cluster.onlineCount > 0 ? Theme.accentColor : "transparent"
                onClicked: root.clusterRequested()
            }
            GlassButton {
                text: root.closedPanels > 0 ? qsTr("Panels (%1 closed)").arg(root.closedPanels) : qsTr("Panels")
                tone: root.closedPanels > 0 ? Theme.warningColor : "transparent"
                onClicked: root.panelsRequested()
            }
            }
        }

        // Il profilo con cui si scrivono i QSO nuovi.
        FixedBlock {
            Text {
                text: qsTr("Station")
                color: Theme.textSecondary
                font.pixelSize: 11
            }
            StyledComboBox {
                id: stationBox
                Layout.preferredWidth: 160
                model: root.profiles
                textRole: "name"
                valueRole: "profileId"
                displayText: root.profiles.activeProfile.name ? root.profiles.activeProfile.name : qsTr("No profile")
                currentIndex: root.profiles.rowForId(root.profiles.activeProfileId)
                onActivated: (index) => {
                    const p = root.profiles.get(index)
                    if (p.deleted)
                        root.profilesRequested()
                    else
                        root.profiles.activeProfileId = p.id
                }
                Connections {
                    target: root.profiles
                    function onActiveChanged() { stationBox.currentIndex = root.profiles.rowForId(root.profiles.activeProfileId) }
                }
            }
            GlassButton {
                text: "✎"
                minimumWidth: 30
                implicitWidth: 30
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Station profiles")
                onClicked: root.profilesRequested()
            }
        }

        Item {
            id: spacer
            width: Math.max(0, bar.width - bar.rowWidth)
            height: 1
        }

        // DecoDXLog Cloud: come sta il collegamento e cosa aspetta di partire.
        FixedBlock {
            Led {
                color: decolog.cloud.busy ? Theme.primaryColor
                     : decolog.cloud.linked ? Theme.accentColor
                     : decolog.cloud.server.length ? Theme.warningColor : Theme.textSecondary
            }
            Column {
                Text {
                    text: decolog.cloud.linked ? qsTr("Cloud %1").arg(decolog.cloud.callsign)
                        : decolog.cloud.server.length ? qsTr("Cloud not linked")
                        : qsTr("Cloud not configured")
                    color: Theme.textPrimary
                    font.family: Theme.monoFamily
                    font.pixelSize: 12
                    font.bold: true
                }
                Text {
                    text: decolog.cloud.busy ? qsTr("syncing…")
                        : decolog.cloud.queued > 0 ? qsTr("%1 queued").arg(decolog.cloud.queued)
                        : decolog.cloud.lastSync.length ? qsTr("synced %1").arg(decolog.cloud.lastSync)
                        : qsTr("%1 queued").arg(decolog.dirtyCount)
                    color: decolog.cloud.queued > 0 ? Theme.warningColor : Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
            }
            // Solo l'icona: lo stato e' gia' scritto accanto, e la barra
            // cosi' sta su una riga anche su uno schermo normale.
            GlassButton {
                text: "⟳"
                tone: Theme.primaryColor
                filled: true
                minimumWidth: 30
                implicitWidth: 30
                fontPixelSize: 14
                enabled: decolog.cloud.linked && !decolog.cloud.busy
                onClicked: decolog.cloud.syncNow()
                ToolTip.visible: hovered
                ToolTip.text: decolog.cloud.linked ? qsTr("Sync now") : qsTr("Sign in from Setup → Sync & Cloud")
            }
        }

        FixedBlock {
            hPadding: 10
            spacing: 6
            StyledTextField {
                id: searchField
                Layout.preferredWidth: 140
                placeholderText: qsTr("Search…")
                text: decolog.qsoModel.filterText
                onTextEdited: decolog.qsoModel.filterText = text
                Keys.onEscapePressed: { text = ""; decolog.qsoModel.filterText = "" }
            }
            GlassButton {
                text: "⌕"
                tone: Theme.secondaryColor
                minimumWidth: 30
                implicitWidth: 30
                buttonHeight: 30
                fontPixelSize: 14
                onClicked: root.focusSearch()
            }
        }
    }
}
