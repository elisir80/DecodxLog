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
        eventField.text = ""
        kindBox.currentIndex = 0
        open()
    }

    // Per cosa e' un log: quello di tutti i giorni non porta etichetta.
    readonly property var kinds: ["general", "contest", "award", "other"]
    function kindLabel(kind) {
        return kind === "contest" ? qsTr("Contest")
             : kind === "award" ? qsTr("Award")
             : kind === "other" ? qsTr("Other") : qsTr("Everyday")
    }

    title: qsTr("Station logs")
    info: qsTr("%n log(s)", "", (library.logs || []).length)
    dotColor: Theme.secondaryColor
    dialogKey: "logs"
    width: 700
    height: 620

    // Cancellare un log non si fa con un clic: si scrive il suo nome. Il file va
    // nel Cestino, quindi si recupera.
    Popup {
        id: deleteDialog
        property var target: ({})
        function ask(log) { target = log; confirmField.text = ""; open(); confirmField.forceActiveFocus() }
        anchors.centerIn: Overlay.overlay
        modal: true
        padding: 16
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { color: Theme.panelColor; border.color: Theme.errorColor; radius: 6 }
        contentItem: ColumnLayout {
            spacing: 10
            Text {
                text: qsTr("Delete the log %1").arg(deleteDialog.target.name || "")
                color: Theme.errorColor
                font.pixelSize: 14
                font.bold: true
            }
            Text {
                Layout.maximumWidth: 420
                wrapMode: Text.Wrap
                color: Theme.textPrimary
                text: qsTr("The file goes to the Recycle Bin with all its QSO, and the log leaves the list. "
                           + "The QSO in the Cloud are not touched.")
            }
            LabeledField {
                label: qsTr("Write the name of the log to confirm")
                StyledTextField {
                    id: confirmField
                    Layout.preferredWidth: 300
                    mono: false
                    Keys.onReturnPressed: if (deleteGo.enabled) deleteGo.clicked()
                }
            }
            RowLayout {
                spacing: 8
                Item { Layout.fillWidth: true }
                GlassButton { text: qsTr("Cancel"); onClicked: deleteDialog.close() }
                GlassButton {
                    id: deleteGo
                    text: qsTr("Delete")
                    tone: Theme.errorColor
                    filled: true
                    enabled: (deleteDialog.target.name || "").length > 0
                             && confirmField.text.trim() === deleteDialog.target.name
                    onClicked: {
                        deleteDialog.close()
                        root.message = root.library.deleteLog(deleteDialog.target.path)
                        root.messageOk = !root.message.length
                        if (root.messageOk)
                            root.message = qsTr("Log deleted: it is in the Recycle Bin.")
                    }
                }
            }
        }
    }

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
                                Pill {
                                    visible: (modelData.kind || "general") !== "general"
                                    text: (modelData.event || "").length
                                          ? root.kindLabel(modelData.kind) + " · " + modelData.event
                                          : root.kindLabel(modelData.kind)
                                    tone: modelData.kind === "contest" ? Theme.warningColor : Theme.secondaryColor
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
                        GlassButton {
                            visible: !modelData.current
                            text: qsTr("Delete")
                            tone: Theme.errorColor
                            buttonHeight: 26
                            onClicked: deleteDialog.ask(modelData)
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
                Layout.preferredWidth: 150
                label: qsTr("For")
                StyledComboBox {
                    id: kindBox
                    Layout.fillWidth: true
                    mono: false
                    model: root.kinds.map(k => root.kindLabel(k))
                }
            }
            LabeledField {
                Layout.fillWidth: true
                visible: kindBox.currentIndex === 1 || kindBox.currentIndex === 2
                label: kindBox.currentIndex === 2 ? qsTr("Award") : qsTr("Contest")
                StyledTextField {
                    id: eventField
                    Layout.fillWidth: true
                    mono: false
                    placeholderText: kindBox.currentIndex === 2 ? "VUCC" : "CQ WW SSB"
                    onAccepted: makeButton.clicked()
                }
            }
        }
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
                    placeholderText: eventField.text.trim().length
                                     ? eventField.text.trim() + " " + new Date().getFullYear()
                                     : qsTr("CQ WW SSB 2026")
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
                    root.message = root.library.createLog(nameField.text, "",
                                                          root.kinds[kindBox.currentIndex], eventField.text)
                    root.messageOk = !root.message.length
                    if (root.messageOk) {
                        nameField.text = ""
                        eventField.text = ""
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
            text: qsTr("The file goes in %1. Delete moves a log you are not using to the Recycle Bin; Forget only takes it off the list.")
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
