// DecoDXLog — lo scarico da LoTW per un periodo: le conferme dei QSO fatti
// dal … al …, estremi compresi. Uno dei due si puo' lasciare vuoto (dal primo
// QSO, o fino a oggi). Le date si scrivono come nel resto del programma.
import QtQuick
import QtQuick.Layouts
import Decodium.UI

RowLayout {
    id: root
    spacing: 8
    // Scaricato: chi lo ospita (un menu, una finestra) si puo' chiudere.
    signal started()
    // Quale servizio: "lotw", "eqsl" o "qrz". Con pickService si sceglie qui.
    property string service: "lotw"
    property bool pickService: false
    readonly property bool busy: service === "lotw" ? decolog.lotwBusy : decolog.confirmBusy

    function fromIso() { return decolog.readDate(fromField.text) }
    function toIso() { return decolog.readDate(toField.text) }
    readonly property bool valid: (fromField.text.trim().length === 0 || fromIso().length > 0)
                                  && (toField.text.trim().length === 0 || toIso().length > 0)
                                  && (fromField.text.trim().length > 0 || toField.text.trim().length > 0)

    LabeledField {
        visible: root.pickService
        Layout.fillWidth: false
        label: qsTr("Service")
        StyledComboBox {
            Layout.preferredWidth: 110
            readonly property var ids: ["lotw", "eqsl", "qrz"]
            model: ["LoTW", "eQSL", "QRZ"]
            currentIndex: Math.max(0, ids.indexOf(root.service))
            onActivated: root.service = ids[currentIndex]
        }
    }
    LabeledField {
        Layout.fillWidth: false
        label: qsTr("QSOs from")
        StyledTextField {
            id: fromField
            Layout.preferredWidth: 130
            placeholderText: decolog.dateHint
            color: text.length > 0 && root.fromIso().length === 0 ? Theme.errorColor : Theme.textPrimary
        }
    }
    LabeledField {
        Layout.fillWidth: false
        label: qsTr("to")
        StyledTextField {
            id: toField
            Layout.preferredWidth: 130
            placeholderText: decolog.showDate(decolog.utcNow().date)
            color: text.length > 0 && root.toIso().length === 0 ? Theme.errorColor : Theme.textPrimary
            Keys.onReturnPressed: if (root.valid) go.clicked()
        }
    }
    GlassButton {
        id: go
        Layout.alignment: Qt.AlignBottom
        Layout.bottomMargin: 2
        text: qsTr("Download this period")
        tone: Theme.primaryColor
        enabled: root.valid && !root.busy
        onClicked: {
            if (root.service === "lotw")
                decolog.syncLotwRange(root.fromIso(), root.toIso())
            else
                decolog.syncConfirmationsRange(root.service, root.fromIso(), root.toIso())
            root.started()
        }
    }
    Item { Layout.fillWidth: true }
}
