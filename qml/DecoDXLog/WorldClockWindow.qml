// DecoDXLog — l'orologio mondiale in grande: la mappa giorno/notte con la
// grayline, le citta' con l'ora e il Sole, e i dettagli di quella scelta.
// Si chiude con la ✕, con Esc o con un clic fuori; il focus torna al
// pulsante della barra in basso.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI
import DecoDXLog.Native

Popup {
    id: root

    readonly property var clock: decolog.worldClock
    // Chi l'ha aperta: ci torna il focus quando si chiude.
    property Item opener: null

    modal: true
    dim: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    width: Math.min(1360, (Overlay.overlay ? Overlay.overlay.width : 1400) - 32)
    height: Math.min(844, (Overlay.overlay ? Overlay.overlay.height : 900) - 32)
    padding: 0
    onClosed: if (root.opener) root.opener.forceActiveFocus()

    Overlay.modal: Rectangle { color: Qt.rgba(3 / 255, 6 / 255, 12 / 255, 0.78) }

    background: Item {
        // Un bagliore ciano leggero intorno.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -6
            radius: 22
            color: "transparent"
            border.width: 6
            border.color: Qt.rgba(Theme.secondaryColor.r, Theme.secondaryColor.g, Theme.secondaryColor.b, 0.10)
        }
        Rectangle {
            anchors.fill: parent
            radius: 16
            color: Theme.bgDeep
            border.color: Theme.borderColor
            border.width: 1
        }
    }

    component Mono: Text {
        color: Theme.textPrimary
        font.family: Theme.monoFamily
    }
    component Label2: Text {
        color: Theme.textSecondary
        font.family: Theme.uiFamily
        font.pixelSize: 13
    }
    component StateDot: Row {
        property string state: "day"
        property string label: ""
        spacing: 5
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 8
            height: 8
            radius: 4
            color: parent.state === "day" ? "#FFE9A8" : parent.state === "grayline" ? Theme.secondaryColor : Theme.textSecondary
        }
        Text {
            text: parent.label
            color: parent.state === "grayline" ? Theme.secondaryColor : Theme.textSecondary
            font.family: Theme.uiFamily
            font.pixelSize: 12
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ── Intestazione ────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            Layout.leftMargin: 24
            Layout.rightMargin: 12
            spacing: 16
            ColumnLayout {
                spacing: 0
                Text {
                    text: qsTr("World clock")
                    color: Theme.textPrimary
                    font.family: Theme.uiFamily
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                }
                Label2 { text: qsTr("Time zones, sunrise, sunset and grayline in real time") }
            }
            Item { Layout.fillWidth: true }
            Row {
                spacing: 10
                Text {
                    anchors.baseline: utcBig.baseline
                    text: "UTC"
                    color: Theme.secondaryColor
                    font.family: Theme.monoFamily
                    font.pixelSize: 16
                    font.bold: true
                }
                Mono {
                    id: utcBig
                    text: root.clock.utcClock
                    font.pixelSize: 44
                }
                Mono {
                    anchors.baseline: utcBig.baseline
                    text: root.clock.utcDate
                    color: Theme.textSecondary
                    font.pixelSize: 15
                }
            }
            Item { Layout.fillWidth: true }
            Label2 {
                text: qsTr("Sun overhead: %1").arg(root.clock.subSolar.text)
                font.family: Theme.monoFamily
            }
            AbstractButton {
                id: closeButton
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                Accessible.name: qsTr("Close")
                background: Rectangle {
                    radius: 8
                    color: closeButton.hovered ? Theme.panelHeader : "transparent"
                    border.color: closeButton.visualFocus ? Theme.secondaryColor : "transparent"
                    border.width: 2
                }
                contentItem: Item {
                    WorldClockIcon { anchors.centerIn: parent; kind: "close"; color: Theme.textPrimary; width: 16; height: 16 }
                }
                onClicked: root.close()
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderColor }

        // ── Corpo: mappa e dettagli a sinistra, citta' a destra ─────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 16
            spacing: 16

            Flickable {
                id: leftScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: leftColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: PanelScrollBar {}

                ColumnLayout {
                    id: leftColumn
                    // Un margine per la barra di scorrimento: non copre la mappa.
                    width: leftScroll.width - 12
                    spacing: 10

                    // La mappa, 2:1.
                    Item {
                        id: mapBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: width / 2

                        WorldMapItem {
                            anchors.fill: parent
                            detailed: true
                            time: root.clock.now
                            gridColor: Theme.textSecondary
                            graylineColor: Theme.secondaryColor
                        }
                        // Il Sole a picco.
                        Rectangle {
                            readonly property var sun: root.clock.subSolar
                            width: 24
                            height: 24
                            radius: 12
                            color: "#FFE9A8"
                            x: (sun.lon + 180) / 360 * mapBox.width - width / 2
                            y: (90 - sun.lat) / 180 * mapBox.height - height / 2
                            Rectangle {
                                anchors.centerIn: parent
                                width: 40
                                height: 40
                                radius: 20
                                color: "transparent"
                                border.width: 6
                                border.color: Qt.rgba(1, 0.91, 0.66, 0.25)
                            }
                        }
                        // Le citta'.
                        Repeater {
                            model: root.clock.cities
                            Item {
                                id: marker
                                required property var modelData
                                readonly property bool chosen: modelData.id === root.clock.selected
                                readonly property bool labelLeft: modelData.lon > 110
                                x: (modelData.lon + 180) / 360 * mapBox.width
                                y: (90 - modelData.lat) / 180 * mapBox.height
                                z: chosen ? 3 : 2
                                Rectangle {
                                    readonly property real size: marker.chosen ? 16 : 10
                                    width: size
                                    height: size
                                    radius: size / 2
                                    x: -size / 2
                                    y: -size / 2
                                    color: marker.chosen || marker.modelData.home ? Theme.secondaryColor : Theme.primaryColor
                                    border.width: marker.chosen ? 2 : 0
                                    border.color: Theme.textPrimary
                                }
                                Rectangle {
                                    x: marker.labelLeft ? -width - 10 : 10
                                    y: -height / 2
                                    width: chip.implicitWidth + 10
                                    height: chip.implicitHeight + 4
                                    radius: 4
                                    color: marker.chosen ? Theme.secondaryColor : Qt.rgba(0.04, 0.06, 0.1, 0.82)
                                    Mono {
                                        id: chip
                                        anchors.centerIn: parent
                                        text: marker.modelData.short + " " + marker.modelData.time
                                        font.pixelSize: 11
                                        color: marker.chosen ? Theme.bgDeep : Theme.textPrimary
                                    }
                                }
                                MouseArea {
                                    x: -10; y: -10; width: 20; height: 20
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.clock.selected = marker.modelData.id
                                }
                            }
                        }
                        Label2 { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 4; text: "180°W"; font.family: Theme.monoFamily; font.pixelSize: 11 }
                        Label2 { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 4; text: "180°E"; font.family: Theme.monoFamily; font.pixelSize: 11 }
                    }

                    // La legenda.
                    Flow {
                        Layout.fillWidth: true
                        spacing: 14
                        Repeater {
                            model: [
                                { label: qsTr("Day"), layers: 0 },
                                { label: qsTr("Civil twilight"), layers: 1 },
                                { label: qsTr("Nautical"), layers: 2 },
                                { label: qsTr("Astronomical"), layers: 3 },
                                { label: qsTr("Night"), layers: 4 }
                            ]
                            Row {
                                required property var modelData
                                spacing: 6
                                Rectangle {
                                    width: 14; height: 10; radius: 2
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: "#2A4262"
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 2
                                        color: "#02050B"
                                        opacity: 1 - Math.pow(0.74, modelData.layers)
                                    }
                                }
                                Label2 { text: modelData.label; font.pixelSize: 12 }
                            }
                        }
                        Row {
                            spacing: 6
                            Rectangle { width: 16; height: 2; color: Theme.secondaryColor; anchors.verticalCenter: parent.verticalCenter }
                            Label2 { text: qsTr("Grayline"); font.pixelSize: 12 }
                        }
                        Label2 { text: qsTr("Equirectangular map · updated every minute"); font.pixelSize: 12; opacity: 0.8 }
                    }

                    // I dettagli della citta' scelta.
                    Rectangle {
                        readonly property var d: root.clock.detail
                        Layout.fillWidth: true
                        implicitHeight: detailColumn.implicitHeight + 32
                        radius: 16
                        color: Theme.panelColor
                        border.color: Theme.borderColor
                        ColumnLayout {
                            id: detailColumn
                            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
                            spacing: 12
                            RowLayout {
                                spacing: 12
                                Text {
                                    text: parent.parent.parent.d.name || ""
                                    color: Theme.textPrimary
                                    font.family: Theme.uiFamily
                                    font.pixelSize: 20
                                    font.weight: Font.DemiBold
                                }
                                Mono {
                                    text: [parent.parent.parent.d.locator, parent.parent.parent.d.coords, parent.parent.parent.d.offset]
                                          .filter(s => s).join("  ·  ")
                                    color: Theme.textSecondary
                                    font.pixelSize: 13
                                }
                                StateDot {
                                    state: parent.parent.parent.d.state || "day"
                                    label: parent.parent.parent.d.stateText || ""
                                }
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 4
                                columnSpacing: 8
                                rowSpacing: 8
                                Repeater {
                                    model: detailColumn.parent.d.tiles || []
                                    Rectangle {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 1
                                        implicitHeight: 76
                                        radius: 8
                                        color: Theme.panelHeader
                                        Column {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 3
                                            Label2 { text: modelData.label; font.pixelSize: 12 }
                                            Mono { text: modelData.value; font.pixelSize: 17; elide: Text.ElideRight; width: parent.width }
                                            Mono { text: modelData.sub; font.pixelSize: 11; color: Theme.textSecondary; elide: Text.ElideRight; width: parent.width }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── I fusi orari ────────────────────────────────────────────────
            ColumnLayout {
                Layout.preferredWidth: 408
                Layout.maximumWidth: 408
                Layout.fillHeight: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Time zones")
                        color: Theme.textPrimary
                        font.family: Theme.uiFamily
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    Item { Layout.fillWidth: true }
                    StyledComboBox {
                        id: addBox
                        Layout.preferredWidth: 190
                        property var choices: root.clock.catalog().filter(c => !c.present)
                        model: [qsTr("Add a city…")].concat(choices.map(c => c.name))
                        currentIndex: 0
                        onActivated: {
                            if (currentIndex > 0)
                                root.clock.addCity(choices[currentIndex - 1].id)
                            currentIndex = 0
                        }
                        Connections {
                            target: root.clock
                            function onCitiesChanged() { addBox.choices = root.clock.catalog().filter(c => !c.present) }
                        }
                    }
                }

                ListView {
                    id: cityList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 8
                    model: root.clock.cities
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: PanelScrollBar {}
                    delegate: Rectangle {
                        id: card
                        required property var modelData
                        readonly property bool chosen: modelData.id === root.clock.selected
                        width: cityList.width - 6
                        implicitHeight: Math.max(80, cardRow.implicitHeight + 20)
                        radius: 12
                        color: chosen ? Theme.panelHeader : Theme.panelColor
                        border.width: chosen ? 2 : 1
                        border.color: chosen ? Theme.secondaryColor : Theme.borderColor
                        HoverHandler { id: cardHover }
                        TapHandler { onTapped: root.clock.selected = card.modelData.id }
                        RowLayout {
                            id: cardRow
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text {
                                    text: card.modelData.name
                                    color: Theme.textPrimary
                                    font.family: Theme.uiFamily
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Mono {
                                    text: [card.modelData.offset, card.modelData.locator, card.modelData.date].join(" · ")
                                          + (card.modelData.dayShift ? " " + card.modelData.dayShift : "")
                                    color: Theme.textSecondary
                                    font.pixelSize: 11
                                }
                                RowLayout {
                                    spacing: 10
                                    StateDot { state: card.modelData.state; label: card.modelData.stateText }
                                    // Nella barra in basso, e toglierla: solo col mouse sopra.
                                    Text {
                                        visible: !card.modelData.home && (cardHover.hovered || card.modelData.footer)
                                        text: card.modelData.footer ? qsTr("in the bar") : qsTr("put in the bar")
                                        color: card.modelData.footer ? Theme.secondaryColor : Theme.textSecondary
                                        font.family: Theme.uiFamily
                                        font.pixelSize: 11
                                        font.underline: footerTap.hovered
                                        HoverHandler { id: footerTap; cursorShape: Qt.PointingHandCursor }
                                        TapHandler { onTapped: root.clock.toggleFooter(card.modelData.id) }
                                    }
                                    Text {
                                        visible: !card.modelData.home && cardHover.hovered
                                        text: qsTr("remove")
                                        color: Theme.textSecondary
                                        font.family: Theme.uiFamily
                                        font.pixelSize: 11
                                        font.underline: removeTap.hovered
                                        HoverHandler { id: removeTap; cursorShape: Qt.PointingHandCursor }
                                        TapHandler { onTapped: root.clock.removeCity(card.modelData.id) }
                                    }
                                }
                            }
                            ColumnLayout {
                                spacing: 2
                                Row {
                                    Layout.alignment: Qt.AlignRight
                                    Mono { text: card.modelData.time; font.pixelSize: 26 }
                                    Mono {
                                        text: card.modelData.seconds
                                        font.pixelSize: 14
                                        color: Theme.textSecondary
                                        anchors.baseline: parent.children[0].baseline
                                    }
                                }
                                Mono {
                                    Layout.alignment: Qt.AlignRight
                                    text: card.modelData.polar ? card.modelData.polar
                                                               : "↑ " + card.modelData.rise + "  ↓ " + card.modelData.set
                                    color: Theme.textSecondary
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
