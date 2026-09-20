// DecoLog — la finestra principale (mockup 1a).
//
// La stessa grammatica di Decodium: barra superiore a blocchi, pannelli su
// SplitView ridimensionabili con le misure che restano da una sessione
// all'altra, barra di stato con le pillole dei servizi.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import Decodium.UI

ApplicationWindow {
    id: window

    width: 1440
    height: 900
    minimumWidth: 1100
    minimumHeight: 640
    visible: true
    title: "DecoLog " + decolog.version
           + (decolog.stationProfiles.activeProfile.stationCallsign
              ? " — " + decolog.stationProfiles.activeProfile.stationCallsign : "")
    color: Theme.bgDeep
    font.pixelSize: Theme.fontSize

    // Le impostazioni arrivate da un altro computer valgono subito: il tema si
    // ridipinge senza aspettare il riavvio.
    Connections {
        target: decolog.cloud
        function onSettingsApplied() { Theme.reload() }
    }

    Settings {
        id: layout
        category: "layout"
        property alias windowWidth: window.width
        property alias windowHeight: window.height
        property real leftWidth: 300
        property real rightWidth: 300
        property real bottomHeight: 280
        // Il cluster tiene un'altezza sua: chi guarda gli spot vuole la fascia
        // alta, chi guarda il registro la vuole bassa, e nessuno dei due deve
        // rifarla ogni volta che cambia scheda.
        property real clusterBottomHeight: 340
        property real mapWidth: 308
        property string hiddenColumns: ""
        property string columnWidths: ""
        property var savedFilters: ({})
        // I pannelli chiusi e quelli in finestra propria, come liste di chiavi
        // separate da virgola. Restano da una sessione all'altra.
        property string hiddenPanels: "cw"
        property string detachedPanels: ""
    }

    // ── I pannelli: chi sono, dove stanno ───────────────────────────────────
    //
    // Ogni pannello ha una chiave. Con quella si sa come si chiama, da quale
    // file nasce quando lo si stacca, e se adesso e' agganciato, in finestra o
    // chiuso. Chiuso vuol dire chiuso davvero: lo spazio non resta vuoto.
    readonly property var panelKeys: ["newqso", "logbook", "callinfo", "cw", "rotor", "ft2", "tabs", "map"]

    function panelTitle(key) {
        switch (key) {
        case "newqso":   return qsTr("New QSO")
        case "logbook":  return qsTr("Logbook")
        case "callinfo": return qsTr("Callsign card")
        case "cw":       return qsTr("CW")
        case "rotor":    return qsTr("Rotator")
        case "ft2":      return qsTr("FT2 Award")
        case "tabs":     return qsTr("Awards, statistics, QSL, activity")
        case "map":      return qsTr("Map")
        }
        return key
    }
    function panelSource(key) {
        switch (key) {
        case "newqso":   return "NewQsoPanel.qml"
        case "logbook":  return "LogbookPanel.qml"
        case "callinfo": return "CallInfoPanel.qml"
        case "cw":       return "CwPanel.qml"
        case "rotor":    return "RotorPanel.qml"
        case "ft2":      return "Ft2AwardPanel.qml"
        case "tabs":     return "BottomTabs.qml"
        case "map":      return "MapPanel.qml"
        }
        return ""
    }

    function panelListOf(text) {
        const out = []
        const parts = String(text || "").split(",")
        for (let i = 0; i < parts.length; ++i) {
            const k = parts[i].trim()
            if (k.length > 0 && window.panelKeys.indexOf(k) >= 0 && out.indexOf(k) < 0)
                out.push(k)
        }
        return out
    }
    readonly property var hiddenPanels: window.panelListOf(layout.hiddenPanels)
    readonly property var detachedPanels: window.panelListOf(layout.detachedPanels)

    // Le misure si ricordano solo quando la disposizione e' intera: se un
    // pannello e' chiuso o in finestra, gli altri si allargano per riempire il
    // vuoto, e quella non e' una misura scelta da nessuno.
    readonly property bool layoutIsWhole: window.hiddenPanels.length === 0 && window.detachedPanels.length === 0

    function isPanelHidden(key) { return window.hiddenPanels.indexOf(key) >= 0 }
    function isPanelDetached(key) { return window.detachedPanels.indexOf(key) >= 0 }
    function isPanelDocked(key) { return !window.isPanelHidden(key) && !window.isPanelDetached(key) }
    function panelState(key) {
        return window.isPanelHidden(key) ? qsTr("closed")
             : window.isPanelDetached(key) ? qsTr("window") : qsTr("docked")
    }

    function showPanel(key) {
        layout.hiddenPanels = window.hiddenPanels.filter(function (k) { return k !== key }).join(",")
    }
    function closePanel(key) {
        // Chiuso e' chiuso: se era in finestra, la finestra sparisce.
        layout.detachedPanels = window.detachedPanels.filter(function (k) { return k !== key }).join(",")
        if (!window.isPanelHidden(key))
            layout.hiddenPanels = window.hiddenPanels.concat([key]).join(",")
    }
    function detachPanel(key) {
        window.showPanel(key)
        if (!window.isPanelDetached(key))
            layout.detachedPanels = window.detachedPanels.concat([key]).join(",")
    }
    function attachPanel(key) {
        layout.detachedPanels = window.detachedPanels.filter(function (k) { return k !== key }).join(",")
        window.showPanel(key)
    }
    function togglePanel(key) {
        if (window.isPanelHidden(key)) window.showPanel(key)
        else window.closePanel(key)
    }
    function resetPanels() {
        layout.hiddenPanels = ""
        layout.detachedPanels = ""
    }

    // ── Azioni comuni a barra, scorciatoie e pannelli ───────────────────────
    function openQso(id) {
        if (id > 0)
            qsoDialog.openFor(id)
    }
    function focusSearch() { topBar.focusSearch() }
    function openStats() {
        statsWindow.active = true
        if (statsWindow.item) {
            statsWindow.item.raise()
            statsWindow.item.requestActivate()
        }
    }
    function openActivation() { activationDialog.openDialog() }
    function openContest() {
        contestWindow.active = true
        if (contestWindow.item) {
            contestWindow.item.raise()
            contestWindow.item.requestActivate()
        }
    }
    function openRotor() {
        rotorWindow.active = true
        if (rotorWindow.item) {
            rotorWindow.item.raise()
            rotorWindow.item.requestActivate()
        }
    }
    function openCards() {
        cardsWindow.active = true
        if (cardsWindow.item) {
            cardsWindow.item.raise()
            cardsWindow.item.requestActivate()
        }
    }
    function openCluster(tab) {
        clusterWindow.tab = tab
        clusterWindow.active = true
        if (clusterWindow.item) {
            clusterWindow.item.tab = tab
            clusterWindow.item.raise()
            clusterWindow.item.requestActivate()
        }
    }

    Component.onCompleted: {
        const what = startupShow.split(":")
        if (what[0] === "new") newQsoDialog.open()
        else if (what[0] === "qso") openQso(parseInt(what[1]))
        else if (what[0] === "profiles") profilesDialog.open()
        else if (what[0] === "setup") { setupDialog.page = parseInt(what[1] || "3"); setupDialog.open() }
        else if (what[0] === "menu") logbook.showMenu(what[1])
        else if (what[0] === "select") logbook.showSelection(what[1], what[2])
        // Lavori di manutenzione, utili anche da riga di comando.
        else if (what[0] === "combo") { window.showPanel("cw"); comboTimer.start() }
        else if (what[0] === "combo2") { newQsoDialog.open(); combo2Timer.start() }
        else if (what[0] === "cwsend") { window.showPanel("cw"); cwSendTimer.start() }
        else if (what[0] === "radioprobe") { bottomTabs.currentTab = 3; decolog.rig.probeRadio() }
        else if (what[0] === "repair") decolog.repairImportedFields()
        else if (what[0] === "fillall") decolog.completeMissingFromCallbook()
        else if (what[0] === "maintenance") { decolog.repairImportedFields(); decolog.completeMissingFromCallbook() }
        else if (what[0] === "tab") bottomTabs.currentTab = parseInt(what[1])
        else if (what[0] === "pop") popWindow.active = true
        else if (what[0] === "panels") { if (what[1]) { const how = what.slice(2); for (let i = 0; i < how.length; ++i) { if (what[1] === "close") window.closePanel(how[i]); else if (what[1] === "detach") window.detachPanel(how[i]); else if (what[1] === "show") window.showPanel(how[i]) } } else panelsPopup.open() }
        else if (what[0] === "cluster") openCluster(parseInt(what[1] || "0"))
        else if (what[0] === "activation") activationDialog.openDialog()
        else if (what[0] === "modes") newQsoPanel.showModes()
        else if (what[0] === "stats") openStats()
        else if (what[0] === "cards") openCards()
        else if (what[0] === "cloud") {
            // cloud:signup:CALL:PASSWORD · cloud:login:CALL:PASSWORD · cloud:sync
            if (what[1] === "signup") decolog.cloud.signup(what[2], what[3])
            else if (what[1] === "login") decolog.cloud.login(what[2], what[3])
            else if (what[1] === "purge") decolog.cloud.purgeCloud("DELETE")
            else if (what[1] === "loginpurge") { decolog.cloud.login(what[2], what[3]); purgeAfterLogin.start() }
            else decolog.cloud.syncNow()
            bottomTabs.currentTab = 3
        }
        else if (what[0] === "rotor") {
            if (what[1] === "window") {
                openRotor()
                if (what[2] !== undefined && rotorWindow.item)
                    rotorWindow.item.showTab(parseInt(what[2]))
            }
            else
                decolog.rotor.pointTo(parseFloat(what[1] || "0"), what[2] || "")
        }
        else if (what[0] === "contest") {
            openContest()
            if (what[1] === "cabrillo" && contestWindow.item)
                contestWindow.item.openCabrillo()
        }
        else if (what[0] === "call") decolog.lookupCall = what[1]
        else if (what[0] === "awards") {
            // awards:<id>[:map|:missing|:unconfirmed]
            if (what[2] === "map") awardsDialog.showMap = true
            else if (what[2]) awardsDialog.view = what[2]
            awardsDialog.openAt(what[1] || "dxcc")
        }
    }

    // Per le prove: svuota il Cloud appena entrato.
    Timer { id: purgeAfterLogin; interval: 4000; onTriggered: decolog.cloud.purgeCloud("DELETE") }

    Timer { id: comboTimer; interval: 800; onTriggered: cwPanel.showCombo() }
    Timer { id: combo2Timer; interval: 900; onTriggered: newQsoDialog.showBandCombo() }
    Timer { id: cwSendTimer; interval: 1200; onTriggered: decolog.rig.sendMacro(0, {}) }

    NewQsoDialog { id: newQsoDialog }
    QsoDetailDialog { id: qsoDialog }
    StationProfilesDialog { id: profilesDialog }
    SetupDialog { id: setupDialog }
    ActivationDialog { id: activationDialog }
    AwardsDialog {
        id: awardsDialog
        onOpenQso: (id) => window.openQso(id)
    }

    FileDialog {
        id: importDialog
        title: qsTr("Import ADIF")
        nameFilters: [qsTr("ADIF files (*.adi *.adif)"), qsTr("All files (*)")]
        onAccepted: decolog.importAdif(selectedFile)
    }
    FileDialog {
        id: exportDialog
        title: qsTr("Export ADIF")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "adi"
        nameFilters: [qsTr("ADIF files (*.adi)")]
        onAccepted: decolog.exportAdif(selectedFile)
    }

    Loader {
        id: statsWindow
        active: false
        sourceComponent: StatsWindow {
            onClosing: statsWindow.active = false
        }
    }

    Loader {
        id: rotorWindow
        active: false
        sourceComponent: RotorWindow {
            onClosing: rotorWindow.active = false
        }
    }

    Loader {
        id: contestWindow
        active: false
        sourceComponent: ContestWindow {
            onClosing: contestWindow.active = false
        }
    }

    Loader {
        id: cardsWindow
        active: false
        sourceComponent: QslCardsWindow {
            onClosing: cardsWindow.active = false
        }
    }

    Loader {
        id: clusterWindow
        property int tab: 0
        active: false
        sourceComponent: ClusterWindow {
            tab: clusterWindow.tab
            onClosing: clusterWindow.active = false
        }
    }

    Loader {
        id: popWindow
        active: false
        sourceComponent: LogbookWindow {
            onClosing: popWindow.active = false
        }
    }

    // Una finestra per ogni pannello staccato: nasce quando si stacca, muore
    // quando si riaggancia o si chiude.
    Instantiator {
        model: window.detachedPanels
        delegate: PanelWindow {
            panelKey: modelData
            panelTitle: window.panelTitle(modelData)
            panelSource: window.panelSource(modelData)
            onClosing: window.attachPanel(panelKey)
            onAttachRequested: window.attachPanel(panelKey)
            onCloseRequested: window.closePanel(panelKey)
            onOpenQsoRequested: (id) => window.openQso(id)
            onAwardRequested: (id) => awardsDialog.openAt(id)
            onClusterRequested: (tab) => window.openCluster(tab)
            onStatsRequested: window.openStats()
            onRotorRequested: window.openRotor()
        }
    }

    // ── Il menu dei pannelli ────────────────────────────────────────────────
    Popup {
        id: panelsPopup
        parent: Overlay.overlay
        x: window.width - width - 16
        y: 72
        width: 320
        padding: 12
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: Theme.panelColor; border.color: Theme.glassBorder; radius: 6 }

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            Text {
                text: qsTr("PANELS")
                color: Theme.secondaryColor
                font.family: Theme.monoFamily
                font.pixelSize: 11
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Click a panel to close it or bring it back. The arrow detaches it into "
                           + "a window of its own; a closed panel frees its space instead of leaving a hole.")
                color: Theme.textSecondary
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            Repeater {
                model: window.panelKeys
                delegate: Rectangle {
                    id: panelRow
                    required property string modelData
                    readonly property bool closed: window.isPanelHidden(panelRow.modelData)
                    Layout.fillWidth: true
                    implicitHeight: 26
                    radius: 4
                    color: rowArea.containsMouse ? Theme.glassOverlay : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 4
                        spacing: 8

                        Rectangle {
                            implicitWidth: 8
                            implicitHeight: 8
                            radius: 4
                            color: panelRow.closed ? Theme.borderColor : Theme.accentColor
                        }
                        Text {
                            Layout.fillWidth: true
                            text: window.panelTitle(panelRow.modelData)
                            color: panelRow.closed ? Theme.textSecondary : Theme.textPrimary
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        Text {
                            text: window.panelState(panelRow.modelData)
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 10
                        }
                        PanelControl {
                            glyph: window.isPanelDetached(panelRow.modelData) ? "↩" : "⤢"
                            hint: window.isPanelDetached(panelRow.modelData)
                                  ? qsTr("Put it back in the main window")
                                  : qsTr("Detach it into its own window")
                            onClicked: window.isPanelDetached(panelRow.modelData)
                                       ? window.attachPanel(panelRow.modelData)
                                       : window.detachPanel(panelRow.modelData)
                        }
                    }

                    MouseArea {
                        id: rowArea
                        anchors.fill: parent
                        anchors.rightMargin: 24
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: window.togglePanel(panelRow.modelData)
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSoft }
            GlassButton {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Restore the default layout")
                buttonHeight: 24
                fontPixelSize: 11
                onClicked: { window.resetPanels(); panelsPopup.close() }
            }
        }
    }

    Shortcut { sequence: "Ctrl+N"; onActivated: newQsoDialog.open() }
    Shortcut { sequence: "Ctrl+F"; onActivated: window.focusSearch() }
    Shortcut { sequence: "Ctrl+,"; onActivated: setupDialog.open() }
    Shortcut { sequence: "Ctrl+I"; onActivated: importDialog.open() }
    Shortcut { sequence: "Ctrl+E"; onActivated: exportDialog.open() }
    Shortcut { sequence: "Ctrl+K"; onActivated: window.openCluster(0) }
    Shortcut { sequence: "Ctrl+T"; onActivated: activationDialog.openDialog() }
    Shortcut { sequence: "Ctrl+Shift+T"; onActivated: window.openContest() }
    Shortcut { sequence: "Ctrl+R"; onActivated: window.openRotor() }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            id: topBar
            Layout.fillWidth: true
            onSetupRequested: setupDialog.open()
            onImportRequested: importDialog.open()
            onExportRequested: exportDialog.open()
            onAwardsRequested: awardsDialog.openAt("")
            onClusterRequested: window.openCluster(0)
            onActivationRequested: activationDialog.openDialog()
            onProfilesRequested: profilesDialog.open()
            closedPanels: window.hiddenPanels.length
            onPanelsRequested: panelsPopup.opened ? panelsPopup.close() : panelsPopup.open()
        }

        SplitView {
            id: verticalSplit
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8
            orientation: Qt.Vertical
            handle: splitHandle

            SplitView {
                SplitView.fillHeight: true
                orientation: Qt.Horizontal
                handle: splitHandle

                NewQsoPanel {
                    id: newQsoPanel
                    SplitView.preferredWidth: layout.leftWidth
                    SplitView.minimumWidth: 260
                    visible: window.isPanelDocked("newqso")
                    panelKey: "newqso"
                    onWidthChanged: if (width > 0 && window.layoutIsWhole) layout.leftWidth = width
                    onExpandRequested: newQsoDialog.open()
                    onDetachRequested: window.detachPanel("newqso")
                    onCloseRequested: window.closePanel("newqso")
                }

                LogbookPanel {
                    id: logbook
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 480
                    visible: window.isPanelDocked("logbook")
                    panelKey: "logbook"
                    hiddenColumns: layout.hiddenColumns
                    columnWidths: layout.columnWidths
                    savedFilters: layout.savedFilters
                    onHiddenColumnsEdited: (value) => layout.hiddenColumns = value
                    onColumnWidthsEdited: (value) => layout.columnWidths = value
                    onSavedFiltersEdited: (value) => layout.savedFilters = value
                    onOpenQso: (id) => window.openQso(id)
                    onPopRequested: window.detachPanel("logbook")
                    onDetachRequested: window.detachPanel("logbook")
                    onCloseRequested: window.closePanel("logbook")
                }

                SplitView {
                    id: rightColumn
                    SplitView.preferredWidth: layout.rightWidth
                    SplitView.minimumWidth: 260
                    visible: window.isPanelDocked("callinfo") || window.isPanelDocked("cw")
                             || window.isPanelDocked("rotor") || window.isPanelDocked("ft2")
                    onWidthChanged: if (width > 0 && window.layoutIsWhole) layout.rightWidth = width
                    orientation: Qt.Vertical
                    handle: splitHandle

                    CallInfoPanel {
                        SplitView.fillHeight: true
                        SplitView.minimumHeight: 80
                        visible: window.isPanelDocked("callinfo")
                        panelKey: "callinfo"
                        onOpenQso: (id) => window.openQso(id)
                        onDetachRequested: window.detachPanel("callinfo")
                        onCloseRequested: window.closePanel("callinfo")
                    }
                    CwPanel {
                        id: cwPanel
                        SplitView.preferredHeight: 260
                        SplitView.minimumHeight: 120
                        visible: window.isPanelDocked("cw")
                        panelKey: "cw"
                        onDetachRequested: window.detachPanel("cw")
                        onCloseRequested: window.closePanel("cw")
                    }
                    RotorPanel {
                        SplitView.preferredHeight: implicitHeight
                        SplitView.minimumHeight: 60
                        visible: decolog.rotor.enabled && window.isPanelDocked("rotor")
                        panelKey: "rotor"
                        onWindowRequested: window.openRotor()
                        onDetachRequested: window.detachPanel("rotor")
                        onCloseRequested: window.closePanel("rotor")
                    }
                    Ft2AwardPanel {
                        SplitView.preferredHeight: implicitHeight
                        SplitView.minimumHeight: 60
                        visible: window.isPanelDocked("ft2")
                        panelKey: "ft2"
                        onDetailsRequested: awardsDialog.openAt("ft2")
                        onDetachRequested: window.detachPanel("ft2")
                        onCloseRequested: window.closePanel("ft2")
                    }
                }
            }

            SplitView {
                // Sul DX Cluster la fascia si alza da sola, perche' cinque righe
                // di spot non sono un cluster, sono un assaggio — ma si alza e
                // basta: da li' si tira dove si vuole, anche piu' in basso, e
                // l'altezza scelta resta quella del cluster. Prima il minimo
                // stesso diventava 340 e la fascia non si poteva piu' abbassare
                // finche' si stava sugli spot.
                SplitView.preferredHeight: bottomTabs.currentTab === 4 ? layout.clusterBottomHeight
                                                                       : layout.bottomHeight
                SplitView.minimumHeight: 130
                visible: window.isPanelDocked("tabs") || window.isPanelDocked("map")
                onHeightChanged: {
                    if (height > 0 && window.layoutIsWhole) {
                        if (bottomTabs.currentTab === 4)
                            layout.clusterBottomHeight = height
                        else
                            layout.bottomHeight = height
                    }
                }
                orientation: Qt.Horizontal
                handle: splitHandle

                BottomTabs {
                    id: bottomTabs
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 320
                    visible: window.isPanelDocked("tabs")
                    panelKey: "tabs"
                    onAwardRequested: (id) => awardsDialog.openAt(id)
                    onClusterRequested: (tab) => window.openCluster(tab)
                    onStatsRequested: window.openStats()
                    onDetachRequested: window.detachPanel("tabs")
                    onCloseRequested: window.closePanel("tabs")
                }
                // Allineata alla colonna di destra, come nel mockup, finche' non
                // la si tira da un'altra parte.
                MapPanel {
                    SplitView.preferredWidth: layout.mapWidth
                    SplitView.minimumWidth: 180
                    visible: window.isPanelDocked("map")
                    panelKey: "map"
                    onWidthChanged: if (width > 0 && window.layoutIsWhole) layout.mapWidth = width
                    onDetachRequested: window.detachPanel("map")
                    onCloseRequested: window.closePanel("map")
                }
            }
        }

        StatusRail { Layout.fillWidth: true }
    }

    Component {
        id: splitHandle
        Rectangle {
            id: handleRoot
            implicitWidth: 8
            implicitHeight: 8
            color: "transparent"
            Rectangle {
                anchors.centerIn: parent
                width: handleRoot.width > handleRoot.height ? 40 : 2
                height: handleRoot.width > handleRoot.height ? 2 : 40
                radius: 1
                color: handleRoot.SplitHandle.pressed ? Theme.primaryColor
                     : handleRoot.SplitHandle.hovered ? Theme.textSecondary : Theme.borderSoft
            }
        }
    }
}
