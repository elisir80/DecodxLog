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

    function fromIso() { return period.fromIso }
    function toIso() { return period.toIso }
    readonly property bool valid: period.valid
    // Il fuoco alla prima data: un popup in una finestra sua non lo da' da solo.
    function focusFirst() { period.focusFirst() }

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
    DatePeriodFields {
        id: period
        onAccepted: if (go.enabled) go.clicked()
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
