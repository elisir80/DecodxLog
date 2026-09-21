// DecoDXLog — la scheda CONTROLLO del posto di comando: quadrante sopra, mondo
// vero sotto, e a destra display, memorie, comandi e puntamento.
// Copia di `desktop/qml/DecoRotor/ControlPage.qml`.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: page

    property bool nightMode: true
    property date currentTime: new Date()
    // La stazione scelta sulla mappa: il quadrante ne mostra il puntino, cosi'
    // si legge subito la sua direzione sulla corona dei gradi.
    property var chosenSpot: null

    RotorPalette { id: rt; dark: page.nightMode }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state
    readonly property var home: decolog.myPosition
    readonly property real homeLat: home && home.lat !== undefined ? home.lat : 41.5
    readonly property real homeLon: home && home.lon !== undefined ? home.lon : 12.5
    readonly property var bearing: rotor.bearing

    spacing: rt.spacing

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: page.currentTime = new Date()
    }

    // Quadrante sopra, mondo vero sotto: la maniglia decide quanto spazio dare
    // all'uno o all'altro secondo quello che si sta facendo.
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
                nightMode: page.nightMode
                azimuth: page.st.az || 0
                target: page.st.azTarget !== undefined && page.st.azTarget >= 0 ? page.st.azTarget : -1
                beamwidth: page.st.beamwidth || 45
                hasPosition: page.st.connected === true
                moving: page.st.moving === true
                limitMin: page.st.azMin !== undefined ? page.st.azMin : 0
                limitMax: page.st.azMax !== undefined ? page.st.azMax : 360
                latitude: page.homeLat
                longitude: page.homeLon
                pinLatitude: page.chosenSpot ? page.chosenSpot.lat
                           : (page.bearing.lat !== undefined ? page.bearing.lat : 0)
                pinLongitude: page.chosenSpot ? page.chosenSpot.lon
                            : (page.bearing.lon !== undefined ? page.bearing.lon : 0)
                pinValid: page.chosenSpot !== null || page.bearing.lat !== undefined
                onBearingRequested: (degrees) => page.rotor.pointTo(degrees, "")
            }

            // Ora locale e QTH negli angoli liberi del riquadro, dove il
            // quadrante rotondo non arriva.
            Column {
                anchors.top: parent.top
                anchors.left: parent.left
                spacing: 0

                Text {
                    text: Qt.formatTime(page.currentTime, "HH:mm")
                    color: rt.textSecondary
                    font.pixelSize: 22
                    font.family: rt.monoFamily
                    font.bold: true
                }

                Text {
                    text: Qt.formatDate(page.currentTime, "ddd d MMM")
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
                    text: page.st.locator || decolog.myGrid
                    color: rt.textSecondary
                    font.pixelSize: rt.fontBody
                    font.family: rt.monoFamily
                    font.bold: true
                }

                Text {
                    anchors.right: parent.right
                    text: qsTr("azimuthal map from the QTH")
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
                nightMode: page.nightMode
                homeLatitude: page.homeLat
                homeLongitude: page.homeLon
                onSpotChosen: (spot) => page.chosenSpot = spot
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
                Layout.preferredHeight: page.st.hasEl === true ? 220 : 176
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

    Popup {
        id: memories

        parent: Overlay.overlay
        x: (parent.width - width) / 2
        y: Math.max(20, (parent.height - height) / 2)
        width: Math.min(560, parent.width - 80)
        height: Math.min(480, parent.height - 60)
        modal: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Item {}

        RotorMemories {
            anchors.fill: parent
        }
    }
}
