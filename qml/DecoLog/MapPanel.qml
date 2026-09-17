// DecoLog — mappa: i locatori lavorati su una proiezione equirettangolare, la
// stazione e la direzione verso il nominativo selezionato.
//
// Nel mockup e' un segnaposto "stile MapStatisticsPanel": qui c'e' una prima
// versione che usa solo i dati del log, senza mappe esterne.
import QtQuick
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    readonly property var target: decolog.callInfo.position
    readonly property var home: decolog.myPosition

    title: qsTr("Map")
    showDot: false
    padding: 8
    headerTools: [
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: [decolog.callInfo.call, decolog.callInfo.gridsquare].filter(s => s).join(" · ")
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
        }
    ]

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

            function px(lon) { return (lon + 180) / 360 * width }
            function py(lat) { return (90 - lat) / 180 * height }

            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()

                // Reticolo ogni 30° e l'equatore un po' piu' marcato.
                ctx.lineWidth = 1
                ctx.strokeStyle = Theme.borderSoft
                for (let lon = -150; lon < 180; lon += 30) {
                    ctx.beginPath(); ctx.moveTo(px(lon), 0); ctx.lineTo(px(lon), height); ctx.stroke()
                }
                for (let lat = -60; lat <= 60; lat += 30) {
                    ctx.strokeStyle = lat === 0 ? Theme.glassBorder : Theme.borderSoft
                    ctx.beginPath(); ctx.moveTo(0, py(lat)); ctx.lineTo(width, py(lat)); ctx.stroke()
                }

                ctx.fillStyle = Theme.secondaryColor
                ctx.globalAlpha = 0.65
                const points = decolog.gridPoints
                for (let i = 0; i < points.length; ++i)
                    ctx.fillRect(px(points[i].lon) - 1, py(points[i].lat) - 1, 2.5, 2.5)
                ctx.globalAlpha = 1.0

                if (root.home && root.home.lat !== undefined && root.target && root.target.lat !== undefined) {
                    ctx.strokeStyle = Theme.accentColor
                    ctx.lineWidth = 1.5
                    ctx.beginPath()
                    ctx.moveTo(px(root.home.lon), py(root.home.lat))
                    ctx.lineTo(px(root.target.lon), py(root.target.lat))
                    ctx.stroke()
                }
                if (root.target && root.target.lat !== undefined) {
                    ctx.fillStyle = Theme.accentColor
                    ctx.beginPath()
                    ctx.arc(px(root.target.lon), py(root.target.lat), 4, 0, Math.PI * 2)
                    ctx.fill()
                }
                if (root.home && root.home.lat !== undefined) {
                    ctx.fillStyle = Theme.warningColor
                    ctx.beginPath()
                    ctx.arc(px(root.home.lon), py(root.home.lat), 4, 0, Math.PI * 2)
                    ctx.fill()
                }
            }

            Connections {
                target: decolog
                function onLogChanged() { canvas.requestPaint() }
                function onLookupChanged() { canvas.requestPaint() }
                function onStationChanged() { canvas.requestPaint() }
            }
            Connections {
                target: Theme
                function onPaletteChanged() { canvas.requestPaint() }
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        Text {
            anchors.centerIn: parent
            visible: decolog.gridPoints.length === 0
            text: qsTr("QSOs with a grid square appear here")
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
        }
    }
}
