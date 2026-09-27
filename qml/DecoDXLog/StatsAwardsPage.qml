// DecoDXLog — la seconda pagina delle statistiche: come sono cresciuti i
// diplomi anno per anno (DXCC lavorati e confermati, zone CQ, locatori), le
// entita' e i nominativi piu' lavorati, e la tabella banda per modo.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Item {
    id: root

    property string mode: ""
    property int year: 0
    property int revision: 0

    readonly property var progress: { revision; return visible ? decolog.statsAwardProgress(mode) : [] }
    readonly property var entities: { revision; return visible ? decolog.statsTopEntities(mode, year, 15) : [] }
    readonly property var calls: { revision; return visible ? decolog.statsTopCalls(mode, year, 15) : [] }
    readonly property var bandMode: { revision; return visible ? decolog.statsBandMode(year) : [] }

    // Le serie della curva dei diplomi.
    readonly property var series: [
        { key: "dxcc", label: qsTr("DXCC worked"), color: Theme.primaryColor },
        { key: "dxccConfirmed", label: qsTr("DXCC confirmed"), color: Theme.accentColor },
        { key: "zones", label: qsTr("CQ zones"), color: Theme.warningColor },
        { key: "grids", label: qsTr("Grids"), color: Theme.secondaryColor }
    ]
    property var hidden: ({})

    GridLayout {
        anchors.fill: parent
        columns: 2
        columnSpacing: 8
        rowSpacing: 8

        // ── La curva dei diplomi ────────────────────────────────────────────
        GlassPanel {
            id: curve
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.columnSpan: 2
            Layout.minimumHeight: 220
            title: qsTr("Awards over the years · cumulative")
            dotColor: Theme.accentColor
            padding: 8

            readonly property int maximum: {
                let m = 1
                for (const row of root.progress)
                    for (const s of root.series)
                        if (!root.hidden[s.key]) m = Math.max(m, row[s.key] || 0)
                return m
            }
            onMaximumChanged: plot.requestPaint()

            ColumnLayout {
                anchors.fill: parent
                spacing: 4
                Row {
                    spacing: 14
                    Repeater {
                        model: root.series
                        Row {
                            required property var modelData
                            spacing: 5
                            opacity: root.hidden[modelData.key] ? 0.35 : 1
                            Rectangle { width: 14; height: 3; anchors.verticalCenter: parent.verticalCenter; color: modelData.color }
                            Text {
                                text: modelData.label + (root.progress.length ? " · " + (root.progress[root.progress.length - 1][modelData.key] || 0) : "")
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                            TapHandler {
                                onTapped: {
                                    const h = Object.assign({}, root.hidden)
                                    h[modelData.key] = !h[modelData.key]
                                    root.hidden = h
                                    plot.requestPaint()
                                }
                            }
                        }
                    }
                }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Text {
                        anchors.centerIn: parent
                        visible: root.progress.length === 0
                        text: qsTr("no QSO")
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }
                    // La scala a sinistra.
                    Column {
                        id: yAxis
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 16
                        width: 34
                        Repeater {
                            model: 5
                            Text {
                                required property int index
                                width: yAxis.width - 4
                                height: yAxis.height / 4
                                horizontalAlignment: Text.AlignRight
                                verticalAlignment: index === 4 ? Text.AlignBottom : Text.AlignTop
                                visible: index < 4 || true
                                text: Math.round(curve.maximum * (4 - index) / 4)
                                color: Theme.textSecondary
                                font.family: Theme.monoFamily
                                font.pixelSize: 9
                            }
                        }
                    }
                    Canvas {
                        id: plot
                        anchors.left: yAxis.right
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 16
                        renderStrategy: Canvas.Cooperative
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                        Connections {
                            target: root
                            function onProgressChanged() { plot.requestPaint() }
                        }
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            const rows = root.progress
                            ctx.strokeStyle = Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.18)
                            ctx.lineWidth = 1
                            for (let i = 0; i <= 4; ++i) {
                                const y = Math.round(height * i / 4) + 0.5
                                ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                            }
                            if (rows.length === 0)
                                return
                            const step = rows.length > 1 ? width / (rows.length - 1) : 0
                            for (const s of root.series) {
                                if (root.hidden[s.key])
                                    continue
                                ctx.strokeStyle = s.color
                                ctx.fillStyle = s.color
                                ctx.lineWidth = 2
                                ctx.beginPath()
                                for (let i = 0; i < rows.length; ++i) {
                                    const x = rows.length > 1 ? i * step : width / 2
                                    const y = height - height * (rows[i][s.key] || 0) / curve.maximum
                                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                                }
                                ctx.stroke()
                                for (let i = 0; i < rows.length; ++i) {
                                    const x = rows.length > 1 ? i * step : width / 2
                                    const y = height - height * (rows[i][s.key] || 0) / curve.maximum
                                    ctx.beginPath(); ctx.arc(x, y, 2.5, 0, Math.PI * 2); ctx.fill()
                                }
                            }
                        }
                    }
                    // Gli anni sotto.
                    Item {
                        anchors.left: plot.left
                        anchors.right: plot.right
                        anchors.bottom: parent.bottom
                        height: 14
                        Repeater {
                            model: root.progress
                            Text {
                                required property var modelData
                                required property int index
                                readonly property int every: Math.max(1, Math.ceil(root.progress.length * 40 / Math.max(1, plot.width)))
                                visible: index % every === 0 || index === root.progress.length - 1
                                x: (root.progress.length > 1 ? index * plot.width / (root.progress.length - 1) : plot.width / 2) - width / 2
                                text: modelData.year
                                color: Theme.textSecondary
                                font.family: Theme.monoFamily
                                font.pixelSize: 9
                            }
                        }
                    }
                }
            }
        }

        // ── Classifiche ─────────────────────────────────────────────────────
        component Ranking: GlassPanel {
            id: ranking
            property var rows: []
            property color barColor: Theme.primaryColor
            property bool calls: false
            readonly property int maximum: rows.length ? rows[0].count : 1
            padding: 8
            ListView {
                anchors.fill: parent
                clip: true
                model: ranking.rows
                spacing: 2
                ScrollBar.vertical: PanelScrollBar {}
                delegate: Item {
                    required property var modelData
                    required property int index
                    width: ListView.view.width - 16
                    height: 18
                    Text {
                        id: pos
                        width: 22
                        text: (index + 1) + "."
                        color: Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Rectangle {
                        anchors.left: pos.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: 14
                        radius: 2
                        width: (parent.width - pos.width - 44) * modelData.count / Math.max(1, ranking.maximum)
                        color: ranking.barColor
                        opacity: 0.35
                    }
                    Text {
                        anchors.left: pos.right
                        anchors.leftMargin: 4
                        anchors.right: count.left
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                        text: modelData.key
                        color: Theme.textPrimary
                        font.family: Theme.monoFamily
                        font.pixelSize: 11
                        TapHandler {
                            enabled: ranking.calls
                            onTapped: decolog.lookupCall = modelData.key
                        }
                    }
                    Text {
                        id: count
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.count
                        color: Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: 11
                    }
                }
            }
        }

        Ranking {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200
            title: qsTr("Most worked entities")
            dotColor: Theme.primaryColor
            rows: root.entities
        }
        Ranking {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200
            title: qsTr("Most worked callsigns")
            dotColor: Theme.secondaryColor
            barColor: Theme.secondaryColor
            rows: root.calls
            calls: true
        }

        // ── Banda per modo ──────────────────────────────────────────────────
        GlassPanel {
            id: matrix
            Layout.fillWidth: true
            Layout.columnSpan: 2
            Layout.preferredHeight: 60 + Math.max(1, matrix.modes.length) * 20
            title: qsTr("Band by mode")
            dotColor: Theme.warningColor
            padding: 8

            readonly property var bands: {
                const order = ["2190m", "630m", "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m",
                               "6m", "4m", "2m", "70cm", "23cm"]
                const seen = []
                for (const r of root.bandMode) if (seen.indexOf(r.band) < 0) seen.push(r.band)
                return seen.sort((a, b) => {
                    const ia = order.indexOf(a), ib = order.indexOf(b)
                    return (ia < 0 ? 99 : ia) - (ib < 0 ? 99 : ib)
                })
            }
            readonly property var modes: {
                const totals = {}
                for (const r of root.bandMode) totals[r.mode] = (totals[r.mode] || 0) + r.count
                return Object.keys(totals).sort((a, b) => totals[b] - totals[a])
            }
            readonly property int maximum: {
                let m = 1
                for (const r of root.bandMode) m = Math.max(m, r.count)
                return m
            }
            function countOf(band, mode) {
                for (const r of root.bandMode) if (r.band === band && r.mode === mode) return r.count
                return 0
            }

            Column {
                anchors.fill: parent
                spacing: 2
                readonly property real cellW: Math.max(34, (width - 60) / Math.max(1, matrix.bands.length))
                Row {
                    Item { width: 60; height: 14 }
                    Repeater {
                        model: matrix.bands
                        Text {
                            required property string modelData
                            width: parent.parent.cellW
                            horizontalAlignment: Text.AlignHCenter
                            text: modelData
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 10
                        }
                    }
                }
                Repeater {
                    model: matrix.modes
                    Row {
                        id: modeRow
                        required property string modelData
                        Text {
                            width: 60
                            height: 18
                            verticalAlignment: Text.AlignVCenter
                            text: modeRow.modelData
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 10
                        }
                        Repeater {
                            model: matrix.bands
                            Rectangle {
                                required property string modelData
                                readonly property int n: matrix.countOf(modelData, modeRow.modelData)
                                width: modeRow.parent.cellW - 2
                                height: 18
                                radius: 2
                                color: n > 0 ? Qt.rgba(Theme.warningColor.r, Theme.warningColor.g, Theme.warningColor.b,
                                                       0.15 + 0.7 * Math.sqrt(n / matrix.maximum))
                                             : Theme.bgMedium
                                Text {
                                    anchors.centerIn: parent
                                    visible: parent.n > 0
                                    text: parent.n
                                    color: Theme.textPrimary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 9
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
