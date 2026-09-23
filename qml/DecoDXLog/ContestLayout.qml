// DecoDXLog — la modalita' contest dentro la finestra principale.
//
// In gara le finestre sono tante e devono stare tutte in vista: qui stanno
// agganciate nel corpo del programma, in tre colonne come nei programmi da gara
// — il cluster a sinistra, in mezzo l'inserimento, il log e la scheda del
// nominativo, a destra i conti e la mappa. Niente finestre che vanno sotto,
// che si perdono dietro a un'altra o che non si chiudono: i bordi fra un
// pannello e l'altro si trascinano, e la disposizione segue la finestra quando
// la si allarga o la si stringe.
//
// Ogni pannello ha la sua ✕, e dal menu Contest Mode in alto si riapre.
import QtQuick
import QtQuick.Controls
import QtCore
import Decodium.UI

Item {
    id: root

    // Quello che i pannelli chiedono e che sa fare solo la finestra principale.
    signal openQso(var id)
    signal awardRequested(string id)
    signal clusterRequested(int tab)
    signal statsRequested()
    signal rotorRequested()
    signal contestRequested()

    // I pannelli della gara, nell'ordine del menu.
    readonly property var allKeys: ["contest", "cluster", "logbook", "callinfo", "rate", "score", "map", "cw"]
    readonly property var defaultKeys: ["contest", "cluster", "logbook", "callinfo", "rate", "score", "map"]

    // Quali pannelli sono aperti, e le misure: restano da una gara all'altra.
    Settings {
        id: saved
        category: "layout/contest"
        property string shown: ""
        property real leftWidth: 0
        property real rightWidth: 0
        property real entryHeight: 0
        property real bottomHeight: 0
        property real cwWidth: 0
        property real scoreHeight: 0
        property real rateHeight: 0
    }

    readonly property var shownList: {
        const out = []
        const parts = saved.shown.length > 0 ? saved.shown.split(",") : root.defaultKeys
        for (let i = 0; i < parts.length; ++i) {
            const k = String(parts[i]).trim()
            if (root.allKeys.indexOf(k) >= 0 && out.indexOf(k) < 0)
                out.push(k)
        }
        return out
    }
    function isShown(key) { return root.shownList.indexOf(key) >= 0 }
    function setShown(key, on) {
        const list = root.shownList.slice()
        const i = list.indexOf(key)
        if (on && i < 0)
            list.push(key)
        else if (!on && i >= 0)
            list.splice(i, 1)
        saved.shown = list.join(",")
    }
    function toggle(key) { root.setShown(key, !root.isShown(key)) }
    // All'ingresso in una gara in telegrafia la CW si apre da sola.
    function prepare(cw) {
        if (cw)
            root.setShown("cw", true)
    }
    // Le misure di partenza, sulla finestra di adesso.
    function resetLayout() {
        saved.shown = root.defaultKeys.concat(decolog.activation.isCwContest() ? ["cw"] : []).join(",")
        saved.leftWidth = 0
        saved.rightWidth = 0
        saved.entryHeight = 0
        saved.bottomHeight = 0
        saved.cwWidth = 0
        saved.scoreHeight = 0
        saved.rateHeight = 0
    }
    // Le misure scelte trascinando i bordi si ricordano a bordo lasciato.
    function remember() {
        if (leftSlot.visible) saved.leftWidth = leftSlot.width
        if (rightColumn.visible) saved.rightWidth = rightColumn.width
        if (entrySlot.visible) saved.entryHeight = entrySlot.height
        if (bottomRow.visible) saved.bottomHeight = bottomRow.height
        if (cwSlot.visible && root.isShown("callinfo")) saved.cwWidth = cwSlot.width
        if (scoreSlot.visible) saved.scoreHeight = scoreSlot.height
        if (rateSlot.visible) saved.rateHeight = rateSlot.height
    }

    // Una casella: il pannello, agganciato, con la sua ✕.
    component Slot: Item {
        id: slot
        property string key: ""
        readonly property Item item: loader.item
        visible: root.isShown(key)
        SplitView.minimumWidth: 220
        SplitView.minimumHeight: 110

        function sourceOf(k) {
            switch (k) {
            case "contest":  return "ContestEntryPanel.qml"
            case "cluster":  return "ClusterPanel.qml"
            case "logbook":  return "LogbookPanel.qml"
            case "callinfo": return "CallInfoPanel.qml"
            case "rate":     return "ContestRatePanel.qml"
            case "score":    return "ContestScorePanel.qml"
            case "map":      return "MapPanel.qml"
            case "cw":       return "CwPanel.qml"
            }
            return ""
        }

        Loader {
            id: loader
            anchors.fill: parent
            active: slot.visible
            source: active ? slot.sourceOf(slot.key) : ""
            onLoaded: {
                // La chiave fa comparire la ✕; staccare qui non serve, e il
                // cluster in gara si fa essenziale.
                if (item.panelKey !== undefined)
                    item.panelKey = slot.key
                if (item.detachable !== undefined)
                    item.detachable = false
                if (item.dockable !== undefined)
                    item.dockable = false
                if (item.contestMode !== undefined)
                    item.contestMode = true
                if (item.showPopButton !== undefined)
                    item.showPopButton = false
            }
        }
        Connections {
            target: loader.item
            ignoreUnknownSignals: true
            function onCloseRequested() { root.setShown(slot.key, false) }
            function onOpenQso(id) { if (id > 0) root.openQso(id) }
            function onAwardRequested(id) { root.awardRequested(id) }
            function onDetailsRequested(id) { root.awardRequested(id) }
            function onStatsRequested() { root.statsRequested() }
            function onClusterRequested(tab) { root.clusterRequested(tab) }
            function onWindowRequested() { root.rotorRequested() }
            function onContestRequested() { root.contestRequested() }
        }
    }

    component Handle: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        color: SplitHandle.pressed ? Theme.primaryColor
             : SplitHandle.hovered ? Theme.glassBorder : "transparent"
    }

    SplitView {
        id: columns
        anchors.fill: parent
        anchors.margins: 6
        orientation: Qt.Horizontal
        handle: Handle {}
        // Su uno schermo basso quello che non ci sta si taglia al bordo,
        // invece di finire sopra la barra di stato.
        clip: true
        onResizingChanged: if (!resizing) root.remember()

        // A sinistra il cluster, alto quanto la finestra.
        Slot {
            id: leftSlot
            key: "cluster"
            SplitView.preferredWidth: saved.leftWidth > 0 ? saved.leftWidth : root.width * 0.24
        }

        // In mezzo il lavoro: l'inserimento, il log, e sotto la scheda del
        // nominativo e la CW.
        SplitView {
            id: middleColumn
            orientation: Qt.Vertical
            SplitView.fillWidth: true
            SplitView.minimumWidth: 320
            // Si guarda la lista, non il visible dei figli: quello di un figlio e'
            // falso anche solo perche' e' falso quello del padre, e la colonna
            // restava spenta per sempre.
            visible: root.isShown("contest") || root.isShown("logbook") || root.isShown("callinfo") || root.isShown("cw")
            handle: Handle {}
            onResizingChanged: if (!resizing) root.remember()

            Slot {
                id: entrySlot
                key: "contest"
                // Mai piu' bassa di quanto serve a vedere lo scambio e
                // Registra: su uno schermo piccolo i campi vanno a capo e la
                // casella si alza con loro.
                readonly property real needed: entrySlot.item ? entrySlot.item.implicitHeight : 170
                SplitView.preferredHeight: Math.max(needed, saved.entryHeight > 0 ? saved.entryHeight : root.height * 0.28)
                SplitView.minimumHeight: needed
            }
            Slot {
                id: logSlot
                key: "logbook"
                SplitView.fillHeight: true
                SplitView.minimumHeight: 80
            }
            SplitView {
                id: bottomRow
                orientation: Qt.Horizontal
                visible: root.isShown("callinfo") || root.isShown("cw")
                SplitView.preferredHeight: saved.bottomHeight > 0 ? saved.bottomHeight : root.height * 0.30
                SplitView.minimumHeight: 80
                handle: Handle {}
                onResizingChanged: if (!resizing) root.remember()
                Slot {
                    id: callinfoSlot
                    key: "callinfo"
                    SplitView.fillWidth: true
                }
                Slot {
                    id: cwSlot
                    key: "cw"
                    SplitView.preferredWidth: saved.cwWidth > 0 ? saved.cwWidth : bottomRow.width * 0.45
                }
            }
        }

        // A destra i conti: punteggio, ritmo, mappa.
        SplitView {
            id: rightColumn
            orientation: Qt.Vertical
            visible: root.isShown("score") || root.isShown("rate") || root.isShown("map")
            SplitView.preferredWidth: saved.rightWidth > 0 ? saved.rightWidth : root.width * 0.24
            SplitView.minimumWidth: 220
            handle: Handle {}
            onResizingChanged: if (!resizing) root.remember()

            Slot {
                id: scoreSlot
                key: "score"
                SplitView.preferredHeight: saved.scoreHeight > 0 ? saved.scoreHeight : root.height * 0.30
            }
            Slot {
                id: rateSlot
                key: "rate"
                SplitView.preferredHeight: saved.rateHeight > 0 ? saved.rateHeight : root.height * 0.34
            }
            Slot {
                id: mapSlot
                key: "map"
                SplitView.fillHeight: true
            }
        }
    }

    // Tutto chiuso: si dice come riaprire, invece di lasciare un vuoto.
    Text {
        anchors.centerIn: parent
        visible: root.shownList.length === 0
        text: qsTr("All the contest panels are closed: open them again from Contest Mode, up in the bar.")
        color: Theme.textSecondary
        font.pixelSize: 13
    }
}
