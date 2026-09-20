// DecoDXLog — il rotore nella colonna di destra: il quadrante di DecoRotor in
// piccolo, i gradi, il bersaglio e i comandi che servono mentre si opera.
//
// Il pannello compare solo se il rotore e' acceso: chi non ce l'ha non se lo
// trova fra i piedi. Per il quadrante grande c'e' la finestra (Ctrl+R).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    signal windowRequested()

    readonly property var rotor: decolog.rotor
    readonly property var state: rotor.state
    readonly property var home: decolog.myPosition
    readonly property var dx: decolog.callInfo.position

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
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Open ▾")
            buttonHeight: 20
            fontPixelSize: 10
            onClicked: root.windowRequested()
        }
    ]

    implicitHeight: 208

    RowLayout {
        anchors.fill: parent
        spacing: 10

        RotorDial {
            Layout.preferredWidth: 132
            Layout.preferredHeight: 132
            Layout.alignment: Qt.AlignVCenter

            azimuth: root.state.az || 0
            target: root.state.azTarget !== undefined && root.state.azTarget >= 0 ? root.state.azTarget : -1
            beamwidth: root.state.beamwidth || 45
            hasPosition: root.state.connected === true
            moving: root.state.moving === true
            latitude: root.home && root.home.lat !== undefined ? root.home.lat : 41.5
            longitude: root.home && root.home.lon !== undefined ? root.home.lon : 12.5
            pinValid: root.dx !== undefined && root.dx !== null && root.dx.lat !== undefined
            pinLatitude: pinValid ? root.dx.lat : 0
            pinLongitude: pinValid ? root.dx.lon : 0

            onBearingRequested: (degrees) => root.rotor.pointTo(degrees, "")
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4

            RowLayout {
                spacing: 6
                Text {
                    text: root.state.connected ? Math.round(root.state.az || 0) + "°" : "—"
                    color: Theme.textPrimary
                    font.family: Theme.monoFamily
                    font.pixelSize: 22
                    font.bold: true
                }
                Text {
                    visible: (root.state.azTarget || -1) >= 0
                    text: "→ " + Math.round(root.state.azTarget || 0) + "°"
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
                    text: "−10"; buttonHeight: 22; fontPixelSize: 11
                    enabled: root.state.connected
                    onClicked: root.rotor.nudge(-10)
                }
                GlassButton {
                    text: "−1"; buttonHeight: 22; fontPixelSize: 11
                    enabled: root.state.connected
                    onClicked: root.rotor.nudge(-1)
                }
                GlassButton {
                    text: "+1"; buttonHeight: 22; fontPixelSize: 11
                    enabled: root.state.connected
                    onClicked: root.rotor.nudge(1)
                }
                GlassButton {
                    text: "+10"; buttonHeight: 22; fontPixelSize: 11
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
                    text: qsTr("Park"); buttonHeight: 22; fontPixelSize: 11
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
