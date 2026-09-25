// DecoDXLog — il DX cluster: filtri rapidi in alto, spot confrontati col log.
// Clic: il nominativo va in Call info. Doppio clic: Decodium si sintonizza.
// `compact` e' la versione per la scheda in basso della finestra principale.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Decodium.UI

GlassPanel {
    id: root

    property bool compact: false
    // Durante un contest il cluster serve a una cosa sola: vedere chi porta un
    // moltiplicatore che non si ha. Via tutto il resto, e quelli che contano si
    // vedono da lontano.
    property bool contestMode: false
    // In contest: i filtri chiusi, e la possibilita' di vedere solo gli spot
    // che portano un moltiplicatore.
    property bool filtersOpen: false
    property bool onlyMultipliers: false
    signal windowRequested(int tab)

    readonly property var cluster: decolog.cluster
    readonly property var model: cluster.spots
    readonly property var quickBands: ["160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"]
    readonly property var quickModes: ["FT2", "FT8", "FT4", "CW", "SSB"]

    // ── Le colonne ──────────────────────────────────────────────────────────
    // Si scelgono e si mettono in ordine come nel log: dal chip "colonne" o
    // trascinando l'intestazione. In gara la disposizione e' una a parte, di
    // solito piu' stretta.
    readonly property var columnCatalog: [
        { key: "utc", title: "UTC", field: "", width: 44 },
        { key: "freq", title: qsTr("kHz"), field: "", width: 76 },
        { key: "call", title: qsTr("DX"), field: "", width: 110 },
        { key: "status", title: qsTr("Status"), field: "", width: 86 },
        { key: "entity", title: qsTr("Entity"), field: "", width: 1, stretch: 3 },
        { key: "mode", title: qsTr("Mode"), field: "", width: 46 },
        { key: "band", title: qsTr("Band"), field: "", width: 44 },
        { key: "spotter", title: qsTr("Spotter"), field: "", width: 96 },
        { key: "info", title: qsTr("Info"), field: "", width: 1, stretch: 4 },
        { key: "distance", title: qsTr("km · az"), field: "", width: 86 },
        { key: "source", title: qsTr("Source"), field: "", width: 70 },
        { key: "cont", title: qsTr("Continent"), field: "", width: 40 },
        { key: "dxcc", title: "DXCC", field: "", width: 48 },
        { key: "grid", title: qsTr("Grid"), field: "", width: 60 },
        { key: "snr", title: "dB", field: "", width: 40 },
        { key: "refs", title: qsTr("Reference"), field: "", width: 90 },
        { key: "comment", title: qsTr("Comment"), field: "", width: 1, stretch: 3 }
    ]
    readonly property var defaultLayout: root.contestMode
        ? ["utc", "freq", "call", "status", "mode", "band", "spotter", "info"]
        : ["utc", "freq", "call", "status", "entity", "mode", "band", "spotter", "info", "distance", "source"]
    Settings {
        id: columnStore
        category: "layout"
        property string clusterColumns: ""
        property string clusterColumnsContest: ""
        // Le larghezze scelte a mano, {chiave: pixel}: in gara sono a parte,
        // come le colonne.
        property string clusterWidths: ""
        property string clusterWidthsContest: ""
    }
    // Le larghezze salvate, e quella che si sta tirando adesso col mouse (si
    // salva quando si lascia: non a ogni pixel).
    readonly property var storedWidths: {
        const text = root.contestMode ? columnStore.clusterWidthsContest : columnStore.clusterWidths
        try { return text.length > 0 ? JSON.parse(text) : ({}) } catch (e) { return ({}) }
    }
    property string resizingKey: ""
    property real resizingWidth: 0
    function widthOf(key) {
        if (key === root.resizingKey)
            return root.resizingWidth
        const w = root.storedWidths[key]
        return w > 0 ? w : root.columnDef(key).width
    }
    // Una colonna che si allarga da sola (entita', info, commento) smette di
    // farlo quando le si da' una misura a mano.
    function stretchOf(key) {
        const def = root.columnDef(key)
        return def.stretch && !(root.storedWidths[key] > 0) && key !== root.resizingKey ? def.stretch : 0
    }
    // Quanto e' larga la tabella: le colonne che si allargano da sole hanno
    // almeno 70 px. Se il pannello e' piu' stretto la tabella scorre di lato,
    // invece di tagliare le ultime colonne.
    readonly property real tableWidth: {
        let w = 24 + 8 * Math.max(0, root.columns.length - 1)
        for (const key of root.columns)
            w += root.stretchOf(key) > 0 ? 70 : root.widthOf(key)
        return w
    }
    readonly property real rowWidth: Math.max(list.width, root.tableWidth)
    function setWidth(key, width) {
        const map = Object.assign({}, root.storedWidths)
        map[key] = Math.max(24, Math.round(width))
        const text = JSON.stringify(map)
        if (root.contestMode)
            columnStore.clusterWidthsContest = text
        else
            columnStore.clusterWidths = text
    }
    function resetWidths() {
        if (root.contestMode)
            columnStore.clusterWidthsContest = ""
        else
            columnStore.clusterWidths = ""
    }
    readonly property var columns: {
        const stored = root.contestMode ? columnStore.clusterColumnsContest : columnStore.clusterColumns
        const keys = stored.length > 0 ? stored.split(",") : root.defaultLayout
        const known = root.columnCatalog.map(c => c.key)
        let out = keys.filter(k => known.indexOf(k) >= 0)
        // La scheda stretta in basso mostra l'essenziale, ma solo finche'
        // l'operatore non ha scelto lui: le colonne che aggiunge si vedono.
        if (root.compact && stored.length === 0)
            out = out.filter(k => ["band", "spotter", "distance", "source"].indexOf(k) < 0)
        if (out.indexOf("call") < 0)
            out.unshift("call")
        return out
    }
    function columnDef(key) {
        for (const c of root.columnCatalog)
            if (c.key === key)
                return c
        return { key: key, title: key, width: 60 }
    }
    function setLayout(list) {
        const text = list.join(",")
        if (root.contestMode)
            columnStore.clusterColumnsContest = text
        else
            columnStore.clusterColumns = text
    }
    function moveColumn(from, to) {
        const list = root.columns.slice()
        if (from < 0 || from >= list.length || to < 0 || to >= list.length || from === to)
            return
        const key = list.splice(from, 1)[0]
        list.splice(to, 0, key)
        root.setLayout(list)
    }
    // Per le prove col mouse vero.
    function headerItem(i) { return headRepeater.itemAt(i) }
    function openColumns() { clusterColumnsDialog.open() }
    property int headerDropTarget: -1
    function headerIndexAt(x) {
        for (let i = 0; i < headRepeater.count; ++i) {
            const h = headRepeater.itemAt(i)
            if (h && x < h.x + h.width + 4)
                return i
        }
        return headRepeater.count - 1
    }

    function has(key, value) { return (cluster.filter[key] || []).indexOf(value) >= 0 }
    function toggle(key, value) {
        const f = Object.assign({}, cluster.filter)
        const list = (f[key] || []).slice()
        const i = list.indexOf(value)
        if (i >= 0) list.splice(i, 1); else list.push(value)
        f[key] = list
        cluster.filter = f
    }
    function setKey(key, value) {
        const f = Object.assign({}, cluster.filter)
        f[key] = value
        cluster.filter = f
    }
    function statusColor(status) {
        if (status & 1) return Theme.errorColor
        if (status & 2) return Theme.warningColor
        if (status & 4) return Theme.secondaryColor
        if (status & 8) return Theme.primaryColor
        if (status & 32) return Theme.textSecondary
        if (status & 16) return Theme.accentColor
        return Theme.borderSoft
    }
    function modeColor(mode) {
        if (mode === "FT2") return Theme.accentColor
        if (mode === "FT8" || mode === "FT4") return Theme.primaryColor
        if (mode === "CW") return Theme.secondaryColor
        return Theme.textPrimary
    }
    // Per le schermate di prova.
    function showMenu(name) {
        if (name === "row") rowMenu.popupFor(root.model.get(0))
        else if (name === "filters") filterPopup.open()
    }

    title: compact ? "" : qsTr("DX Cluster")
    dotColor: cluster.onlineCount > 0 ? Theme.accentColor : Theme.errorColor
    padding: 0

    headerTools: [
        Pill {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("%1/%2 online").arg(root.cluster.onlineCount).arg(root.cluster.sources.length)
            tone: root.cluster.onlineCount > 0 ? Theme.accentColor : Theme.errorColor
            interactive: true
            pillHeight: 22
            onClicked: root.windowRequested(1)
        },
        Text {
            anchors.verticalCenter: parent.verticalCenter
            // Corto: la testata di una finestra stretta non ha posto per una
            // frase, e i due numeri dicono gia' tutto.
            text: qsTr("%1/%2 spots").arg(root.model.count).arg(root.model.totalCount)
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            text: root.cluster.voiceEnabled ? "🔊" : "🔇"
            buttonHeight: 24
            fontPixelSize: 12
            onClicked: root.cluster.voiceEnabled = !root.cluster.voiceEnabled
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("→ Decodium")
            tone: root.cluster.sendToDecodium ? Theme.accentColor : "transparent"
            buttonHeight: 24
            fontPixelSize: 11
            onClicked: root.cluster.sendToDecodium = !root.cluster.sendToDecodium
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.compact
            text: qsTr("Open cluster")
            tone: Theme.primaryColor
            buttonHeight: 24
            fontPixelSize: 11
            onClicked: root.windowRequested(0)
        }
    ]

    component Chip: Rectangle {
        id: chip
        property string label
        property bool on: false
        property color tone: Theme.primaryColor
        signal toggled()
        implicitHeight: 22
        implicitWidth: chipText.implicitWidth + 12
        radius: 4
        color: on ? Qt.rgba(tone.r, tone.g, tone.b, 0.22) : chipArea.containsMouse ? Theme.glassOverlay : "transparent"
        border.width: 1
        border.color: on ? tone : Theme.glassBorder
        Text {
            id: chipText
            anchors.centerIn: parent
            text: chip.label
            color: chip.on ? chip.tone : Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
            font.bold: chip.on
        }
        MouseArea {
            id: chipArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: chip.toggled()
        }
    }
    component Head: Text {
        color: Theme.secondaryColor
        font.family: Theme.monoFamily
        font.pixelSize: Theme.fontSize
        font.bold: true
        elide: Text.ElideRight
    }

    ColumnsDialog {
        id: clusterColumnsDialog
        panel: root
        shown: root.columns
        all: root.columnCatalog
        allowCustom: false
        allowWidths: true
    }

    Popup {
        id: filterPopup
        anchors.centerIn: Overlay.overlay
        modal: true
        padding: 16
        width: Math.min(820, (Overlay.overlay ? Overlay.overlay.width : 900) - 40)
        background: Rectangle { color: Theme.panelColor; border.color: Theme.glassBorder; radius: 6 }
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Spot filters"); color: Theme.textPrimary; font.pixelSize: 15; font.bold: true }
                Item { Layout.fillWidth: true }
                GlassButton { text: qsTr("Clear all"); onClicked: root.cluster.filter = ({}) }
                GlassButton { text: qsTr("Close"); tone: Theme.primaryColor; filled: true; onClicked: filterPopup.close() }
            }
            SpotFilterEditor {
                Layout.fillWidth: true
                filter: root.cluster.filter
                onEdited: (f) => root.cluster.filter = f
            }
            RowLayout {
                spacing: 8
                StyledTextField { id: saveName; Layout.preferredWidth: 200; mono: false; placeholderText: qsTr("Name for these filters") }
                GlassButton {
                    text: qsTr("Save")
                    enabled: saveName.text.trim().length > 0
                    onClicked: { root.cluster.saveFilter(saveName.text); saveName.text = "" }
                }
                Repeater {
                    model: Object.keys(root.cluster.savedFilters)
                    Pill {
                        required property string modelData
                        text: modelData + "  ✕"
                        tone: Theme.secondaryColor
                        interactive: true
                        onClicked: root.cluster.deleteSavedFilter(modelData)
                    }
                }
            }
        }
    }

    StyledMenu {
        id: savedMenu
        Repeater {
            model: Object.keys(root.cluster.savedFilters)
            StyledMenuItem {
                required property string modelData
                text: modelData
                onTriggered: root.cluster.applySavedFilter(modelData)
            }
        }
        StyledMenuItem {
            visible: Object.keys(root.cluster.savedFilters).length === 0
            height: visible ? implicitHeight : 0
            enabled: false
            text: qsTr("No saved filters: use More filters → Save")
        }
    }

    StyledMenu {
        id: rowMenu
        property var spot: ({})
        function popupFor(s) { spot = s || ({}); popup() }
        StyledMenuItem {
            text: qsTr("Tune Decodium to %1").arg(rowMenu.spot.call || "")
            onTriggered: root.cluster.tune(rowMenu.spot.spotKey)
        }
        StyledMenuItem { text: qsTr("Show in Call info"); onTriggered: root.cluster.lookupSpot(rowMenu.spot.spotKey) }
        StyledMenuItem {
            enabled: decolog.rotor.enabled && rowMenu.spot.azimuth !== undefined && rowMenu.spot.azimuth !== null
            text: rowMenu.spot.azimuth !== undefined && rowMenu.spot.azimuth !== null
                  ? qsTr("Point the rotor at %1 (%2°)").arg(rowMenu.spot.call || "").arg(rowMenu.spot.azimuth)
                  : qsTr("Point the rotor")
            onTriggered: decolog.rotor.pointTo(rowMenu.spot.azimuth, rowMenu.spot.call || "")
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
        StyledMenuItem {
            enabled: (rowMenu.spot.dxcc || 0) > 0
            text: qsTr("Only %1").arg(rowMenu.spot.entity || qsTr("this entity"))
            onTriggered: {
                const f = Object.assign({}, root.cluster.filter)
                f.dxcc = [rowMenu.spot.dxcc]
                root.cluster.filter = f
            }
        }
        StyledMenuItem {
            text: qsTr("Alert me when %1 is spotted").arg(rowMenu.spot.call || "")
            onTriggered: root.cluster.saveAlertRule({ name: rowMenu.spot.call, voice: true, decodium: true,
                                                      filter: { calls: rowMenu.spot.call, maxAgeMinutes: 10 } })
        }
        StyledMenuItem {
            text: qsTr("Hide spots from %1").arg(rowMenu.spot.spotter || "")
            visible: false
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // In contest la riga dei filtri si apre solo quando serve: in una
        // colonna stretta quei pulsanti si mangiavano mezza finestra, e quello
        // che conta sono gli spot.
        RowLayout {
            Layout.fillWidth: true
            visible: root.contestMode
            spacing: 4
            Chip {
                label: root.filtersOpen ? qsTr("hide filters") : qsTr("filters")
                on: root.filtersOpen
                onToggled: root.filtersOpen = !root.filtersOpen
            }
            Chip {
                label: qsTr("mult only")
                tone: Theme.warningColor
                on: root.onlyMultipliers
                onToggled: root.onlyMultipliers = !root.onlyMultipliers
            }
            Chip {
                label: qsTr("columns")
                on: false
                onToggled: clusterColumnsDialog.open()
            }
            Item { Layout.fillWidth: true }
            Text {
                text: root.model.count + ""
                color: Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: 11
                rightPadding: 8
            }
        }

        // ── Filtri rapidi ───────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            visible: !root.contestMode || root.filtersOpen
            implicitHeight: quick.implicitHeight + 12
            color: "transparent"
            Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.borderSoft }
            Flow {
                id: quick
                anchors.fill: parent
                anchors.margins: 6
                anchors.leftMargin: 10
                spacing: 4

                Chip {
                    label: qsTr("Decodium band")
                    tone: Theme.accentColor
                    on: root.cluster.followDecodiumBand
                    onToggled: root.cluster.followDecodiumBand = !root.cluster.followDecodiumBand
                }
                Item { width: 6; height: 22 }
                Repeater {
                    model: root.cluster.followDecodiumBand ? [] : root.quickBands
                    Chip { required property string modelData; label: modelData; on: root.has("bands", modelData); onToggled: root.toggle("bands", modelData) }
                }
                Item { width: 6; height: 22 }
                Repeater {
                    model: root.quickModes
                    Chip {
                        required property string modelData
                        label: modelData
                        tone: modelData === "FT2" ? Theme.accentColor : Theme.primaryColor
                        on: root.has("modes", modelData)
                        onToggled: root.toggle("modes", modelData)
                    }
                }
                Item { width: 6; height: 22 }
                Chip {
                    label: "NEW DXCC"
                    tone: Theme.errorColor
                    on: ((root.cluster.filter.anyStatus || 0) & 1) !== 0
                    onToggled: root.setKey("anyStatus", (root.cluster.filter.anyStatus || 0) ^ 1)
                }
                Chip {
                    label: qsTr("NEW BAND/MODE")
                    tone: Theme.warningColor
                    on: ((root.cluster.filter.anyStatus || 0) & 14) === 14
                    onToggled: root.setKey("anyStatus", ((root.cluster.filter.anyStatus || 0) & 14) === 14
                                                        ? (root.cluster.filter.anyStatus & ~14) : ((root.cluster.filter.anyStatus || 0) | 14))
                }
                Chip {
                    label: qsTr("hide worked")
                    tone: Theme.secondaryColor
                    on: !!root.cluster.filter.hideWorkedBand
                    onToggled: root.setKey("hideWorkedBand", !root.cluster.filter.hideWorkedBand)
                }
                Chip {
                    visible: !root.compact
                    label: "POTA/SOTA"
                    tone: Theme.secondaryColor
                    on: !!root.cluster.filter.onlyActivations
                    onToggled: root.setKey("onlyActivations", !root.cluster.filter.onlyActivations)
                }
                StyledTextField {
                    width: root.compact ? 110 : 150
                    fieldHeight: 22
                    uppercase: true
                    placeholderText: qsTr("Call / entity…")
                    text: root.cluster.filter.text || ""
                    onTextEdited: searchDelay.restart()
                    Timer { id: searchDelay; interval: 400; onTriggered: root.setKey("text", parent.text) }
                }
                Chip { label: qsTr("More filters…"); tone: Theme.primaryColor; on: false; onToggled: filterPopup.open() }
                Chip { label: qsTr("Saved ▾"); tone: Theme.primaryColor; on: false; onToggled: savedMenu.popup() }
                Chip { label: qsTr("columns"); tone: Theme.primaryColor; on: false; onToggled: clusterColumnsDialog.open() }
            }
        }

        // ── Intestazione ────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.rowHeight
            color: Theme.panelHeader
            clip: true
            RowLayout {
                id: headRow
                // Scorre di lato insieme alle righe.
                x: 14 - list.contentX
                y: 0
                width: root.rowWidth - 24
                height: parent.height
                spacing: 8
                Repeater {
                    id: headRepeater
                    model: root.columns
                    delegate: Head {
                        id: head
                        required property string modelData
                        required property int index
                        readonly property var def: root.columnDef(modelData)
                        Layout.preferredWidth: root.widthOf(modelData)
                        Layout.fillWidth: root.stretchOf(modelData) > 0
                        Layout.horizontalStretchFactor: root.stretchOf(modelData) > 0 ? root.stretchOf(modelData) : -1
                        Layout.minimumWidth: root.stretchOf(modelData) > 0 ? 70 : root.widthOf(modelData)
                        text: def.title
                        horizontalAlignment: modelData === "freq" || modelData === "distance" ? Text.AlignRight : Text.AlignLeft
                        // Presa e portata su un'altra intestazione, la colonna si
                        // sposta li'.
                        Rectangle {
                            anchors { left: parent.left; top: parent.top; bottom: parent.bottom; leftMargin: -5 }
                            width: 3
                            color: Theme.accentColor
                            visible: root.headerDropTarget === head.index
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.PointingHandCursor
                            property real pressX: 0
                            onPressed: (mouse) => pressX = mouse.x
                            onPositionChanged: (mouse) => {
                                if (pressed)
                                    root.headerDropTarget = Math.abs(mouse.x - pressX) > 8
                                        ? root.headerIndexAt(mapToItem(headRow, mouse.x, 0).x) : -1
                            }
                            onReleased: (mouse) => {
                                const target = root.headerIndexAt(mapToItem(headRow, mouse.x, 0).x)
                                root.headerDropTarget = -1
                                if (Math.abs(mouse.x - pressX) > 8)
                                    root.moveColumn(head.index, target)
                            }
                            onCanceled: root.headerDropTarget = -1
                        }
                        // Il bordo destro dell'intestazione: tirato, allarga o
                        // stringe la colonna, come nel log.
                        Rectangle {
                            anchors { right: parent.right; rightMargin: -5; top: parent.top; bottom: parent.bottom; topMargin: 4; bottomMargin: 4 }
                            width: 1
                            color: resizer.containsMouse || resizer.pressed ? Theme.accentColor : Theme.borderSoft
                        }
                        MouseArea {
                            id: resizer
                            // Fra la fine di questa e l'inizio della successiva
                            // (8 px): la presa per spostare non ci arriva.
                            anchors { right: parent.right; rightMargin: -8; top: parent.top; bottom: parent.bottom }
                            width: 11
                            z: 2
                            hoverEnabled: true
                            cursorShape: Qt.SplitHCursor
                            property real startX: 0
                            property real startWidth: 0
                            onPressed: (mouse) => {
                                startX = mapToItem(headRow, mouse.x, 0).x
                                startWidth = head.width
                                root.resizingWidth = startWidth
                                root.resizingKey = head.modelData
                            }
                            onPositionChanged: (mouse) => {
                                if (pressed)
                                    root.resizingWidth = Math.max(24, startWidth + mapToItem(headRow, mouse.x, 0).x - startX)
                            }
                            onReleased: {
                                const key = root.resizingKey
                                const w = root.resizingWidth
                                root.resizingKey = ""
                                if (key.length > 0)
                                    root.setWidth(key, w)
                            }
                            onCanceled: root.resizingKey = ""
                            onDoubleClicked: {
                                // Doppio clic: torna alla sua misura.
                                const map = Object.assign({}, root.storedWidths)
                                delete map[head.modelData]
                                const text = Object.keys(map).length > 0 ? JSON.stringify(map) : ""
                                if (root.contestMode)
                                    columnStore.clusterWidthsContest = text
                                else
                                    columnStore.clusterWidths = text
                            }
                        }
                    }
                }
            }
        }

        // ── Spot ────────────────────────────────────────────────────────────
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.model
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: root.rowWidth
            flickableDirection: Flickable.AutoFlickIfNeeded
            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar { policy: root.tableWidth > list.width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }
            // Chi sta leggendo in basso non viene riportato in cima da ogni spot.
            onCountChanged: if (atYBeginning) positionViewAtBeginning()

            delegate: Rectangle {
                id: line
                required property int index
                required property string spotKey
                required property string call
                required property string freq
                required property string band
                required property string mode
                required property string time
                required property string spotter
                required property string spotters
                required property int spotCount
                required property string comment
                required property string source
                required property string sourceName
                required property string entity
                required property int dxcc
                required property string continent
                required property int status
                required property string statusLabel
                required property var distance
                required property var azimuth
                required property var snr
                required property string refs
                required property string grid
                required property bool lotw
                required property bool fresh

                // Quanto vale questo spot nel contest aperto: i punti, e se
                // porta un moltiplicatore nuovo. Vuoto fuori dai contest.
                readonly property var contestValue: root.contestMode
                    ? decolog.activation.spotValue(call, band, mode) : ({})
                readonly property bool newMultiplier: !!contestValue.newMultiplier

                width: root.rowWidth
                height: visible ? Theme.rowHeight : 0
                visible: !root.onlyMultipliers || line.newMultiplier
                // Le righe restano del colore del pannello: il fondo giallo dei
                // moltiplicatori copriva tutto l'elenco in gara. Quello che conta
                // lo dicono il filo a sinistra e la pasticca, in un colore solo.
                color: area.containsMouse ? Theme.glassOverlay
                     : !root.contestMode && fresh && (status & 15)
                       ? Qt.rgba(root.statusColor(status).r, root.statusColor(status).g, root.statusColor(status).b, 0.10)
                     : "transparent"
                Rectangle {
                    anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                    width: line.newMultiplier ? 4 : 3
                    color: line.newMultiplier ? Theme.accentColor
                         : root.contestMode ? "transparent" : root.statusColor(line.status)
                }
                Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.borderSoft }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 10
                    spacing: 8
                    Repeater {
                        model: root.columns
                        delegate: Loader {
                            required property string modelData
                            Layout.preferredWidth: root.widthOf(modelData)
                            Layout.fillWidth: root.stretchOf(modelData) > 0
                            Layout.horizontalStretchFactor: root.stretchOf(modelData) > 0 ? root.stretchOf(modelData) : -1
                            Layout.minimumWidth: root.stretchOf(modelData) > 0 ? 70 : root.widthOf(modelData)
                            Layout.preferredHeight: 18
                            sourceComponent: modelData === "call" ? callCell
                                           : modelData === "status" ? statusCell
                                           : modelData === "entity" ? entityCell
                                           : modelData === "info" ? infoCell
                                           : textCell
                            property string cellKey: modelData
                        }
                    }
                }

                // Le celle. Il testo semplice e' una sola, che guarda la chiave.
                Component {
                    id: textCell
                    Text {
                        readonly property string key: parent ? parent.cellKey : ""
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        horizontalAlignment: key === "freq" || key === "distance" ? Text.AlignRight : Text.AlignLeft
                        font.family: key === "comment" ? Theme.uiFamily : Theme.monoFamily
                        font.pixelSize: key === "source" ? 10 : Theme.fontSize
                        font.bold: key === "mode"
                        text: key === "utc" ? line.time
                            : key === "freq" ? line.freq
                            : key === "mode" ? line.mode
                            : key === "band" ? line.band
                            : key === "spotter" ? line.spotter
                            : key === "distance" ? (line.distance !== undefined && line.distance !== null
                                                    ? line.distance + " · " + line.azimuth + "°" : "")
                            : key === "source" ? (line.source === "rbn" ? "RBN" : line.source === "hamalert" ? "HamAlert"
                                                  : line.source === "pota" ? "POTA" : line.sourceName)
                            : key === "cont" ? line.continent
                            : key === "dxcc" ? (line.dxcc > 0 ? String(line.dxcc) : "")
                            : key === "grid" ? line.grid
                            : key === "snr" ? (line.snr !== undefined && line.snr !== null ? String(line.snr) : "")
                            : key === "refs" ? line.refs
                            : key === "comment" ? line.comment
                            : ""
                        color: key === "freq" ? Theme.textPrimary
                             : key === "mode" ? root.modeColor(line.mode)
                             : key === "source" ? (line.source === "hamalert" ? Theme.warningColor : Theme.textSecondary)
                             : key === "refs" ? Theme.accentColor
                             : key === "snr" ? Theme.secondaryColor
                             : key === "comment" ? Theme.textPrimary
                             : Theme.textSecondary
                    }
                }
                Component {
                    id: callCell
                    RowLayout {
                        spacing: 4
                        Text {
                            text: line.call
                            color: (line.status & 32) ? Theme.textSecondary : Theme.textPrimary
                            font.family: Theme.monoFamily
                            font.pixelSize: Theme.fontSize + 1
                            font.bold: true
                        }
                        Text {
                            visible: line.spotCount > 1
                            text: "×" + line.spotCount
                            color: Theme.secondaryColor
                            font.family: Theme.monoFamily
                            font.pixelSize: 10
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
                Component {
                    id: statusCell
                    Item {
                        implicitHeight: 18
                        // Nel contest conta il moltiplicatore, non il DXCC nuovo.
                        Rectangle {
                            visible: line.newMultiplier
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: 18
                            radius: 3
                            color: Qt.rgba(Theme.accentColor.r, Theme.accentColor.g,
                                           Theme.accentColor.b, 0.14)
                            border.color: Theme.accentColor
                            Text {
                                anchors.centerIn: parent
                                width: parent.width - 6
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                text: line.contestValue.label || qsTr("mult")
                                color: Theme.accentColor
                                font.family: Theme.monoFamily
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                        Rectangle {
                            visible: !root.contestMode && !line.newMultiplier && line.statusLabel.length > 0
                            anchors.verticalCenter: parent.verticalCenter
                            width: badge.implicitWidth + 10
                            height: 16
                            radius: 3
                            color: Qt.rgba(root.statusColor(line.status).r, root.statusColor(line.status).g, root.statusColor(line.status).b,
                                           (line.status & 32) ? 0.10 : 0.22)
                            border.width: 1
                            border.color: root.statusColor(line.status)
                            Text {
                                id: badge
                                anchors.centerIn: parent
                                text: line.statusLabel
                                color: root.statusColor(line.status)
                                font.family: Theme.monoFamily
                                font.pixelSize: 9
                                font.bold: true
                            }
                        }
                    }
                }
                Component {
                    id: entityCell
                    Text {
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        textFormat: Text.StyledText
                        text: (line.entity || "—") + " <font color=\"" + Theme.textSecondary + "\">" + line.continent
                              + (line.lotw ? " · LoTW" : "") + ((line.status & 64) ? " · " + qsTr("unconf.") : "") + "</font>"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSize
                    }
                }
                Component {
                    id: infoCell
                    Text {
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        textFormat: Text.StyledText
                        text: (line.refs.length ? "<font color=\"" + Theme.accentColor + "\">" + line.refs + "</font> " : "")
                              + (line.snr !== undefined && line.snr !== null ? "<font color=\"" + Theme.secondaryColor + "\">" + line.snr + " dB</font> " : "")
                              + line.comment.replace(/&/g, "&amp;").replace(/</g, "&lt;")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSize
                    }
                }

                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton)
                            rowMenu.popupFor(root.model.get(line.index))
                        else
                            root.cluster.lookupSpot(line.spotKey)
                    }
                    onDoubleClicked: root.cluster.tune(line.spotKey)
                    ToolTip.visible: containsMouse && line.spotCount > 1
                    ToolTip.delay: 700
                    ToolTip.text: qsTr("Spotted by %1").arg(line.spotters)
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: list.count === 0
                text: root.cluster.onlineCount === 0
                      ? qsTr("No source connected. Open the cluster window → Sources to connect a node, RBN, HamAlert or POTA.")
                      : root.model.totalCount > 0 ? qsTr("No spot matches the filters (%1 hidden).").arg(root.model.totalCount)
                                                  : qsTr("Waiting for spots…")
                color: Theme.textSecondary
            }
        }
    }
}
