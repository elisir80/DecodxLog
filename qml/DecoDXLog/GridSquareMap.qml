// DecoDXLog — i locatori a quattro caratteri su una proiezione equirettangolare:
// ogni quadrato (2° di longitudine per 1° di latitudine) colorato se lavorato,
// pieno se confermato. Il reticolo e le lettere sono quelli dei campi (20°×10°).
import QtQuick
import QtQuick.Layouts
import Decodium.UI

Rectangle {
    id: root

    property var grids: []
    property var hovered: null

    radius: 4
    color: Theme.bgMedium
    border.width: 1
    border.color: Theme.borderSoft
    clip: true

    onGridsChanged: canvas.requestPaint()
    Connections {
        target: Theme
        function onPaletteChanged() { canvas.requestPaint() }
    }

    // "JN71" -> {lon, lat} dell'angolo sud-ovest.
    function corner(grid) {
        const g = grid.toUpperCase()
        const lon = (g.charCodeAt(0) - 65) * 20 - 180 + parseInt(g.charAt(2)) * 2
        const lat = (g.charCodeAt(1) - 65) * 10 - 90 + parseInt(g.charAt(3))
        return { lon: lon, lat: lat }
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        anchors.margins: 1

        function px(lon) { return (lon + 180) / 360 * width }
        function py(lat) { return (90 - lat) / 180 * height }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()

            // Campi 20°×10° con la loro sigla.
            ctx.lineWidth = 1
            ctx.strokeStyle = Theme.borderSoft
            for (let f = 0; f <= 18; ++f) {
                ctx.beginPath(); ctx.moveTo(px(-180 + f * 20), 0); ctx.lineTo(px(-180 + f * 20), height); ctx.stroke()
            }
            for (let f = 0; f <= 18; ++f) {
                ctx.beginPath(); ctx.moveTo(0, py(-90 + f * 10)); ctx.lineTo(width, py(-90 + f * 10)); ctx.stroke()
            }
            ctx.fillStyle = Theme.borderSoft
            ctx.font = "9px " + Theme.monoFamily
            for (let x = 0; x < 18; ++x) {
                for (let y = 0; y < 18; ++y) {
                    ctx.fillText(String.fromCharCode(65 + x) + String.fromCharCode(65 + y),
                                 px(-180 + x * 20) + 2, py(-90 + (y + 1) * 10) + 10)
                }
            }

            const w = Math.max(2, width / 180)
            const h = Math.max(2, height / 180)
            for (const g of root.grids) {
                if (!/^[A-R]{2}[0-9]{2}$/i.test(g.grid))
                    continue
                const c = root.corner(g.grid)
                ctx.fillStyle = g.confirmed ? Theme.accentColor : Theme.warningColor
                ctx.globalAlpha = g.confirmed ? 0.95 : 0.6
                ctx.fillRect(px(c.lon), py(c.lat + 1), w, h)
                // Un quadrato e' piccolo su una mappa del mondo: un anello lo fa trovare.
                ctx.globalAlpha = 0.8
                ctx.strokeStyle = g.confirmed ? Theme.accentColor : Theme.warningColor
                ctx.lineWidth = 1.5
                ctx.beginPath()
                ctx.arc(px(c.lon) + w / 2, py(c.lat + 1) + h / 2, 7, 0, 2 * Math.PI)
                ctx.stroke()
            }
            ctx.globalAlpha = 1
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onPositionChanged: (mouse) => {
            const lon = mouse.x / width * 360 - 180
            const lat = 90 - mouse.y / height * 180
            if (lon < -180 || lon >= 180 || lat < -90 || lat >= 90) { root.hovered = null; return }
            const fx = Math.floor((lon + 180) / 20), fy = Math.floor((lat + 90) / 10)
            const sx = Math.floor(((lon + 180) % 20) / 2), sy = Math.floor((lat + 90) % 10)
            const grid = String.fromCharCode(65 + fx) + String.fromCharCode(65 + fy) + sx + sy
            root.hovered = { grid: grid, entry: root.grids.find(g => g.grid.toUpperCase() === grid) }
        }
        onExited: root.hovered = null
    }

    Rectangle {
        anchors { left: parent.left; bottom: parent.bottom; margins: 8 }
        implicitWidth: legend.implicitWidth + 16
        implicitHeight: legend.implicitHeight + 10
        radius: 4
        color: Qt.rgba(Theme.bgDeep.r, Theme.bgDeep.g, Theme.bgDeep.b, 0.85)
        border.color: Theme.borderSoft
        RowLayout {
            id: legend
            anchors.centerIn: parent
            spacing: 10
            Rectangle { implicitWidth: 10; implicitHeight: 10; color: Theme.accentColor }
            Text { text: qsTr("confirmed %1").arg(root.grids.filter(g => g.confirmed).length); color: Theme.textPrimary; font.pixelSize: 11 }
            Rectangle { implicitWidth: 10; implicitHeight: 10; color: Theme.warningColor; opacity: 0.6 }
            Text { text: qsTr("worked %1").arg(root.grids.length); color: Theme.textPrimary; font.pixelSize: 11 }
            Text {
                visible: root.hovered !== null
                text: root.hovered ? root.hovered.grid + " · " + (root.hovered.entry ? (root.hovered.entry.confirmed ? qsTr("confirmed") : qsTr("worked")) : qsTr("not worked")) : ""
                color: Theme.secondaryColor
                font.family: Theme.monoFamily
                font.pixelSize: 11
                font.bold: true
            }
        }
    }
}
