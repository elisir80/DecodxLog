// DecoDXLog — mandare il log a gara finita.
//
// Il programma non manda niente da solo: scrive il Cabrillo dove dici tu e
// apre la pagina del contest, che e' il posto dove il log va caricato. Chi
// carica sei tu, e vedi cosa carichi — un log spedito per sbaglio non si
// richiama indietro.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Decodium.UI

DialogFrame {
    id: root

    readonly property var act: decolog.activation
    property int revision: 0
    readonly property var scoring: { revision; return act.score() }
    property string message: ""
    property bool messageOk: false

    function openDialog() {
        revision++
        message = ""
        messageOk = false
        open()
    }

    title: qsTr("Send the log")
    dotColor: Theme.accentColor
    dialogKey: "contestsubmit"
    width: 620
    height: 460

    FileDialog {
        id: saveDialog
        title: qsTr("Write the Cabrillo")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "cbr"
        nameFilters: [qsTr("Cabrillo (*.cbr *.log)"), qsTr("All files (*)")]
        onAccepted: {
            const info = root.act.cabrilloDefaults()
            info.claimedScore = root.scoring.score || 0
            root.message = root.act.exportCabrillo(selectedFile, info)
            root.messageOk = root.message.length === 0
            if (root.messageOk)
                root.message = qsTr("Cabrillo written. Now upload it on the contest page.")
        }
    }

    // La riga di spiegazione, come nelle altre finestre.
    component Note: Text {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Theme.textSecondary
        font.pixelSize: 12
    }

    body: ColumnLayout {
        anchors.margins: 14
        spacing: 12

        SectionTitle { text: root.scoring.contestId || qsTr("Contest") }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: [
                    { label: qsTr("QSO"), value: String(root.act.qsoCount || 0) },
                    { label: qsTr("Points"), value: String(root.scoring.points || 0) },
                    { label: qsTr("Mult"), value: String(root.scoring.multipliers || 0) },
                    { label: qsTr("Score"), value: String(root.scoring.score || 0) },
                ]
                Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    radius: 5
                    color: Theme.bgDeep
                    border.color: Theme.borderSoft
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 1
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.label
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 10
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.value
                            color: Theme.accentColor
                            font.family: Theme.monoFamily
                            font.pixelSize: 20
                            font.bold: true
                        }
                    }
                }
            }
        }

        Note {
            text: root.scoring.submitDays > 0
                  ? qsTr("The log goes uploaded on the contest page, within %n day(s) from the end "
                         + "of the contest. The score written in the Cabrillo is the one counted "
                         + "here; who checks the logs recounts it anyway.", "", root.scoring.submitDays)
                  : qsTr("The log goes uploaded on the contest page. The score written in the "
                         + "Cabrillo is the one counted here; who checks the logs recounts it anyway.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            GlassButton {
                text: qsTr("Write the Cabrillo…")
                tone: Theme.primaryColor
                filled: true
                onClicked: saveDialog.open()
            }
            GlassButton {
                text: qsTr("Open the contest page")
                tone: Theme.accentColor
                enabled: (root.scoring.submitUrl || "").length > 0
                onClicked: Qt.openUrlExternally(root.scoring.submitUrl)
            }
            Item { Layout.fillWidth: true }
        }

        Text {
            Layout.fillWidth: true
            visible: (root.scoring.submitUrl || "").length > 0
            text: root.scoring.submitUrl
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
            wrapMode: Text.WrapAnywhere
        }
        Text {
            Layout.fillWidth: true
            visible: (root.scoring.submitUrl || "").length === 0
            wrapMode: Text.Wrap
            text: qsTr("For this contest the program does not know where the log goes: write the "
                       + "Cabrillo and send it the way the rules say.")
            color: Theme.warningColor
            font.pixelSize: 12
        }

        Text {
            Layout.fillWidth: true
            visible: root.message.length > 0
            text: root.message
            color: root.messageOk ? Theme.accentColor : Theme.errorColor
            wrapMode: Text.Wrap
            font.pixelSize: 12
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            GlassButton { text: qsTr("Close"); onClicked: root.close() }
        }
    }
}
