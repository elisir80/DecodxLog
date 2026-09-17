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

    Component.onCompleted: {
        const what = startupShow.split(":")
        if (what[0] === "new") newQsoDialog.open()
        else if (what[0] === "qso") openQso(parseInt(what[1]))
        else if (what[0] === "profiles") profilesDialog.open()
        else if (what[0] === "setup") { setupDialog.page = parseInt(what[1] || "3"); setupDialog.open() }
        else if (what[0] === "menu") logbook.showMenu(what[1])
        else if (what[0] === "tab") bottomTabs.currentTab = parseInt(what[1])
        else if (what[0] === "pop") popWindow.active = true
    }

    NewQsoDialog { id: newQsoDialog }
    QsoDetailDialog { id: qsoDialog }
    StationProfilesDialog { id: profilesDialog }
    SetupDialog { id: setupDialog }

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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            id: topBar
            Layout.fillWidth: true
            onSetupRequested: setupDialog.open()
            onImportRequested: importDialog.open()
            onExportRequested: exportDialog.open()
            onAwardsRequested: bottomTabs.currentTab = 0
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
                    Ft2AwardPanel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        onDetailsRequested: bottomTabs.currentTab = 0
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
