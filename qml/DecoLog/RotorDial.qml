// DecoLog — il quadrante del rotore, lo stesso di DecoRotor.
//
// Corona graduata (tacche ogni 2°, numeri ogni 10°), mappa azimutale
// equidistante centrata sul proprio QTH — in quella proiezione la direzione
// letta sulla corona e' la rotta vera, e la distanza dal centro cresce con i
// chilometri — lobo d'antenna, bersaglio tratteggiato e ago della posizione.
// Un clic dentro il disco manda il rotore in quella direzione.
//
// La terraferma e' Natural Earth 110m (pubblico dominio), gli stessi anelli che
// usa DecoRotor, compilati dentro l'eseguibile: niente da scaricare.
import QtQuick
import QtQuick.Shapes

Item {
    id: dial

    RotorPalette { id: rt; dark: dial.nightMode }

    property bool nightMode: true
    property real azimuth: 0
    property real target: -1            // negativo: nessun bersaglio
    property real beamwidth: 45
    property bool hasPosition: false
    property bool moving: false
    property real limitMin: 0
    property real limitMax: 360
    // Il QTH al centro della mappa.
    property real latitude: 41.5
    property real longitude: 12.5
    // Un punto da segnare (il DX, uno spot): dove sta davvero sulla mappa.
    property real pinLatitude: 0
    property real pinLongitude: 0
    property bool pinValid: false

    signal bearingRequested(real degrees)

    readonly property real outerRadius: Math.min(width, height) / 2 - 2
    readonly property real ringThickness: Math.max(10, outerRadius * 0.15)
    readonly property real mapRadius: outerRadius - ringThickness
    readonly property real centreX: width / 2
    readonly property real centreY: height / 2
    // Sotto una certa misura i numeri non si leggerebbero: restano le tacche.
    readonly property bool showNumbers: outerRadius > 74

    // Il disco.
    Rectangle {
        anchors.centerIn: parent
        width: dial.outerRadius * 2
        height: width
        radius: width / 2
        color: rt.dialRing
        border.color: rt.border
        border.width: 1
    }

    // ── Corona dei gradi ────────────────────────────────────────────────────
    Canvas {
        id: scale
        anchors.fill: parent
        antialiasing: true
        renderStrategy: Canvas.Cooperative

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.translate(dial.centreX, dial.centreY)

            const outer = dial.outerRadius - 3
            const digits = Math.max(7, dial.ringThickness * 0.42)
            ctx.font = "bold " + digits + "px " + rt.monoFamily
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"

            const step = dial.showNumbers ? 2 : 10
            for (let deg = 0; deg < 360; deg += step) {
                const decade = deg % 10 === 0
                const cardinal = deg % 90 === 0
                const length = cardinal ? dial.ringThickness * 0.40
                             : decade ? dial.ringThickness * 0.30
                             : dial.ringThickness * 0.16

                ctx.save()
                ctx.rotate(deg * Math.PI / 180)
                ctx.beginPath()
                ctx.lineWidth = decade ? 2 : 1
                ctx.strokeStyle = cardinal ? rt.primary
                                : decade ? rt.textSecondary
                                : rt.textDim
                ctx.moveTo(0, -outer)
                ctx.lineTo(0, -outer + length)
                ctx.stroke()

                if (decade && dial.showNumbers) {
                    ctx.fillStyle = cardinal ? rt.primary : rt.textSecondary
                    ctx.fillText(String(deg), 0, -outer + dial.ringThickness * 0.68)
                }
                ctx.restore()
            }
        }
    }

    // ── Mappa azimutale equidistante ────────────────────────────────────────
    Canvas {
        id: world
        anchors.centerIn: parent
        width: dial.mapRadius * 2
        height: width
        antialiasing: true
        renderStrategy: Canvas.Cooperative

        property var rings: []
        readonly property var ranges: [5000, 10000, 15000]
        readonly property real halfWorldKm: Math.PI * 6371.0

        Component.onCompleted: {
            rings = decolog.landmasses()
            requestPaint()
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        function radius() { return Math.min(width, height) / 2 }

        // Il punto geografico dove finisce sulla tela.
        function project(lat, lon) {
            const rad = Math.PI / 180
            const lat0 = dial.latitude * rad
            const dlon = (lon - dial.longitude) * rad
            const phi = lat * rad

            const cosC = Math.sin(lat0) * Math.sin(phi)
                       + Math.cos(lat0) * Math.cos(phi) * Math.cos(dlon)
            const c = Math.acos(Math.min(1, Math.max(-1, cosC)))

            const y = Math.sin(dlon) * Math.cos(phi)
            const x = Math.cos(lat0) * Math.sin(phi) - Math.sin(lat0) * Math.cos(phi) * Math.cos(dlon)
            const azimuth = Math.atan2(y, x)

            const reach = radius() * c / Math.PI
            return Qt.point(width / 2 + reach * Math.sin(azimuth),
                            height / 2 - reach * Math.cos(azimuth))
        }

        function traceRing(ctx, ring) {
            // Un anello che passa vicino all'antipodo si spalancherebbe lungo il
            // bordo: dove il salto e' assurdo si chiude il tratto e se ne apre
            // un altro, cosi' la costa resta al suo posto.
            const jump = radius() * 1.1
            let previous = null
            let open = false
            for (let i = 0; i < ring.length; i += 2) {
                const point = project(ring[i + 1], ring[i])
                const far = previous !== null
                            && Math.hypot(point.x - previous.x, point.y - previous.y) > jump
                if (!open || far) {
                    if (open) { ctx.closePath(); ctx.fill(); ctx.stroke() }
                    ctx.beginPath()
                    ctx.moveTo(point.x, point.y)
                    open = true
                } else {
                    ctx.lineTo(point.x, point.y)
                }
                previous = point
            }
            if (open) { ctx.closePath(); ctx.fill(); ctx.stroke() }
        }

        function graticule(ctx) {
            const step = 3
            ctx.strokeStyle = rt.mapGrid
            ctx.lineWidth = 1
            for (let lat = -60; lat <= 60; lat += 30) {
                ctx.beginPath()
                for (let lon = -180; lon <= 180; lon += step) {
                    const point = project(lat, lon)
                    if (lon === -180) ctx.moveTo(point.x, point.y); else ctx.lineTo(point.x, point.y)
                }
                ctx.stroke()
            }
            for (let lon = -180; lon < 180; lon += 30) {
                ctx.beginPath()
                for (let lat = -87; lat <= 87; lat += step) {
                    const point = project(lat, lon)
                    if (lat === -87) ctx.moveTo(point.x, point.y); else ctx.lineTo(point.x, point.y)
                }
                ctx.stroke()
            }
        }

        onPaint: {
            const ctx = getContext("2d")
            const reach = radius()
            ctx.reset()
            ctx.save()

            // Il disco fa anche da maschera: niente esce dal quadrante.
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, reach, 0, 2 * Math.PI)
            ctx.clip()
            ctx.fillStyle = rt.dialFace
            ctx.fill()

            graticule(ctx)

            ctx.strokeStyle = rt.mapCoast
            ctx.lineWidth = 1
            for (let i = 0; i < rings.length; ++i) {
                // Tinte alternate solo per staccare una massa continentale
                // dall'altra: non hanno alcun significato geografico.
                ctx.fillStyle = rt.landColour(i)
                traceRing(ctx, rings[i])
            }

            // Cerchi di distanza: in questa proiezione sono cerchi veri.
            ctx.strokeStyle = rt.mapGrid
            for (const km of ranges) {
                if (km >= halfWorldKm)
                    continue
                ctx.beginPath()
                ctx.arc(width / 2, height / 2, reach * km / halfWorldKm, 0, 2 * Math.PI)
                ctx.stroke()
            }
            ctx.restore()
        }
    }

    // ── Settore vietato dai finecorsa ───────────────────────────────────────
    Shape {
        anchors.fill: parent
        visible: dial.limitMax - dial.limitMin < 359.5
        opacity: 0.85

        ShapePath {
            id: forbidden
            fillColor: Qt.rgba(0.85, 0.16, 0.16, 0.16)
            strokeColor: Qt.rgba(0.85, 0.16, 0.16, 0.40)
            strokeWidth: 1

            readonly property real from: dial.limitMax - 90
            readonly property real span: 360 - (dial.limitMax - dial.limitMin)
            readonly property real reach: dial.mapRadius

            startX: dial.centreX
            startY: dial.centreY
            PathLine {
                x: dial.centreX + Math.cos(forbidden.from * Math.PI / 180) * forbidden.reach
                y: dial.centreY + Math.sin(forbidden.from * Math.PI / 180) * forbidden.reach
            }
            PathAngleArc {
                centerX: dial.centreX
                centerY: dial.centreY
                radiusX: forbidden.reach
                radiusY: forbidden.reach
                startAngle: forbidden.from
                sweepAngle: forbidden.span
            }
            PathLine { x: dial.centreX; y: dial.centreY }
        }
    }

    // ── Il lobo: dove "vede" l'antenna ──────────────────────────────────────
    Shape {
        anchors.fill: parent
        visible: dial.hasPosition

        ShapePath {
            id: beam
            fillColor: Qt.rgba(0.13, 0.65, 0.45, 0.22)
            strokeColor: Qt.rgba(0.13, 0.65, 0.45, 0.55)
            strokeWidth: 1

            readonly property real from: dial.azimuth - dial.beamwidth / 2 - 90
            readonly property real span: dial.beamwidth
            readonly property real reach: dial.mapRadius

            startX: dial.centreX
            startY: dial.centreY
            PathLine {
                x: dial.centreX + Math.cos(beam.from * Math.PI / 180) * beam.reach
                y: dial.centreY + Math.sin(beam.from * Math.PI / 180) * beam.reach
            }
            PathAngleArc {
                centerX: dial.centreX
                centerY: dial.centreY
                radiusX: beam.reach
                radiusY: beam.reach
                startAngle: beam.from
                sweepAngle: beam.span
            }
            PathLine { x: dial.centreX; y: dial.centreY }
        }
    }

    // ── Bersaglio: dove sta andando ─────────────────────────────────────────
    Item {
        anchors.centerIn: parent
        width: 2
        height: dial.mapRadius * 2
        visible: dial.target >= 0
        rotation: dial.target

        Repeater {
            model: 10
            Rectangle {
                required property int index
                anchors.horizontalCenter: parent.horizontalCenter
                y: 6 + index * (dial.mapRadius / 10)
                width: 2
                height: Math.max(1, dial.mapRadius / 22)
                color: rt.primary
            }
        }
    }

    // Il punto segnato (il DX, uno spot) dove si trova davvero.
    Rectangle {
        readonly property point position: world.project(dial.pinLatitude, dial.pinLongitude)
        visible: dial.pinValid
        x: world.x + position.x - width / 2
        y: world.y + position.y - height / 2
        width: 9
        height: 9
        radius: 4.5
        color: rt.accent
        border.color: rt.dialFace
        border.width: 2
    }

    // ── Ago ─────────────────────────────────────────────────────────────────
    Item {
        id: needle
        anchors.centerIn: parent
        width: 8
        height: dial.mapRadius * 2
        visible: dial.hasPosition
        rotation: dial.azimuth

        // La rotazione piu' corta, come gira davvero il rotore.
        Behavior on rotation {
            RotationAnimation {
                duration: 220
                direction: RotationAnimation.Shortest
                easing.type: Easing.OutCubic
            }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 2
            width: 4
            height: parent.height / 2 - 2
            radius: 2
            color: dial.moving ? rt.needleMoving : rt.needle
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.verticalCenter
            width: 3
            height: parent.height * 0.10
            radius: 1.5
            color: rt.textDim
        }
    }

    // Il perno.
    Rectangle {
        anchors.centerIn: parent
        width: 14
        height: 14
        radius: 7
        color: rt.bgElevated
        border.color: rt.textSecondary
        border.width: 2
    }

    TapHandler {
        onTapped: (eventPoint) => {
            const dx = eventPoint.position.x - dial.centreX
            const dy = eventPoint.position.y - dial.centreY
            if (Math.sqrt(dx * dx + dy * dy) > dial.outerRadius)
                return
            const degrees = (Math.atan2(dx, -dy) * 180 / Math.PI + 360) % 360
            dial.bearingRequested(Math.round(degrees * 10) / 10)
        }
    }

    // Il QTH cambia (profilo stazione diverso): la mappa si rifa.
    onLatitudeChanged: world.requestPaint()
    onLongitudeChanged: world.requestPaint()
}
