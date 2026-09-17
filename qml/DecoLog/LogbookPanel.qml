// DecoLog — il logbook: filtri a pillole, colonne scelte dall'operatore, una riga
// per QSO con gli stati QSL (L Q C E = LoTW, QRZ, Club Log, eQSL).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    readonly property var model: decolog.qsoModel
    readonly property var hidden: hiddenColumns.length ? hiddenColumns.split(",") : []

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
    function sourceColor(src) {
        return src === "udp" ? Theme.secondaryColor : src === "cld" ? Theme.warningColor : Theme.textSecondary
    }
    // Per le schermate di prova (--show menu:<nome>).
    function showMenu(name) {
        if (name === "columns") columnsMenu.popup(root.width - 260, Theme.panelHeight)
        else if (name === "filters") { addFilterMenu.popup(60, Theme.panelHeight + 30); bandMenu.open() }
        else if (name === "saved") savedMenu.popup(root.width - 200, Theme.panelHeight + 30)
        else if (name === "row") rowMenu.popupFor(root.model.idAt(0))
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
            model: 12
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
        StyledMenuItem {
            text: qsTr("This month")
            onTriggered: root.model.monthFilter = root.thisMonth()
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
                implicitHeight: Theme.rowHeight
                color: "transparent"
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
                    for (let c = 0; c < 12; ++c)
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

                readonly property bool selected: row === root.selectedRow

                implicitHeight: Theme.rowHeight
                clip: true
                color: selected ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.24)
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
                    font.family: cell.columnKey === "name" ? Qt.application.font.family : Theme.monoFamily
                    font.bold: cell.columnKey === "call"
                    color: cell.columnKey === "utc" || cell.columnKey === "dxcc" ? Theme.textSecondary
                         : cell.columnKey === "mode" ? root.modeColor(cell.display)
                         : cell.columnKey === "source" ? root.sourceColor(cell.display)
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
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        root.selectedRow = cell.row
                        root.forceActiveFocus()
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
        StyledMenuItem { text: qsTr("Filter by this call"); onTriggered: root.model.filterText = decolog.lookupCall }
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Down || event.key === Qt.Key_Up) {
            const next = Math.max(0, Math.min(root.model.count - 1, root.selectedRow + (event.key === Qt.Key_Down ? 1 : -1)))
            root.selectedRow = next
            decolog.lookupCall = root.model.callAt(next)
            table.positionViewAtRow(next, TableView.Contain)
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.openQso(root.model.idAt(root.selectedRow))
            event.accepted = true
        }
    }
}
