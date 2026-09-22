// DecoDXLog — il DX cluster: filtri rapidi in alto, spot confrontati col log.
// Clic: il nominativo va in Call info. Doppio clic: Decodium si sintonizza.
// `compact` e' la versione per la scheda in basso della finestra principale.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    property bool compact: false
    // Durante un contest il cluster serve a una cosa sola: vedere chi porta un
    // moltiplicatore che non si ha. Via tutto il resto, e quelli che contano si
    // vedono da lontano.
    property bool contestMode: false
    signal windowRequested(int tab)

    readonly property var cluster: decolog.cluster
    readonly property var model: cluster.spots
    readonly property var quickBands: ["160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"]
    readonly property var quickModes: ["FT2", "FT8", "FT4", "CW", "SSB"]

    function has(key, value) { return (cluster.filter[key] || []).indexOf(value) >= 0 }
    function toggle(key, value) {
        const f = Object.assign({}, cluster.filter)
        const list = (f[key] || []).slice()
        const i = list.indexOf(value)
        if (i >= 0) list.splice(i, 1); else list.push(value)
        f[key] = list
        cluster.filter = f
    }
    function setKey(key, value) {
        const f = Object.assign({}, cluster.filter)
        f[key] = value
        cluster.filter = f
    }
    function statusColor(status) {
        if (status & 1) return Theme.errorColor
        if (status & 2) return Theme.warningColor
        if (status & 4) return Theme.secondaryColor
        if (status & 8) return Theme.primaryColor
        if (status & 32) return Theme.textSecondary
        if (status & 16) return Theme.accentColor
        return Theme.borderSoft
    }
    function modeColor(mode) {
        if (mode === "FT2") return Theme.accentColor
        if (mode === "FT8" || mode === "FT4") return Theme.primaryColor
        if (mode === "CW") return Theme.secondaryColor
        return Theme.textPrimary
    }
    // Per le schermate di prova.
    function showMenu(name) {
        if (name === "row") rowMenu.popupFor(root.model.get(0))
        else if (name === "filters") filterPopup.open()
    }

    title: compact ? "" : qsTr("DX Cluster")
    dotColor: cluster.onlineCount > 0 ? Theme.accentColor : Theme.errorColor
    padding: 0

    headerTools: [
        Pill {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("%1/%2 online").arg(root.cluster.onlineCount).arg(root.cluster.sources.length)
            tone: root.cluster.onlineCount > 0 ? Theme.accentColor : Theme.errorColor
            interactive: true
            pillHeight: 22
            onClicked: root.windowRequested(1)
        },
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("%1 shown · %2 in the last hour").arg(root.model.count).arg(root.model.totalCount)
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            text: root.cluster.voiceEnabled ? "🔊" : "🔇"
            buttonHeight: 24
            fontPixelSize: 12
            onClicked: root.cluster.voiceEnabled = !root.cluster.voiceEnabled
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("→ Decodium")
            tone: root.cluster.sendToDecodium ? Theme.accentColor : "transparent"
            buttonHeight: 24
            fontPixelSize: 11
            onClicked: root.cluster.sendToDecodium = !root.cluster.sendToDecodium
        },
        GlassButton {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.compact
            text: qsTr("Open cluster")
            tone: Theme.primaryColor
            buttonHeight: 24
            fontPixelSize: 11
            onClicked: root.windowRequested(0)
        }
    ]

    component Chip: Rectangle {
        id: chip
        property string label
        property bool on: false
        property color tone: Theme.primaryColor
        signal toggled()
        implicitHeight: 22
        implicitWidth: chipText.implicitWidth + 12
        radius: 4
        color: on ? Qt.rgba(tone.r, tone.g, tone.b, 0.22) : chipArea.containsMouse ? Theme.glassOverlay : "transparent"
        border.width: 1
        border.color: on ? tone : Theme.glassBorder
        Text {
            id: chipText
            anchors.centerIn: parent
            text: chip.label
            color: chip.on ? chip.tone : Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
            font.bold: chip.on
        }
        MouseArea {
            id: chipArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: chip.toggled()
        }
    }
    component Head: Text {
        color: Theme.secondaryColor
        font.family: Theme.monoFamily
        font.pixelSize: Theme.fontSize
        font.bold: true
        elide: Text.ElideRight
    }

    Popup {
        id: filterPopup
        anchors.centerIn: Overlay.overlay
        modal: true
        padding: 16
        width: Math.min(820, (Overlay.overlay ? Overlay.overlay.width : 900) - 40)
        background: Rectangle { color: Theme.panelColor; border.color: Theme.glassBorder; radius: 6 }
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Spot filters"); color: Theme.textPrimary; font.pixelSize: 15; font.bold: true }
                Item { Layout.fillWidth: true }
                GlassButton { text: qsTr("Clear all"); onClicked: root.cluster.filter = ({}) }
                GlassButton { text: qsTr("Close"); tone: Theme.primaryColor; filled: true; onClicked: filterPopup.close() }
            }
            SpotFilterEditor {
                Layout.fillWidth: true
                filter: root.cluster.filter
                onEdited: (f) => root.cluster.filter = f
            }
            RowLayout {
                spacing: 8
                StyledTextField { id: saveName; Layout.preferredWidth: 200; mono: false; placeholderText: qsTr("Name for these filters") }
                GlassButton {
                    text: qsTr("Save")
                    enabled: saveName.text.trim().length > 0
                    onClicked: { root.cluster.saveFilter(saveName.text); saveName.text = "" }
                }
                Repeater {
                    model: Object.keys(root.cluster.savedFilters)
                    Pill {
                        required property string modelData
                        text: modelData + "  ✕"
                        tone: Theme.secondaryColor
                        interactive: true
                        onClicked: root.cluster.deleteSavedFilter(modelData)
                    }
                }
            }
        }
    }

    StyledMenu {
        id: savedMenu
        Repeater {
            model: Object.keys(root.cluster.savedFilters)
            StyledMenuItem {
                required property string modelData
                text: modelData
                onTriggered: root.cluster.applySavedFilter(modelData)
            }
        }
        StyledMenuItem {
            visible: Object.keys(root.cluster.savedFilters).length === 0
            height: visible ? implicitHeight : 0
            enabled: false
            text: qsTr("No saved filters: use More filters → Save")
        }
    }

    StyledMenu {
        id: rowMenu
        property var spot: ({})
        function popupFor(s) { spot = s || ({}); popup() }
        StyledMenuItem {
            text: qsTr("Tune Decodium to %1").arg(rowMenu.spot.call || "")
            onTriggered: root.cluster.tune(rowMenu.spot.spotKey)
        }
        StyledMenuItem { text: qsTr("Show in Call info"); onTriggered: root.cluster.lookupSpot(rowMenu.spot.spotKey) }
        StyledMenuItem {
            enabled: decolog.rotor.enabled && rowMenu.spot.azimuth !== undefined && rowMenu.spot.azimuth !== null
            text: rowMenu.spot.azimuth !== undefined && rowMenu.spot.azimuth !== null
                  ? qsTr("Point the rotor at %1 (%2°)").arg(rowMenu.spot.call || "").arg(rowMenu.spot.azimuth)
                  : qsTr("Point the rotor")
            onTriggered: decolog.rotor.pointTo(rowMenu.spot.azimuth, rowMenu.spot.call || "")
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
        StyledMenuItem {
            enabled: (rowMenu.spot.dxcc || 0) > 0
            text: qsTr("Only %1").arg(rowMenu.spot.entity || qsTr("this entity"))
            onTriggered: {
                const f = Object.assign({}, root.cluster.filter)
                f.dxcc = [rowMenu.spot.dxcc]
                root.cluster.filter = f
            }
        }
        StyledMenuItem {
            text: qsTr("Alert me when %1 is spotted").arg(rowMenu.spot.call || "")
            onTriggered: root.cluster.saveAlertRule({ name: rowMenu.spot.call, voice: true, decodium: true,
                                                      filter: { calls: rowMenu.spot.call, maxAgeMinutes: 10 } })
        }
        StyledMenuItem {
            text: qsTr("Hide spots from %1").arg(rowMenu.spot.spotter || "")
            visible: false
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Filtri rapidi ───────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: quick.implicitHeight + 12
            color: "transparent"
            Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.borderSoft }
            Flow {
                id: quick
                anchors.fill: parent
                anchors.margins: 6
                anchors.leftMargin: 10
                spacing: 4

                Chip {
                    label: qsTr("Decodium band")
                    tone: Theme.accentColor
                    on: root.cluster.followDecodiumBand
                    onToggled: root.cluster.followDecodiumBand = !root.cluster.followDecodiumBand
                }
                Item { width: 6; height: 22 }
                Repeater {
                    model: root.cluster.followDecodiumBand ? [] : root.quickBands
                    Chip { required property string modelData; label: modelData; on: root.has("bands", modelData); onToggled: root.toggle("bands", modelData) }
                }
                Item { width: 6; height: 22 }
                Repeater {
                    model: root.quickModes
                    Chip {
                        required property string modelData
                        label: modelData
                        tone: modelData === "FT2" ? Theme.accentColor : Theme.primaryColor
                        on: root.has("modes", modelData)
                        onToggled: root.toggle("modes", modelData)
                    }
                }
                Item { width: 6; height: 22 }
                Chip {
                    label: "NEW DXCC"
                    tone: Theme.errorColor
                    on: ((root.cluster.filter.anyStatus || 0) & 1) !== 0
                    onToggled: root.setKey("anyStatus", (root.cluster.filter.anyStatus || 0) ^ 1)
                }
                Chip {
                    label: qsTr("NEW BAND/MODE")
                    tone: Theme.warningColor
                    on: ((root.cluster.filter.anyStatus || 0) & 14) === 14
                    onToggled: root.setKey("anyStatus", ((root.cluster.filter.anyStatus || 0) & 14) === 14
                                                        ? (root.cluster.filter.anyStatus & ~14) : ((root.cluster.filter.anyStatus || 0) | 14))
                }
                Chip {
                    label: qsTr("hide worked")
                    tone: Theme.secondaryColor
                    on: !!root.cluster.filter.hideWorkedBand
                    onToggled: root.setKey("hideWorkedBand", !root.cluster.filter.hideWorkedBand)
                }
                Chip {
                    visible: !root.compact
                    label: "POTA/SOTA"
                    tone: Theme.secondaryColor
                    on: !!root.cluster.filter.onlyActivations
                    onToggled: root.setKey("onlyActivations", !root.cluster.filter.onlyActivations)
                }
                StyledTextField {
                    width: root.compact ? 110 : 150
                    fieldHeight: 22
                    uppercase: true
                    placeholderText: qsTr("Call / entity…")
                    text: root.cluster.filter.text || ""
                    onTextEdited: searchDelay.restart()
                    Timer { id: searchDelay; interval: 400; onTriggered: root.setKey("text", parent.text) }
                }
                Chip { label: qsTr("More filters…"); tone: Theme.primaryColor; on: false; onToggled: filterPopup.open() }
                Chip { label: qsTr("Saved ▾"); tone: Theme.primaryColor; on: false; onToggled: savedMenu.popup() }
            }
        }

        // ── Intestazione ────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.rowHeight
            color: Theme.panelHeader
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10
                spacing: 8
                Head { Layout.preferredWidth: 44; text: "UTC" }
                Head { Layout.preferredWidth: 76; text: qsTr("kHz"); horizontalAlignment: Text.AlignRight }
                Head { Layout.preferredWidth: 110; text: qsTr("DX") }
                Head { Layout.preferredWidth: 86; text: qsTr("Status") }
                Head { Layout.preferredWidth: 1; Layout.horizontalStretchFactor: 3; Layout.fillWidth: true; text: qsTr("Entity") }
                Head { Layout.preferredWidth: 46; text: qsTr("Mode") }
                Head { visible: !root.compact; Layout.preferredWidth: 44; text: qsTr("Band") }
                Head { visible: !root.compact; Layout.preferredWidth: 96; text: qsTr("Spotter") }
                Head { Layout.preferredWidth: 1; Layout.horizontalStretchFactor: 4; Layout.fillWidth: true; text: qsTr("Info") }
                Head { visible: !root.compact; Layout.preferredWidth: 86; text: qsTr("km · az"); horizontalAlignment: Text.AlignRight }
                Head { visible: !root.compact; Layout.preferredWidth: 70; text: qsTr("Source") }
            }
        }

        // ── Spot ────────────────────────────────────────────────────────────
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.model
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            // Chi sta leggendo in basso non viene riportato in cima da ogni spot.
            onCountChanged: if (atYBeginning) positionViewAtBeginning()

            delegate: Rectangle {
                id: line
                required property int index
                required property string spotKey
                required property string call
                required property string freq
                required property string band
                required property string mode
                required property string time
                required property string spotter
                required property string spotters
                required property int spotCount
                required property string comment
                required property string source
                required property string sourceName
                required property string entity
                required property int dxcc
                required property string continent
                required property int status
                required property string statusLabel
                required property var distance
                required property var azimuth
                required property var snr
                required property string refs
                required property string grid
                required property bool lotw
                required property bool fresh

                // Quanto vale questo spot nel contest aperto: i punti, e se
                // porta un moltiplicatore nuovo. Vuoto fuori dai contest.
                readonly property var contestValue: root.contestMode
                    ? decolog.activation.spotValue(call, band, mode) : ({})
                readonly property bool newMultiplier: !!contestValue.newMultiplier

                width: ListView.view.width
                height: Theme.rowHeight
                color: area.containsMouse ? Theme.glassOverlay
                     : line.newMultiplier ? Qt.rgba(Theme.warningColor.r, Theme.warningColor.g,
                                                    Theme.warningColor.b, 0.18)
                     : fresh && (status & 15) ? Qt.rgba(root.statusColor(status).r, root.statusColor(status).g, root.statusColor(status).b, 0.10)
                     : "transparent"
                Rectangle {
                    anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                    width: line.newMultiplier ? 5 : 3
                    color: line.newMultiplier ? Theme.warningColor : root.statusColor(line.status)
                }
                Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.borderSoft }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 10
                    spacing: 8
                    Text { Layout.preferredWidth: 44; text: line.time; color: Theme.textSecondary; font.family: Theme.monoFamily; font.pixelSize: Theme.fontSize }
                    Text { Layout.preferredWidth: 76; text: line.freq; horizontalAlignment: Text.AlignRight; color: Theme.textPrimary; font.family: Theme.monoFamily; font.pixelSize: Theme.fontSize }
                    RowLayout {
                        Layout.preferredWidth: 110
                        spacing: 4
                        Text {
                            text: line.call
                            color: (line.status & 32) ? Theme.textSecondary : Theme.textPrimary
                            font.family: Theme.monoFamily
                            font.pixelSize: Theme.fontSize + 1
                            font.bold: true
                        }
                        Text {
                            visible: line.spotCount > 1
                            text: "×" + line.spotCount
                            color: Theme.secondaryColor
                            font.family: Theme.monoFamily
                            font.pixelSize: 10
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Item {
                        Layout.preferredWidth: 86
                        implicitHeight: 18
                        // Nel contest conta il moltiplicatore, non il DXCC nuovo.
                        Rectangle {
                            visible: line.newMultiplier
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: 18
                            radius: 3
                            color: Qt.rgba(Theme.warningColor.r, Theme.warningColor.g,
                                           Theme.warningColor.b, 0.22)
                            border.color: Theme.warningColor
                            Text {
                                anchors.centerIn: parent
                                text: line.contestValue.label || qsTr("mult")
                                color: Theme.warningColor
                                font.family: Theme.monoFamily
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                        Rectangle {
                            visible: !line.newMultiplier && line.statusLabel.length > 0
                            anchors.verticalCenter: parent.verticalCenter
                            width: badge.implicitWidth + 10
                            height: 16
                            radius: 3
                            color: Qt.rgba(root.statusColor(line.status).r, root.statusColor(line.status).g, root.statusColor(line.status).b,
                                           (line.status & 32) ? 0.10 : 0.22)
                            border.width: 1
                            border.color: root.statusColor(line.status)
                            Text {
                                id: badge
                                anchors.centerIn: parent
                                text: line.statusLabel
                                color: root.statusColor(line.status)
                                font.family: Theme.monoFamily
                                font.pixelSize: 9
                                font.bold: true
                            }
                        }
                    }
                    Text {
                        Layout.preferredWidth: 1
                        Layout.horizontalStretchFactor: 3
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        textFormat: Text.StyledText
                        text: (line.entity || "—") + " <font color=\"" + Theme.textSecondary + "\">" + line.continent
                              + (line.lotw ? " · LoTW" : "") + ((line.status & 64) ? " · " + qsTr("unconf.") : "") + "</font>"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSize
                    }
                    Text { Layout.preferredWidth: 46; text: line.mode; color: root.modeColor(line.mode); font.family: Theme.monoFamily; font.pixelSize: Theme.fontSize; font.bold: true }
                    Text { visible: !root.compact; Layout.preferredWidth: 44; text: line.band; color: Theme.textSecondary; font.family: Theme.monoFamily; font.pixelSize: Theme.fontSize }
                    Text {
                        visible: !root.compact
                        Layout.preferredWidth: 96
                        elide: Text.ElideRight
                        text: line.spotter
                        color: Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fontSize
                    }
                    Text {
                        Layout.preferredWidth: 1
                        Layout.horizontalStretchFactor: 4
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        textFormat: Text.StyledText
                        text: (line.refs.length ? "<font color=\"" + Theme.accentColor + "\">" + line.refs + "</font> " : "")
                              + (line.snr !== undefined && line.snr !== null ? "<font color=\"" + Theme.secondaryColor + "\">" + line.snr + " dB</font> " : "")
                              + line.comment.replace(/&/g, "&amp;").replace(/</g, "&lt;")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSize
                    }
                    Text {
                        visible: !root.compact
                        Layout.preferredWidth: 86
                        horizontalAlignment: Text.AlignRight
                        text: line.distance !== undefined && line.distance !== null ? line.distance + " · " + line.azimuth + "°" : ""
                        color: Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fontSize
                    }
                    Text {
                        visible: !root.compact
                        Layout.preferredWidth: 70
                        elide: Text.ElideRight
                        text: line.source === "rbn" ? "RBN" : line.source === "hamalert" ? "HamAlert" : line.source === "pota" ? "POTA" : line.sourceName
                        color: line.source === "hamalert" ? Theme.warningColor : Theme.textSecondary
                        font.pixelSize: 10
                    }
                }

                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton)
                            rowMenu.popupFor(root.model.get(line.index))
                        else
                            root.cluster.lookupSpot(line.spotKey)
                    }
                    onDoubleClicked: root.cluster.tune(line.spotKey)
                    ToolTip.visible: containsMouse && line.spotCount > 1
                    ToolTip.delay: 700
                    ToolTip.text: qsTr("Spotted by %1").arg(line.spotters)
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: list.count === 0
                text: root.cluster.onlineCount === 0
                      ? qsTr("No source connected. Open the cluster window → Sources to connect a node, RBN, HamAlert or POTA.")
                      : root.model.totalCount > 0 ? qsTr("No spot matches the filters (%1 hidden).").arg(root.model.totalCount)
                                                  : qsTr("Waiting for spots…")
                color: Theme.textSecondary
            }
        }
    }
}
