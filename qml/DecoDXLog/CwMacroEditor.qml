// DecoDXLog — la finestra delle macro CW: tutte, o una sola (dal tasto
// destro su un tasto). Le righe sono quelle di CwMacroList, con "+" e "−".
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Popup {
    id: root

    // Una riga di spiegazione in piu' (il contest dice come va l'ESM).
    property string note: ""

    function openAll() { list.only = -1; open() }
    function openFor(index) { list.only = index; open() }

    popupType: Popup.Window
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 800
    modal: true
    padding: 14
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: Theme.panelColor; border.color: Theme.glassBorder; radius: 6 }

    contentItem: ColumnLayout {
        spacing: 8

        Text {
            text: list.only >= 0 ? qsTr("CW MACRO F%1").arg(list.only + 1) : qsTr("CW MACROS")
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
            text: qsTr("On the left what the key shows (\"F1 CQ\", \"CQ\", anything): it is the whole label, "
                       + "the key stays the same size. On the right what goes on air: {CALL} the station you "
                       + "work, {MYCALL} yours, {RST} the report, {NR} your serial, {EXCH} what you received.")
        }
        Text {
            Layout.fillWidth: true
            visible: root.note.length > 0
            wrapMode: Text.Wrap
            color: Theme.textSecondary
            font.pixelSize: 11
            text: root.note
        }
        CwMacroList {
            id: list
            Layout.fillWidth: true
            maxListHeight: 440
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            GlassButton { text: qsTr("Close"); tone: Theme.primaryColor; filled: true; onClicked: root.close() }
        }
    }
}
