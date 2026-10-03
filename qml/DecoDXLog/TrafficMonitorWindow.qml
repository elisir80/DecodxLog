// DecoDXLog — il monitor del traffico con Decodium 4.
//
// Tutto quello che passa fra i due programmi, nei due versi: il protocollo UDP
// (anche quello inoltrato ai programmi accanto), DecoLink, e gli annunci
// DecoPort della radio in rete. Sotto, i comandi per parlare a Decodium: sullo
// stesso socket UDP da cui scrive (come GridTracker o JTAlert) e su DecoLink.
// Registra solo mentre la finestra e' aperta.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

DialogFrame {
    id: root

    readonly property var mon: decolog.traffic
    // La riga scelta si ricorda col suo numero di serie: l'indice scivola a
    // ogni riga nuova in cima.
    property var selectedSerial: -1
    property bool selectedCanReply: false
    property string selectedType: ""
    property string detailText: ""
    readonly property string client: clientBox.currentIndex >= 0 && clientBox.count > 0
                                     ? clientBox.currentText : ""

    title: qsTr("Decodium traffic monitor")
    dotColor: Theme.secondaryColor
    info: root.mon.paused ? qsTr("paused · %1 of %2 rows").arg(root.mon.count).arg(root.mon.total)
                          : qsTr("%1 of %2 rows").arg(root.mon.count).arg(root.mon.total)
    dialogKey: "traffic"
    width: 1240
    height: 780

    onVisibleChanged: root.mon.active = visible

    function directionText(d) {
        return d === "in" ? qsTr("← received")
             : d === "out" ? qsTr("→ sent")
             : d === "fwd" ? qsTr("→ forwarded")
             : d === "fwd-in" ? qsTr("← from forward")
             : d === "back" ? qsTr("→ relayed back")
             : d
    }
    function directionTone(d) {
        return d === "in" ? Theme.secondaryColor
             : d === "out" ? Theme.primaryColor
             : d === "back" ? Theme.accentColor
             : Theme.textSecondary
    }
    function channelTone(c) {
        return c === "UDP" ? Theme.primaryColor : c === "DecoLink" ? Theme.accentColor : Theme.warningColor
    }
    function pick(serial, canReply, type) {
        root.selectedSerial = serial
        root.selectedCanReply = canReply
        root.selectedType = type
        root.detailText = root.mon.detail(serial)
    }

    readonly property var cols: [
        { key: "time", title: qsTr("UTC"), w: 96 },
        { key: "channel", title: qsTr("Channel"), w: 76 },
        { key: "direction", title: qsTr("Direction"), w: 130 },
        { key: "type", title: qsTr("Type"), w: 130 },
        { key: "client", title: qsTr("Program"), w: 120 },
        { key: "summary", title: qsTr("Content"), w: 0 }
    ]

    body: ColumnLayout {
        spacing: 8
        anchors.margins: 12

        // Dove si ascolta, e chi c'e'.
        Flow {
            Layout.fillWidth: true
            spacing: 8
            Pill {
                text: decolog.listening
                      ? qsTr("UDP %1 · %2").arg(decolog.udpPort)
                            .arg(root.mon.udpClients.length > 0 ? root.mon.udpClients.join(", ") : qsTr("nobody has written yet"))
                      : qsTr("UDP %1 not listening").arg(decolog.udpPort)
                tone: decolog.listening ? Theme.primaryColor : Theme.errorColor
            }
            Pill {
                visible: decolog.udpForward.length > 0
                text: qsTr("forwarded to %1").arg(decolog.udpForward)
                tone: Theme.textSecondary
            }
            Pill {
                text: !decolog.decoLinkEnabled ? qsTr("DecoLink off")
                    : decolog.decoLinkListening ? qsTr("DecoLink %1 · %2 connected").arg(decolog.decoLinkPort).arg(root.mon.decoLinkClients)
                    : qsTr("DecoLink %1 not listening").arg(decolog.decoLinkPort)
                tone: decolog.decoLinkListening && root.mon.decoLinkClients > 0 ? Theme.accentColor
                    : decolog.decoLinkListening ? Theme.textSecondary : Theme.errorColor
            }
            Pill {
                text: root.mon.decoPortListening ? qsTr("DecoPort: listening to announcements on 5560")
                                                 : qsTr("DecoPort: %1").arg(root.mon.decoPortError || qsTr("not listening"))
                tone: root.mon.decoPortListening ? Theme.warningColor : Theme.errorColor
            }
        }

        // I filtri.
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ToggleSwitch { text: "UDP"; checked: root.mon.showUdp; onToggled: root.mon.showUdp = checked }
            ToggleSwitch { text: "DecoLink"; checked: root.mon.showDecoLink; onToggled: root.mon.showDecoLink = checked }
            ToggleSwitch { text: "DecoPort"; checked: root.mon.showDecoPort; onToggled: root.mon.showDecoPort = checked }
            ToggleSwitch {
                text: qsTr("hide heartbeats")
                checked: root.mon.hideRoutine
                onToggled: root.mon.hideRoutine = checked
            }
            StyledTextField {
                Layout.preferredWidth: 220
                mono: false
                placeholderText: qsTr("Filter: type, call, text…")
                text: root.mon.textFilter
                onTextChanged: root.mon.textFilter = text
            }
            Item { Layout.fillWidth: true }
            GlassButton {
                text: root.mon.paused ? qsTr("Resume") : qsTr("Pause")
                tone: root.mon.paused ? Theme.warningColor : "transparent"
                onClicked: root.mon.paused = !root.mon.paused
            }
            GlassButton {
                text: qsTr("Copy")
                enabled: root.mon.count > 0
                onClicked: {
                    copyBuffer.text = root.mon.asText()
                    copyBuffer.selectAll()
                    copyBuffer.copy()
                }
            }
            GlassButton {
                text: qsTr("Clear")
                enabled: root.mon.total > 0
                onClicked: {
                    root.mon.clear()
                    root.selectedSerial = -1
                    root.detailText = ""
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            // L'elenco, dalla riga piu' recente.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: Theme.rowHeight
                    color: Theme.panelHeader
                    radius: 4
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        Repeater {
                            model: root.cols
                            Text {
                                required property var modelData
                                width: modelData.w > 0 ? modelData.w : 300
                                leftPadding: 8
                                text: modelData.title
                                color: Theme.secondaryColor
                                font.family: Theme.monoFamily
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }
                }
                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.mon
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: PanelScrollBar {}
                    delegate: Rectangle {
                        id: line
                        required property int index
                        required property string time
                        required property string channel
                        required property string direction
                        required property string peer
                        required property string type
                        required property string client
                        required property string summary
                        required property bool canReply
                        required property var serial
                        readonly property bool chosen: root.selectedSerial === serial
                        width: list.width
                        height: Theme.rowHeight
                        color: chosen ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.22)
                             : hover.containsMouse ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.08)
                             : "transparent"
                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                width: root.cols[0].w; leftPadding: 8
                                text: line.time
                                color: Theme.textSecondary
                                font.family: Theme.monoFamily; font.pixelSize: 11
                            }
                            Text {
                                width: root.cols[1].w; leftPadding: 8
                                text: line.channel
                                color: root.channelTone(line.channel)
                                font.family: Theme.monoFamily; font.pixelSize: 11; font.bold: true
                            }
                            Text {
                                width: root.cols[2].w; leftPadding: 8
                                text: root.directionText(line.direction)
                                color: root.directionTone(line.direction)
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            Text {
                                width: root.cols[3].w; leftPadding: 8
                                text: line.type
                                color: Theme.textPrimary
                                font.family: Theme.monoFamily; font.pixelSize: 11; font.bold: true
                                elide: Text.ElideRight
                            }
                            Text {
                                width: root.cols[4].w; leftPadding: 8
                                text: line.client
                                color: Theme.textSecondary
                                font.family: Theme.monoFamily; font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            Text {
                                width: Math.max(120, list.width - 552); leftPadding: 8
                                text: line.summary
                                color: line.canReply ? Theme.accentColor : Theme.textPrimary
                                font.family: Theme.monoFamily; font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                        Rectangle {
                            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                            height: 1
                            color: Theme.borderSoft
                        }
                        MouseArea {
                            id: hover
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.pick(line.serial, line.canReply, line.type)
                            // Come in Decodium: il doppio clic su una riga decodificata risponde.
                            onDoubleClicked: if (line.canReply) { root.pick(line.serial, true, line.type); root.mon.reply(line.serial, 0) }
                        }
                        ToolTip.visible: hover.containsMouse && line.peer.length > 0
                        ToolTip.delay: 800
                        ToolTip.text: line.peer
                    }
                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 40
                        visible: list.count === 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        text: root.mon.total > 0 ? qsTr("Nothing matches the filters.")
                                                 : qsTr("Waiting for traffic. Start Decodium: its UDP messages, the DecoLink connection and the DecoPort announcements show up here as they pass.")
                    }
                }
            }

            // La riga scelta per intero.
            Rectangle {
                Layout.preferredWidth: 400
                Layout.fillHeight: true
                radius: 6
                color: Theme.bgMedium
                border.color: Theme.glassBorder
                border.width: 1
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    TextArea {
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.WrapAnywhere
                        text: root.detailText.length > 0 ? root.detailText
                                                         : qsTr("Click a row to see it whole: the bytes, or the JSON line.")
                        color: root.detailText.length > 0 ? Theme.textPrimary : Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: 11
                        background: null
                    }
                }
            }
        }

        // ── Verso Decodium ──
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: sendGrid.implicitHeight + 20
            radius: 6
            color: Theme.bgMedium
            border.color: Theme.glassBorder
            border.width: 1

            GridLayout {
                id: sendGrid
                anchors.fill: parent
                anchors.margins: 10
                columns: 2
                columnSpacing: 10
                rowSpacing: 8

                Text {
                    text: qsTr("To Decodium (UDP)")
                    color: Theme.primaryColor
                    font.pixelSize: 12
                    font.bold: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    StyledComboBox {
                        id: clientBox
                        Layout.preferredWidth: 200
                        model: root.mon.udpClients
                        enabled: count > 0
                        displayText: count > 0 ? currentText : qsTr("no program yet")
                    }
                    GlassButton {
                        text: qsTr("Reply to the line")
                        enabled: root.selectedCanReply
                        onClicked: root.mon.reply(root.selectedSerial, 0)
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Like the double click in Decodium: it answers a CQ, and it may start transmitting.")
                    }
                    GlassButton {
                        text: qsTr("Halt TX")
                        tone: Theme.errorColor
                        enabled: clientBox.count > 0
                        onClicked: root.mon.haltTx(root.client, false)
                    }
                    GlassButton {
                        text: qsTr("Auto TX off")
                        enabled: clientBox.count > 0
                        onClicked: root.mon.haltTx(root.client, true)
                    }
                    GlassButton {
                        text: qsTr("Replay")
                        enabled: clientBox.count > 0
                        onClicked: root.mon.replay(root.client)
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Decodium sends again all the decodes it has on screen.")
                    }
                    StyledComboBox {
                        id: clearWhich
                        Layout.preferredWidth: 190
                        model: [qsTr("Band activity"), qsTr("Rx frequency"), qsTr("Both windows")]
                    }
                    GlassButton {
                        text: qsTr("Clear")
                        enabled: clientBox.count > 0
                        onClicked: root.mon.clearWindows(root.client, clearWhich.currentIndex)
                    }
                    Item { Layout.fillWidth: true }
                }

                Item { implicitWidth: 1; implicitHeight: 1 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    StyledTextField {
                        id: freeTextField
                        Layout.preferredWidth: 240
                        placeholderText: qsTr("Free text (13 characters)")
                        maximumLength: 13
                    }
                    GlassButton {
                        text: qsTr("Set")
                        enabled: clientBox.count > 0 && freeTextField.text.trim().length > 0
                        onClicked: root.mon.freeText(root.client, freeTextField.text.trim().toUpperCase(), false)
                    }
                    GlassButton {
                        text: qsTr("Set and transmit")
                        tone: Theme.warningColor
                        enabled: clientBox.count > 0 && freeTextField.text.trim().length > 0
                        onClicked: root.mon.freeText(root.client, freeTextField.text.trim().toUpperCase(), true)
                    }
                    Item { implicitWidth: 16 }
                    StyledTextField {
                        id: gridField
                        Layout.preferredWidth: 90
                        placeholderText: qsTr("Locator")
                        maximumLength: 6
                    }
                    GlassButton {
                        text: qsTr("Send locator")
                        enabled: clientBox.count > 0 && gridField.text.trim().length >= 4
                        onClicked: root.mon.location(root.client, gridField.text)
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Decodium uses it only with the automatic locator turned on.")
                    }
                    Item { Layout.fillWidth: true }
                }

                Item { implicitWidth: 1; implicitHeight: 1 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    StyledTextField {
                        id: highlightCall
                        Layout.preferredWidth: 200
                        placeholderText: qsTr("Callsign to highlight")
                    }
                    Text { text: qsTr("background"); color: Theme.textSecondary; font.pixelSize: 11 }
                    ColorSwatch { id: highlightBg; value: "#ffcc00"; onPicked: (v) => value = v }
                    Text { text: qsTr("text"); color: Theme.textSecondary; font.pixelSize: 11 }
                    ColorSwatch { id: highlightFg; value: "#000000"; onPicked: (v) => value = v }
                    GlassButton {
                        text: qsTr("Highlight")
                        enabled: clientBox.count > 0 && highlightCall.text.trim().length > 0
                        onClicked: root.mon.highlight(root.client, highlightCall.text, highlightBg.value, highlightFg.value, false)
                    }
                    GlassButton {
                        text: qsTr("Remove the highlight")
                        enabled: clientBox.count > 0 && highlightCall.text.trim().length > 0
                        onClicked: root.mon.highlight(root.client, highlightCall.text, "", "", false)
                    }
                    Item { Layout.fillWidth: true }
                }

                Item { implicitWidth: 1; implicitHeight: 1 }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    text: qsTr("They leave from the same UDP port Decodium writes to, with its program name. Decodium carries them out only for its main UDP destination (Settings → Reporting → UDP Server) and with \"Accept UDP requests\" on: if DecoDXLog is its second or third destination it ignores them.")
                }

                Text {
                    text: qsTr("To Decodium (DecoLink)")
                    color: Theme.accentColor
                    font.pixelSize: 12
                    font.bold: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    StyledTextField {
                        id: jsonField
                        Layout.fillWidth: true
                        placeholderText: "{\"type\":\"tune\",\"freqKhz\":14074,\"dialKhz\":14074,\"audioHz\":1500,\"mode\":\"FT8\"}"
                        onAccepted: if (root.mon.decoLinkClients > 0) root.mon.sendDecoLink(text)
                    }
                    GlassButton {
                        text: qsTr("Send")
                        enabled: root.mon.decoLinkClients > 0 && jsonField.text.trim().length > 0
                        onClicked: root.mon.sendDecoLink(jsonField.text)
                    }
                    GlassButton {
                        text: qsTr("Send the log list again")
                        enabled: root.mon.decoLinkClients > 0
                        onClicked: root.mon.resendDecoLinkList()
                    }
                    GlassButton {
                        text: qsTr("Send the award state")
                        enabled: root.mon.decoLinkClients > 0
                        onClicked: root.mon.resendDecoLinkAward()
                    }
                }

                Text {
                    text: "DecoPort"
                    color: Theme.warningColor
                    font.pixelSize: 12
                    font.bold: true
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    text: qsTr("Listen only: the DecoPort session is signed with Decodium's key, and one of its commands keys the transmitter. Here you see which radio is on the network, on what frequency and in which state.")
                }
            }
        }

        // L'esito dell'ultimo invio.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                Layout.fillWidth: true
                text: root.mon.lastResult
                color: root.mon.lastResultOk ? Theme.accentColor : Theme.errorColor
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            GlassButton { text: qsTr("Close"); onClicked: root.close() }
        }
    }

    TextEdit {
        id: copyBuffer
        visible: false
    }
}
