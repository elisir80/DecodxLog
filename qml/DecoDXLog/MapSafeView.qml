// Mappa compatibile: non usa Canvas, FBO o Qt Location. Serve quando il driver
// puo' disegnare l'interfaccia normale ma perde il compositing appena una Canvas
// Qt Quick viene resa visibile (caso osservato con KDE/Mesa vecchio). Le terre
// emerse sono una Shape vettoriale statica: resta una mappa leggibile senza
// riaprire il percorso grafico fragile.
import QtQuick
import QtQuick.Shapes
import Decodium.UI

Rectangle {
    id: root

    property bool showCoast: true
    property bool showGrids: true
    property bool showSpots: true
    property bool showRotor: true
    property var target: null
    property var home: null
    property var grids: []
    property var spots: []
    property var land: []

    readonly property int markerLimit: 1500
    readonly property var visibleGrids: limited(grids)
    readonly property var visibleSpots: limited(spots)
    readonly property bool hasHome: hasPosition(home)
    readonly property bool hasTarget: hasPosition(target)
    readonly property real homeX: hasHome ? px(home.lon) : 0
    readonly property real homeY: hasHome ? py(home.lat) : 0
    readonly property real targetX: hasTarget ? px(target.lon) : 0
    readonly property real targetY: hasTarget ? py(target.lat) : 0
    readonly property string landPath: makeLandPath(land, plot.width, plot.height)

    radius: 4
    color: Theme.bgMedium
    border.width: 1
    border.color: Theme.borderSoft
    clip: true

    function px(lon) { return (lon + 180) / 360 * width }
    function py(lat) { return (90 - lat) / 180 * height }
    function hasPosition(position) {
        return !!position && position.lat !== undefined && position.lon !== undefined
    }
    function limited(points) {
        if (!points || points.length <= markerLimit)
            return points || []
        // Per una carta del mondo altri marker si sovrapporrebbero comunque;
        // il limite mantiene la modalita' di recupero sempre reattiva.
        return points.slice(points.length - markerLimit)
    }
    function makeLandPath(rings, targetWidth, targetHeight) {
        if (!rings || rings.length === 0 || targetWidth <= 0 || targetHeight <= 0)
            return ""

        const commands = []
        for (let ringIndex = 0; ringIndex < rings.length; ++ringIndex) {
            const ring = rings[ringIndex]
            if (!ring || ring.length < 4)
                continue

            const firstX = (Number(ring[0]) + 180) / 360 * targetWidth
            const firstY = (90 - Number(ring[1])) / 180 * targetHeight
            commands.push("M " + firstX.toFixed(2) + " " + firstY.toFixed(2))
            for (let pointIndex = 2; pointIndex + 1 < ring.length; pointIndex += 2) {
                const x = (Number(ring[pointIndex]) + 180) / 360 * targetWidth
                const y = (90 - Number(ring[pointIndex + 1])) / 180 * targetHeight
                commands.push("L " + x.toFixed(2) + " " + y.toFixed(2))
            }
            commands.push("Z")
        }
        return commands.join(" ")
    }

    // Stessa interfaccia del renderer Canvas: MapPanel puo' chiedere un
    // aggiornamento senza sapere quale dei due renderer e' caricato.
    function repaintAll() {}
    function repaintBackground() {}
    function repaintOverlay() {}

    Item {
        id: plot
        anchors.fill: parent
        anchors.margins: 1
        clip: true

        // La Shape e' elaborata una sola volta quando arriva land.json o
        // cambia la dimensione. A differenza di Canvas non crea una texture
        // dinamica e non passa dal render thread di Qt Quick.
        Shape {
            id: landShape
            anchors.fill: parent
            visible: root.showCoast && root.landPath.length > 0
            z: 0

            ShapePath {
                strokeColor: Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g,
                                     Theme.textSecondary.b, 0.78)
                fillColor: Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g,
                                   Theme.primaryColor.b, 0.16)
                strokeWidth: 1
                fillRule: ShapePath.OddEvenFill

                PathSvg {
                    path: root.landPath
                }
            }
        }

        // Reticolo leggero, utile anche senza la cartografia dettagliata.
        Repeater {
            model: 11
            delegate: Rectangle {
                x: (index + 1) * plot.width / 12
                width: 1
                height: plot.height
                color: Theme.borderSoft
                opacity: 0.55
                z: 1
            }
        }
        Repeater {
            model: 5
            delegate: Rectangle {
                y: (index + 1) * plot.height / 6
                width: plot.width
                height: 1
                color: Theme.borderSoft
                opacity: index === 2 ? 0.82 : 0.55
                z: 1
            }
        }

        // I locatori lavorati. Sono volutamente item Qt Quick normali, non
        // una texture dinamica caricata dal render thread.
        Repeater {
            model: root.showGrids ? root.visibleGrids : []
            delegate: Rectangle {
                required property var modelData
                width: 3
                height: 3
                radius: 1.5
                x: root.px(modelData.lon) - width / 2
                y: root.py(modelData.lat) - height / 2
                color: Theme.secondaryColor
                opacity: 0.82
                z: 2
            }
        }

        // Una linea diretta e' piu' economica dell'ortodromia campionata. Se
        // attraverserebbe l'antimeridiano, mostriamo i due punti senza una
        // riga sbagliata che taglia tutta la carta.
        Rectangle {
            readonly property real dx: root.targetX - root.homeX
            readonly property real dy: root.targetY - root.homeY
            visible: root.hasHome && root.hasTarget && Math.abs(dx) <= plot.width / 2
            x: root.homeX
            y: root.homeY - height / 2
            width: Math.sqrt(dx * dx + dy * dy)
            height: 1.5
            transformOrigin: Item.Left
            rotation: Math.atan2(dy, dx) * 180 / Math.PI
            color: Theme.accentColor
            opacity: 0.85
            z: 3
        }

        // Punto DX scelto.
        Rectangle {
            visible: root.hasTarget
            width: 8
            height: 8
            radius: 4
            x: root.targetX - width / 2
            y: root.targetY - height / 2
            color: Theme.accentColor
            z: 5
        }

        // Stazione e, quando disponibile, il verso dell'antenna.
        Rectangle {
            readonly property var rotor: decolog.rotor.state
            visible: root.hasHome && rotor.connected === true && root.showRotor
            x: root.homeX
            y: root.homeY - height / 2
            width: Math.min(plot.width, plot.height) * 0.22
            height: 2
            transformOrigin: Item.Left
            rotation: (rotor.az || 0) - 90
            color: Theme.warningColor
            opacity: 0.85
            z: 4
        }
        Rectangle {
            visible: root.hasHome
            width: 14
            height: 14
            radius: 7
            x: root.homeX - width / 2
            y: root.homeY - height / 2
            color: "transparent"
            border.color: Theme.primaryColor
            border.width: 1.5
            z: 5
        }
        Rectangle {
            visible: root.hasHome
            width: 8
            height: 8
            radius: 4
            x: root.homeX - width / 2
            y: root.homeY - height / 2
            color: Theme.primaryColor
            z: 6
        }

        // Spot cluster, disegnati dopo la rotta cosi' restano sempre cliccabili
        // a vista anche nei pile-up con molti segnali.
        Repeater {
            model: root.showSpots ? root.visibleSpots : []
            delegate: Rectangle {
                required property var modelData
                readonly property bool isNew: (modelData.status & 1) !== 0
                readonly property bool isBandOrMode: (modelData.status & 6) !== 0
                width: isNew ? 8 : 6
                height: width
                radius: width / 2
                x: root.px(modelData.lon) - width / 2
                y: root.py(modelData.lat) - height / 2
                color: isNew ? Theme.errorColor
                             : isBandOrMode ? Theme.warningColor : Theme.accentColor
                opacity: isNew || isBandOrMode ? 0.95 : 0.65
                z: 7
            }
        }
    }
}
