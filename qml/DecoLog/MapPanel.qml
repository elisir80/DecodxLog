// DecoLog — la mappa: coste, notte, locatori lavorati, spot del cluster, la
// stazione e la direzione verso il nominativo scelto.
//
// Proiezione equirettangolare, tutto disegnato con i colori del tema: le coste
// vengono da Natural Earth (pubblico dominio, 29 kB), la linea grigia si calcola
// dalla posizione del Sole. Niente mappe scaricate: funziona anche in portatile,
// senza rete.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    property bool showCoast: true
    property bool showNight: true
    property bool showGrids: true
    property bool showSpots: true

    readonly property var target: decolog.callInfo.position
    readonly property var home: decolog.myPosition
    readonly property var grids: decolog.gridPoints
    property var spots: []
    property var coastline: []
    property int revision: 0

    function reloadSpots() {
        spots = root.showSpots ? decolog.cluster.mapSpots() : []
        canvas.requestPaint()
    }

    title: qsTr("Map")
    showDot: false
    padding: 8
    headerTools: [
        // Le condizioni del momento, dove si guarda la propagazione: SFI e K.
        Text {
            anchors.verticalCenter: parent.verticalCenter
            readonly property var solar: decolog.solar.data
            visible: solar.valid === true
            text: qsTr("SFI %1 · K %2").arg(solar.solarFlux || 0).arg(solar.kIndex || 0)
            color: (solar.kIndex || 0) >= 4 ? Theme.errorColor
                 : (solar.kIndex || 0) >= 3 ? Theme.warningColor : Theme.accentColor
            font.family: Theme.monoFamily
            font.pixelSize: 11
        },
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: [decolog.callInfo.call, decolog.callInfo.gridsquare].filter(s => s).join(" · ")
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Layers ▾")
            buttonHeight: 22
            fontPixelSize: 10
            onClicked: layerMenu.popup()
        }
    ]

    StyledMenu {
        id: layerMenu
        StyledMenuItem {
            text: qsTr("Coastlines"); checkable: true; checked: root.showCoast
            onTriggered: { root.showCoast = checked; canvas.requestPaint() }
        }
        StyledMenuItem {
            text: qsTr("Night"); checkable: true; checked: root.showNight
            onTriggered: { root.showNight = checked; canvas.requestPaint() }
        }
        StyledMenuItem {
            text: qsTr("Worked grids"); checkable: true; checked: root.showGrids
            onTriggered: { root.showGrids = checked; canvas.requestPaint() }
        }
        StyledMenuItem {
            text: qsTr("Cluster spots"); checkable: true; checked: root.showSpots
            onTriggered: { root.showSpots = checked; root.reloadSpots() }
        }
    }

    // Le coste: una volta sola, dal C++ (le risorse dell'eseguibile non si leggono
    // con XMLHttpRequest).
    Component.onCompleted: {
        root.coastline = decolog.coastline()
        root.reloadSpots()
        canvas.requestPaint()
    }

    Connections {
        target: decolog
        function onLogChanged() { canvas.requestPaint() }
        function onLookupChanged() { canvas.requestPaint() }
        function onStationChanged() { canvas.requestPaint() }
    }
    Connections {
        target: decolog.cluster.spots
        function onCountChanged() { root.reloadSpots() }
    }
    // La notte si muove: un aggiornamento ogni due minuti basta e avanza.
    Timer { interval: 120000; running: root.showNight; repeat: true; onTriggered: canvas.requestPaint() }

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: Theme.bgMedium
        border.width: 1
        border.color: Theme.borderSoft
        clip: true

        Canvas {
            id: canvas
            anchors.fill: parent
            anchors.margins: 1
            renderStrategy: Canvas.Cooperative

            function px(lon) { return (lon + 180) / 360 * width }
            function py(lat) { return (90 - lat) / 180 * height }

            // Declinazione del Sole e ora siderale: bastano per la linea grigia.
            function sunPosition(now) {
                const day = now.getTime() / 86400000 + 2440587.5 - 2451545.0
                const meanLongitude = (280.46 + 0.9856474 * day) % 360
                const meanAnomaly = ((357.528 + 0.9856003 * day) % 360) * Math.PI / 180
                const lambda = (meanLongitude + 1.915 * Math.sin(meanAnomaly)
                                + 0.02 * Math.sin(2 * meanAnomaly)) * Math.PI / 180
                const obliquity = 23.439 * Math.PI / 180
                const declination = Math.asin(Math.sin(obliquity) * Math.sin(lambda))
                const utcHours = now.getUTCHours() + now.getUTCMinutes() / 60 + now.getUTCSeconds() / 3600
                // Longitudine del punto subsolare, approssimata (equazione del tempo esclusa).
                const sunLon = 180 - utcHours * 15
                return { declination: declination, lon: sunLon }
            }

            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()

                // ── Notte ───────────────────────────────────────────────────
                if (root.showNight) {
                    const sun = sunPosition(new Date())
                    ctx.fillStyle = Qt.rgba(0, 0, 0, 0.30)
                    const step = 2
                    for (let x = 0; x < width; x += step) {
                        const lon = x / width * 360 - 180
                        const hourAngle = (lon - sun.lon) * Math.PI / 180
                        // Latitudine del terminatore per questa longitudine.
                        const t = -Math.cos(hourAngle) / Math.tan(sun.declination)
                        const lat = Math.atan(t) * 180 / Math.PI
                        if (sun.declination > 0) {
                            // Estate boreale: e' notte a sud del terminatore.
                            ctx.fillRect(x, py(lat), step, height - py(lat))
                        } else {
                            ctx.fillRect(x, 0, step, py(lat))
                        }
                    }
                }

                // ── Coste ───────────────────────────────────────────────────
                if (root.showCoast && root.coastline.length > 0) {
                    ctx.strokeStyle = Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.55)
                    ctx.lineWidth = 1
                    for (const line of root.coastline) {
                        ctx.beginPath()
                        for (let i = 0; i < line.length; ++i) {
                            const x = px(line[i][0]), y = py(line[i][1])
                            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                        }
                        ctx.stroke()
                    }
                }

                // ── Reticolo ────────────────────────────────────────────────
                ctx.lineWidth = 1
                ctx.strokeStyle = Qt.rgba(Theme.borderSoft.r, Theme.borderSoft.g, Theme.borderSoft.b, 0.6)
                for (let lon = -150; lon < 180; lon += 30) {
                    ctx.beginPath(); ctx.moveTo(px(lon), 0); ctx.lineTo(px(lon), height); ctx.stroke()
                }
                for (let lat = -60; lat <= 60; lat += 30) {
                    ctx.strokeStyle = lat === 0 ? Theme.glassBorder
                                                : Qt.rgba(Theme.borderSoft.r, Theme.borderSoft.g, Theme.borderSoft.b, 0.6)
                    ctx.beginPath(); ctx.moveTo(0, py(lat)); ctx.lineTo(width, py(lat)); ctx.stroke()
                }

                // ── Locatori lavorati ───────────────────────────────────────
                if (root.showGrids) {
                    ctx.fillStyle = Qt.rgba(Theme.secondaryColor.r, Theme.secondaryColor.g, Theme.secondaryColor.b, 0.75)
                    for (const point of root.grids)
                        ctx.fillRect(px(point.lon) - 1, py(point.lat) - 1, 2.5, 2.5)
                }

                // ── Spot del cluster ────────────────────────────────────────
                if (root.showSpots) {
                    for (const spot of root.spots) {
                        const x = px(spot.lon), y = py(spot.lat)
                        const isNew = (spot.status & 1) !== 0
                        const isBandOrMode = (spot.status & 6) !== 0
                        ctx.fillStyle = isNew ? Theme.errorColor : isBandOrMode ? Theme.warningColor : Theme.accentColor
                        ctx.globalAlpha = isNew || isBandOrMode ? 0.95 : 0.6
                        ctx.beginPath()
                        ctx.arc(x, y, isNew ? 4 : 3, 0, 2 * Math.PI)
                        ctx.fill()
                        if (isNew) {
                            ctx.globalAlpha = 0.5
                            ctx.strokeStyle = Theme.errorColor
                            ctx.lineWidth = 1.5
                            ctx.beginPath(); ctx.arc(x, y, 8, 0, 2 * Math.PI); ctx.stroke()
                        }
                        ctx.globalAlpha = 1
                    }
                }

                // ── Il nominativo scelto ────────────────────────────────────
                // Il puntino del DX si vede anche se la stazione non ha ancora un
                // locatore: senza, la mappa non direbbe niente al primo avvio.
                if (root.target && root.target.lat !== undefined) {
                    ctx.fillStyle = Theme.accentColor
                    ctx.beginPath(); ctx.arc(px(root.target.lon), py(root.target.lat), 4, 0, 2 * Math.PI); ctx.fill()
                }

                // ── Casa e direzione ────────────────────────────────────────
                if (root.home && root.home.lat !== undefined) {
                    const hx = px(root.home.lon), hy = py(root.home.lat)
                    if (root.target && root.target.lat !== undefined) {
                        // Il cerchio massimo, passo per passo: in equirettangolare
                        // non e' una retta.
                        const toRad = Math.PI / 180
                        const lat1 = root.home.lat * toRad, lon1 = root.home.lon * toRad
                        const lat2 = root.target.lat * toRad, lon2 = root.target.lon * toRad
                        const d = 2 * Math.asin(Math.sqrt(Math.pow(Math.sin((lat2 - lat1) / 2), 2)
                                  + Math.cos(lat1) * Math.cos(lat2) * Math.pow(Math.sin((lon2 - lon1) / 2), 2)))
                        ctx.strokeStyle = Theme.accentColor
                        ctx.lineWidth = 1.5
                        ctx.globalAlpha = 0.9
                        ctx.beginPath()
                        let previousX = null
                        for (let i = 0; i <= 64; ++i) {
                            const f = i / 64
                            const a = Math.sin((1 - f) * d) / Math.sin(d)
                            const b = Math.sin(f * d) / Math.sin(d)
                            const x = a * Math.cos(lat1) * Math.cos(lon1) + b * Math.cos(lat2) * Math.cos(lon2)
                            const y = a * Math.cos(lat1) * Math.sin(lon1) + b * Math.cos(lat2) * Math.sin(lon2)
                            const z = a * Math.sin(lat1) + b * Math.sin(lat2)
                            const lat = Math.atan2(z, Math.sqrt(x * x + y * y)) / toRad
                            const lon = Math.atan2(y, x) / toRad
                            const cx = px(lon), cy = py(lat)
                            // Il salto all'antimeridiano spezza la linea.
                            if (i === 0 || (previousX !== null && Math.abs(cx - previousX) > width / 2))
                                ctx.moveTo(cx, cy)
                            else
                                ctx.lineTo(cx, cy)
                            previousX = cx
                        }
                        ctx.stroke()
                        ctx.globalAlpha = 1

                    }
                    ctx.fillStyle = Theme.primaryColor
                    ctx.beginPath(); ctx.arc(hx, hy, 4, 0, 2 * Math.PI); ctx.fill()
                    ctx.strokeStyle = Theme.primaryColor
                    ctx.lineWidth = 1.5
                    ctx.beginPath(); ctx.arc(hx, hy, 7, 0, 2 * Math.PI); ctx.stroke()
                }
            }
        }

        // La scritta quando non c'e' niente da mostrare.
        Text {
            anchors.centerIn: parent
            visible: root.grids.length === 0 && root.spots.length === 0 && !root.home.lat
            width: parent.width - 30
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("QSOs with a grid square appear here")
            color: Theme.textSecondary
            font.pixelSize: 12
        }

        // Legenda discreta, in basso.
        Row {
            anchors { left: parent.left; bottom: parent.bottom; margins: 6 }
            spacing: 10
            visible: root.spots.length > 0 || root.grids.length > 0
            Row {
                spacing: 4
                visible: root.grids.length > 0
                Rectangle { width: 6; height: 6; y: 4; color: Theme.secondaryColor }
                Text { text: qsTr("%1 grids").arg(root.grids.length); color: Theme.textSecondary; font.pixelSize: 10 }
            }
            Row {
                spacing: 4
                visible: root.spots.length > 0
                Rectangle { width: 6; height: 6; radius: 3; y: 4; color: Theme.accentColor }
                Text { text: qsTr("%1 spots").arg(root.spots.length); color: Theme.textSecondary; font.pixelSize: 10 }
            }
        }
    }
}
