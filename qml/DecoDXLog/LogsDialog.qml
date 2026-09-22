// DecoDXLog — i log della stazione.
//
// Uno per tutti i giorni, e uno per ogni contest: i duplicati, il punteggio e
// il Cabrillo si contano su un log solo, e a gara finita i QSO non si mescolano
// con quelli di sempre. Aprire un altro log riavvia il programma su quel file.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Decodium.UI

DialogFrame {
    id: root

    readonly property var library: decolog.logs
    property string message: ""
    property bool messageOk: false

    function openDialog() {
        message = ""
        messageOk = false
        nameField.text = ""
        open()
    }

    title: qsTr("Station logs")
    info: qsTr("%n log(s)", "", (library.logs || []).length)
    dotColor: Theme.secondaryColor
    dialogKey: "logs"
    width: 700
    height: 620

    FileDialog {
        id: addDialog
        title: qsTr("Open an existing log")
        nameFilters: [qsTr("DecoDXLog logs (*.sqlite)"), qsTr("All files (*)")]
        onAccepted: { root.message = root.library.addExisting(selectedFile); root.messageOk = !root.message.length }
    }

    body: ColumnLayout {
        anchors.margins: 14
        spacing: 12

        // ── I log che ci sono ───────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 240
            color: Theme.bgDeep
            border.color: Theme.borderSoft
            radius: 6

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                model: root.library.logs
                ScrollBar.vertical: PanelScrollBar {}
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 46
                    color: modelData.current ? Theme.rowMatchBg : "transparent"
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            RowLayout {
                                spacing: 6
                                Text {
                                    text: modelData.name
                                    color: modelData.missing ? Theme.errorColor : Theme.textPrimary
                                    font.family: Theme.monoFamily
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                Pill {
                                    visible: modelData.current
                                    text: qsTr("open now")
                                    tone: Theme.accentColor
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                                text: modelData.missing ? qsTr("%1 — the file is not there any more").arg(modelData.path)
                                      : modelData.qsos >= 0 ? qsTr("%1 · %n QSO", "", modelData.qsos).arg(modelData.folder)
                                                            : modelData.folder
                                color: Theme.textSecondary
                                font.pixelSize: 11
                            }
                        }
                        GlassButton {
                            visible: !modelData.current && !modelData.missing
                            text: qsTr("Open")
                            tone: Theme.primaryColor
                            buttonHeight: 26
                            onClicked: {
                                // Il programma riparte su quel log: si avvisa
                                // prima, perche' le finestre si chiudono tutte.
                                root.message = root.library.openLog(modelData.path)
                            }
                        }
                        GlassButton {
                            visible: !modelData.current
                            text: qsTr("Forget")
                            buttonHeight: 26
                            onClicked: root.library.forget(modelData.path)
                        }
                    }
                }
            }
        }
        Text {
            Layout.fillWidth: true
            visible: (root.library.logs || []).length === 0
            text: qsTr("Only the log you are using. Make another one for a contest.")
            color: Theme.textSecondary
            font.pixelSize: 12
        }

        // ── Un log nuovo ────────────────────────────────────────────────────
        SectionTitle { text: qsTr("New log") }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Name")
                StyledTextField {
                    id: nameField
                    Layout.fillWidth: true
                    mono: false
                    placeholderText: qsTr("CQ WW SSB 2026")
                    onAccepted: makeButton.clicked()
                }
            }
            GlassButton {
                id: makeButton
                Layout.alignment: Qt.AlignBottom
                text: qsTr("Create")
                tone: Theme.accentColor
                filled: true
                onClicked: {
                    root.message = root.library.createLog(nameField.text)
                    root.messageOk = !root.message.length
                    if (root.messageOk) {
                        nameField.text = ""
                        root.message = qsTr("Log created. Press Open to work on it.")
                    }
                }
            }
            GlassButton {
                Layout.alignment: Qt.AlignBottom
                text: qsTr("Add an existing one…")
                onClicked: addDialog.open()
            }
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("The file goes in %1 and stays there: DecoDXLog never deletes a log.")
                      .arg(root.library.defaultFolder())
            color: Theme.textSecondary
            wrapMode: Text.Wrap
            font.pixelSize: 11
        }

        Text {
            Layout.fillWidth: true
            visible: root.message.length > 0
            text: root.message
            color: root.messageOk ? Theme.accentColor : Theme.warningColor
            wrapMode: Text.Wrap
            font.pixelSize: 12
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ToggleSwitch {
                checked: root.library.askAtStart
                onToggled: root.library.askAtStart = checked
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Ask which log to open when DecoDXLog starts")
                color: Theme.textPrimary
                font.pixelSize: 12
            }
            GlassButton { text: qsTr("Close"); onClicked: root.close() }
        }
    }
}
