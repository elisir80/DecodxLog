// DecoDXLog — le macro CW da scrivere, una riga per macro.
//
// A sinistra la scritta del tasto (tutta libera, anche "F1"), a destra il testo
// che va in aria; "−" toglie la riga, "+ Aggiungi" ne mette una in fondo. Le
// prime dodici stanno sui tasti F1-F12, le altre si mandano col clic. Serve
// alla finestra delle macro (CwMacroEditor) e alle Impostazioni.
//
// Le righe sono tante quante le macro e restano quelle: il modello e' il
// numero, non l'elenco. Con l'elenco come modello ogni salvataggio rifaceva
// tutte le righe, e il campo dove si stava scrivendo spariva sotto le dita —
// quello che si scriveva dopo il primo campo non arrivava mai alla radio ne'
// al file. Si salva uscendo dal campo, e comunque quando la finestra si chiude
// (commit()).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

ColumnLayout {
    id: root

    // -1: tutte; altrimenti quella sola.
    property int only: -1
    // Dentro una finestra le righe scorrono sotto questa altezza; nelle
    // Impostazioni (0) si vedono tutte, scorre la pagina.
    property real maxListHeight: 0
    readonly property int count: decolog.rig.macros.length
    readonly property int maxCount: 24

    // Salva quello che c'e' nei campi, anche se il cursore e' ancora li'.
    signal commitRequested()
    function commit() { root.commitRequested() }

    spacing: 8

    ScrollView {
        id: scroll
        Layout.fillWidth: true
        Layout.preferredHeight: root.maxListHeight > 0 ? Math.min(root.maxListHeight, rows.implicitHeight)
                                                       : rows.implicitHeight
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: rows
            width: scroll.availableWidth
            spacing: 6
            Repeater {
                model: root.count
                RowLayout {
                    id: macroRow
                    required property int index
                    readonly property var macro: decolog.rig.macros[index] || ({})
                    visible: root.only < 0 || root.only === index
                    Layout.fillWidth: true
                    spacing: 8

                    function save() {
                        if (macroLabel.text !== (macro.label || "") || macroText.text !== (macro.text || ""))
                            decolog.rig.setMacro(macroRow.index, macroLabel.text, macroText.text)
                    }
                    Connections {
                        target: root
                        function onCommitRequested() { macroRow.save() }
                    }

                    Text {
                        Layout.preferredWidth: 30
                        // Il tasto funzione: solo le prime dodici ne hanno uno.
                        text: macroRow.index < 12 ? "F" + (macroRow.index + 1) : "·"
                        color: Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: 11
                    }
                    StyledTextField {
                        id: macroLabel
                        Layout.preferredWidth: 150
                        mono: true
                        text: macroRow.macro.label || ""
                        placeholderText: macroRow.index < 12 ? "F" + (macroRow.index + 1) : ""
                        onEditingFinished: macroRow.save()
                    }
                    StyledTextField {
                        id: macroText
                        Layout.fillWidth: true
                        text: macroRow.macro.text || ""
                        uppercase: true
                        onEditingFinished: macroRow.save()
                    }
                    GlassButton {
                        text: "−"
                        tone: Theme.errorColor
                        minimumWidth: 30
                        implicitWidth: 30
                        fontPixelSize: 14
                        enabled: root.count > 1
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove this macro")
                        onClicked: {
                            root.commit()
                            decolog.rig.removeMacro(macroRow.index)
                            if (root.only >= root.count)
                                root.only = -1
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        GlassButton {
            text: qsTr("+ Add a macro")
            tone: Theme.accentColor
            enabled: root.count < root.maxCount
            onClicked: {
                root.commit()
                decolog.rig.addMacro()
                root.only = -1
                Qt.callLater(() => scroll.ScrollBar.vertical.position = 1.0 - scroll.ScrollBar.vertical.size)
            }
        }
        GlassButton {
            visible: root.only >= 0
            text: qsTr("All macros…")
            onClicked: root.only = -1
        }
        Item { Layout.fillWidth: true }
        // Riporta tutte le macro a quelle di partenza: al primo clic chiede
        // conferma, al secondo (entro quattro secondi) lo fa.
        GlassButton {
            id: resetButton
            property bool armed: false
            text: armed ? qsTr("Sure? Click again") : qsTr("Default macros")
            tone: armed ? Theme.errorColor : "transparent"
            onClicked: {
                if (!armed) {
                    armed = true
                    disarm.restart()
                    return
                }
                armed = false
                disarm.stop()
                decolog.rig.resetMacros()
            }
            Timer { id: disarm; interval: 4000; onTriggered: resetButton.armed = false }
        }
    }
}
