// DecoLog — una casella della disposizione.
//
// Le caselle stanno ferme (la colonna di sinistra, il centro, le quattro di
// destra, le due della fascia in basso): quello che cambia e' il pannello che
// ci sta dentro. Si prende un pannello per la maniglia ⠿, lo si lascia sopra un
// altro, e i due si scambiano di casella — come il layout DX-Pedition di
// Decodium. Le misure restano alla casella, non al pannello: chi va a sinistra
// prende la larghezza della colonna di sinistra.
//
// La casella e' un Item e non un Loader, anche se dentro ha un Loader: quello
// che si scrive dentro un Loader diventa il suo sourceComponent, e il riquadro
// del magnete sarebbe finito li' invece che sullo schermo.
import QtQuick
import Decodium.UI

Item {
    id: slot

    // La casella e chi ci sta adesso.
    property string slotId: ""
    property string panelKey: ""
    // Il pannello e' qui solo se non e' chiuso e non e' in finestra propria.
    property bool docked: true
    // Acceso mentre si trascina un pannello e questa casella e' quella sotto il
    // dito: il magnete che dice dove finirebbe.
    property bool highlighted: false

    readonly property Item item: holder.item

    signal menuRequested(string key, real screenX, real screenY)
    signal moveStarted(string key)
    signal moveMoved(string key, real screenX, real screenY)
    signal moveEnded(string key, real screenX, real screenY)
    signal detachRequested(string key)
    signal closeRequested(string key)
    signal openQsoRequested(var id)
    signal awardRequested(string id)
    signal clusterRequested(int tab)
    signal statsRequested()
    signal rotorRequested()
    signal expandRequested()
    signal popRequested(string key)

    // Quello che serve solo a qualche pannello: le colonne e i filtri del log,
    // la scheda aperta nelle linguette in basso. Passano di qui perche' la
    // casella non sa in anticipo chi ci finira' dentro.
    property string hiddenColumns: ""
    property string columnWidths: ""
    property var savedFilters: ({})
    signal hiddenColumnsEdited(string value)
    signal columnWidthsEdited(string value)
    signal savedFiltersEdited(var value)

    property int currentTab: -1
    function setTab(n) { if (holder.item && holder.item.currentTab !== undefined) holder.item.currentTab = n }
    function showMenu(name) { if (holder.item && holder.item.showMenu !== undefined) holder.item.showMenu(name) }
    function showSelection(rows, what) {
        if (holder.item && holder.item.showSelection !== undefined)
            holder.item.showSelection(rows, what)
    }
    function showModes() { if (holder.item && holder.item.showModes !== undefined) holder.item.showModes() }
    function showCombo() { if (holder.item && holder.item.showCombo !== undefined) holder.item.showCombo() }

    function sourceOf(key) {
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

    visible: holder.active
    // Chi si misura da solo (il rotore, il riquadro dei premi) detta l'altezza
    // della casella, come faceva quando stava li' scritto a mano.
    implicitWidth: holder.item ? holder.item.implicitWidth : 0
    implicitHeight: holder.item ? holder.item.implicitHeight : 0

    Loader {
        id: holder
        anchors.fill: parent
        active: slot.docked && slot.panelKey.length > 0
        source: active ? slot.sourceOf(slot.panelKey) : ""
        onLoaded: {
            if (item && item.panelKey !== undefined)
                item.panelKey = slot.panelKey
            if (item && item.currentTab !== undefined)
                slot.currentTab = item.currentTab
        }
    }

    // Le proprieta' che solo certi pannelli hanno: si legano quando ci sono.
    Binding {
        target: holder.item
        property: "hiddenColumns"
        value: slot.hiddenColumns
        when: holder.item !== null && holder.item.hiddenColumns !== undefined
        restoreMode: Binding.RestoreNone
    }
    Binding {
        target: holder.item
        property: "columnWidths"
        value: slot.columnWidths
        when: holder.item !== null && holder.item.columnWidths !== undefined
        restoreMode: Binding.RestoreNone
    }
    Binding {
        target: holder.item
        property: "savedFilters"
        value: slot.savedFilters
        when: holder.item !== null && holder.item.savedFilters !== undefined
        restoreMode: Binding.RestoreNone
    }

    // Il magnete: il riquadro acceso sulla casella dove il pannello atterra.
    Rectangle {
        anchors.fill: parent
        visible: slot.highlighted
        color: Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.20)
        border.color: Theme.primaryColor
        border.width: 3
        radius: 6
        z: 100

        Text {
            anchors.centerIn: parent
            text: qsTr("here")
            color: Theme.primaryColor
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontSize + 2
            font.bold: true
        }
    }

    // I pannelli hanno segnali diversi: si ascolta quello che c'e' e si passa
    // avanti a chi ospita la casella.
    Connections {
        target: holder.item
        ignoreUnknownSignals: true
        function onMenuRequested(x, y) { slot.menuRequested(slot.panelKey, x, y) }
        function onMoveStarted() { slot.moveStarted(slot.panelKey) }
        function onMoveMoved(x, y) { slot.moveMoved(slot.panelKey, x, y) }
        function onMoveEnded(x, y) { slot.moveEnded(slot.panelKey, x, y) }
        function onDetachRequested() { slot.detachRequested(slot.panelKey) }
        function onCloseRequested() { slot.closeRequested(slot.panelKey) }
        function onOpenQso(id) { slot.openQsoRequested(id) }
        function onAwardRequested(id) { slot.awardRequested(id) }
        function onDetailsRequested(id) { slot.awardRequested(id) }
        function onClusterRequested(tab) { slot.clusterRequested(tab) }
        function onStatsRequested() { slot.statsRequested() }
        function onWindowRequested() { slot.rotorRequested() }
        function onExpandRequested() { slot.expandRequested() }
        function onPopRequested() { slot.popRequested(slot.panelKey) }
        function onHiddenColumnsEdited(value) { slot.hiddenColumnsEdited(value) }
        function onColumnWidthsEdited(value) { slot.columnWidthsEdited(value) }
        function onSavedFiltersEdited(value) { slot.savedFiltersEdited(value) }
        function onCurrentTabChanged() { slot.currentTab = holder.item.currentTab }
    }
}
