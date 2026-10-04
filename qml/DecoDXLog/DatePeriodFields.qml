// DecoDXLog — «dal … al …»: le due date di un periodo, scritte come nel resto del
// programma (25/09/2026 in italiano) e tenute ISO. Una delle due si puo' lasciare
// vuota: dal primo QSO, o fino a oggi. Lo usano lo scarico da LoTW/eQSL/QRZ e
// l'export del log, cosi' si scelgono i QSO allo stesso modo ovunque.
import QtQuick
import QtQuick.Layouts
import Decodium.UI

RowLayout {
    id: root
    spacing: 8

    property string fromLabel: qsTr("QSOs from")
    readonly property string fromIso: decolog.readDate(fromField.text)
    readonly property string toIso: decolog.readDate(toField.text)
    readonly property bool hasFrom: fromField.text.trim().length > 0
    readonly property bool hasTo: toField.text.trim().length > 0
    // Il periodo finisce prima di cominciare.
    readonly property bool inverted: fromIso.length > 0 && toIso.length > 0 && fromIso > toIso
    // Date che si leggono, almeno una, e nell'ordine giusto.
    readonly property bool valid: (!hasFrom || fromIso.length > 0) && (!hasTo || toIso.length > 0)
                                  && (hasFrom || hasTo) && !inverted
    // Invio nell'ultima data: chi ospita il campo decide cosa farne.
    signal accepted()

    function focusFirst() { fromField.forceActiveFocus() }
    function clear() { fromField.text = ""; toField.text = "" }
    // Date gia' scritte, nella lingua del programma (per partire da un periodo noto).
    function setTexts(from, to) { fromField.text = from; toField.text = to }

    LabeledField {
        Layout.fillWidth: false
        label: root.fromLabel
        StyledTextField {
            id: fromField
            Layout.preferredWidth: 130
            placeholderText: decolog.dateHint
            color: text.length > 0 && root.fromIso.length === 0 || root.inverted ? Theme.errorColor : Theme.textPrimary
            Keys.onReturnPressed: toField.forceActiveFocus()
        }
    }
    LabeledField {
        Layout.fillWidth: false
        label: qsTr("to")
        StyledTextField {
            id: toField
            Layout.preferredWidth: 130
            placeholderText: decolog.showDate(decolog.utcNow().date)
            color: text.length > 0 && root.toIso.length === 0 || root.inverted ? Theme.errorColor : Theme.textPrimary
            Keys.onReturnPressed: root.accepted()
        }
    }
}
