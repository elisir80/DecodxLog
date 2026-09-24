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
// Ogni pannello ha la sua ✕, e dal menu Contest Mode in alto si riapre. Preso
// per la maniglia ⠿ e lasciato sopra un altro, i due si scambiano di posto.
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
        // Chi sta in quale casella: "casella:pannello", separati da virgola.
        property string places: ""
        property real leftWidth: 0
        property real rightWidth: 0
        property real entryHeight: 0
        property real bottomHeight: 0
        property real cwWidth: 0
        property real scoreHeight: 0
        property real rateHeight: 0
    }

    // Vuoto vuol dire "mai scelto": si parte con i pannelli di serie. Tutti
    // chiusi si scrive "-", se no chiudere l'ultimo li riapriva tutti.
    readonly property var shownList: {
        const out = []
        const parts = saved.shown === "-" ? []
                    : saved.shown.length > 0 ? saved.shown.split(",") : root.defaultKeys
        for (let i = 0; i < parts.length; ++i) {
            const k = String(parts[i]).trim()
            if (root.allKeys.indexOf(k) >= 0 && out.indexOf(k) < 0)
                out.push(k)
        }
        return out
    }
    function isShown(key) { return root.shownList.indexOf(key) >= 0 }

    // Le caselle stanno ferme; quello che cambia e' il pannello dentro.
    readonly property var slotIds: ["left", "entry", "log", "bottomA", "bottomB", "rightA", "rightB", "rightC"]
    readonly property var defaultPlaces: ({ left: "cluster", entry: "contest", log: "logbook",
                                            bottomA: "callinfo", bottomB: "cw",
                                            rightA: "score", rightB: "rate", rightC: "map" })
    readonly property var places: {
        const out = Object.assign({}, root.defaultPlaces)
        const used = {}
        for (const part of saved.places.split(",")) {
            const slotId = part.split(":")[0]
            const key = part.split(":")[1]
            if (out[slotId] !== undefined && root.allKeys.indexOf(key) >= 0 && !used[key]) {
                out[slotId] = key
                used[key] = true
            }
        }
        // Un pannello rimasto fuori (un salvataggio a meta') torna dove c'e'
        // un doppione: ogni pannello deve avere la sua casella.
        const seen = {}
        const missing = root.allKeys.filter(k => !Object.values(out).includes(k))
        for (const id of root.slotIds) {
            if (seen[out[id]] && missing.length > 0)
                out[id] = missing.shift()
            seen[out[id]] = true
        }
        return out
    }
    function keyAt(slotId) { return root.places[slotId] || "" }
    function slotShown(slotId) { return root.isShown(root.keyAt(slotId)) }
    function swapSlots(a, b) {
        if (a === b || !a || !b)
            return
        const out = Object.assign({}, root.places)
        const k = out[a]
        out[a] = out[b]
        out[b] = k
        saved.places = root.slotIds.map(id => id + ":" + out[id]).join(",")
    }

    // Per le prove: il pannello della casella `from` preso per la maniglia e
    // lasciato in mezzo alla casella `to`, come farebbe il mouse.
    function testDrag(from, to) {
        // La disposizione prende le misure un attimo dopo essere comparsa.
        testDragTimer.from = from
        testDragTimer.to = to
        testDragTimer.restart()
    }
    Timer {
        id: testDragTimer
        property string from
        property string to
        interval: 500
        onTriggered: root.doTestDrag(from, to)
    }
    function doTestDrag(from, to) {
        const all = { left: leftSlot, entry: entrySlot, log: logSlot, bottomA: callinfoSlot,
                      bottomB: cwSlot, rightA: scoreSlot, rightB: rateSlot, rightC: mapSlot }
        const target = all[to]
        const p = target.mapToGlobal(target.width / 2, target.height / 2)
        root.dragFrom = from
        root.swapSlots(root.dragFrom, root.slotUnder(p.x, p.y))
        root.dragFrom = ""
    }

    // Il trascinamento: da quale casella parte e su quale sta per cadere.
    property string dragFrom: ""
    property string dragTarget: ""
    function slotUnder(screenX, screenY) {
        const all = [leftSlot, entrySlot, logSlot, callinfoSlot, cwSlot, scoreSlot, rateSlot, mapSlot]
        for (const s of all) {
            if (!s.visible)
                continue
            const p = s.mapFromGlobal(screenX, screenY)
            if (p.x >= 0 && p.y >= 0 && p.x < s.width && p.y < s.height)
                return s.slotId
        }
        return ""
    }
    function setShown(key, on) {
        const list = root.shownList.slice()
        const i = list.indexOf(key)
        if (on && i < 0)
            list.push(key)
        else if (!on && i >= 0)
            list.splice(i, 1)
        saved.shown = list.length > 0 ? list.join(",") : "-"
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
        saved.places = ""
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
        if (cwSlot.visible && root.slotShown("bottomA")) saved.cwWidth = cwSlot.width
        if (scoreSlot.visible) saved.scoreHeight = scoreSlot.height
        if (rateSlot.visible) saved.rateHeight = rateSlot.height
    }

    // Una casella: il pannello, agganciato, con la sua ✕.
    component Slot: Item {
        id: slot
        property string slotId: ""
        readonly property string key: root.keyAt(slotId)
        readonly property Item item: loader.item
        // L'inserimento non si stringe mai sotto quanto serve a vedere lo
        // scambio e Registra, in qualunque casella stia.
        readonly property real needed: key === "contest" && item ? item.implicitHeight : 110
        visible: root.isShown(key)
        SplitView.minimumWidth: 220
        SplitView.minimumHeight: needed

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
            // La maniglia ⠿: si prende e si lascia sopra un'altra casella.
            function onMoveStarted() { root.dragFrom = slot.slotId; root.dragTarget = "" }
            function onMoveMoved(x, y) {
                const under = root.slotUnder(x, y)
                root.dragTarget = under !== root.dragFrom ? under : ""
            }
            function onMoveEnded(x, y) {
                const under = root.slotUnder(x, y)
                root.swapSlots(root.dragFrom, under)
                root.dragFrom = ""
                root.dragTarget = ""
            }
        }

        // Il magnete: dove il pannello atterra.
        Rectangle {
            anchors.fill: parent
            visible: root.dragTarget === slot.slotId
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
    }

    // I bordi fra i pannelli si vedono sempre, un filo: un bordo invisibile
    // non si trova, e i pannelli sembrano bloccati.
    component Handle: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        color: SplitHandle.pressed ? Theme.primaryColor
             : SplitHandle.hovered ? Theme.accentColor : "transparent"
        Rectangle {
            anchors.centerIn: parent
            width: parent.width > parent.height ? Math.min(40, parent.width) : 2
            height: parent.width > parent.height ? 2 : Math.min(40, parent.height)
            radius: 1
            color: Theme.glassBorder
            visible: !parent.SplitHandle.pressed && !parent.SplitHandle.hovered
        }
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
            slotId: "left"
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
            visible: root.slotShown("entry") || root.slotShown("log") || root.slotShown("bottomA") || root.slotShown("bottomB")
            handle: Handle {}
            onResizingChanged: if (!resizing) root.remember()

            Slot {
                id: entrySlot
                slotId: "entry"
                SplitView.preferredHeight: Math.max(needed, saved.entryHeight > 0 ? saved.entryHeight : root.height * 0.28)
            }
            Slot {
                id: logSlot
                slotId: "log"
                SplitView.fillHeight: true
            }
            SplitView {
                id: bottomRow
                orientation: Qt.Horizontal
                visible: root.slotShown("bottomA") || root.slotShown("bottomB")
                SplitView.preferredHeight: saved.bottomHeight > 0 ? saved.bottomHeight : root.height * 0.30
                SplitView.minimumHeight: 80
                handle: Handle {}
                onResizingChanged: if (!resizing) root.remember()
                Slot {
                    id: callinfoSlot
                    slotId: "bottomA"
                    SplitView.fillWidth: true
                }
                Slot {
                    id: cwSlot
                    slotId: "bottomB"
                    SplitView.preferredWidth: saved.cwWidth > 0 ? saved.cwWidth : bottomRow.width * 0.45
                }
            }
        }

        // A destra i conti: punteggio, ritmo, mappa.
        SplitView {
            id: rightColumn
            orientation: Qt.Vertical
            visible: root.slotShown("rightA") || root.slotShown("rightB") || root.slotShown("rightC")
            SplitView.preferredWidth: saved.rightWidth > 0 ? saved.rightWidth : root.width * 0.24
            SplitView.minimumWidth: 220
            handle: Handle {}
            onResizingChanged: if (!resizing) root.remember()

            Slot {
                id: scoreSlot
                slotId: "rightA"
                SplitView.preferredHeight: saved.scoreHeight > 0 ? saved.scoreHeight : root.height * 0.30
            }
            Slot {
                id: rateSlot
                slotId: "rightB"
                SplitView.preferredHeight: saved.rateHeight > 0 ? saved.rateHeight : root.height * 0.34
            }
            Slot {
                id: mapSlot
                slotId: "rightC"
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
