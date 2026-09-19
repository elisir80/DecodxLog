// DecoLog — il logbook: filtri a pillole, colonne scelte dall'operatore, una riga
// per QSO con gli stati QSL (L Q C E = LoTW, QRZ, Club Log, eQSL).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Decodium.UI

GlassPanel {
    id: root

    // Colonne nascoste, come chiavi separate da virgola ("dxcc,source").
    property string hiddenColumns: ""
    property var savedFilters: ({})
    property bool showPopButton: true
    signal hiddenColumnsEdited(string value)
    signal savedFiltersEdited(var value)
    signal openQso(var id)
    signal popRequested()

    property int selectedRow: -1
    // Selezione multipla: il clic sinistro aggiunge o toglie una riga, lo shift
    // prende tutto quello che sta in mezzo, Esc lascia andare tutto.
    property var selectedIds: []
    readonly property var model: decolog.qsoModel
    readonly property var hidden: hiddenColumns.length ? hiddenColumns.split(",") : []

    function isSelected(id) { return root.selectedIds.indexOf(id) >= 0 }
    function clearSelection() { root.selectedIds = [] }
    function selectOnly(row) {
        const id = root.model.idAt(row)
        root.selectedIds = id > 0 ? [id] : []
    }
    function toggleRow(row) {
        const id = root.model.idAt(row)
        if (id <= 0)
            return
        const list = root.selectedIds.slice()
        const i = list.indexOf(id)
        if (i >= 0) list.splice(i, 1)
        else list.push(id)
        root.selectedIds = list
    }
    function selectRange(row) {
        // Dall'ultima riga toccata fino a questa, estremi compresi.
        const anchor = root.selectedRow < 0 ? row : root.selectedRow
        const list = root.selectedIds.slice()
        for (let r = Math.min(anchor, row); r <= Math.max(anchor, row); ++r) {
            const id = root.model.idAt(r)
            if (id > 0 && list.indexOf(id) < 0)
                list.push(id)
        }
        root.selectedIds = list
    }

    function isHidden(key) { return hidden.indexOf(key) >= 0 }
    function toggleColumn(key) {
        const list = hidden.slice()
        const i = list.indexOf(key)
        if (i >= 0) list.splice(i, 1)
        else list.push(key)
        hiddenColumnsEdited(list.join(","))
    }
    function modeColor(mode) {
        if (mode === "FT2") return Theme.accentColor
        if (mode === "FT8" || mode === "FT4") return Theme.primaryColor
        return Theme.textPrimary
    }
    function qslColor(code) {
        return code === "c" ? Theme.accentColor : code === "s" ? Theme.warningColor : Theme.textSecondary
    }
    // Le quattro lettere della colonna QSL, dette a parole: passando il mouse
    // sopra la L si legge com'e' andata con LoTW, e cosi' per le altre.
    function qslService(index) {
        return ["LoTW", "QRZ Logbook", "Club Log", "eQSL"][index] || ""
    }
    function qslTip(index, code) {
        const name = root.qslService(index)
        return code === "c" ? qsTr("%1: confirmed — received").arg(name)
             : code === "s" ? qsTr("%1: sent, waiting for the confirmation").arg(name)
             : qsTr("%1: not sent").arg(name)
    }
    function sourceColor(src) {
        return src === "udp" ? Theme.secondaryColor : src === "cld" ? Theme.warningColor : Theme.textSecondary
    }
    // Per le schermate di prova (--show menu:<nome>).
    function showMenu(name) {
        if (name === "columns") columnsMenu.popup(root.width - 260, Theme.panelHeight)
        else if (name === "filters") { addFilterMenu.popup(60, Theme.panelHeight + 30); bandMenu.open() }
        else if (name === "saved") savedMenu.popup(root.width - 200, Theme.panelHeight + 30)
        else if (name === "row") rowMenu.popupFor(root.model.idAt(0))
        else if (name === "actions") actionsMenu.popup(root.width - 320, Theme.panelHeight)
        else if (name === "tag") tagPopup.openFor(root.model.shownIds(), true)
        else if (name === "dates") datePopup.open()
    }
    // Per le schermate di prova (--show select:<righe separate da virgola>:<cosa>).
    function showSelection(rows, what) {
        const list = []
        const wanted = String(rows).split(",")
        for (let i = 0; i < wanted.length; ++i) {
            const id = root.model.idAt(parseInt(wanted[i]))
            if (id > 0)
                list.push(id)
        }
        root.selectedIds = list
        root.selectedRow = parseInt(wanted[0])
        if (what === "menu") rowMenu.popupFor(list[0])
        else if (what === "confirm") confirmRowDelete.openFor(list)
        else if (what === "confirm2") { confirmRowDelete.openFor(list); confirmRowDelete.step = 2 }
        else if (what === "delete") { decolog.deleteQsos(list); root.clearSelection() }
        else if (what === "callbook") { for (let k = 0; k < list.length; ++k) decolog.completeQsoFromCallbook(list[k]) }
    }
    function qslFilterLabel(key) {
        return { confirmed: qsTr("confirmed"), lotw: qsTr("LoTW confirmed"), card: qsTr("card confirmed"),
                 eqsl: qsTr("eQSL confirmed"), unconfirmed: qsTr("not confirmed") }[key] || key
    }
    function thisMonth() {
        const now = decolog.utcNow()
        return now.date.substring(0, 7)
    }

    title: qsTr("Logbook")
    dotColor: Theme.primaryColor
    padding: 0

    headerLeading: [
        Pill {
            visible: root.selectedIds.length > 0
            text: qsTr("%1 selected").arg(root.selectedIds.length)
            tone: Theme.primaryColor
            pillHeight: 20
            fontPixelSize: 10
        },
        Pill {
            visible: decolog.clientConnected
            text: "LIVE"
            tone: Theme.accentColor
            pillHeight: 20
            fontPixelSize: 10
        }
    ]
    headerTools: [
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.model.filtered
                  ? qsTr("%1 QSO · %2 shown").arg(root.model.totalCount.toLocaleString(Qt.locale("en_US"), "f", 0))
                                             .arg(root.model.count.toLocaleString(Qt.locale("en_US"), "f", 0))
                  : qsTr("%1 QSO").arg(root.model.totalCount.toLocaleString(Qt.locale("en_US"), "f", 0))
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Actions ▾")
            buttonHeight: 24
            fontPixelSize: 11
            onClicked: actionsMenu.popup()
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Columns")
            buttonHeight: 24
            fontPixelSize: 11
            onClicked: columnsMenu.popup()
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.showPopButton
            text: qsTr("Pop")
            tone: Theme.primaryColor
            buttonHeight: 24
            fontPixelSize: 11
            onClicked: root.popRequested()
        }
    ]

    StyledMenu {
        id: columnsMenu
        Repeater {
            model: root.model.columns
            StyledMenuItem {
                required property int index
                // Nominativo e ora non si nascondono: senza, la riga non dice niente.
                enabled: index > 1
                checkable: true
                checked: !root.isHidden(root.model.columnKey(index))
                text: root.model.columnTitle(index)
                onTriggered: root.toggleColumn(root.model.columnKey(index))
            }
        }
    }

    StyledMenu {
        id: addFilterMenu
        StyledMenu {
            id: bandMenu
            title: qsTr("Band")
            Repeater {
                model: bandMenu.opened || addFilterMenu.opened ? root.model.bandsInLog() : []
                StyledMenuItem {
                    required property string modelData
                    text: modelData
                    checkable: true
                    checked: root.model.bandFilter.indexOf(modelData) >= 0
                    onTriggered: {
                        const list = root.model.bandFilter.slice()
                        const i = list.indexOf(modelData)
                        if (i >= 0) list.splice(i, 1); else list.push(modelData)
                        root.model.bandFilter = list
                    }
                }
            }
        }
        StyledMenu {
            id: modeMenu
            title: qsTr("Mode")
            Repeater {
                model: modeMenu.opened || addFilterMenu.opened ? root.model.modesInLog() : []
                StyledMenuItem {
                    required property string modelData
                    text: modelData
                    checkable: true
                    checked: root.model.modeFilter.indexOf(modelData) >= 0
                    onTriggered: {
                        const list = root.model.modeFilter.slice()
                        const i = list.indexOf(modelData)
                        if (i >= 0) list.splice(i, 1); else list.push(modelData)
                        root.model.modeFilter = list
                    }
                }
            }
        }
        StyledMenu {
            id: dxccMenu
            title: qsTr("DXCC entity")
            Repeater {
                model: dxccMenu.opened ? decolog.dxccInLog() : []
                StyledMenuItem {
                    required property var modelData
                    text: "%1  %2 (%3)".arg(modelData.dxcc).arg(modelData.name || "?").arg(modelData.count)
                    checkable: true
                    checked: root.model.dxccFilter === modelData.dxcc
                    onTriggered: root.model.dxccFilter = checked ? modelData.dxcc : 0
                }
            }
        }
        StyledMenu {
            title: qsTr("QSL")
            Repeater {
                model: ["confirmed", "lotw", "card", "eqsl", "unconfirmed"]
                StyledMenuItem {
                    required property string modelData
                    text: root.qslFilterLabel(modelData)
                    checkable: true
                    checked: root.model.qslFilter === modelData
                    onTriggered: root.model.qslFilter = checked ? modelData : ""
                }
            }
        }
        StyledMenu {
            id: profileMenu
            title: qsTr("Station profile")
            Repeater {
                model: decolog.stationProfiles
                StyledMenuItem {
                    required property var profileId
                    required property string name
                    required property bool deleted
                    visible: !deleted
                    height: visible ? implicitHeight : 0
                    text: name
                    checkable: true
                    checked: root.model.profileFilter === profileId
                    onTriggered: root.model.profileFilter = checked ? profileId : 0
                }
            }
        }
        StyledMenu {
            id: tagMenu
            title: qsTr("Tag")
            Repeater {
                model: tagMenu.opened ? root.model.tagsInLog() : []
                StyledMenuItem {
                    required property var modelData
                    text: "%1 (%2)".arg(modelData.key).arg(modelData.count)
                    checkable: true
                    checked: root.model.tagFilter.toLowerCase() === modelData.key.toLowerCase()
                    onTriggered: root.model.tagFilter = checked ? modelData.key : ""
                }
            }
            StyledMenuItem {
                visible: tagMenu.opened && root.model.tagsInLog().length === 0
                height: visible ? implicitHeight : 0
                enabled: false
                text: qsTr("No tags in the log yet")
            }
        }
        StyledMenuItem {
            text: qsTr("This month")
            onTriggered: root.model.monthFilter = root.thisMonth()
        }
        StyledMenuItem {
            text: qsTr("Date range…")
            onTriggered: datePopup.open()
        }
    }

    // ── Azioni sulle righe mostrate ─────────────────────────────────────────
    StyledMenu {
        id: actionsMenu
        StyledMenuItem {
            text: qsTr("Tag the %1 QSO shown…").arg(root.model.count)
            enabled: root.model.count > 0
            onTriggered: tagPopup.openFor(root.model.shownIds(), true)
        }
        StyledMenuItem {
            text: qsTr("Remove a tag from the QSO shown…")
            enabled: root.model.count > 0
            onTriggered: tagPopup.openFor(root.model.shownIds(), false)
        }
        StyledMenuItem {
            text: qsTr("Export the %1 QSO shown to ADIF…").arg(root.model.count)
            enabled: root.model.count > 0
            onTriggered: exportShown.open()
        }
        StyledMenuItem {
            text: qsTr("Complete the QSO shown from the callbook…")
            enabled: root.model.count > 0 && decolog.callbookProvider !== "off"
            onTriggered: decolog.completeShownFromCallbook()
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
        StyledMenuItem {
            text: qsTr("Clear all filters")
            enabled: root.model.filtered
            onTriggered: root.model.clearFilters()
        }
    }

    // Cancellare e' morbido, ma si chiede lo stesso, e due volte: un clic
    // sbagliato capita, e qui le righe possono essere tante.
    Popup {
        id: confirmRowDelete
        property var ids: []
        property string call: ""
        property int step: 1
        function openFor(list) {
            ids = list
            call = list.length === 1 ? root.model.valueAt(root.model.rowForId(list[0]), 1) : ""
            step = 1
            open()
        }
        anchors.centerIn: Overlay.overlay
        modal: true
        padding: 16
        onClosed: step = 1
        background: Rectangle { color: Theme.panelColor; border.color: Theme.borderColor; radius: 6 }
        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: confirmRowDelete.step === 1
                      ? (confirmRowDelete.ids.length === 1
                         ? qsTr("Delete %1? The QSO stays in the history and can be recovered.").arg(confirmRowDelete.call)
                         : qsTr("Delete the %1 QSO selected? They stay in the history and can be recovered.").arg(confirmRowDelete.ids.length))
                      : (confirmRowDelete.ids.length === 1
                         ? qsTr("Once more, to be sure: delete %1?").arg(confirmRowDelete.call)
                         : qsTr("Once more, to be sure: delete %1 QSO?").arg(confirmRowDelete.ids.length))
                color: confirmRowDelete.step === 1 ? Theme.textPrimary : Theme.errorColor
                wrapMode: Text.Wrap
                Layout.maximumWidth: 360
            }
            RowLayout {
                spacing: 8
                Item { Layout.fillWidth: true }
                GlassButton { text: qsTr("Cancel"); onClicked: confirmRowDelete.close() }
                GlassButton {
                    id: confirmDeleteButton
                    text: confirmRowDelete.step === 1 ? qsTr("Delete") : qsTr("Delete for good")
                    tone: Theme.errorColor
                    filled: true
                    onClicked: {
                        // Il primo clic chiede di nuovo, il secondo cancella.
                        if (confirmRowDelete.step === 1) {
                            confirmRowDelete.step = 2
                            return
                        }
                        const list = confirmRowDelete.ids
                        confirmRowDelete.close()
                        decolog.deleteQsos(list)
                        root.clearSelection()
                    }
                }
            }
        }
    }

    FileDialog {
        id: exportShown
        title: qsTr("Export the QSO shown")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "adi"
        nameFilters: [qsTr("ADIF files (*.adi)")]
        onAccepted: decolog.exportQsos(root.model.shownIds(), selectedFile)
    }

    // Etichetta da aggiungere o togliere a un gruppo di QSO.
    Popup {
        id: tagPopup
        property var ids: []
        property bool adding: true
        function openFor(list, add) { ids = list; adding = add; open() }
        anchors.centerIn: parent
        modal: true
        padding: 14
        background: Rectangle { color: Theme.panelColor; border.color: Theme.glassBorder; radius: 6 }
        onOpened: { tagField.text = ""; tagField.forceActiveFocus() }
        ColumnLayout {
            spacing: 8
            Text {
                text: tagPopup.adding ? qsTr("Add a tag to %1 QSO").arg(tagPopup.ids.length)
                                      : qsTr("Remove a tag from %1 QSO").arg(tagPopup.ids.length)
                color: Theme.textPrimary
                font.pixelSize: 13
                font.bold: true
            }
            StyledTextField {
                id: tagField
                Layout.preferredWidth: 300
                mono: false
                placeholderText: qsTr("e.g. pota, field day, portable")
                Keys.onReturnPressed: tagApply.clicked()
            }
            Flow {
                Layout.preferredWidth: 300
                spacing: 4
                Repeater {
                    model: tagPopup.opened ? root.model.tagsInLog().slice(0, 16) : []
                    Pill {
                        required property var modelData
                        text: modelData.key
                        tone: Theme.secondaryColor
                        interactive: true
                        onClicked: tagField.text = modelData.key
                    }
                }
            }
            Text {
                id: tagResult
                Layout.preferredWidth: 300
                wrapMode: Text.Wrap
                visible: text.length > 0
                color: Theme.textSecondary
                font.pixelSize: 11
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8
                GlassButton { text: qsTr("Close"); onClicked: tagPopup.close() }
                GlassButton {
                    id: tagApply
                    text: tagPopup.adding ? qsTr("Add tag") : qsTr("Remove tag")
                    tone: tagPopup.adding ? Theme.accentColor : Theme.warningColor
                    filled: true
                    enabled: tagField.text.trim().length > 0 && tagField.text.indexOf(",") < 0
                    onClicked: {
                        const n = decolog.tagQsos(tagPopup.ids, tagField.text.trim(), tagPopup.adding)
                        tagResult.text = ""
                        tagPopup.close()
                    }
                }
            }
        }
    }

    // Intervallo di date, estremi compresi.
    Popup {
        id: datePopup
        anchors.centerIn: parent
        modal: true
        padding: 14
        background: Rectangle { color: Theme.panelColor; border.color: Theme.glassBorder; radius: 6 }
        onOpened: { fromField.text = root.model.dateFrom; toField.text = root.model.dateTo; fromField.forceActiveFocus() }
        ColumnLayout {
            spacing: 8
            RowLayout {
                spacing: 10
                LabeledField {
                    label: qsTr("From (UTC)")
                    StyledTextField { id: fromField; Layout.preferredWidth: 130; placeholderText: "2026-01-01" }
                }
                LabeledField {
                    label: qsTr("To (included)")
                    StyledTextField { id: toField; Layout.preferredWidth: 130; placeholderText: "2026-12-31"; Keys.onReturnPressed: dateApply.clicked() }
                }
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8
                GlassButton { text: qsTr("Cancel"); onClicked: datePopup.close() }
                GlassButton {
                    id: dateApply
                    text: qsTr("Apply")
                    tone: Theme.accentColor
                    filled: true
                    onClicked: {
                        root.model.monthFilter = ""
                        root.model.dateFrom = fromField.text
                        root.model.dateTo = toField.text
                        datePopup.close()
                    }
                }
            }
        }
    }

    StyledMenu {
        id: savedMenu
        Repeater {
            model: Object.keys(root.savedFilters)
            StyledMenuItem {
                required property string modelData
                text: modelData
                onTriggered: root.model.applyFilterState(root.savedFilters[modelData])
            }
        }
        MenuSeparator {
            visible: Object.keys(root.savedFilters).length > 0
            contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft }
        }
        StyledMenuItem {
            text: qsTr("Save current filters…")
            enabled: root.model.filtered
            onTriggered: saveFilterPopup.open()
        }
        StyledMenu {
            title: qsTr("Delete")
            enabled: Object.keys(root.savedFilters).length > 0
            Repeater {
                model: Object.keys(root.savedFilters)
                StyledMenuItem {
                    required property string modelData
                    text: modelData
                    onTriggered: {
                        const copy = Object.assign({}, root.savedFilters)
                        delete copy[modelData]
                        root.savedFiltersEdited(copy)
                    }
                }
            }
        }
    }

    Popup {
        id: saveFilterPopup
        anchors.centerIn: parent
        modal: true
        padding: 12
        background: Rectangle { color: Theme.panelColor; border.color: Theme.glassBorder; radius: 6 }
        onOpened: { filterName.text = ""; filterName.forceActiveFocus() }
        ColumnLayout {
            spacing: 8
            LabeledField {
                label: qsTr("Filter name")
                StyledTextField {
                    id: filterName
                    Layout.preferredWidth: 240
                    mono: false
                    Keys.onReturnPressed: saveButton.clicked()
                }
            }
            GlassButton {
                id: saveButton
                Layout.alignment: Qt.AlignRight
                text: qsTr("Save")
                tone: Theme.accentColor
                filled: true
                enabled: filterName.text.trim().length > 0
                onClicked: {
                    const copy = Object.assign({}, root.savedFilters)
                    copy[filterName.text.trim()] = root.model.filterState()
                    root.savedFiltersEdited(copy)
                    saveFilterPopup.close()
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Filtri ──────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: filterRow.implicitHeight + 12
            color: "transparent"
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 1
                color: Theme.borderSoft
            }

            RowLayout {
                id: filterRow
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 6

                Text {
                    text: qsTr("Filters")
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Pill {
                        visible: root.model.filterText.length > 0
                        text: qsTr("Search: %1 ✕").arg(root.model.filterText)
                        tone: Theme.secondaryColor
                        rounded: false
                        interactive: true
                        onClicked: root.model.filterText = ""
                    }
                    Pill {
                        visible: root.model.bandFilter.length > 0
                        text: qsTr("Band: %1 ✕").arg(root.model.bandFilter.join(" "))
                        tone: Theme.secondaryColor
                        rounded: false
                        interactive: true
                        onClicked: root.model.bandFilter = []
                    }
                    Pill {
                        visible: root.model.modeFilter.length > 0
                        text: qsTr("Mode: %1 ✕").arg(root.model.modeFilter.join(" "))
                        tone: Theme.secondaryColor
                        rounded: false
                        interactive: true
                        onClicked: root.model.modeFilter = []
                    }
                    Pill {
                        visible: root.model.monthFilter.length > 0
                        text: {
                            const d = new Date(root.model.monthFilter + "-01T00:00:00Z")
                            return d.toLocaleDateString(Qt.locale("en_US"), "MMM yyyy") + " ✕"
                        }
                        tone: Theme.secondaryColor
                        rounded: false
                        interactive: true
                        onClicked: root.model.monthFilter = ""
                    }
                    Pill {
                        visible: root.model.dxccFilter > 0
                        text: qsTr("DXCC: %1 ✕").arg(root.model.dxccFilter + " " + decolog.dxccName(root.model.dxccFilter))
                        tone: Theme.secondaryColor
                        rounded: false
                        interactive: true
                        onClicked: root.model.dxccFilter = 0
                    }
                    Pill {
                        visible: root.model.qslFilter.length > 0
                        text: qsTr("QSL: %1 ✕").arg(root.qslFilterLabel(root.model.qslFilter))
                        tone: Theme.secondaryColor
                        rounded: false
                        interactive: true
                        onClicked: root.model.qslFilter = ""
                    }
                    Pill {
                        visible: root.model.profileFilter > 0
                        text: qsTr("Station: %1 ✕").arg(decolog.stationProfiles.byId(root.model.profileFilter).name || root.model.profileFilter)
                        tone: Theme.secondaryColor
                        rounded: false
                        interactive: true
                        onClicked: root.model.profileFilter = 0
                    }
                    Pill {
                        visible: root.model.tagFilter.length > 0
                        text: qsTr("Tag: %1 ✕").arg(root.model.tagFilter)
                        tone: Theme.accentColor
                        rounded: false
                        interactive: true
                        onClicked: root.model.tagFilter = ""
                    }
                    Pill {
                        visible: root.model.dateFrom.length > 0 || root.model.dateTo.length > 0
                        text: "%1 → %2 ✕".arg(root.model.dateFrom || "…").arg(root.model.dateTo || "…")
                        tone: Theme.secondaryColor
                        rounded: false
                        interactive: true
                        onClicked: { root.model.dateFrom = ""; root.model.dateTo = "" }
                    }
                    Rectangle {
                        implicitHeight: 22
                        implicitWidth: addText.implicitWidth + 16
                        radius: 4
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.glassBorder
                        Text {
                            id: addText
                            anchors.centerIn: parent
                            text: qsTr("+ add")
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 11
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: addFilterMenu.popup()
                        }
                    }
                }
                Text {
                    text: qsTr("Saved:")
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
                Pill {
                    text: Object.keys(root.savedFilters).length ? qsTr("%1 filters ▾").arg(Object.keys(root.savedFilters).length)
                                                               : qsTr("none ▾")
                    tone: Theme.glassBorder
                    rounded: false
                    interactive: true
                    textColor: Theme.textPrimary
                    onClicked: savedMenu.popup()
                }
            }
        }

        // ── Tabella ─────────────────────────────────────────────────────────
        HorizontalHeaderView {
            id: header
            Layout.fillWidth: true
            syncView: table
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                required property var display
                required property int index
                readonly property bool isQsl: root.model.columnKey(index) === "qsl"
                implicitHeight: Theme.rowHeight
                color: "transparent"

                // Cosa vogliono dire le quattro lettere, per chi non le sa a memoria.
                HoverHandler { id: headHover; enabled: parent.isQsl }
                ToolTip.visible: headHover.hovered
                ToolTip.delay: 400
                ToolTip.text: qsTr("L LoTW · Q QRZ Logbook · C Club Log · E eQSL")

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    verticalAlignment: Text.AlignVCenter
                    text: parent.display
                    color: Theme.secondaryColor
                    font.family: Theme.monoFamily
                    font.pixelSize: Theme.fontSize
                    font.bold: true
                    elide: Text.ElideRight
                }
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.glassBorder }

        TableView {
            id: table
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.model
            boundsBehavior: Flickable.StopAtBounds
            columnWidthProvider: function (column) {
                if (root.isHidden(root.model.columnKey(column)))
                    return 0
                // Il nome prende lo spazio che avanza.
                if (column === 8) {
                    let used = 0
                    for (let c = 0; c < root.model.columns; ++c)
                        if (c !== 8 && !root.isHidden(root.model.columnKey(c)))
                            used += root.model.columnWidthHint(c)
                    return Math.max(root.model.columnWidthHint(8), table.width - used)
                }
                return root.model.columnWidthHint(column)
            }
            rowHeightProvider: function () { return Theme.rowHeight }
            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}
            onWidthChanged: forceLayout()

            Connections {
                target: Theme
                function onDensityChanged() { table.forceLayout() }
            }
            Connections {
                target: root
                function onHiddenColumnsChanged() { table.forceLayout() }
            }

            delegate: Rectangle {
                id: cell
                required property var display
                required property string columnKey
                required property string modeName
                required property bool isNew
                required property int row
                required property var qsoId

                readonly property bool selected: root.isSelected(qsoId)
                readonly property bool current: row === root.selectedRow

                implicitHeight: Theme.rowHeight
                clip: true
                color: selected ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.24)
                     : current ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.10)
                     : isNew ? Theme.rowMatchBg
                     : "transparent"

                Rectangle {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    height: 1
                    color: Theme.borderSoft
                }

                Text {
                    visible: cell.columnKey !== "qsl"
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 4
                    verticalAlignment: Text.AlignVCenter
                    text: cell.display
                    elide: Text.ElideRight
                    font.pixelSize: cell.columnKey === "source" ? 10 : Theme.fontSize
                    font.family: cell.columnKey === "name" || cell.columnKey === "tags" ? Theme.uiFamily : Theme.monoFamily
                    font.bold: cell.columnKey === "call"
                    color: cell.columnKey === "utc" || cell.columnKey === "dxcc" ? Theme.textSecondary
                         : cell.columnKey === "mode" ? root.modeColor(cell.display)
                         : cell.columnKey === "source" ? root.sourceColor(cell.display)
                         : cell.columnKey === "tags" ? Theme.accentColor
                         : Theme.textPrimary
                }

                Row {
                    visible: cell.columnKey === "qsl"
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    Repeater {
                        model: cell.columnKey === "qsl" ? ["L", "Q", "C", "E"] : []
                        Text {
                            required property string modelData
                            required property int index
                            text: modelData
                            color: root.qslColor(String(cell.display).charAt(index))
                            font.family: Theme.monoFamily
                            font.pixelSize: Theme.fontSize
                            font.bold: true

                            HoverHandler { id: qslHover; cursorShape: Qt.WhatsThisCursor }
                            ToolTip.visible: qslHover.hovered
                            ToolTip.delay: 400
                            ToolTip.text: root.qslTip(index, String(cell.display).charAt(index))
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        root.forceActiveFocus()
                        if (mouse.button === Qt.RightButton) {
                            // Il menu vale per quello che e' scelto: se si clicca
                            // fuori dalla selezione, la selezione diventa questa riga.
                            if (!root.isSelected(cell.qsoId))
                                root.selectOnly(cell.row)
                        } else if (mouse.modifiers & Qt.ShiftModifier) {
                            root.selectRange(cell.row)
                        } else {
                            root.toggleRow(cell.row)
                        }
                        root.selectedRow = cell.row
                        decolog.lookupCall = root.model.callAt(cell.row)
                        if (mouse.button === Qt.RightButton)
                            rowMenu.popupFor(cell.qsoId)
                    }
                    onDoubleClicked: root.openQso(cell.qsoId)
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: root.model.count === 0
                text: root.model.filtered ? qsTr("No QSO matches the filters.")
                                          : qsTr("The log is empty. Log a QSO in Decodium, or import an ADIF file.")
                color: Theme.textSecondary
            }
        }
    }

    StyledMenu {
        id: rowMenu
        property var qsoId: 0
        function popupFor(id) { qsoId = id; popup() }
        StyledMenuItem { text: qsTr("Open / edit…"); onTriggered: root.openQso(rowMenu.qsoId) }
        // Cancellare un QSO si fa da dove lo si guarda, non solo dalla scheda:
        // e' morbida, la riga resta nello storico e si recupera.
        StyledMenuItem {
            readonly property int chosen: root.selectedIds.length
            text: chosen > 1 ? qsTr("Delete the %1 QSO selected…").arg(chosen) : qsTr("Delete QSO…")
            onTriggered: confirmRowDelete.openFor(chosen > 1 ? root.selectedIds : [rowMenu.qsoId])
        }
        // Il callbook sa nome, locatore e indirizzo: se al QSO mancano, glieli
        // mette adesso.
        StyledMenuItem {
            text: qsTr("Complete from the callbook")
            enabled: decolog.callbookProvider !== "off"
            onTriggered: decolog.completeQsoFromCallbook(rowMenu.qsoId)
        }
        StyledMenuItem { text: qsTr("Filter by this call"); onTriggered: root.model.filterText = decolog.lookupCall }
        StyledMenuItem {
            readonly property int dxcc: parseInt(root.model.valueAt(root.model.rowForId(rowMenu.qsoId), 9)) || 0
            enabled: dxcc > 0
            text: dxcc > 0 ? qsTr("Filter by entity: %1").arg(decolog.dxccName(dxcc) || dxcc) : qsTr("Filter by entity")
            onTriggered: root.model.dxccFilter = dxcc
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
        StyledMenuItem {
            text: qsTr("Paper QSL: queue for the bureau")
            onTriggered: decolog.cards.enqueue([rowMenu.qsoId], "B")
        }
        StyledMenuItem {
            text: qsTr("Paper QSL: queue as direct")
            onTriggered: decolog.cards.enqueue([rowMenu.qsoId], "D")
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
        StyledMenuItem { text: qsTr("Add tag…"); onTriggered: tagPopup.openFor([rowMenu.qsoId], true) }
        StyledMenu {
            id: removeTagMenu
            readonly property var tags: {
                const v = root.model.valueAt(root.model.rowForId(rowMenu.qsoId), 12)
                return v.length ? v.split(", ") : []
            }
            title: qsTr("Remove tag")
            enabled: tags.length > 0
            Repeater {
                model: removeTagMenu.tags
                StyledMenuItem {
                    required property string modelData
                    text: modelData
                    onTriggered: decolog.tagQsos([rowMenu.qsoId], modelData, false)
                }
            }
        }
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Down || event.key === Qt.Key_Up) {
            const next = Math.max(0, Math.min(root.model.count - 1, root.selectedRow + (event.key === Qt.Key_Down ? 1 : -1)))
            if (event.modifiers & Qt.ShiftModifier)
                root.selectRange(next)
            else
                root.selectOnly(next)
            root.selectedRow = next
            decolog.lookupCall = root.model.callAt(next)
            table.positionViewAtRow(next, TableView.Contain)
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            // Esc: la selezione se ne va, la riga corrente resta dov'e'.
            root.clearSelection()
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.openQso(root.model.idAt(root.selectedRow))
            event.accepted = true
        }
    }
}
