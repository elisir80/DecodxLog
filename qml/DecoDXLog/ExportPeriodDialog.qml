// DecoDXLog — esportare i QSO di un periodo: dal … al …, come si sceglie lo
// scarico da LoTW. Dice quanti QSO sono e poi chiede dove salvarli.
//
// E' un pezzo solo: il riquadro per le date e la finestra del salvataggio. Chi lo
// usa (il menu File, le azioni del log) lo mette dove vuole e chiama openDialog().
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Decodium.UI

Item {
    id: root
    width: 0
    height: 0

    // Vuoto, o con un periodo di partenza (date nella lingua del programma).
    function openDialog(from, to) {
        period.setTexts(from || "", to || "")
        popup.open()
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
        onOpened: period.focusFirst()

        // Quanti QSO ci sono nel periodo; -1 finche' le date non si leggono.
        readonly property int count: period.valid ? decolog.countQsoBetween(period.fromIso, period.toIso) : -1

        contentItem: ColumnLayout {
            spacing: 10

            Text {
                text: qsTr("EXPORT A PERIOD")
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
                text: qsTr("The QSOs made from the first to the last day, both included (UTC). Leave one of the two "
                           + "empty to start from the first QSO or to go on to the last.")
            }
            DatePeriodFields {
                id: period
                Layout.fillWidth: true
                fromLabel: qsTr("From (UTC)")
                onAccepted: if (exportButton.enabled) exportButton.clicked()
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 12
                color: period.inverted || popup.count === 0 ? Theme.warningColor : Theme.textSecondary
                text: period.inverted ? qsTr("The period starts after it ends")
                    : popup.count === 0 ? qsTr("No QSO in this period")
                    : popup.count > 0 ? qsTr("%1 QSO in this period").arg(popup.count)
                    : ""
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                GlassButton { text: qsTr("Cancel"); onClicked: popup.close() }
                GlassButton {
                    id: exportButton
                    text: popup.count > 0 ? qsTr("Export %1 QSO…").arg(popup.count) : qsTr("Export…")
                    tone: Theme.accentColor
                    filled: true
                    enabled: period.valid && popup.count > 0
                    onClicked: {
                        file.from = period.fromIso
                        file.to = period.toIso
                        popup.close()
                        file.openFor()
                    }
                }
            }
        }
    }

    // Il nome di partenza dice il periodo, cosi' non si salvano tre "export.adi"
    // uno sopra l'altro.
    FileDialog {
        id: file
        property string from: ""
        property string to: ""
        function openFor() {
            const name = from && to ? from + "_" + to
                       : from ? "from-" + from : "until-" + to
            currentFile = "file:///decolog-" + name + ".adi"
            open()
        }
        title: qsTr("Save the QSO of the period")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "adi"
        nameFilters: [qsTr("ADIF files (*.adi)")]
        onAccepted: decolog.exportPeriod(from, to, selectedFile)
        // Se non si salva, il periodo scelto resta li' per riprovare.
        onRejected: popup.open()
    }
}
