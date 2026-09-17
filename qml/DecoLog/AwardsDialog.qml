// DecoLog — award: elenco a sinistra con lavorati/confermati, tabella a destra
// con un elemento per riga e una colonna per banda (● confermato, ○ lavorato),
// i totali per banda in fondo e, per i locatori, una mappa.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

DialogFrame {
    id: root

    signal openQso(var id)

    property string awardId: "dxcc"
    property string search: ""
    // "all", "unconfirmed", "missing"
    property string view: "all"
    property bool showMap: false
    // Si ricalcola quando cambiano log, filtri o selezione.
    property int revision: 0
    readonly property var summary: decolog.awardSummary
    readonly property var current: summary.find(a => a.id === awardId) || ({})
    readonly property bool hasMissing: decolog.awardHasMissing(awardId)
    readonly property string effectiveView: view === "missing" && !hasMissing ? "all" : view
    readonly property bool mapView: showMap && awardId === "grids"
    readonly property var items: { revision; return decolog.awardItems(awardId, search, effectiveView) }
    readonly property var totals: { revision; return decolog.awardBandTotals(awardId) }
    readonly property var grids: { revision; return awardId === "grids" ? decolog.awardGrids() : [] }
    readonly property var bands: decolog.awardBands

    function openAt(id) {
        if (id)
            awardId = id
        revision++
        open()
    }

    title: qsTr("Awards")
    dotColor: Theme.accentColor
    info: qsTr("computed from the log · confirmations: %1")
          .arg([decolog.awardConfirmLotw ? "LoTW" : "", decolog.awardConfirmCard ? qsTr("card") : "",
                decolog.awardConfirmEqsl ? "eQSL" : ""].filter(s => s).join(" + ") || qsTr("none"))
    width: Math.min(1180, parent ? parent.width - 60 : 1180)
    height: Math.min(720, parent ? parent.height - 60 : 720)

    Connections {
        target: decolog
        function onAwardsChanged() { root.revision++ }
    }

    contentItem: RowLayout {
        spacing: 0

        // ── Elenco degli award ──────────────────────────────────────────────
        ListView {
            Layout.preferredWidth: 250
            Layout.fillHeight: true
            Layout.margins: 8
            clip: true
            spacing: 4
            model: root.summary
            delegate: Rectangle {
                id: card
                required property var modelData
                readonly property bool active: modelData.id === root.awardId
                width: ListView.view.width
                implicitHeight: cardColumn.implicitHeight + 16
                radius: 5
                color: active ? Theme.glassOverlay : cardArea.containsMouse ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.06) : "transparent"
                border.width: 1
                border.color: active ? Theme.primaryColor : Theme.borderSoft

                ColumnLayout {
                    id: cardColumn
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: card.modelData.title
                            color: card.active ? Theme.primaryColor : Theme.textPrimary
                            font.family: Theme.monoFamily
                            font.pixelSize: 13
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: card.modelData.worked + (card.modelData.total > 0 ? " / " + card.modelData.total : "")
                            color: Theme.textPrimary
                            font.family: Theme.monoFamily
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }
                    MeterBar {
                        Layout.fillWidth: true
                        readonly property int goal: card.modelData.total > 0 ? card.modelData.total
                                                    : card.modelData.target > 0 ? card.modelData.target : 0
                        visible: goal > 0
                        label: qsTr("confirmed %1").arg(card.modelData.confirmed)
                        valueText: card.modelData.target > 0 && card.modelData.confirmed >= card.modelData.target
                                   ? qsTr("base award ✓") : card.modelData.target > 0 ? qsTr("goal %1").arg(card.modelData.target) : ""
                        fraction: goal > 0 ? card.modelData.confirmed / goal : 0
                        barColor: card.modelData.id === "ft2" ? Theme.accentColor : Theme.primaryColor
                    }
                    Text {
                        visible: !parent.children[1].visible
                        text: qsTr("confirmed %1").arg(card.modelData.confirmed)
                        color: Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: 11
                    }
                }
                MouseArea {
                    id: cardArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.awardId = card.modelData.id
                }
            }
        }

        Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: Theme.borderSoft }

        // ── Dettaglio ───────────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    text: root.current.title || ""
                    color: Theme.textPrimary
                    font.family: Theme.monoFamily
                    font.pixelSize: 20
                    font.bold: true
                }
                Pill { text: qsTr("worked %1").arg(root.current.worked || 0); tone: Theme.secondaryColor }
                Pill { text: qsTr("confirmed %1").arg(root.current.confirmed || 0); tone: Theme.accentColor }
                Pill {
                    visible: root.bands.length > 1
                    text: qsTr("band slots %1 / %2").arg(root.current.slotsWorked || 0).arg(root.current.slotsConfirmed || 0)
                    tone: Theme.primaryColor
                }
                Item { Layout.fillWidth: true }
                StyledComboBox {
                    visible: !root.mapView
                    Layout.preferredWidth: 150
                    readonly property var views: ["all", "unconfirmed", "missing"]
                    model: root.hasMissing ? [qsTr("All worked"), qsTr("Not confirmed"), qsTr("Never worked")]
                                           : [qsTr("All worked"), qsTr("Not confirmed")]
                    currentIndex: Math.max(0, views.indexOf(root.effectiveView))
                    onActivated: root.view = views[currentIndex]
                }
                GlassButton {
                    visible: root.awardId === "grids"
                    text: root.showMap ? qsTr("Table") : qsTr("Map")
                    tone: Theme.primaryColor
                    onClicked: root.showMap = !root.showMap
                }
                StyledTextField {
                    Layout.preferredWidth: 180
                    placeholderText: qsTr("Search…")
                    onTextChanged: root.search = text
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                StyledComboBox {
                    Layout.preferredWidth: 120
                    model: [qsTr("All bands")].concat(root.bands)
                    currentIndex: decolog.awardBand.length ? Math.max(0, root.bands.indexOf(decolog.awardBand) + 1) : 0
                    onActivated: decolog.awardBand = currentIndex === 0 ? "" : currentText
                }
                StyledComboBox {
                    Layout.preferredWidth: 140
                    readonly property var groups: ["", "FT2", "FT8", "DIGITAL", "CW", "PHONE"]
                    model: [qsTr("All modes"), "FT2", "FT8", qsTr("Digital"), "CW", qsTr("Phone")]
                    currentIndex: Math.max(0, groups.indexOf(decolog.awardModeGroup))
                    onActivated: decolog.awardModeGroup = groups[currentIndex]
                }
                Rectangle { implicitWidth: 1; implicitHeight: 20; color: Theme.borderSoft }
                Text { text: qsTr("Confirmed by"); color: Theme.textSecondary; font.pixelSize: 11 }
                ToggleSwitch { text: "LoTW"; checked: decolog.awardConfirmLotw; onToggled: decolog.awardConfirmLotw = checked }
                ToggleSwitch { text: qsTr("Card"); checked: decolog.awardConfirmCard; onToggled: decolog.awardConfirmCard = checked }
                ToggleSwitch { text: "eQSL"; checked: decolog.awardConfirmEqsl; onToggled: decolog.awardConfirmEqsl = checked }
                Item { Layout.fillWidth: true }
                StyledComboBox {
                    id: profileBox
                    Layout.preferredWidth: 150
                    readonly property var ids: {
                        const list = [0]
                        for (let i = 0; i < decolog.stationProfiles.count; ++i) {
                            const p = decolog.stationProfiles.get(i)
                            if (!p.deleted) list.push(p.id)
                        }
                        return list
                    }
                    model: ids.map(id => id === 0 ? qsTr("All stations") : decolog.stationProfiles.byId(id).name)
                    currentIndex: Math.max(0, ids.indexOf(decolog.awardProfile))
                    onActivated: decolog.awardProfile = ids[currentIndex]
                }
                StyledComboBox {
                    Layout.preferredWidth: 130
                    readonly property var tags: { root.revision; return [""].concat(decolog.qsoModel.tagsInLog().map(t => t.key)) }
                    model: tags.map(t => t.length ? "#" + t : qsTr("All tags"))
                    currentIndex: Math.max(0, tags.findIndex(t => t.toLowerCase() === decolog.awardTag.toLowerCase()))
                    onActivated: decolog.awardTag = tags[currentIndex]
                }
            }

            GridSquareMap {
                visible: root.mapView
                Layout.fillWidth: true
                Layout.fillHeight: true
                grids: root.grids
            }

            // Intestazione della tabella.
            Rectangle {
                visible: !root.mapView
                Layout.fillWidth: true
                implicitHeight: Theme.rowHeight
                color: Theme.panelHeader
                radius: 4
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8
                    component Head: Text {
                        color: Theme.secondaryColor
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fontSize
                        font.bold: true
                    }
                    Head { Layout.preferredWidth: 80; text: root.awardId === "was" ? qsTr("State") : root.awardId === "waz" ? qsTr("Zone") : qsTr("Key") }
                    Head { Layout.fillWidth: true; text: qsTr("Name / first QSO") }
                    Repeater {
                        model: root.bands
                        Head {
                            required property string modelData
                            Layout.preferredWidth: 44
                            horizontalAlignment: Text.AlignHCenter
                            text: modelData
                        }
                    }
                    Head { Layout.preferredWidth: 40; horizontalAlignment: Text.AlignRight; text: "QSO" }
                    Head { Layout.preferredWidth: 90; text: qsTr("Last") }
                }
            }

            ListView {
                id: table
                visible: !root.mapView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.items
                ScrollBar.vertical: ScrollBar {}
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    id: line
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    height: Theme.rowHeight
                    color: lineArea.containsMouse ? Theme.glassOverlay : "transparent"
                    Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.borderSoft }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8
                        Text {
                            Layout.preferredWidth: 80
                            text: line.modelData.key
                            color: line.modelData.confirmed ? Theme.accentColor : Theme.textPrimary
                            font.family: Theme.monoFamily
                            font.pixelSize: Theme.fontSize
                            font.bold: true
                        }
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            textFormat: Text.StyledText
                            text: (line.modelData.name.length ? line.modelData.name + "  " : "")
                                  + "<font color=\"" + Theme.textSecondary + "\">"
                                  + (line.modelData.missing ? "· " + qsTr("never worked") : line.modelData.firstCall + " · " + line.modelData.first)
                                  + "</font>"
                            color: line.modelData.missing ? Theme.textSecondary : Theme.textPrimary
                            font.pixelSize: Theme.fontSize
                        }
                        Repeater {
                            model: root.bands
                            Text {
                                required property string modelData
                                readonly property bool conf: line.modelData.bandsConfirmed.indexOf(modelData) >= 0
                                readonly property bool work: line.modelData.bandsWorked.indexOf(modelData) >= 0
                                Layout.preferredWidth: 44
                                horizontalAlignment: Text.AlignHCenter
                                text: conf ? "●" : work ? "○" : "·"
                                color: conf ? Theme.accentColor : work ? Theme.warningColor : Theme.borderSoft
                                font.pixelSize: Theme.fontSize + 1
                            }
                        }
                        Text {
                            Layout.preferredWidth: 40
                            horizontalAlignment: Text.AlignRight
                            text: line.modelData.missing ? "" : line.modelData.qsoCount
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: Theme.fontSize
                        }
                        Text {
                            Layout.preferredWidth: 90
                            text: line.modelData.last
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: Theme.fontSize
                        }
                    }
                    MouseArea {
                        id: lineArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onDoubleClicked: root.openQso(line.modelData.firstQsoId)
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: table.count === 0
                    width: parent.width - 40
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: root.effectiveView === "missing" && root.search.length === 0 ? qsTr("Everything worked. Well done.")
                        : root.search.length || root.effectiveView !== "all" ? qsTr("Nothing matches.")
                        : root.awardId === "was" ? qsTr("No QSO with a US state (STATE field) in the log.")
                        : qsTr("No QSO counts for this award with the current filters.")
                    color: Theme.textSecondary
                }
            }

            Rectangle {
                visible: !root.mapView && root.bands.length > 0
                Layout.fillWidth: true
                implicitHeight: Theme.rowHeight + 4
                color: Theme.panelHeader
                radius: 4
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8
                    Text {
                        Layout.preferredWidth: 80
                        text: qsTr("Per band")
                        color: Theme.secondaryColor
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fontSize
                        font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("worked / confirmed")
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }
                    Repeater {
                        model: root.totals
                        Column {
                            required property var modelData
                            Layout.preferredWidth: 44
                            Text {
                                width: 44
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData.worked
                                color: modelData.worked ? Theme.warningColor : Theme.textSecondary
                                font.family: Theme.monoFamily
                                font.pixelSize: 10
                            }
                            Text {
                                width: 44
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData.confirmed
                                color: modelData.confirmed ? Theme.accentColor : Theme.textSecondary
                                font.family: Theme.monoFamily
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                    }
                    Item { Layout.preferredWidth: 40 }
                    Item { Layout.preferredWidth: 90 }
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("● confirmed  ○ worked  ·  double-click opens the first QSO. DXCC counts entities with a DXCC number; "
                           + "use Setup → General → Fill missing DXCC for older QSOs.")
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                font.pixelSize: 11
            }
        }
    }
}
