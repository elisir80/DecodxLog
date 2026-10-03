// DecoDXLog — Decodium dentro il log: Full Spectrum e Signal RX in un pannello
// compatto, sulla lavagna come gli altri (si sposta, si ridimensiona, si
// attacca ai bordi, si stacca in una finestra).
//
// Full Spectrum e' tutto quello che Decodium sente nella banda; Signal RX e'
// il QSO in corso: le nostre trasmissioni, chi ci chiama, il corrispondente.
// Larghi, i due elenchi stanno uno accanto all'altro; alti e stretti, uno
// sopra l'altro. Le righe sono colorate secondo il log (entita' nuova, banda
// nuova, nominativo nuovo) con gli stessi colori del cluster.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Decodium.UI

GlassPanel {
    id: root

    readonly property var feed: decolog.decodium
    readonly property bool sideBySide: width >= height * 1.45 && width >= 560

    // Quanta parte dello spazio ha Full Spectrum: separata per i due modi di
    // stare insieme, perche' la meta' di una colonna non e' la meta' di una riga.
    Settings {
        id: store
        category: "layout/decodiumpanel"
        property real splitRows: 0.62
        property real splitColumns: 0.58
        property bool newestOnTop: true
    }

    // Per le schermate di prova.
    function showMenu(name) {
        if (name === "row") fullList.showMenu()
        else if (name === "signalmenu") signalList.showMenu()
    }

    title: qsTr("Decodium")
    dotColor: feed.online ? Theme.accentColor : Theme.errorColor
    padding: 0

    headerTools: [
        Pill {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.feed.online && root.feed.mode.length > 0
            text: root.feed.band.length > 0 ? root.feed.mode + " · " + root.feed.band : root.feed.mode
            tone: Theme.primaryColor
            pillHeight: 20
        },
        PanelControl {
            anchors.verticalCenter: parent.verticalCenter
            glyph: store.newestOnTop ? "↓" : "↑"
            hint: store.newestOnTop ? qsTr("Newest on top: click for newest at the bottom, like Decodium")
                                    : qsTr("Newest at the bottom, like Decodium: click for newest on top")
            onClicked: store.newestOnTop = !store.newestOnTop
        },
        PanelControl {
            anchors.verticalCenter: parent.verticalCenter
            glyph: "⟲"
            hint: qsTr("Ask Decodium to send again the decodes it has on screen")
            onClicked: root.feed.replay()
        }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Lo stato di Decodium, in una riga: trasmette o ascolta, dove, con chi.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 24
            color: root.feed.transmitting ? Qt.rgba(Theme.errorColor.r, Theme.errorColor.g, Theme.errorColor.b, 0.18)
                                          : "transparent"
            visible: root.feed.online
            RowLayout {
                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                spacing: 10
                Text {
                    text: root.feed.transmitting ? qsTr("TX") : root.feed.decoding ? qsTr("RX · decoding") : qsTr("RX")
                    color: root.feed.transmitting ? Theme.errorColor : Theme.accentColor
                    font.family: Theme.monoFamily; font.pixelSize: 11; font.bold: true
                }
                Text {
                    text: qsTr("Rx %1 Hz · Tx %2 Hz").arg(root.feed.rxDf).arg(root.feed.txDf)
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily; font.pixelSize: 11
                }
                Text {
                    visible: root.feed.dxCall.length > 0
                    text: qsTr("DX %1").arg(root.feed.dxCall)
                    color: Theme.warningColor
                    font.family: Theme.monoFamily; font.pixelSize: 11; font.bold: true
                }
                Text {
                    Layout.fillWidth: true
                    visible: root.feed.transmitting && root.feed.txMessage.length > 0
                    text: root.feed.txMessage
                    elide: Text.ElideRight
                    color: Theme.errorColor
                    font.family: Theme.monoFamily; font.pixelSize: 11
                }
                Item { Layout.fillWidth: true; visible: !(root.feed.transmitting && root.feed.txMessage.length > 0) }
            }
        }

        // Nessuna notizia di Decodium e niente da vedere: si dice cosa fare.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.feed.online && fullList.model.count === 0 && signalList.model.count === 0
            Text {
                anchors.centerIn: parent
                width: parent.width - 32
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                font.pixelSize: 12
                text: decolog.listening
                      ? qsTr("Waiting for Decodium on UDP port %1.\nIn Decodium: Settings → Reporting → UDP Server 127.0.0.1, port %1, and \"Accept UDP requests\".").arg(decolog.udpPort)
                      : qsTr("DecoDXLog is not listening on a UDP port: set it in Settings → Decodium link.")
            }
        }

        // I due elenchi: uno accanto all'altro o uno sopra l'altro, con la
        // maniglia in mezzo. La proporzione si ricorda e segue il pannello
        // quando lo si ridimensiona sulla lavagna.
        Item {
            id: split
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.feed.online || fullList.model.count > 0 || signalList.model.count > 0
            readonly property real grip: 6
            readonly property real extent: root.sideBySide ? width : height
            readonly property real minPart: root.sideBySide ? 170 : 64
            readonly property real ratio: root.sideBySide ? store.splitColumns : store.splitRows
            // La parte di Full Spectrum, in pixel, tenuta dentro i minimi.
            readonly property real firstSize: extent <= 0 ? 0
                : Math.max(minPart, Math.min(extent - grip - minPart, Math.round(extent * ratio - grip / 2)))

            DecodeList {
                id: fullList
                model: root.feed.fullSpectrum
                title: qsTr("Full Spectrum")
                tone: Theme.primaryColor
                which: 0
                newestOnTop: store.newestOnTop
                emptyText: qsTr("Every decode Decodium hears on the band shows up here.")
                x: 0
                y: 0
                width: root.sideBySide ? split.firstSize : split.width
                height: root.sideBySide ? split.height : split.firstSize
            }
            Rectangle {
                id: divider
                x: root.sideBySide ? fullList.width : 0
                y: root.sideBySide ? 0 : fullList.height
                width: root.sideBySide ? split.grip : split.width
                height: root.sideBySide ? split.height : split.grip
                color: gripArea.pressed ? Theme.primaryColor
                     : gripArea.containsMouse ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.5)
                     : Theme.borderSoft
                MouseArea {
                    id: gripArea
                    anchors.fill: parent
                    anchors.margins: -3
                    hoverEnabled: true
                    cursorShape: root.sideBySide ? Qt.SizeHorCursor : Qt.SizeVerCursor
                    onPositionChanged: (mouse) => {
                        if (!pressed || split.extent <= 0)
                            return
                        const p = mapToItem(split, mouse.x, mouse.y)
                        const at = root.sideBySide ? p.x : p.y
                        const r = Math.max(0.1, Math.min(0.9, at / split.extent))
                        if (root.sideBySide) store.splitColumns = r
                        else store.splitRows = r
                    }
                }
            }
            DecodeList {
                id: signalList
                model: root.feed.signalRx
                title: qsTr("Signal RX")
                tone: Theme.warningColor
                which: 1
                newestOnTop: store.newestOnTop
                emptyText: qsTr("Your transmissions, whoever calls you and your QSO partner show up here.")
                x: root.sideBySide ? split.firstSize + split.grip : 0
                y: root.sideBySide ? 0 : split.firstSize + split.grip
                width: root.sideBySide ? split.width - x : split.width
                height: root.sideBySide ? split.height : split.height - y
            }
        }
    }
}
