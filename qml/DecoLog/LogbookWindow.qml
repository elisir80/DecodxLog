// DecoLog — il logbook in una finestra a parte ("Pop"), per un secondo monitor.
// Stesso modello della finestra principale: filtri e colonne restano in comune.
import QtQuick
import QtQuick.Controls
import QtCore
import Decodium.UI

ApplicationWindow {
    id: root

    width: 1100
    height: 700
    visible: true
    title: qsTr("DecoLog — Logbook")
    color: Theme.bgDeep

    Settings {
        id: shared
        category: "layout"
        property string hiddenColumns: ""
        property var savedFilters: ({})
        property alias popWidth: root.width
        property alias popHeight: root.height
    }

    QsoDetailDialog { id: qsoDialog }

    LogbookPanel {
        anchors.fill: parent
        anchors.margins: 8
        showPopButton: false
        hiddenColumns: shared.hiddenColumns
        savedFilters: shared.savedFilters
        onHiddenColumnsEdited: (value) => shared.hiddenColumns = value
        onSavedFiltersEdited: (value) => shared.savedFilters = value
        onOpenQso: (id) => { if (id > 0) qsoDialog.openFor(id) }
    }
}
