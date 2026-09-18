// DecoLog — il posto di comando DecoRotor, dentro DecoLog.
//
// E' la pagina "Controllo" del programma originale, rifatta com'e': testata con
// le spie, quadrante sopra e mondo vero sotto separati da una maniglia, e a
// destra display, memorie a tasto diretto, comandi e puntamento. Sotto, la
// striscia di stato. Il gateway e' lo stesso: qui cambia solo la finestra che lo
// mostra.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtCore

ApplicationWindow {
    id: root

    property date currentTime: new Date()
    // La stazione scelta sulla mappa: il quadrante ne mostra il puntino, cosi'
    // si legge subito la sua direzione sulla corona dei gradi.
    property var chosenSpot: null
    property bool nightMode: true

    RotorPalette { id: rt; dark: root.nightMode }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state
    readonly property var home: decolog.myPosition
    readonly property real homeLat: home && home.lat !== undefined ? home.lat : 41.5
    readonly property real homeLon: home && home.lon !== undefined ? home.lon : 12.5
    readonly property var bearing: rotor.bearing

    // Lo shack puo' avere un ultrawide scalato o un portatile: la finestra si
    // adatta allo spazio realmente disponibile invece di eccederlo.
    width: Math.min(1400, Screen.desktopAvailableWidth - 60)
    height: Math.min(900, Screen.desktopAvailableHeight - 40)
    minimumWidth: 940
    minimumHeight: 600
    visible: true
    title: qsTr("DecoRotor — controllo rotore PRO.SIS.TEL")
    color: rt.bgDeep

    Settings {
        category: "rotorWindow"
        property alias width: root.width
        property alias height: root.height
        property alias nightMode: root.nightMode
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: root.currentTime = new Date()
    }

    header: RotorTopBar {
        nightMode: root.nightMode
        onLightToggled: root.nightMode = !root.nightMode
    }

    footer: RotorStatus {
        nightMode: root.nightMode
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: rt.spacing
        spacing: rt.spacing

        // Quadrante sopra, mondo vero sotto: la maniglia decide quanto spazio
        // dare all'uno o all'altro secondo quello che si sta facendo.
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 660
            Layout.minimumWidth: 380
            orientation: Qt.Vertical

            handle: Rectangle {
                implicitHeight: 10
                color: "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: 54
                    height: 3
                    radius: 1.5
                    color: SplitHandle.pressed || SplitHandle.hovered ? rt.primary : rt.borderSoft
                }
            }

            RotorGlass {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 260

                RotorDial {
                    anchors.fill: parent
                    nightMode: root.nightMode
                    azimuth: root.st.az || 0
                    target: root.st.azTarget !== undefined && root.st.azTarget >= 0 ? root.st.azTarget : -1
                    beamwidth: root.st.beamwidth || 45
                    hasPosition: root.st.connected === true
                    moving: root.st.moving === true
                    limitMin: root.st.azMin !== undefined ? root.st.azMin : 0
                    limitMax: root.st.azMax !== undefined ? root.st.azMax : 360
                    latitude: root.homeLat
                    longitude: root.homeLon
                    pinLatitude: root.chosenSpot ? root.chosenSpot.lat
                               : (root.bearing.lat !== undefined ? root.bearing.lat : 0)
                    pinLongitude: root.chosenSpot ? root.chosenSpot.lon
                                : (root.bearing.lon !== undefined ? root.bearing.lon : 0)
                    pinValid: root.chosenSpot !== null || root.bearing.lat !== undefined
                    onBearingRequested: (degrees) => root.rotor.pointTo(degrees, "")
                }

                // Ora locale e QTH negli angoli liberi del riquadro, dove il
                // quadrante rotondo non arriva.
                Column {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    spacing: 0

                    Text {
                        text: Qt.formatTime(root.currentTime, "HH:mm")
                        color: rt.textSecondary
                        font.pixelSize: 22
                        font.family: rt.monoFamily
                        font.bold: true
                    }

                    Text {
                        text: Qt.formatDate(root.currentTime, "ddd d MMM")
                        color: rt.textDim
                        font.pixelSize: rt.fontSmall
                    }
                }

                Column {
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    spacing: 0

                    Text {
                        anchors.right: parent.right
                        text: root.st.locator || decolog.myGrid
                        color: rt.textSecondary
                        font.pixelSize: rt.fontBody
                        font.family: rt.monoFamily
                        font.bold: true
                    }

                    Text {
                        anchors.right: parent.right
                        text: qsTr("mappa azimutale dal QTH")
                        color: rt.textDim
                        font.pixelSize: rt.fontSmall
                    }
                }
            }

            Rectangle {
                SplitView.preferredHeight: 320
                SplitView.minimumHeight: 180

                color: rt.bgPanel
                border.color: rt.border
                border.width: 1
                radius: rt.radius
                clip: true

                RotorMap {
                    anchors.fill: parent
                    anchors.margins: 1
                    nightMode: root.nightMode
                    homeLatitude: root.homeLat
                    homeLongitude: root.homeLon
                    onSpotChosen: (spot) => root.chosenSpot = spot
                }
            }
        }

        // Su schermi bassi la colonna scorre invece di troncare i pannelli.
        ScrollView {
            Layout.fillHeight: true
            Layout.preferredWidth: 560
            Layout.minimumWidth: 450
            Layout.maximumWidth: 620
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: rt.spacing

                RotorDisplay {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.st.hasEl === true ? 220 : 176
                }

                RotorMemoryGrid {
                    Layout.fillWidth: true
                    onManageRequested: memories.open()
                }

                RotorCommandBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 122
                    onPresetsRequested: memories.open()
                }

                RotorPointing {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 104
                }
            }
        }
    }

    Popup {
        id: memories

        x: (root.width - width) / 2
        y: Math.max(20, (root.height - height) / 2)
        width: Math.min(560, root.width - 80)
        height: Math.min(480, root.height - 60)
        modal: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Item {}

        RotorMemories {
            anchors.fill: parent
        }
    }
}
