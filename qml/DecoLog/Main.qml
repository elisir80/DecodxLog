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
        property real bottomHeight: 200
        property string hiddenColumns: ""
        property var savedFilters: ({})
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
        else if (what[0] === "repair") decolog.repairImportedFields()
        else if (what[0] === "fillall") decolog.completeMissingFromCallbook()
        else if (what[0] === "maintenance") { decolog.repairImportedFields(); decolog.completeMissingFromCallbook() }
        else if (what[0] === "tab") bottomTabs.currentTab = parseInt(what[1])
        else if (what[0] === "pop") popWindow.active = true
        else if (what[0] === "cluster") openCluster(parseInt(what[1] || "0"))
        else if (what[0] === "activation") activationDialog.openDialog()
        else if (what[0] === "modes") newQsoPanel.showModes()
        else if (what[0] === "stats") openStats()
        else if (what[0] === "cards") openCards()
        else if (what[0] === "cloud") {
            // cloud:signup:CALL:PASSWORD · cloud:login:CALL:PASSWORD · cloud:sync
            if (what[1] === "signup") decolog.cloud.signup(what[2], what[3])
            else if (what[1] === "login") decolog.cloud.login(what[2], what[3])
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
                    onWidthChanged: if (width > 0) layout.leftWidth = width
                    onExpandRequested: newQsoDialog.open()
                }

                LogbookPanel {
                    id: logbook
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 480
                    hiddenColumns: layout.hiddenColumns
                    savedFilters: layout.savedFilters
                    onHiddenColumnsEdited: (value) => layout.hiddenColumns = value
                    onSavedFiltersEdited: (value) => layout.savedFilters = value
                    onOpenQso: (id) => window.openQso(id)
                    onPopRequested: popWindow.active = true
                }

                ColumnLayout {
                    id: rightColumn
                    SplitView.preferredWidth: layout.rightWidth
                    SplitView.minimumWidth: 260
                    onWidthChanged: if (width > 0) layout.rightWidth = width
                    spacing: 8

                    CallInfoPanel {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        onOpenQso: (id) => window.openQso(id)
                    }
                    RotorPanel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        visible: decolog.rotor.enabled
                        onWindowRequested: window.openRotor()
                    }
                    Ft2AwardPanel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        onDetailsRequested: awardsDialog.openAt("ft2")
                    }
                }
            }

            RowLayout {
                SplitView.preferredHeight: layout.bottomHeight
                SplitView.minimumHeight: 130
                onHeightChanged: if (height > 0) layout.bottomHeight = height
                spacing: 8

                BottomTabs {
                    id: bottomTabs
                    onAwardRequested: (id) => awardsDialog.openAt(id)
                    onClusterRequested: (tab) => window.openCluster(tab)
                    onStatsRequested: window.openStats()
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                // Allineata alla colonna di destra, come nel mockup.
                MapPanel {
                    Layout.preferredWidth: rightColumn.width + 8
                    Layout.fillHeight: true
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
