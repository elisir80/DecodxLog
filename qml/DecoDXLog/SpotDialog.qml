// DecoDXLog — segnalare una stazione al DX cluster.
//
// Si riceve il cluster, e va anche mandato: la stazione che si sta lavorando o che si
// sente sulla banda, con la frequenza e due parole di commento. Il riquadro controlla
// prima (nominativo, frequenza dentro le bande, commento al massimo di 30 caratteri) e
// dice se qualcuno l'ha gia' segnalata da poco; poi manda «DX <kHz> <nominativo>
// <commento>» al nodo scelto, lo stesso comando che si scriverebbe nella console.
//
// E' un pezzo solo: chi lo usa (il QSO nuovo, l'elenco delle decodifiche, il cluster, il
// menu) lo mette dove vuole e chiama openFor() o openCurrent().
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Item {
    id: root
    width: 0
    height: 0
    // Fuori dai layout che lo ospitano: non occupa posto, ne' conta come figlio.
    visible: false

    readonly property var cluster: decolog.cluster

    // Con quello che si sa gia': nominativo, frequenza in kHz, commento.
    function openFor(call, freqKhz, comment) {
        callField.text = call || ""
        freqField.text = freqKhz ? String(freqKhz) : ""
        commentField.text = comment || ""
        failure.text = ""
        popup.open()
    }
    // Con la stazione di adesso: il nominativo nella scheda, la frequenza della radio (o
    // la dial di Decodium), il modo.
    function openCurrent() {
        const info = decolog.callInfo
        const hz = decolog.rig.connected ? decolog.rig.frequencyHz
                                         : Math.round(parseFloat(decolog.dialFrequency || "0") * 1e6)
        openFor(info && info.call ? info.call : "",
                hz > 0 ? (hz / 1000).toFixed(1) : "",
                decolog.rig.connected && decolog.rig.mode ? decolog.rig.mode : decolog.currentMode)
    }

    Popup {
        id: popup
        // In una finestra sua il popup riceve i tasti solo se ha il fuoco: senza
        // "focus: true" il campo prende il cursore ma quello che si scrive si perde.
        popupType: Popup.Window
        focus: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 560
        modal: true
        padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: Theme.panelColor; border.color: Theme.glassBorder; radius: 6 }
        onOpened: (callField.text.length > 0 ? freqField : callField).forceActiveFocus()

        // I nodi a cui si puo' parlare: i cluster telnet collegati.
        readonly property var nodes: root.cluster.sources.filter(s => s.type === "cluster" && s.online)
        // Quello che il riquadro sta per mandare, controllato.
        readonly property var draft: root.cluster.checkSpot(callField.text, freqField.text, commentField.text)
        readonly property bool touched: callField.text.length > 0 || freqField.text.length > 0
        readonly property bool duplicate: !!(draft.ok && draft.duplicate && draft.duplicate.length > 0)

        function send() {
            if (!sendButton.enabled)
                return
            const error = root.cluster.postSpot(callField.text, freqField.text, commentField.text,
                                                nodeBox.ids[Math.max(0, nodeBox.currentIndex)])
            if (error.length > 0)
                failure.text = error
            else
                close()
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text {
                text: qsTr("SPOT A STATION")
                color: Theme.secondaryColor
                font.family: Theme.monoFamily
                font.pixelSize: 11
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                font.pixelSize: 11
                text: qsTr("Sends the spot to a cluster node as DX <kHz> <call> <comment>, the same line you would type in "
                           + "the console. Spot what you have really heard or worked; the comment is cut at 30 characters.")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                LabeledField {
                    Layout.fillWidth: false
                    label: qsTr("Call")
                    StyledTextField {
                        id: callField
                        Layout.preferredWidth: 150
                        uppercase: true
                        onTextChanged: failure.text = ""
                        Keys.onReturnPressed: freqField.forceActiveFocus()
                    }
                }
                LabeledField {
                    Layout.fillWidth: false
                    label: qsTr("Frequency (kHz)")
                    StyledTextField {
                        id: freqField
                        Layout.preferredWidth: 150
                        placeholderText: "14074.0"
                        onTextChanged: failure.text = ""
                        Keys.onReturnPressed: commentField.forceActiveFocus()
                    }
                }
                LabeledField {
                    Layout.fillWidth: true
                    label: qsTr("Comment")
                    StyledTextField {
                        id: commentField
                        Layout.fillWidth: true
                        mono: false
                        maximumLength: 30
                        placeholderText: qsTr("FT8 -10 dB JN71")
                        onTextChanged: failure.text = ""
                        Keys.onReturnPressed: popup.send()
                    }
                }
            }
            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Send to")
                StyledComboBox {
                    id: nodeBox
                    Layout.fillWidth: true
                    readonly property var ids: [""].concat(popup.nodes.map(s => s.id))
                    model: [qsTr("First node online")].concat(popup.nodes.map(s => s.name))
                }
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 12
                // Un errore dice cosa manca; il resto dice cosa sta per partire.
                color: failure.text.length > 0 || popup.nodes.length === 0 || (popup.touched && !popup.draft.ok)
                       ? Theme.errorColor : popup.duplicate ? Theme.warningColor : Theme.textSecondary
                text: failure.text.length > 0 ? failure.text
                    : popup.nodes.length === 0 ? qsTr("No cluster node connected")
                    : popup.touched && !popup.draft.ok ? popup.draft.error
                    : popup.duplicate ? popup.draft.duplicate
                    : popup.draft.ok ? qsTr("%1 · %2 kHz").arg(popup.draft.band).arg(popup.draft.freqKhz)
                    : ""
            }
            Text { id: failure; visible: false }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                GlassButton { text: qsTr("Cancel"); onClicked: popup.close() }
                GlassButton {
                    id: sendButton
                    text: popup.duplicate ? qsTr("Spot anyway") : qsTr("Send spot")
                    tone: Theme.accentColor
                    filled: true
                    enabled: popup.draft.ok && popup.nodes.length > 0
                    onClicked: popup.send()
                }
            }
        }
    }
}
