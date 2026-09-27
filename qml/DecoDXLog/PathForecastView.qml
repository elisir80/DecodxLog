// DecoDXLog — la previsione di propagazione verso un DX: le bande in riga,
// le 24 ore UTC in colonna, ogni casella colorata da chiuso a buono. L'ora di
// adesso e' segnata. Il DX e' quello della scheda nominativo, oppure un
// locatore scritto a mano.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

ColumnLayout {
    id: root

    // Il locatore scritto a mano vince; se no la posizione della scheda.
    property string manualGrid: ""
    readonly property var cardPosition: decolog.callInfo.position || ({})
    readonly property string cardCall: decolog.callInfo.call || ""
    property int revision: 0
    readonly property var forecast: {
        revision
        if (manualGrid.length >= 4)
            return decolog.solar.pathForecast(manualGrid)
        return decolog.solar.pathForecast(cardPosition)
    }
    readonly property var hours: forecast.hours || []
    readonly property var bandNames: forecast.bands || []

    function qualityColor(q) {
        switch (q) {
        case 3: return Theme.accentColor
        case 2: return Qt.rgba(Theme.accentColor.r, Theme.accentColor.g, Theme.accentColor.b, 0.55)
        case 1: return Theme.warningColor
        default: return Theme.bgMedium
        }
    }

    // L'ora cambia, e con lei la colonna segnata.
    Timer { interval: 60000; repeat: true; running: root.visible; onTriggered: root.revision++ }
    Connections {
        target: decolog.solar
        function onChanged() { root.revision++ }
    }

    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        SectionTitle { text: qsTr("Path forecast") }
        Text {
            Layout.fillWidth: true
            elide: Text.ElideRight
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
            text: root.forecast.valid
                  ? [root.manualGrid.length >= 4 ? root.manualGrid.toUpperCase() : root.cardCall,
                     qsTr("%1 km").arg(root.forecast.distanceKm),
                     qsTr("%1°").arg(root.forecast.azimuth),
                     qsTr("%n hop(s)", "", root.forecast.hops),
                     root.forecast.estimatedSun ? qsTr("SFI %1 (estimated)").arg(root.forecast.solarFlux)
                                                : qsTr("SFI %1").arg(root.forecast.solarFlux)].filter(s => s).join(" · ")
                  : (root.forecast.reason || "")
        }
        StyledTextField {
            Layout.preferredWidth: 120
            placeholderText: qsTr("Locator")
            text: root.manualGrid
            onTextEdited: root.manualGrid = text.trim()
        }
    }

    // La griglia.
    Item {
        Layout.fillWidth: true
        visible: root.forecast.valid === true
        implicitHeight: grid.implicitHeight
        readonly property real labelW: 40
        readonly property real cellW: Math.max(12, (width - labelW) / 24)

        Column {
            id: grid
            spacing: 2
            // Le ore in cima.
            Row {
                Item { width: grid.parent.labelW; height: 14 }
                Repeater {
                    model: 24
                    Text {
                        required property int index
                        width: grid.parent.cellW
                        horizontalAlignment: Text.AlignHCenter
                        text: index % 3 === 0 ? String(index).padStart(2, "0") : ""
                        color: index === root.forecast.currentHour ? Theme.textPrimary : Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: 9
                        font.bold: index === root.forecast.currentHour
                    }
                }
            }
            Repeater {
                // Dalla banda piu' alta in cima, come le scale delle frequenze.
                model: root.bandNames.length
                Row {
                    id: bandRow
                    required property int index
                    readonly property int band: root.bandNames.length - 1 - index
                    Text {
                        width: grid.parent.labelW
                        height: 14
                        text: root.bandNames[bandRow.band]
                        color: Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: 10
                        verticalAlignment: Text.AlignVCenter
                    }
                    Repeater {
                        model: root.hours
                        Rectangle {
                            required property var modelData
                            required property int index
                            width: grid.parent.cellW - 1
                            height: 14
                            color: root.qualityColor(modelData.quality[bandRow.band])
                            border.width: index === root.forecast.currentHour ? 1 : 0
                            border.color: Theme.textPrimary
                            ToolTip.visible: cellHover.hovered
                            ToolTip.delay: 300
                            ToolTip.text: qsTr("%1 · %2 UTC · MUF %3 MHz · LUF %4 MHz")
                                          .arg(root.bandNames[bandRow.band]).arg(String(modelData.hour).padStart(2, "0") + ":00")
                                          .arg(modelData.muf).arg(modelData.luf)
                            HoverHandler { id: cellHover }
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        visible: root.forecast.valid === true
        spacing: 12
        Repeater {
            model: [{ q: 3, t: qsTr("good") }, { q: 2, t: qsTr("fair") }, { q: 1, t: qsTr("marginal") }, { q: 0, t: qsTr("closed") }]
            Row {
                required property var modelData
                spacing: 4
                Rectangle { width: 12; height: 10; anchors.verticalCenter: parent.verticalCenter; color: root.qualityColor(modelData.q); border.color: Theme.borderSoft }
                Text { text: modelData.t; color: Theme.textSecondary; font.pixelSize: 11 }
            }
        }
        Text {
            Layout.fillWidth: true
            elide: Text.ElideRight
            text: qsTr("Simplified F2 model (MUF/LUF), not VOACAP: a guide to when a band opens, not a promise.")
            color: Theme.textSecondary
            font.pixelSize: 10
            opacity: 0.8
        }
    }
}
