// DecoDXLog — la finestra principale (mockup 1a).
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
    title: "DecoDXLog " + decolog.version
           + (decolog.stationProfiles.activeProfile.stationCallsign
              ? " — " + decolog.stationProfiles.activeProfile.stationCallsign : "")
    color: Theme.bgDeep
    font.pixelSize: Theme.fontSize

    // La X della finestra principale chiude DecoDXLog per davvero. Senza questo,
    // con un pannello in finestra propria il programma restava in piedi: Qt
    // aspetta che si chiuda l'ultima finestra, e quella staccata era ancora li'.
    onClosing: {
        window.quitting = true
        Qt.quit()
    }

    // Le impostazioni arrivate da un altro computer valgono subito: il tema si
    // ridipinge senza aspettare il riavvio.
    Connections {
        target: decolog.cloud
        function onSettingsApplied() { Theme.reload() }
    }

    // Se lo schermo dove stava non c'e' piu', si torna su questo.
    OnScreen { target: window }

    Settings {
        id: layout
        category: "layout"
        property alias windowWidth: window.width
        property alias windowHeight: window.height
        // Anche dove sta, non solo quanto e' grande: chi lavora con due monitor
        // mette DecoDXLog su quello di destra e vuole ritrovarlo li'. Le
        // finestre staccate la posizione se la ricordavano da sempre; questa,
        // che e' la principale, no — e si riapriva dove decideva Windows.
        property alias windowX: window.x
        property alias windowY: window.y
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
        // Il banco del contest si dispone da solo una volta sola: dopo comanda
        // chi ha spostato le finestre.
        property bool contestDeskArranged: false
        // La modalita' contest, e com'era la finestra principale prima di
        // entrarci: all'uscita si rimette tutto uguale.
        property bool contestModeOn: false
        property bool contestDeskPlaced: false
        property string preContestHidden: ""
        property string preContestDetached: ""
        property int preContestVisibility: 2
        property string contestDeskLayout: "columns"
        // Disposizione bloccata: le maniglie non si tirano e i pannelli non si
        // spostano. Si mette e si toglie col tasto destro sulla testata di un
        // pannello qualsiasi.
        property bool layoutLocked: false
        // Chi sta in quale casella: "casella=pannello", separati da virgola.
        // Vuoto vuol dire la disposizione di partenza.
        property string panelSlots: ""
    }

    readonly property real defaultLeftWidth: 300
    readonly property real defaultRightWidth: 300
    readonly property real defaultBottomHeight: 280
    readonly property real defaultClusterBottomHeight: 340
    readonly property real defaultMapWidth: 308

    // ── I pannelli: chi sono, dove stanno ───────────────────────────────────
    //
    // Ogni pannello ha una chiave. Con quella si sa come si chiama, da quale
    // file nasce quando lo si stacca, e se adesso e' agganciato, in finestra o
    // chiuso. Chiuso vuol dire chiuso davvero: lo spazio non resta vuoto.
    readonly property var panelKeys: ["newqso", "logbook", "callinfo", "cw", "rotor", "ft2", "tabs", "map",
                                      "contest", "score", "rate", "cluster", "desk"]
    // Gli ultimi quattro vivono solo in finestra: nel contest ognuno se li
    // mette dove vuole, e nella disposizione agganciata non hanno un posto.
    readonly property var windowOnlyPanels: ["contest", "score", "rate", "cluster", "desk"]
    function isWindowOnly(key) { return window.windowOnlyPanels.indexOf(key) >= 0 }

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
        case "contest":  return qsTr("Contest entry")
        case "score":    return qsTr("Score and multipliers")
        case "rate":     return qsTr("How it is going")
        case "cluster":  return qsTr("DX Cluster")
        case "desk":     return qsTr("Contest desk")
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
        case "contest":  return "ContestEntryPanel.qml"
        case "score":    return "ContestScorePanel.qml"
        case "rate":     return "ContestRatePanel.qml"
        case "cluster":  return "ClusterPanel.qml"
        case "desk":     return "ContestDeskPanel.qml"
        }
        return ""
    }

    // ── Le caselle della disposizione ───────────────────────────────────────
    //
    // Otto posti fissi; quale pannello ci stia dentro lo dice questa mappa, e
    // si cambia trascinando un pannello per la maniglia sopra un altro.
    readonly property var slotIds: ["left", "center", "rightA", "rightB", "rightC", "rightD",
                                    "bottomLeft", "bottomRight"]
    readonly property var defaultSlots: ({"left": "newqso", "center": "logbook",
                                          "rightA": "callinfo", "rightB": "cw",
                                          "rightC": "rotor", "rightD": "ft2",
                                          "bottomLeft": "tabs", "bottomRight": "map"})

    function slotMap() {
        const map = {}
        for (let i = 0; i < window.slotIds.length; ++i)
            map[window.slotIds[i]] = window.defaultSlots[window.slotIds[i]]
        const parts = String(layout.panelSlots || "").split(",")
        const seen = []
        for (let j = 0; j < parts.length; ++j) {
            const pair = parts[j].split("=")
            const slotId = String(pair[0] || "").trim()
            const key = String(pair[1] || "").trim()
            // Solo caselle e pannelli che esistono, e ogni pannello una volta
            // sola: una mappa storta lascerebbe un pannello in due posti.
            if (window.slotIds.indexOf(slotId) >= 0 && window.panelKeys.indexOf(key) >= 0
                && seen.indexOf(key) < 0) {
                map[slotId] = key
                seen.push(key)
            }
        }
        // Quello che nella mappa non c'e' finisce nella prima casella libera.
        const used = []
        for (let s = 0; s < window.slotIds.length; ++s)
            used.push(map[window.slotIds[s]])
        for (let k = 0; k < window.panelKeys.length; ++k) {
            const key = window.panelKeys[k]
            if (used.indexOf(key) >= 0)
                continue
            for (let s2 = 0; s2 < window.slotIds.length; ++s2) {
                if (used.indexOf(map[window.slotIds[s2]]) !== s2) {
                    map[window.slotIds[s2]] = key
                    used[s2] = key
                    break
                }
            }
        }
        return map
    }

    readonly property var slotsNow: window.slotMap()
    function panelAt(slotId) { return window.slotsNow[slotId] || "" }
    function slotOf(key) {
        for (let i = 0; i < window.slotIds.length; ++i) {
            if (window.slotsNow[window.slotIds[i]] === key)
                return window.slotIds[i]
        }
        return ""
    }

    function swapSlots(slotA, slotB) {
        if (!slotA || !slotB || slotA === slotB)
            return
        const map = window.slotMap()
        const keep = map[slotA]
        map[slotA] = map[slotB]
        map[slotB] = keep
        const out = []
        for (let i = 0; i < window.slotIds.length; ++i)
            out.push(window.slotIds[i] + "=" + map[window.slotIds[i]])
        layout.panelSlots = out.join(",")
    }

    // ── Il trascinamento: si prende un pannello e si vede dove finisce ──────
    property string draggingKey: ""
    property string draggingFrom: ""
    property string dragTargetSlot: ""
    readonly property var slotItems: [slotLeft, slotCenter, slotRightA, slotRightB,
                                      slotRightC, slotRightD, slotBottomLeft, slotBottomRight]

    function panelItem(key) {
        for (let i = 0; i < window.slotItems.length; ++i) {
            if (window.slotItems[i].panelKey === key)
                return window.slotItems[i]
        }
        return null
    }
    readonly property int tabsTab: {
        const it = window.panelItem("tabs")
        return it ? it.currentTab : -1
    }

    function slotUnder(screenX, screenY) {
        for (let i = 0; i < window.slotItems.length; ++i) {
            const s = window.slotItems[i]
            if (!s.visible || s.width <= 0 || s.height <= 0)
                continue
            const at = s.mapToGlobal(0, 0)
            if (screenX >= at.x && screenX <= at.x + s.width
                && screenY >= at.y && screenY <= at.y + s.height)
                return s.slotId
        }
        return ""
    }

    function beginPanelDrag(key, slotId) {
        if (layout.layoutLocked)
            return
        window.draggingKey = key
        window.draggingFrom = slotId
        window.dragTargetSlot = ""
    }
    function updatePanelDrag(screenX, screenY) {
        if (!window.draggingKey)
            return
        const over = window.slotUnder(screenX, screenY)
        window.dragTargetSlot = over === window.draggingFrom ? "" : over
    }
    function endPanelDrag(screenX, screenY) {
        if (!window.draggingKey)
            return
        const over = window.slotUnder(screenX, screenY)
        if (over && over !== window.draggingFrom)
            window.swapSlots(window.draggingFrom, over)
        window.draggingKey = ""
        window.draggingFrom = ""
        window.dragTargetSlot = ""
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
    function panelShows(key) {
        return window.isPanelDocked(key)
    }
    function panelState(key) {
        return window.isPanelHidden(key) ? qsTr("closed")
             : window.isPanelDetached(key) ? qsTr("window") : qsTr("docked")
    }

    // Queste quattro leggono sempre layout.hiddenPanels / layout.detachedPanels,
    // mai le liste calcolate qui sopra: una property che dipende da un'altra si
    // rifa' quando le pare, e due chiamate di fila — stacca questo, stacca
    // quello — leggevano ancora la lista di prima e si cancellavano a vicenda.
    // Il pannello staccato per primo spariva dall'elenco con la finestra
    // ancora aperta, e da li' venivano le finestre orfane.
    function showPanel(key) {
        layout.hiddenPanels = window.panelListOf(layout.hiddenPanels)
                                    .filter(function (k) { return k !== key }).join(",")
    }
    function closePanel(key) {
        // Chiuso e' chiuso: se era in finestra, la finestra sparisce.
        layout.detachedPanels = window.panelListOf(layout.detachedPanels)
                                      .filter(function (k) { return k !== key }).join(",")
        if (key === "rotor")
            rotorWindow.active = false
        const hidden = window.panelListOf(layout.hiddenPanels)
        if (hidden.indexOf(key) < 0)
            layout.hiddenPanels = hidden.concat([key]).join(",")
    }
    function detachPanel(key) {
        window.showPanel(key)
        const detached = window.panelListOf(layout.detachedPanels)
        if (detached.indexOf(key) < 0)
            layout.detachedPanels = detached.concat([key]).join(",")
        if (key === "rotor")
            window.openRotor()
    }
    function attachPanel(key) {
        layout.detachedPanels = window.panelListOf(layout.detachedPanels)
                                      .filter(function (k) { return k !== key }).join(",")
        if (key === "rotor")
            rotorWindow.active = false
        window.showPanel(key)
    }
    function togglePanel(key) {
        if (window.isPanelHidden(key)) window.showPanel(key)
        else window.closePanel(key)
    }
    function resetPanels() {
        layout.panelSlots = ""
        layout.hiddenPanels = ""
        layout.detachedPanels = ""
        layout.layoutLocked = false
        layout.leftWidth = window.defaultLeftWidth
        layout.rightWidth = window.defaultRightWidth
        layout.bottomHeight = window.defaultBottomHeight
        layout.clusterBottomHeight = window.defaultClusterBottomHeight
        layout.mapWidth = window.defaultMapWidth
        window.draggingKey = ""
        window.draggingFrom = ""
        window.dragTargetSlot = ""
        window.syncDetachedWindows()
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
    // Il banco del contest: ogni cosa in una finestra sua, che si sposta e si
    // ridimensiona come si vuole — anche su un altro monitor. Sono le stesse
    // finestre dei pannelli staccati, quindi misura e posizione restano dove le
    // si mette, e la volta dopo si riaprono li'.
    //
    // La CW entra solo se il contest e' in telegrafia: in SSB e' una finestra
    // da spostare e basta.
    readonly property var contestDeskPanels: ["desk", "contest", "cluster", "logbook", "callinfo",
                                              "rate", "score", "map"]
    // La modalita' contest, come nei programmi da gara: la finestra principale
    // smette di essere il log di tutti i giorni — niente pannello del nuovo QSO,
    // niente log, niente schede di diplomi, statistiche e propagazione — e
    // diventa la base su cui stanno le finestre della gara, tenute insieme a lei
    // e sempre davanti. Si esce dalla pulsantiera, o chiudendo la sessione, e
    // la finestra principale torna com'era.
    readonly property bool contestModeOn: layout.contestModeOn
    // Davanti a tutto: si toglie dalla pulsantiera, per chi lavora con un
    // altro programma accanto.
    property bool contestOnTop: true
    function isContestDeskKey(key) {
        return window.contestDeskPanels.indexOf(key) >= 0 || key === "cw"
    }

    function openContestDesk() {
        if (!layout.contestModeOn) {
            // Com'era prima: la si rimette uguale quando la gara finisce.
            layout.preContestHidden = layout.hiddenPanels
            layout.preContestDetached = layout.detachedPanels
            layout.preContestVisibility = window.visibility
            layout.contestModeOn = true
        }
        const wanted = window.contestDeskPanels.slice()
        if (decolog.activation.isCwContest())
            wanted.push("cw")
        for (let i = 0; i < wanted.length; ++i)
            window.detachPanel(wanted[i])
        // La base occupa lo schermo: le finestre della gara ci stanno sopra.
        if (window.visibility !== Window.Maximized && window.visibility !== Window.FullScreen)
            window.showMaximized()
        // La prima volta si mettono in ordine sulla base; dopo comanda chi le
        // ha spostate.
        if (!layout.contestDeskPlaced) {
            deskPlaceTimer.restart()
            layout.contestDeskPlaced = true
        }
        Qt.callLater(function () { window.raisePanel("contest") })
    }

    function exitContestMode() {
        if (!layout.contestModeOn)
            return
        layout.contestModeOn = false
        // Le finestre della gara si chiudono, e la finestra principale torna
        // come l'operatore l'aveva lasciata.
        layout.detachedPanels = layout.preContestDetached
        layout.hiddenPanels = layout.preContestHidden
        if (layout.preContestVisibility === Window.Windowed)
            window.showNormal()
    }

    // La finestra massimizzata ci mette un attimo a prendere la misura: le
    // finestre si dispongono dopo, sulla misura vera.
    Timer { id: exitProbe; interval: 900; onTriggered: window.exitContestMode() }
    Timer {
        id: deskPlaceTimer
        interval: 350
        onTriggered: window.arrangeContestDesk(layout.contestDeskLayout || "columns")
    }

    // Il programma chiuso in gara si riapre in gara; ma se la sessione nel
    // frattempo non c'e' piu', la finestra principale torna com'era.
    Timer {
        running: true
        interval: 500
        onTriggered: if (layout.contestModeOn && !decolog.activation.active) window.exitContestMode()
    }

    // Chiusa la sessione, finita la gara: si esce anche dalla modalita'.
    Connections {
        target: decolog.activation
        function onChanged() {
            if (layout.contestModeOn && !decolog.activation.active)
                window.exitContestMode()
        }
    }

    // Il comando che arriva dalla pulsantiera del banco.
    function deskDo(what, arg) {
        if (what === "toggle") {
            if (window.isPanelDetached(arg))
                window.closePanel(arg)
            else
                window.detachPanel(arg)
        } else if (what === "arrange") {
            layout.contestDeskLayout = arg
            window.arrangeContestDesk(arg)
        } else if (what === "ontop") {
            window.contestOnTop = arg === "1"
            if (window.contestOnTop)
                Qt.callLater(window.raiseContestDesk)
        } else if (what === "exit") {
            window.exitContestMode()
        } else if (what === "export") {
            window.openContest()
        } else if (what === "submit") {
            submitDialog.openDialog()
        }
    }

    function raiseContestDesk() {
        const keys = window.contestDeskPanels.concat(["cw"])
        for (let i = 0; i < keys.length; ++i) {
            const win = window.panelWindowFor(keys[i])
            if (win)
                win.raise()
        }
        window.raisePanel("contest")
    }

    // Le disposizioni: tre modi di mettere le stesse finestre, presi da come si
    // lavora davvero in gara. Nessuna e' giusta per tutti — per questo si
    // cambia con un pulsante, e quello che si sposta a mano resta dov'e'.
    function arrangeContestDesk(which) {
        const screen = window.screen
        if (!screen)
            return
        // La base e' la finestra principale sotto la sua barra: le finestre
        // della gara ci stanno sopra, raggruppate, invece che sparse.
        const X0 = window.x
        const Y0 = window.y + topBar.height
        const W = window.width
        const H = window.height - topBar.height
        const place = function (key, x, y, w, h) {
            const win = window.panelWindowFor(key)
            if (!win)
                return
            win.x = Math.round(X0 + x)
            win.y = Math.round(Y0 + y)
            // Sotto una certa misura una finestra non mostra piu' niente:
            // meglio che esca dal bordo che darla vuota.
            win.width = Math.round(Math.max(w, 360))
            win.height = Math.round(Math.max(h, key === "contest" ? 250
                                                : key === "desk" ? 240 : 190))
        }

        if (which === "centred") {
            // L'inserimento largo in mezzo, con il log sotto: gli occhi stanno
            // li' e non si spostano. Conti e cluster ai lati.
            const side = Math.round(W * 0.24)
            const middle = W - side * 2
            place("cluster", 0, 0, side, H)
            place("desk", W - side, H - Math.round(H * 0.22), side, Math.round(H * 0.22))
            place("contest", side, 0, middle, Math.round(H * 0.34))
            place("logbook", side, Math.round(H * 0.34), middle, Math.round(H * 0.38))
            place("callinfo", side, Math.round(H * 0.72), Math.round(middle / 2), Math.round(H * 0.28))
            place("cw", side + Math.round(middle / 2), Math.round(H * 0.72),
                  Math.round(middle / 2), Math.round(H * 0.28))
            place("score", W - side, 0, side, Math.round(H * 0.34))
            place("rate", W - side, Math.round(H * 0.34), side, Math.round(H * 0.24))
            place("map", W - side, Math.round(H * 0.58), side, Math.round(H * 0.20))
            return
        }

        if (which === "two") {
            // Due schermi: il lavoro su quello davanti, quello che si guarda di
            // sfuggita sull'altro. Se di schermo ce n'e' uno solo, si ricade
            // sulle colonne invece di mandare meta' banco nel nulla.
            const screens = Qt.application.screens
            if (!screens || screens.length < 2) {
                window.arrangeContestDesk("columns")
                return
            }
            const second = screens[0] === screen ? screens[1] : screens[0]
            const put = function (key, x, y, w, h) {
                const win = window.panelWindowFor(key)
                if (!win)
                    return
                win.x = Math.round(x)
                win.y = Math.round(y)
                win.width = Math.round(Math.max(w, 360))
                win.height = Math.round(Math.max(h, 190))
            }
            place("contest", 0, 0, W, Math.round(H * 0.32))
            place("logbook", 0, Math.round(H * 0.32), Math.round(W * 0.62), Math.round(H * 0.44))
            place("cluster", Math.round(W * 0.62), Math.round(H * 0.32), Math.round(W * 0.38),
                  Math.round(H * 0.68))
            place("callinfo", 0, Math.round(H * 0.76), Math.round(W * 0.62), Math.round(H * 0.24))
            const X = second.virtualX, Y = second.virtualY
            const SW = second.desktopAvailableWidth, SH = second.desktopAvailableHeight
            put("score", X, Y, Math.round(SW / 2), Math.round(SH * 0.5))
            put("rate", X + Math.round(SW / 2), Y, Math.round(SW / 2), Math.round(SH * 0.5))
            put("map", X, Y + Math.round(SH * 0.5), Math.round(SW / 2), Math.round(SH * 0.5))
            put("cw", X + Math.round(SW / 2), Y + Math.round(SH * 0.5), Math.round(SW / 2),
                Math.round(SH * 0.35))
            put("desk", X + Math.round(SW / 2), Y + Math.round(SH * 0.85), Math.round(SW / 2),
                Math.round(SH * 0.15))
            return
        }

        // "columns": tre colonne — cluster, lavoro, conti. Regge uno schermo
        // solo, che e' il caso di quasi tutti.
        const left = Math.round(W * 0.21)
        const right = Math.round(W * 0.23)
        const middle = W - left - right
        place("cluster", 0, 0, left, H)
        place("contest", left, 0, middle, Math.round(H * 0.30))
        place("logbook", left, Math.round(H * 0.30), middle, Math.round(H * 0.42))
        place("callinfo", left, Math.round(H * 0.72), Math.round(middle / 2), Math.round(H * 0.28))
        place("cw", left + Math.round(middle / 2), Math.round(H * 0.72),
              Math.round(middle / 2), Math.round(H * 0.28))
        place("score", W - right, 0, right, Math.round(H * 0.36))
        place("rate", W - right, Math.round(H * 0.36), right, Math.round(H * 0.26))
        place("map", W - right, Math.round(H * 0.62), right, Math.round(H * 0.20))
        place("desk", W - right, Math.round(H * 0.82), right, Math.round(H * 0.18))
    }

    function openRotor() {
        rotorWindow.active = true
        if (rotorWindow.item) {
            rotorWindow.item.raise()
            rotorWindow.item.requestActivate()
        }
    }
    function openCards(view) {
        cardsWindow.active = true
        if (cardsWindow.item) {
            if (view)
                cardsWindow.item.view = view
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

    // Lo spot finto entra nel modello un attimo dopo: si aspetta, poi si fa il
    // doppio clic sulla prima riga.
    Timer {
        id: clusterTuneTimer
        interval: 600
        onTriggered: {
            const model = decolog.cluster.spots
            if (model && model.count > 0)
                decolog.cluster.tune(model.get(0).spotKey)
            window.panelItem("tabs").setTab(3)
        }
    }

    Component.onCompleted: {
        const what = startupShow.split(":")
        if (what[0] === "contestdesk") window.openContestDesk()
        // Per le prove: si entra e si esce, e la finestra principale deve
        // tornare com'era.
        else if (what[0] === "contestexit") { window.openContestDesk(); exitProbe.start() }
        // Per misurare: apre il banco e registra N QSO di fila, dicendo quanto
        // ci mette ognuno. Un QSO in gara deve essere istantaneo.
        else if (what[0] === "benchswitch") { window.openContestDesk(); switchTimer.left = parseInt(what[1] || "20"); switchTimer.start() }
        else if (what[0] === "benchqso") { window.openContestDesk(); benchTimer.left = parseInt(what[1] || "5"); benchTimer.start() }
        else if (what[0] === "new") newQsoDialog.open()
        else if (what[0] === "qso") { openQso(parseInt(what[1])); if (what[2]) qsoDialog.currentTab = parseInt(what[2]) }
        else if (what[0] === "profiles") profilesDialog.open()
        else if (what[0] === "setup") { setupDialog.page = parseInt(what[1] || "3"); setupDialog.open(); if (what[2] === "end") Qt.callLater(setupDialog.scrollToBottom) }
        else if (what[0] === "menu") window.panelItem("logbook").showMenu(what[1])
        else if (what[0] === "select") window.panelItem("logbook").showSelection(what[1], what[2])
        // Lavori di manutenzione, utili anche da riga di comando.
        else if (what[0] === "combo") { window.showPanel("cw"); comboTimer.start() }
        else if (what[0] === "combo2") { newQsoDialog.open(); combo2Timer.start() }
        else if (what[0] === "cwsend") { window.showPanel("cw"); cwSendTimer.start() }
        else if (what[0] === "radioprobe") { window.panelItem("tabs").setTab(3); decolog.rig.probeRadio() }
        // Il VFO della barra: "tune:14.074" o "tune:14.074:FT8"; "modes" apre
        // l'elenco dei modi, per guardarlo.
        else if (what[0] === "tune") { window.panelItem("tabs").setTab(3)
                                      tuneTimer.mhz = parseFloat(what[1]); tuneTimer.mode = what[2] || ""
                                      tuneTimer.start() }
        else if (what[0] === "modes") topBar.openModeMenu()
        else if (what[0] === "repair") decolog.repairImportedFields()
        else if (what[0] === "fillall") decolog.completeMissingFromCallbook()
        else if (what[0] === "maintenance") { decolog.repairImportedFields(); decolog.completeMissingFromCallbook() }
        else if (what[0] === "tab") window.panelItem("tabs").setTab(parseInt(what[1]))
        else if (what[0] === "pop") popWindow.active = true
        else if (what[0] === "panels") { if (what[1]) { const how = what.slice(2); for (let i = 0; i < how.length; ++i) { if (what[1] === "close") window.closePanel(how[i]); else if (what[1] === "detach") window.detachPanel(how[i]); else if (what[1] === "show") window.showPanel(how[i]); else if (what[1] === "attach") window.attachPanel(how[i]) } } else panelsPopup.open() }
        // "cluster:spot:14025.1:3Y0J:CW" mette una riga come se venisse da un
        // nodo e ci fa sopra il doppio clic: serve a guardare se la radio ci va.
        else if (what[0] === "cluster" && what[1] === "spot") {
            const now = new Date()
            const hhmm = ("0" + now.getUTCHours()).slice(-2) + ("0" + now.getUTCMinutes()).slice(-2)
            decolog.cluster.injectLine("DX de IK0TEST:  " + what[2] + "  " + what[3]
                                       + "  " + (what[4] || "CW") + " 18 dB  " + hhmm + "Z")
            clusterTuneTimer.start()
        }
        else if (what[0] === "cluster") openCluster(parseInt(what[1] || "0"))
        else if (what[0] === "logs") logsDialog.openDialog()
        else if (what[0] === "activation") { activationDialog.openDialog(what[1] || ""); if (what[2] === "choose") Qt.callLater(activationDialog.chooseContest) }
        else if (what[0] === "modes") window.panelItem("newqso").showModes()
        // Per le prove: apre tutte le finestre due volte di fila. Due volte
        // perche' il guaio da cercare e' proprio quello — la finestra che si
        // sdoppia invece di venire in primo piano.
        else if (what[0] === "windows") {
            for (let round = 0; round < 2; ++round) {
                openStats()
                openCluster(0)
                openCards()
                openContest()
                openRotor()
                popWindow.active = true
                window.detachPanel("callinfo")
                window.detachPanel("ft2")
                window.detachPanel("map")
            }
        }
        else if (what[0] === "swap") window.swapSlots(what[1], what[2])
        else if (what[0] === "dragging") {
            dragDropTimer.holdOn = true
            dragDropTimer.from = what[1]
            dragDropTimer.to = what[2]
            dragDropTimer.start()
        }
        else if (what[0] === "drag") {
            // Le misure delle caselle arrivano quando la disposizione e' fatta:
            // prima di allora mapToGlobal risponde a caso.
            dragDropTimer.from = what[1]
            dragDropTimer.to = what[2]
            dragDropTimer.start()
        }
        else if (what[0] === "lock") layout.layoutLocked = what[1] !== "off"
        else if (what[0] === "layoutmenu") layoutMenu.openAt(what[1] || "logbook", 420, 300)
        // "lookup:JA1ZZZ": la scheda del nominativo, con la griglia banda x modo.
        else if (what[0] === "lookup") { if (what[2] === "detach") window.detachPanel("callinfo"); else window.showPanel("callinfo"); decolog.lookupCall = what[1] || "" }
        else if (what[0] === "about") aboutDialog.open()
        else if (what[0] === "update") updateDialog.open()
        else if (what[0] === "updatecheck") { window.panelItem("tabs").setTab(3); decolog.updates.checkNow() }
        else if (what[0] === "updateget") { decolog.updates.checkNow(); updateGetTimer.start() }
        else if (what[0] === "mainmenu") topBar.openMainMenu()
        else if (what[0] === "stats") openStats()
        else if (what[0] === "cards") { openCards(what[1])
                                       if (what[2] === "menu" && cardsWindow.item)
                                           cardsWindow.item.showCardMenu()
                                       else if (what[2] === "standard")
                                           decolog.cards.addStandardCardFields() }
        else if (what[0] === "cloud") {
            // cloud:signup:CALL:PASSWORD · cloud:login:CALL:PASSWORD · cloud:sync
            if (what[1] === "signup") decolog.cloud.signup(what[2], what[3])
            else if (what[1] === "login") decolog.cloud.login(what[2], what[3])
            else if (what[1] === "purge") decolog.cloud.purgeCloud("DELETE")
            else if (what[1] === "loginpurge") { decolog.cloud.login(what[2], what[3]); purgeAfterLogin.start() }
            else decolog.cloud.syncNow()
            window.panelItem("tabs").setTab(3)
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

    // Il rilascio arriva dopo, cosi' nella schermata si vede il magnete acceso
    // sulla casella dove il pannello sta per atterrare.
    Timer {
        id: dragDropTimer
        property string from: ""
        property string to: ""
        property bool holdOn: false
        property bool released: false
        interval: released ? 1400 : 700
        repeat: true
        onTriggered: {
            const a = window.slotItems.find(s => s.slotId === dragDropTimer.from)
            const b = window.slotItems.find(s => s.slotId === dragDropTimer.to)
            if (!a || !b) {
                stop()
                return
            }
            const end = b.mapToGlobal(b.width / 2, b.height / 2)
            if (!released) {
                // "dragging" tiene il pannello in mano: serve per fotografare
                // il magnete acceso sulla casella di arrivo.
                if (dragDropTimer.holdOn) {
                    window.beginPanelDrag(a.panelKey, a.slotId)
                    window.updatePanelDrag(end.x, end.y)
                    stop()
                    return
                }
                window.beginPanelDrag(a.panelKey, a.slotId)
                window.updatePanelDrag(end.x, end.y)
                released = true
                restart()
                return
            }
            window.endPanelDrag(end.x, end.y)
            stop()
        }
    }

    Timer { id: comboTimer; interval: 800
           onTriggered: { const it = window.panelItem("cw"); if (it) it.showCombo() } }
    Timer { id: combo2Timer; interval: 900; onTriggered: newQsoDialog.showBandCombo() }
    Timer { id: cwSendTimer; interval: 1200; onTriggered: decolog.rig.sendMacro(0, {}) }
    // Prova dell'aggiornamento: si aspetta la risposta, poi si scarica.
    Timer { id: updateGetTimer; interval: 2500; onTriggered: decolog.updates.downloadAndInstall() }
    // La radio ci mette un attimo a rispondere: la prova aspetta che ci sia.
    Timer { id: tuneTimer; interval: 1500
           property real mhz: 0
           property string mode: ""
           onTriggered: decolog.tuneTo(mhz, mode) }

    NewQsoDialog { id: newQsoDialog }
    QsoDetailDialog { id: qsoDialog }
    StationProfilesDialog { id: profilesDialog }
    AboutDialog { id: aboutDialog }
    UpdateDialog { id: updateDialog }

    // Quando il controllo trova una versione nuova la finestra si apre da se':
    // e' l'unico momento in cui l'aggiornamento si fa vedere.
    Connections {
        target: decolog.updates
        function onUpdateFound(version) { updateDialog.open() }
    }

    SetupDialog { id: setupDialog; onUpdateRequested: updateDialog.open() }
    ActivationDialog { id: activationDialog }
    ContestSubmitDialog { id: submitDialog }
    Timer {
        id: benchTimer
        property int left: 0
        property int n: 0
        interval: 2500
        repeat: true
        onTriggered: {
            if (left <= 0) { stop(); return }
            left--; n++
            const t0 = Date.now()
            const now = decolog.utcNow()
            const err = decolog.logManualQso({ call: "B" + (Date.now() % 100000) + "X" + n, date: now.date, time: now.time,
                                               band: "20m", mode: "CW", rst_sent: "599", rst_rcvd: "599",
                                               srx: String(10 + n % 30) })
            console.warn("BENCH qso " + n + ": " + (Date.now() - t0) + " ms " + err
                         + " | scatto piu' lungo dal QSO prima: " + benchWatch.worst + " ms")
            benchWatch.worst = 0
        }
    }
    // Passa da una finestra del banco all'altra, come fa chi opera, e dice
    // quanto ci mette.
    Timer {
        id: switchTimer
        property int left: 0
        property int n: 0
        readonly property var order: ["contest", "cluster", "logbook", "callinfo", "score", "rate", "map", "desk"]
        interval: 400
        repeat: true
        onTriggered: {
            if (left <= 0) { stop(); return }
            left--; n++
            const key = order[n % order.length]
            const t0 = Date.now()
            window.raisePanel(key)
            console.warn("BENCH switch " + key + ": " + (Date.now() - t0) + " ms | scatto piu' lungo: "
                         + benchWatch.worst + " ms")
            benchWatch.worst = 0
        }
    }
    // Misura la fluidita': un battito ogni 16 ms, e il buco piu' lungo fra due
    // battiti e' quanto il programma e' rimasto fermo.
    Timer {
        id: benchWatch
        property double last: 0
        property int worst: 0
        interval: 16
        repeat: true
        running: benchTimer.running || switchTimer.running
        onTriggered: {
            const now = Date.now()
            if (last > 0)
                worst = Math.max(worst, now - last)
            last = now
        }
    }
    LogsDialog {
        id: logsDialog
        // All'avvio si chiede quale log aprire, per chi tiene un log per ogni
        // contest. Di serie no: chi ne ha uno solo non deve rispondere a niente.
        Component.onCompleted: if (decolog.logs.askAtStart) Qt.callLater(logsDialog.openDialog)
    }
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

    // Queste finestre nascono quando servono e muoiono quando si chiudono. Il
    // Loader si spegne con Qt.callLater e non dentro l'onClosing: spegnerlo li'
    // vuol dire distruggere la finestra mentre sta ancora chiudendosi, ed e' il
    // genere di cosa che fa cadere il programma invece di chiudere una finestra.
    Loader {
        id: statsWindow
        active: false
        sourceComponent: StatsWindow {
            onClosing: Qt.callLater(function () { statsWindow.active = false })
        }
    }

    Loader {
        id: rotorWindow
        active: false
        sourceComponent: RotorWindow {
            onClosing: Qt.callLater(function () {
                if (!window.quitting && window.isPanelDetached("rotor"))
                    window.attachPanel("rotor")
                rotorWindow.active = false
            })
        }
    }

    Loader {
        id: contestWindow
        active: false
        sourceComponent: ContestWindow {
            onClosing: Qt.callLater(function () { contestWindow.active = false })
        }
    }

    Loader {
        id: cardsWindow
        active: false
        sourceComponent: QslCardsWindow {
            onClosing: Qt.callLater(function () { cardsWindow.active = false })
        }
    }

    Loader {
        id: clusterWindow
        property int tab: 0
        property bool contestMode: false
        active: false
        sourceComponent: ClusterWindow {
            tab: clusterWindow.tab
            contestMode: clusterWindow.contestMode
            onClosing: Qt.callLater(function () { clusterWindow.active = false })
        }
    }

    Loader {
        id: popWindow
        active: false
        sourceComponent: LogbookWindow {
            onClosing: Qt.callLater(function () { popWindow.active = false })
        }
    }

    // Una finestra per ogni pannello staccato: nasce quando si stacca, muore
    // quando si riaggancia o si chiude. Il modello e' un ListModel tenuto in
    // pari riga per riga, non la lista calcolata: con una lista JS ogni
    // cambiamento faceva rinascere *tutte* le finestre — quelle gia' aperte
    // sparivano e tornavano altrove, svuotate di quello che avevano dentro.
    // Quando si chiude il programma le finestre staccate si chiudono anche
    // loro, ma quella non e' una richiesta di riagganciare: senza questo, uscire
    // da DecoDXLog riportava dentro tutti i pannelli e la volta dopo li si
    // ritrovava nella finestra principale.
    property bool quitting: false
    Connections {
        target: Qt.application
        function onAboutToQuit() { window.quitting = true }
    }

    ListModel {
        id: detachedModel
        // All'avvio le finestre staccate sono quelle dell'ultima volta.
        Component.onCompleted: window.syncDetachedWindows()
    }

    function syncDetachedWindows() {
        const wanted = window.panelListOf(layout.detachedPanels)
                             .filter(function (key) { return key !== "rotor" })
        if (window.isPanelDetached("rotor"))
            window.openRotor()
        // Prima via quelle che non servono piu', poi dentro quelle nuove: cosi'
        // le finestre che restano non vengono nemmeno sfiorate.
        for (let i = detachedModel.count - 1; i >= 0; --i) {
            if (wanted.indexOf(detachedModel.get(i).key) < 0)
                detachedModel.remove(i)
        }
        for (let j = 0; j < wanted.length; ++j) {
            let there = false
            for (let k = 0; k < detachedModel.count; ++k) {
                if (detachedModel.get(k).key === wanted[j]) {
                    there = true
                    break
                }
            }
            if (!there)
                detachedModel.append({key: wanted[j]})
        }
    }

    Connections {
        target: layout
        function onDetachedPanelsChanged() { window.syncDetachedWindows() }
    }

    // La finestra di un pannello staccato, per spostarla o portarla davanti.
    function panelWindowFor(key) {
        for (let i = 0; i < detachedModel.count; ++i) {
            if (detachedModel.get(i).key === key)
                return detachedWindows.objectAt(i)
        }
        return null
    }
    function raisePanel(key) {
        const win = window.panelWindowFor(key)
        if (win) {
            win.raise()
            win.requestActivate()
        }
    }

    Instantiator {
        id: detachedWindows
        model: detachedModel
        delegate: PanelWindow {
            required property string key
            panelKey: key
            panelTitle: window.panelTitle(key)
            panelSource: window.panelSource(key)
            // Quelli del contest vivono solo in finestra: riagganciarli
            // vorrebbe dire farli sparire, perche' nella disposizione della
            // finestra principale non hanno un posto.
            dockable: !window.isWindowOnly(key)
            contestMode: window.contestModeOn && window.isContestDeskKey(key)
            pinned: window.contestOnTop
            // Tenute insieme alla finestra principale: stanno sopra di lei, si
            // riducono con lei e non riempiono la barra delle applicazioni.
            transientParent: window.contestModeOn && window.isContestDeskKey(key) ? window : null
            // La X di una finestra staccata riaggancia il pannello. Ma quando a
            // chiudersi e' tutto il programma, la finestra si chiude lo stesso e
            // quello non e' un riaggancio: prima tornavano dentro tutti.
            onClosing: {
                if (window.quitting)
                    return
                if (window.isWindowOnly(panelKey))
                    window.closePanel(panelKey)
                else
                    window.attachPanel(panelKey)
            }
            onAttachRequested: window.attachPanel(panelKey)
            onCloseRequested: window.closePanel(panelKey)
            onOpenQsoRequested: (id) => window.openQso(id)
            onAwardRequested: (id) => awardsDialog.openAt(id)
            onClusterRequested: (tab) => window.openCluster(tab)
            onStatsRequested: window.openStats()
            onRotorRequested: window.openRotor()
            onContestRequested: window.openContest()
            onDeskCommand: (what, arg) => window.deskDo(what, arg)
            deskOpenPanels: window.detachedPanels
            deskAllOnTop: window.contestOnTop
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

    // Il menu che esce col tasto destro sulla testata di un pannello: quello
    // che si puo' fare alla disposizione, li' dove la si guarda.
    StyledMenu {
        id: layoutMenu
        property string key: ""
        function openAt(panelKey, screenX, screenY) {
            layoutMenu.key = panelKey
            const p = window.contentItem.mapFromGlobal(screenX, screenY)
            layoutMenu.popup(window.contentItem, p.x, p.y)
        }

        StyledMenuItem {
            text: layout.layoutLocked ? qsTr("Unlock the layout") : qsTr("Lock the layout")
            onTriggered: layout.layoutLocked = !layout.layoutLocked
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
        StyledMenuItem {
            text: qsTr("Detach this panel into its own window")
            enabled: layoutMenu.key.length > 0 && !window.isPanelDetached(layoutMenu.key)
            onTriggered: window.detachPanel(layoutMenu.key)
        }
        StyledMenuItem {
            text: qsTr("Close this panel")
            enabled: layoutMenu.key.length > 0
            onTriggered: window.closePanel(layoutMenu.key)
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
        StyledMenuItem {
            text: qsTr("Panels…")
            onTriggered: panelsPopup.open()
        }
        StyledMenuItem {
            text: qsTr("Restore the default layout")
            onTriggered: window.resetPanels()
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
            onLogsRequested: logsDialog.openDialog()
            onImportRequested: importDialog.open()
            onExportRequested: exportDialog.open()
            onAwardsRequested: awardsDialog.openAt("")
            onClusterRequested: window.openCluster(0)
            onActivationRequested: activationDialog.openDialog()
            onProfilesRequested: profilesDialog.open()
            closedPanels: window.hiddenPanels.length
            onPanelsRequested: panelsPopup.opened ? panelsPopup.close() : panelsPopup.open()
            onAboutRequested: aboutDialog.open()
            onLogFolderRequested: decolog.openDatabaseFolder()
            onQuitRequested: { window.quitting = true; Qt.quit() }
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

                PanelSlot {
                    id: slotLeft
                    slotId: "left"
                    panelKey: window.panelAt("left")
                    docked: window.panelShows(panelKey)
                    highlighted: window.dragTargetSlot === "left"
                    onMenuRequested: (key, x, y) => layoutMenu.openAt(key, x, y)
                    onMoveStarted: (key) => window.beginPanelDrag(key, "left")
                    onMoveMoved: (key, x, y) => window.updatePanelDrag(x, y)
                    onMoveEnded: (key, x, y) => window.endPanelDrag(x, y)
                    onDetachRequested: (key) => window.detachPanel(key)
                    onCloseRequested: (key) => window.closePanel(key)
                    onOpenQsoRequested: (id) => window.openQso(id)
                    onAwardRequested: (id) => awardsDialog.openAt(id)
                    onClusterRequested: (tab) => window.openCluster(tab)
                    onStatsRequested: window.openStats()
                    onRotorRequested: window.openRotor()
                    SplitView.preferredWidth: layout.leftWidth
                    SplitView.minimumWidth: 260
                    onWidthChanged: if (width > 0 && window.layoutIsWhole) layout.leftWidth = width
                    onExpandRequested: newQsoDialog.open()
                }

                PanelSlot {
                    id: slotCenter
                    slotId: "center"
                    panelKey: window.panelAt("center")
                    docked: window.panelShows(panelKey)
                    highlighted: window.dragTargetSlot === "center"
                    onMenuRequested: (key, x, y) => layoutMenu.openAt(key, x, y)
                    onMoveStarted: (key) => window.beginPanelDrag(key, "center")
                    onMoveMoved: (key, x, y) => window.updatePanelDrag(x, y)
                    onMoveEnded: (key, x, y) => window.endPanelDrag(x, y)
                    onDetachRequested: (key) => window.detachPanel(key)
                    onCloseRequested: (key) => window.closePanel(key)
                    onOpenQsoRequested: (id) => window.openQso(id)
                    onAwardRequested: (id) => awardsDialog.openAt(id)
                    onClusterRequested: (tab) => window.openCluster(tab)
                    onStatsRequested: window.openStats()
                    onRotorRequested: window.openRotor()
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 480
                    hiddenColumns: layout.hiddenColumns
                    columnWidths: layout.columnWidths
                    savedFilters: layout.savedFilters
                    onHiddenColumnsEdited: (value) => layout.hiddenColumns = value
                    onColumnWidthsEdited: (value) => layout.columnWidths = value
                    onSavedFiltersEdited: (value) => layout.savedFilters = value
                    onPopRequested: (key) => window.detachPanel(key)
                }

                SplitView {
                    id: rightColumn
                    SplitView.preferredWidth: layout.rightWidth
                    SplitView.minimumWidth: 260
                    visible: slotRightA.visible || slotRightB.visible
                             || slotRightC.visible || slotRightD.visible
                    onWidthChanged: if (width > 0 && window.layoutIsWhole) layout.rightWidth = width
                    orientation: Qt.Vertical
                    handle: splitHandle

                    PanelSlot {
                        id: slotRightA
                        slotId: "rightA"
                        panelKey: window.panelAt("rightA")
                        docked: window.panelShows(panelKey)
                        highlighted: window.dragTargetSlot === "rightA"
                        onMenuRequested: (key, x, y) => layoutMenu.openAt(key, x, y)
                        onMoveStarted: (key) => window.beginPanelDrag(key, "rightA")
                        onMoveMoved: (key, x, y) => window.updatePanelDrag(x, y)
                        onMoveEnded: (key, x, y) => window.endPanelDrag(x, y)
                        onDetachRequested: (key) => window.detachPanel(key)
                        onCloseRequested: (key) => window.closePanel(key)
                        onOpenQsoRequested: (id) => window.openQso(id)
                        onAwardRequested: (id) => awardsDialog.openAt(id)
                        onClusterRequested: (tab) => window.openCluster(tab)
                        onStatsRequested: window.openStats()
                        onRotorRequested: window.openRotor()
                        SplitView.fillHeight: true
                        // La scheda del nominativo e' quella che si guarda di
                        // piu': schiacciata a una riga non serve a niente, e
                        // gli altri pannelli della colonna, con le loro misure
                        // preferite, la riducevano proprio a quello.
                        SplitView.minimumHeight: panelKey === "rotor" ? 220
                                               : panelKey === "callinfo" ? 200 : 80
                    }
                    PanelSlot {
                        id: slotRightB
                        slotId: "rightB"
                        panelKey: window.panelAt("rightB")
                        docked: window.panelShows(panelKey)
                        highlighted: window.dragTargetSlot === "rightB"
                        onMenuRequested: (key, x, y) => layoutMenu.openAt(key, x, y)
                        onMoveStarted: (key) => window.beginPanelDrag(key, "rightB")
                        onMoveMoved: (key, x, y) => window.updatePanelDrag(x, y)
                        onMoveEnded: (key, x, y) => window.endPanelDrag(x, y)
                        onDetachRequested: (key) => window.detachPanel(key)
                        onCloseRequested: (key) => window.closePanel(key)
                        onOpenQsoRequested: (id) => window.openQso(id)
                        onAwardRequested: (id) => awardsDialog.openAt(id)
                        onClusterRequested: (tab) => window.openCluster(tab)
                        onStatsRequested: window.openStats()
                        onRotorRequested: window.openRotor()
                        SplitView.preferredHeight: panelKey === "rotor" ? Math.max(implicitHeight, 220) : 260
                        SplitView.minimumHeight: panelKey === "rotor" ? 220 : 120
                    }
                    PanelSlot {
                        id: slotRightC
                        slotId: "rightC"
                        panelKey: window.panelAt("rightC")
                        docked: window.panelShows(panelKey)
                        highlighted: window.dragTargetSlot === "rightC"
                        onMenuRequested: (key, x, y) => layoutMenu.openAt(key, x, y)
                        onMoveStarted: (key) => window.beginPanelDrag(key, "rightC")
                        onMoveMoved: (key, x, y) => window.updatePanelDrag(x, y)
                        onMoveEnded: (key, x, y) => window.endPanelDrag(x, y)
                        onDetachRequested: (key) => window.detachPanel(key)
                        onCloseRequested: (key) => window.closePanel(key)
                        onOpenQsoRequested: (id) => window.openQso(id)
                        onAwardRequested: (id) => awardsDialog.openAt(id)
                        onClusterRequested: (tab) => window.openCluster(tab)
                        onStatsRequested: window.openStats()
                        onRotorRequested: window.openRotor()
                        SplitView.preferredHeight: panelKey === "rotor" ? Math.max(implicitHeight, 220) : implicitHeight
                        SplitView.minimumHeight: panelKey === "rotor" ? 220 : 60
                    }
                    PanelSlot {
                        id: slotRightD
                        slotId: "rightD"
                        panelKey: window.panelAt("rightD")
                        docked: window.panelShows(panelKey)
                        highlighted: window.dragTargetSlot === "rightD"
                        onMenuRequested: (key, x, y) => layoutMenu.openAt(key, x, y)
                        onMoveStarted: (key) => window.beginPanelDrag(key, "rightD")
                        onMoveMoved: (key, x, y) => window.updatePanelDrag(x, y)
                        onMoveEnded: (key, x, y) => window.endPanelDrag(x, y)
                        onDetachRequested: (key) => window.detachPanel(key)
                        onCloseRequested: (key) => window.closePanel(key)
                        onOpenQsoRequested: (id) => window.openQso(id)
                        onAwardRequested: (id) => awardsDialog.openAt(id)
                        onClusterRequested: (tab) => window.openCluster(tab)
                        onStatsRequested: window.openStats()
                        onRotorRequested: window.openRotor()
                        SplitView.preferredHeight: panelKey === "rotor" ? Math.max(implicitHeight, 220) : implicitHeight
                        SplitView.minimumHeight: panelKey === "rotor" ? 220 : 60
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
                SplitView.preferredHeight: window.tabsTab === 4 ? layout.clusterBottomHeight
                                                                       : layout.bottomHeight
                SplitView.minimumHeight: 130
                visible: slotBottomLeft.visible || slotBottomRight.visible
                onHeightChanged: {
                    if (height > 0 && window.layoutIsWhole) {
                        if (window.tabsTab === 4)
                            layout.clusterBottomHeight = height
                        else
                            layout.bottomHeight = height
                    }
                }
                orientation: Qt.Horizontal
                handle: splitHandle

                PanelSlot {
                    id: slotBottomLeft
                    slotId: "bottomLeft"
                    panelKey: window.panelAt("bottomLeft")
                    docked: window.panelShows(panelKey)
                    highlighted: window.dragTargetSlot === "bottomLeft"
                    onMenuRequested: (key, x, y) => layoutMenu.openAt(key, x, y)
                    onMoveStarted: (key) => window.beginPanelDrag(key, "bottomLeft")
                    onMoveMoved: (key, x, y) => window.updatePanelDrag(x, y)
                    onMoveEnded: (key, x, y) => window.endPanelDrag(x, y)
                    onDetachRequested: (key) => window.detachPanel(key)
                    onCloseRequested: (key) => window.closePanel(key)
                    onOpenQsoRequested: (id) => window.openQso(id)
                    onAwardRequested: (id) => awardsDialog.openAt(id)
                    onClusterRequested: (tab) => window.openCluster(tab)
                    onStatsRequested: window.openStats()
                    onRotorRequested: window.openRotor()
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 320
                }
                // Allineata alla colonna di destra, come nel mockup, finche' non
                // la si tira da un'altra parte.
                PanelSlot {
                    id: slotBottomRight
                    slotId: "bottomRight"
                    panelKey: window.panelAt("bottomRight")
                    docked: window.panelShows(panelKey)
                    highlighted: window.dragTargetSlot === "bottomRight"
                    onMenuRequested: (key, x, y) => layoutMenu.openAt(key, x, y)
                    onMoveStarted: (key) => window.beginPanelDrag(key, "bottomRight")
                    onMoveMoved: (key, x, y) => window.updatePanelDrag(x, y)
                    onMoveEnded: (key, x, y) => window.endPanelDrag(x, y)
                    onDetachRequested: (key) => window.detachPanel(key)
                    onCloseRequested: (key) => window.closePanel(key)
                    onOpenQsoRequested: (id) => window.openQso(id)
                    onAwardRequested: (id) => awardsDialog.openAt(id)
                    onClusterRequested: (tab) => window.openCluster(tab)
                    onStatsRequested: window.openStats()
                    onRotorRequested: window.openRotor()
                    SplitView.preferredWidth: layout.mapWidth
                    SplitView.minimumWidth: 180
                    onWidthChanged: if (width > 0 && window.layoutIsWhole) layout.mapWidth = width
                }
            }
        }

        StatusRail { Layout.fillWidth: true }
    }

    // La base del banco: in modalita' contest copre la disposizione di tutti i
    // giorni invece di nasconderla. Nasconderla faceva stringere a zero le
    // colonne della disposizione, e all'uscita dalla gara la scheda
    // nominativo, il rotore e la fila in basso non tornavano piu'.
    ContestBase {
        x: verticalSplit.x
        y: verticalSplit.y
        width: verticalSplit.width
        height: verticalSplit.height
        z: 20
        visible: window.contestModeOn
        onArrangeRequested: window.arrangeContestDesk(layout.contestDeskLayout || "columns")
        onExitRequested: window.exitContestMode()
        onDeskRequested: window.openContestDesk()
    }

    Component {
        id: splitHandle
        Rectangle {
            id: handleRoot
            implicitWidth: 8
            implicitHeight: 8
            color: "transparent"
            // Disposizione bloccata: la maniglia resta disegnata ma non si tira.
            enabled: !layout.layoutLocked
            Rectangle {
                anchors.centerIn: parent
                width: handleRoot.width > handleRoot.height ? 40 : 2
                height: handleRoot.width > handleRoot.height ? 2 : 40
                radius: 1
                opacity: layout.layoutLocked ? 0.4 : 1
                color: handleRoot.SplitHandle.pressed ? Theme.primaryColor
                     : handleRoot.SplitHandle.hovered ? Theme.textSecondary : Theme.borderSoft
            }
        }
    }
}
