// DecoDXLog — le macro CW, da scrivere: tutte e dodici, o una sola.
//
// openAll() le mostra tutte; openFor(indice) solo quella del tasto cliccato
// col destro. La scritta del tasto e' libera (anche "F1"); il testo va in aria
// com'e', con i buchi riempiti al momento.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Popup {
    id: root

    // -1: tutte; altrimenti quella sola.
    property int only: -1

    function openAll() { root.only = -1; open() }
    function openFor(index) { root.only = index; open() }

    popupType: Popup.Window
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 760
    modal: true
    padding: 14
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: Theme.panelColor; border.color: Theme.glassBorder; radius: 6 }

    contentItem: ColumnLayout {
        spacing: 8

        Text {
            text: root.only >= 0 ? qsTr("CW MACRO F%1").arg(root.only + 1) : qsTr("CW MACROS")
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
        Repeater {
            model: decolog.rig.macros
            RowLayout {
                id: macroRow
                required property var modelData
                required property int index
                visible: root.only < 0 || root.only === index
                Layout.fillWidth: true
                spacing: 8
                Text {
                    Layout.preferredWidth: 30
                    text: "F" + (macroRow.index + 1)
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
                StyledTextField {
                    id: macroLabel
                    Layout.preferredWidth: 150
                    mono: true
                    text: macroRow.modelData.label
                    placeholderText: "F" + (macroRow.index + 1)
                    onEditingFinished: decolog.rig.setMacro(macroRow.index, text, macroText.text)
                }
                StyledTextField {
                    id: macroText
                    Layout.fillWidth: true
                    text: macroRow.modelData.text
                    uppercase: true
                    onEditingFinished: decolog.rig.setMacro(macroRow.index, macroLabel.text, text)
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            GlassButton {
                visible: root.only < 0
                text: qsTr("Default macros")
                onClicked: decolog.rig.resetMacros()
            }
            GlassButton {
                visible: root.only >= 0
                text: qsTr("All macros…")
                onClicked: root.only = -1
            }
            Item { Layout.fillWidth: true }
            GlassButton { text: qsTr("Close"); tone: Theme.primaryColor; filled: true; onClicked: root.close() }
        }
    }
}
