// DecoLog — la mappa satellitare sotto il quadrante, come nel posto di comando.
//
// I riquadri arrivano dal gateway DecoRotor, che fa da cache: quello che si e'
// gia' guardato resta su disco e si rivede anche con la rete giu'; se il gateway
// non c'e' si resta sulla cartografia di OpenStreetMap. Sopra la mappa compaiono
// gli spot del cluster di DecoLog, ognuno con la sua rotta dal QTH; toccarne uno
// porta l'antenna li' sopra.
// Copia di `desktop/qml/DecoRotor/SatelliteMap.qml`, con gli spot di DecoLog.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtLocation
import QtPositioning

Item {
    id: view

    property real homeLatitude: 41.5
    property real homeLongitude: 12.5
    property string selectedCall: ""
    property bool nightMode: true

    // Vero dopo la prima inquadratura: da li' in poi comanda l'operatore.
    property bool framed: false
    property int revision: 0

    RotorPalette { id: rt; dark: view.nightMode }

    readonly property var rotor: decolog.rotor
    readonly property var spots: { revision; return decolog.cluster.mapSpots() }
    readonly property int spotCount: spots.length

    readonly property var selectedSpot: {
        for (let i = 0; i < spots.length; ++i) {
            if (spots[i].call === view.selectedCall)
                return spots[i]
        }
        return null
    }

    readonly property var home: QtPositioning.coordinate(homeLatitude, homeLongitude)

    signal spotChosen(var spot)

    onSelectedSpotChanged: view.spotChosen(selectedSpot)

    Connections {
        target: decolog.cluster.spots
        function onCountChanged() { view.revision++ }
    }

    // La larghezza definitiva arriva dopo il primo passaggio del layout: e'
    // quella che dice quanto mondo ci sta, quindi si inquadra allora.
    onWidthChanged: {
        if (!framed && width > 0) {
            framed = true
            centreOnHome()
        }
    }

    function centreOnHome() {
        map.center = view.home
        map.zoomLevel = Math.max(map.minimumZoomLevel,
                                 Math.min(5, Math.log2(Math.max(width, 320) / 256)))
    }

    function frameSpots() {
        if (view.spotCount === 0) {
            centreOnHome()
            return
        }
        map.fitViewportToMapItems()
    }

    // Ortodromia campionata: la rotta vera, non il segmento sulla carta.
    function greatCircle(from, to) {
        const rad = Math.PI / 180
        const lat1 = from.latitude * rad
        const lon1 = from.longitude * rad
        const lat2 = to.latitude * rad
        const lon2 = to.longitude * rad

        const delta = 2 * Math.asin(Math.sqrt(
            Math.pow(Math.sin((lat1 - lat2) / 2), 2)
            + Math.cos(lat1) * Math.cos(lat2) * Math.pow(Math.sin((lon1 - lon2) / 2), 2)))

        const steps = 96
        const legs = []
        let leg = []
        let previous = null

        for (let i = 0; i <= steps; ++i) {
            const f = i / steps
            let lat
            let lon
            if (delta === 0) {
                lat = from.latitude
                lon = from.longitude
            } else {
                const a = Math.sin((1 - f) * delta) / Math.sin(delta)
                const b = Math.sin(f * delta) / Math.sin(delta)
                const x = a * Math.cos(lat1) * Math.cos(lon1) + b * Math.cos(lat2) * Math.cos(lon2)
                const y = a * Math.cos(lat1) * Math.sin(lon1) + b * Math.cos(lat2) * Math.sin(lon2)
                const z = a * Math.sin(lat1) + b * Math.sin(lat2)
                lat = Math.atan2(z, Math.sqrt(x * x + y * y)) / rad
                lon = Math.atan2(y, x) / rad
            }
            // Dove la rotta passa l'antimeridiano la linea si spezza, altrimenti
            // attraverserebbe la carta da parte a parte.
            if (previous !== null && Math.abs(lon - previous) > 180) {
                legs.push(leg)
                leg = []
            }
            leg.push(QtPositioning.coordinate(lat, lon))
            previous = lon
        }
        legs.push(leg)
        return legs
    }

    Plugin {
        id: tiles

        name: "osm"

        PluginParameter {
            name: "osm.useragent"
            value: "DecoLog/DecoRotor (rotore d'antenna amatoriale)"
        }

        // Con il gateway in piedi i riquadri arrivano da li'; se la porta HTTP
        // non ha risposto si resta sulla cartografia stradale di OpenStreetMap.
        PluginParameter {
            name: "osm.mapping.providersrepository.disabled"
            value: view.rotor.tileEndpoint.length > 0
        }

        PluginParameter {
            name: "osm.mapping.custom.host"
            value: view.rotor.tileEndpoint
        }

        PluginParameter {
            name: "osm.mapping.custom.mapcopyright"
            value: qsTr("Riquadri dal gateway DecoRotor")
        }
    }

    Map {
        id: map

        anchors.fill: parent
        plugin: tiles
        center: view.home
        minimumZoomLevel: 1.5
        maximumZoomLevel: 17
        copyrightsVisible: false
        color: rt.dialFace

        onSupportedMapTypesChanged: map.chooseMapType()
        Component.onCompleted: map.chooseMapType()

        function chooseMapType() {
            for (let i = 0; i < supportedMapTypes.length; ++i) {
                if (supportedMapTypes[i].style === MapType.CustomMap) {
                    activeMapType = supportedMapTypes[i]
                    return
                }
            }
            if (supportedMapTypes.length > 0)
                activeMapType = supportedMapTypes[0]
        }

        DragHandler {
            target: null
            onTranslationChanged: (delta) => map.pan(-delta.x, -delta.y)
        }

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            rotationScale: 1 / 120
            property: "zoomLevel"
        }

        PinchHandler {
            id: pinch

            target: null
            property var origin: QtPositioning.coordinate()

            onActiveChanged: {
                if (active)
                    origin = map.toCoordinate(pinch.centroid.position, false)
            }
            onScaleChanged: (delta) => {
                map.zoomLevel += Math.log2(delta)
                map.alignCoordinateToPoint(pinch.origin, pinch.centroid.position)
            }
        }

        // Rotta dal QTH alla stazione scelta, spezzata dove passa l'antimeridiano.
        MapItemView {
            model: view.selectedSpot === null
                   ? []
                   : view.greatCircle(view.home,
                                      QtPositioning.coordinate(view.selectedSpot.lat,
                                                               view.selectedSpot.lon))

            delegate: MapPolyline {
                required property var modelData

                line.width: 3
                line.color: rt.primary
                opacity: 0.95
                path: modelData
                z: 2
            }
        }

        // Il proprio QTH.
        MapQuickItem {
            coordinate: view.home
            anchorPoint.x: 9
            anchorPoint.y: 9
            z: 3

            sourceItem: Item {
                width: 18
                height: 18

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: "transparent"
                    border.color: rt.accent
                    border.width: 2
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 6
                    height: 6
                    radius: 3
                    color: rt.accent
                }
            }
        }

        // Gli spot del cluster.
        MapItemView {
            model: view.spots

            delegate: MapQuickItem {
                id: pin

                required property var modelData

                readonly property bool chosen: modelData.call === view.selectedCall
                readonly property bool working: modelData.call === decolog.callInfo.call
                readonly property real freshness: Math.max(0, 1 - (modelData.age || 0) / 900)

                coordinate: QtPositioning.coordinate(modelData.lat, modelData.lon)
                anchorPoint.x: 7
                anchorPoint.y: 7
                z: chosen ? 6 : 4

                sourceItem: Item {
                    width: 14
                    height: 14

                    Rectangle {
                        id: dot

                        anchors.centerIn: parent
                        width: pin.chosen ? 14 : 11
                        height: width
                        radius: width / 2
                        color: pin.working ? rt.warning : rt.primary
                        opacity: 0.35 + 0.65 * pin.freshness
                        border.color: pin.chosen ? rt.textPrimary : Qt.rgba(0, 0, 0, 0.55)
                        border.width: pin.chosen ? 2 : 1

                        Behavior on width {
                            NumberAnimation { duration: 120 }
                        }
                    }

                    Text {
                        anchors.left: dot.right
                        anchors.leftMargin: 4
                        anchors.verticalCenter: dot.verticalCenter
                        visible: pin.chosen || pin.working
                                 || view.spotCount <= 15 || map.zoomLevel > 3.2
                        text: pin.modelData.call
                        color: rt.textPrimary
                        font.pixelSize: 11
                        font.bold: true
                        style: Text.Outline
                        styleColor: rt.dark ? "#000000" : "#FFFFFF"
                    }

                    TapHandler {
                        onTapped: view.selectedCall = pin.modelData.call
                    }

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
        }
    }

    // ── fascia superiore: cosa sta arrivando dal cluster ────────────────────
    Rectangle {
        id: header

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 34
        color: rt.dark ? Qt.rgba(0.05, 0.08, 0.13, 0.72) : Qt.rgba(1.0, 1.0, 1.0, 0.78)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 6
            spacing: 10

            RotorLed {
                label: decolog.cluster.onlineCount > 0
                       ? qsTr("CLUSTER %1").arg(decolog.cluster.onlineCount)
                       : qsTr("CLUSTER SPENTO")
                colour: decolog.cluster.onlineCount > 0 ? rt.accent : rt.danger
                blinking: false
            }

            Text {
                text: qsTr("%n stazione/i", "", view.spotCount)
                color: rt.textSecondary
                font.pixelSize: rt.fontSmall
            }

            Item { Layout.fillWidth: true }

            Repeater {
                model: [
                    { glyph: "+", action: "in" },
                    { glyph: "−", action: "out" },
                    { glyph: "⤢", action: "fit" },
                    { glyph: "⌂", action: "home" },
                    { glyph: "✕", action: "clear" }
                ]

                Rectangle {
                    id: knob

                    required property var modelData

                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 24
                    radius: 6
                    color: touch.hovered ? rt.bgElevated : "transparent"
                    border.color: rt.borderSoft
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: knob.modelData.glyph
                        color: rt.textSecondary
                        font.pixelSize: 13
                        font.bold: true
                    }

                    HoverHandler {
                        id: touch
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: {
                            switch (knob.modelData.action) {
                            case "in": map.zoomLevel = Math.min(map.maximumZoomLevel, map.zoomLevel + 1); break
                            case "out": map.zoomLevel = Math.max(map.minimumZoomLevel, map.zoomLevel - 1); break
                            case "fit": view.frameSpots(); break
                            case "home": view.centreOnHome(); break
                            case "clear": view.selectedCall = ""; break
                            }
                        }
                    }
                }
            }
        }
    }

    // ── scheda della stazione scelta ───────────────────────────────────────
    Rectangle {
        id: card

        anchors.left: parent.left
        anchors.bottom: footer.top
        anchors.margins: 10
        width: 252
        height: 108
        radius: 10
        color: rt.dark ? Qt.rgba(0.05, 0.08, 0.13, 0.72) : Qt.rgba(1.0, 1.0, 1.0, 0.78)
        border.color: rt.border
        border.width: 1
        visible: view.selectedSpot !== null

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    Layout.fillWidth: true
                    text: view.selectedSpot ? view.selectedSpot.call : ""
                    color: rt.textPrimary
                    font.pixelSize: 18
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    text: "✕"
                    color: close.hovered ? rt.textPrimary : rt.textDim
                    font.pixelSize: 13

                    HoverHandler {
                        id: close
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: view.selectedCall = ""
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: view.selectedSpot
                      ? qsTr("%1 · %2° · %3 km").arg(view.selectedSpot.grid || "—")
                            .arg(view.selectedSpot.az)
                            .arg(view.selectedSpot.km)
                      : ""
                color: rt.textSecondary
                font.pixelSize: rt.fontSmall
                font.family: rt.monoFamily
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: {
                    if (view.selectedSpot === null)
                        return ""
                    const parts = []
                    if (view.selectedSpot.band)
                        parts.push(view.selectedSpot.band)
                    if (view.selectedSpot.mode)
                        parts.push(view.selectedSpot.mode)
                    if (view.selectedSpot.snr !== null && view.selectedSpot.snr !== undefined)
                        parts.push(view.selectedSpot.snr + " dB")
                    parts.push(Math.round(view.selectedSpot.age) + " s fa")
                    return parts.join(" · ")
                }
                color: rt.textDim
                font.pixelSize: rt.fontSmall
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                RotorButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    text: qsTr("PUNTA")
                    kind: 1
                    onClicked: view.rotor.pointTo(view.selectedSpot.az, view.selectedSpot.call)
                }

                RotorButton {
                    Layout.preferredWidth: 94
                    Layout.preferredHeight: 30
                    text: qsTr("MEMORIA")
                    onClicked: view.rotor.savePresetHere(view.selectedSpot.call)
                }
            }
        }
    }

    // ── fascia inferiore: puntamento per locatore ──────────────────────────
    RotorLocatorBar {
        id: footer

        nightMode: view.nightMode
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: footer.top
        anchors.margins: 6
        text: view.rotor.tileEndpoint.length > 0 ? qsTr("riquadri dal gateway DecoRotor")
                                                 : qsTr("© OpenStreetMap contributors")
        color: rt.textDim
        font.pixelSize: 9
        opacity: 0.85
    }
}
