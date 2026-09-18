// DecoLog — il rotore in grande: il quadrante di DecoRotor, i comandi e le
// rotte che il log conosce gia'.
//
// Qui il quadrante ha lo spazio che merita: corona graduata leggibile, mappa
// azimutale del proprio QTH, lobo, bersaglio. A destra le otto direzioni, i
// passi, lo STOP, il puntamento per locatore e i nominativi da puntare con un
// clic: quello che si sta lavorando e gli ultimi spot del cluster.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Decodium.UI

ApplicationWindow {
    id: root

    readonly property var rotor: decolog.rotor
    readonly property var state: rotor.state
    readonly property var home: decolog.myPosition
    readonly property var dx: decolog.callInfo.position
    property int revision: 0
    readonly property var spots: { revision; return decolog.cluster.mapSpots().slice(0, 8) }

    width: 900
    height: 620
    visible: true
    title: qsTr("DecoLog — Rotor")
    color: Theme.bgDeep

    Settings {
        category: "rotorWindow"
        property alias width: root.width
        property alias height: root.height
    }

    Connections {
        target: decolog.cluster.spots
        function onCountChanged() { root.revision++ }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // ── Quadrante ───────────────────────────────────────────────────────
        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 320
            Layout.preferredWidth: 560
            title: root.state.modelLabel || qsTr("Rotor")
            dotColor: root.state.connected ? (root.state.moving ? Theme.warningColor : Theme.accentColor)
                                           : Theme.errorColor
            headerTools: [
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.state.port ? qsTr("control box on %1").arg(root.state.port) : ""
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
            ]

            RotorDial {
                anchors.centerIn: parent
                width: Math.min(parent.width - 20, parent.height - 20)
                height: width

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
        }

        // ── Comandi ─────────────────────────────────────────────────────────
        ColumnLayout {
            Layout.preferredWidth: 300
            Layout.minimumWidth: 260
            Layout.maximumWidth: 320
            Layout.fillHeight: true
            spacing: 8

            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 128
                title: qsTr("Where it is pointing")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    RowLayout {
                        spacing: 8
                        Text {
                            text: root.state.connected ? Math.round(root.state.az || 0) + "°" : "—"
                            color: Theme.textPrimary
                            font.family: Theme.monoFamily
                            font.pixelSize: 34
                            font.bold: true
                        }
                        Text {
                            visible: (root.state.azTarget || -1) >= 0
                            text: "→ " + Math.round(root.state.azTarget || 0) + "°"
                            color: Theme.warningColor
                            font.family: Theme.monoFamily
                            font.pixelSize: 18
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.rotor.status
                        color: root.state.connected ? Theme.textSecondary : Theme.warningColor
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 176
                title: qsTr("Point")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    // Le otto direzioni, come sul frontalino.
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        columnSpacing: 4
                        rowSpacing: 4
                        Repeater {
                            model: [{ t: qsTr("N"), a: 0 }, { t: qsTr("NE"), a: 45 },
                                    { t: qsTr("E"), a: 90 }, { t: qsTr("SE"), a: 135 },
                                    { t: qsTr("S"), a: 180 }, { t: qsTr("SW"), a: 225 },
                                    { t: qsTr("W"), a: 270 }, { t: qsTr("NW"), a: 315 }]
                            GlassButton {
                                required property var modelData
                                Layout.fillWidth: true
                                text: modelData.t
                                buttonHeight: 26
                                fontPixelSize: 12
                                enabled: root.state.connected
                                onClicked: root.rotor.pointTo(modelData.a, modelData.t)
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Repeater {
                            model: [-10, -1, 1, 10]
                            GlassButton {
                                required property int modelData
                                Layout.fillWidth: true
                                text: (modelData > 0 ? "+" : "−") + Math.abs(modelData)
                                buttonHeight: 26
                                fontPixelSize: 12
                                enabled: root.state.connected
                                onClicked: root.rotor.nudge(modelData)
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        StyledTextField {
                            id: locatorField
                            Layout.fillWidth: true
                            uppercase: true
                            placeholderText: qsTr("locator, e.g. FN31PR")
                            Keys.onReturnPressed: root.rotor.pointLocator(text, false)
                        }
                        GlassButton {
                            text: qsTr("Go")
                            tone: Theme.primaryColor
                            filled: true
                            buttonHeight: 28
                            enabled: root.state.connected && locatorField.text.trim().length >= 4
                                     && root.state.backend !== "rotctld"
                            onClicked: root.rotor.pointLocator(locatorField.text, false)
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            GlassPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: qsTr("From the log and the cluster")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    GlassButton {
                        readonly property var info: decolog.callInfo
                        Layout.fillWidth: true
                        text: info.azimuth !== undefined
                              ? qsTr("On the DX: %1 · %2°").arg(info.call || "").arg(info.azimuth)
                              : qsTr("No call being worked")
                        tone: Theme.primaryColor
                        buttonHeight: 26
                        fontPixelSize: 12
                        enabled: root.state.connected && info.azimuth !== undefined
                        onClicked: root.rotor.pointTo(info.azimuth, info.call || "")
                    }

                    Text {
                        text: qsTr("Last spots")
                        color: Theme.secondaryColor
                        font.family: Theme.monoFamily
                        font.pixelSize: 11
                        font.bold: true
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.spots
                        ScrollBar.vertical: ScrollBar {}
                        delegate: ItemDelegate {
                            required property var modelData
                            width: ListView.view.width
                            height: 24
                            enabled: root.state.connected && modelData.azimuth !== undefined
                            contentItem: RowLayout {
                                spacing: 6
                                Text {
                                    Layout.preferredWidth: 110
                                    text: modelData.call
                                    color: Theme.textPrimary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Text {
                                    text: modelData.azimuth !== undefined ? modelData.azimuth + "°" : "—"
                                    color: Theme.accentColor
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 12
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.band || ""
                                    color: Theme.textSecondary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }
                            background: Rectangle {
                                color: parent.hovered ? Theme.glassOverlay : "transparent"
                                radius: 3
                            }
                            onClicked: root.rotor.pointTo(modelData.azimuth, modelData.call)
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: root.spots.length === 0
                            text: qsTr("No spot with a known bearing.")
                            color: Theme.textSecondary
                            font.pixelSize: 11
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                GlassButton {
                    Layout.fillWidth: true
                    text: qsTr("STOP")
                    tone: Theme.errorColor
                    filled: true
                    buttonHeight: 32
                    enabled: root.state.connected
                    onClicked: root.rotor.stopNow(false)
                }
                GlassButton {
                    text: qsTr("Park")
                    buttonHeight: 32
                    enabled: root.state.connected
                    onClicked: root.rotor.park()
                }
            }
        }
    }
}
