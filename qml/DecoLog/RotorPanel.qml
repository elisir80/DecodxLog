// DecoLog — il rotore: la rosa, i gradi, e il bersaglio.
//
// Si vede dove guarda l'antenna e dove la si sta mandando; il lobo e' quello
// dichiarato nelle impostazioni. Cliccando sulla rosa si punta li'. Il pannello
// compare solo se il rotore e' acceso: chi non ce l'ha non se lo trova fra i
// piedi.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    readonly property var rotor: decolog.rotor
    readonly property var state: rotor.state
    readonly property real azimuth: state.az || 0
    readonly property real target: state.azTarget !== undefined && state.azTarget >= 0 ? state.azTarget : -1
    readonly property int beam: state.beamwidth || 45

    title: qsTr("Rotor")
    dotColor: state.connected ? (state.moving ? Theme.warningColor : Theme.accentColor) : Theme.errorColor
    padding: 8
    headerTools: [
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.rotor.lastTarget
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    ]

    implicitHeight: 200

    Connections {
        target: decolog.rotor
        function onStateChanged() { dial.requestPaint() }
        function onChanged() { dial.requestPaint() }
    }
    onAzimuthChanged: dial.requestPaint()

    RowLayout {
        anchors.fill: parent
        spacing: 10

        // ── La rosa ─────────────────────────────────────────────────────────
        Item {
            Layout.preferredWidth: 104
            Layout.preferredHeight: 104
            Layout.alignment: Qt.AlignVCenter

            Canvas {
                id: dial
                anchors.fill: parent

                onPaint: {
                    const ctx = getContext("2d")
                    const w = width
                    const h = height
                    const r = Math.min(w, h) / 2 - 6
                    const cx = w / 2
                    const cy = h / 2
                    ctx.reset()

                    // Il quadrante.
                    ctx.strokeStyle = Theme.glassBorder
                    ctx.fillStyle = Theme.bgMedium
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    ctx.arc(cx, cy, r, 0, Math.PI * 2)
                    ctx.fill()
                    ctx.stroke()

                    // Tacche ogni 30°, più lunghe ai punti cardinali.
                    ctx.strokeStyle = Theme.textSecondary
                    for (let a = 0; a < 360; a += 30) {
                        const rad = (a - 90) * Math.PI / 180
                        const inner = r - (a % 90 === 0 ? 10 : 5)
                        ctx.beginPath()
                        ctx.moveTo(cx + inner * Math.cos(rad), cy + inner * Math.sin(rad))
                        ctx.lineTo(cx + r * Math.cos(rad), cy + r * Math.sin(rad))
                        ctx.stroke()
                    }

                    if (!root.state.connected)
                        return

                    // Il lobo dell'antenna.
                    const half = root.beam / 2 * Math.PI / 180
                    const heading = (root.azimuth - 90) * Math.PI / 180
                    ctx.fillStyle = Qt.alpha(Theme.accentColor, 0.18)
                    ctx.beginPath()
                    ctx.moveTo(cx, cy)
                    ctx.arc(cx, cy, r - 2, heading - half, heading + half)
                    ctx.closePath()
                    ctx.fill()

                    // Dove punta adesso.
                    ctx.strokeStyle = Theme.accentColor
                    ctx.lineWidth = 2.5
                    ctx.beginPath()
                    ctx.moveTo(cx, cy)
                    ctx.lineTo(cx + (r - 3) * Math.cos(heading), cy + (r - 3) * Math.sin(heading))
                    ctx.stroke()

                    // Dove deve andare.
                    if (root.target >= 0) {
                        const t = (root.target - 90) * Math.PI / 180
                        ctx.strokeStyle = Theme.warningColor
                        ctx.lineWidth = 1.5
                        ctx.setLineDash([4, 3])
                        ctx.beginPath()
                        ctx.moveTo(cx, cy)
                        ctx.lineTo(cx + (r - 3) * Math.cos(t), cy + (r - 3) * Math.sin(t))
                        ctx.stroke()
                        ctx.setLineDash([])
                    }

                    ctx.fillStyle = Theme.textPrimary
                    ctx.beginPath()
                    ctx.arc(cx, cy, 3, 0, Math.PI * 2)
                    ctx.fill()
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: root.state.connected ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: (mouse) => {
                        // Puntare dove si clicca: e' il gesto più naturale su una rosa.
                        const dx = mouse.x - width / 2
                        const dy = mouse.y - height / 2
                        if (Math.sqrt(dx * dx + dy * dy) < 12)
                            return
                        const deg = (Math.atan2(dy, dx) * 180 / Math.PI + 90 + 360) % 360
                        root.rotor.pointTo(deg, "")
                    }
                }
            }

            // N E S O sul bordo.
            Repeater {
                model: [{ t: qsTr("N"), a: 0 }, { t: qsTr("E"), a: 90 },
                        { t: qsTr("S"), a: 180 }, { t: qsTr("W"), a: 270 }]
                Text {
                    required property var modelData
                    readonly property real radius: Math.min(parent.width, parent.height) / 2 - 14
                    x: parent.width / 2 + radius * Math.sin(modelData.a * Math.PI / 180) - width / 2
                    y: parent.height / 2 - radius * Math.cos(modelData.a * Math.PI / 180) - height / 2
                    text: modelData.t
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        // ── Numeri e comandi ────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4

            RowLayout {
                spacing: 6
                Text {
                    text: root.state.connected ? Math.round(root.azimuth) + "°" : "—"
                    color: Theme.textPrimary
                    font.family: Theme.monoFamily
                    font.pixelSize: 22
                    font.bold: true
                }
                Text {
                    visible: root.target >= 0
                    text: "→ " + Math.round(root.target) + "°"
                    color: Theme.warningColor
                    font.family: Theme.monoFamily
                    font.pixelSize: 13
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.rotor.status
                color: root.state.connected ? Theme.textSecondary : Theme.warningColor
                font.pixelSize: 11
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Item { Layout.fillHeight: true }

            Flow {
                Layout.fillWidth: true
                spacing: 4
                GlassButton {
                    text: "−10"
                    buttonHeight: 22
                    fontPixelSize: 11
                    enabled: root.state.connected
                    onClicked: root.rotor.nudge(-10)
                }
                GlassButton {
                    text: "−1"
                    buttonHeight: 22
                    fontPixelSize: 11
                    enabled: root.state.connected
                    onClicked: root.rotor.nudge(-1)
                }
                GlassButton {
                    text: "+1"
                    buttonHeight: 22
                    fontPixelSize: 11
                    enabled: root.state.connected
                    onClicked: root.rotor.nudge(1)
                }
                GlassButton {
                    text: "+10"
                    buttonHeight: 22
                    fontPixelSize: 11
                    enabled: root.state.connected
                    onClicked: root.rotor.nudge(10)
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 4
                GlassButton {
                    text: qsTr("STOP")
                    tone: Theme.errorColor
                    buttonHeight: 22
                    fontPixelSize: 11
                    enabled: root.state.connected
                    onClicked: root.rotor.stopNow(false)
                }
                GlassButton {
                    text: qsTr("Park")
                    buttonHeight: 22
                    fontPixelSize: 11
                    enabled: root.state.connected
                    onClicked: root.rotor.park()
                }
                GlassButton {
                    readonly property var info: decolog.callInfo
                    text: qsTr("On the DX")
                    tone: Theme.primaryColor
                    buttonHeight: 22
                    fontPixelSize: 11
                    enabled: root.state.connected && info.azimuth !== undefined
                    onClicked: root.rotor.pointTo(info.azimuth, info.call || "")
                }
            }
        }
    }
}
