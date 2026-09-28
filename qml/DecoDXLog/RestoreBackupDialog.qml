// DecoDXLog — rimettere a posto un backup.
//
// Si sceglie una copia, si guarda cosa c'e' dentro prima di toccare niente
// (quanti QSO, fino a quando, se SQLite la trova sana) e la si confronta con il
// log di adesso. Il ripristino riavvia il programma: la copia si rimette a log
// chiuso, e il log com'era prima resta nella cartella dei backup.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Decodium.UI

DialogFrame {
    id: root

    property var files: []
    property var current: ({})
    property string selected: ""
    property var details: null
    property bool inspecting: false
    property bool armed: false
    property string message: ""

    function openDialog() {
        files = decolog.backupFiles()
        current = decolog.currentLogInfo()
        message = ""
        armed = false
        choose(files.length ? files[0].path : "")
        open()
    }

    function choose(path) {
        selected = path
        details = null
        armed = false
        message = ""
        if (path.length) {
            inspecting = true
            decolog.inspectBackup(path)
        }
    }

    title: qsTr("Restore a backup")
    info: qsTr("%n cop(ies) in the backup folder", "", files.length)
    dotColor: Theme.warningColor
    dialogKey: "restore"
    width: 720
    height: 640

    Connections {
        target: decolog
        function onBackupInspected(info) {
            if (info.path !== root.selected && decolog.localPath(root.selected) !== info.path)
                return
            root.details = info
            root.inspecting = false
        }
    }

    // Il pulsante si arma e si disarma da solo: un clic per sbaglio non basta.
    Timer { id: disarm; interval: 6000; onTriggered: root.armed = false }

    FileDialog {
        id: otherFile
        title: qsTr("Choose a backup")
        nameFilters: [qsTr("DecoDXLog logs (*.sqlite)"), qsTr("All files (*)")]
        onAccepted: root.choose(decolog.localPath(selectedFile))
    }

    body: ColumnLayout {
        anchors.margins: 14
        spacing: 10

        // Il log di adesso, per il confronto.
        Text {
            Layout.fillWidth: true
            elide: Text.ElideMiddle
            text: qsTr("Log now: %1 · %n QSO · last %2", "", root.current.qsos || 0)
                  .arg(root.current.name || "").arg(root.current.last || "—")
            color: Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: 12
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 220
            color: Theme.bgDeep
            border.color: Theme.borderSoft
            radius: 6

            ListView {
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                model: root.files
                ScrollBar.vertical: PanelScrollBar {}
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 34
                    radius: 4
                    color: modelData.path === root.selected ? Theme.rowMatchBg
                         : hover.hovered ? Theme.bgLight : "transparent"
                    HoverHandler { id: hover }
                    TapHandler { onTapped: root.choose(modelData.path) }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 10
                        Text {
                            text: modelData.when
                            color: Theme.textPrimary
                            font.family: Theme.monoFamily
                            font.pixelSize: 12
                        }
                        Pill {
                            visible: modelData.safety
                            text: qsTr("before a restore")
                            tone: Theme.warningColor
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            elide: Text.ElideMiddle
                            color: Theme.textSecondary
                            font.pixelSize: 11
                        }
                        Text {
                            text: modelData.size
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 11
                        }
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: root.files.length === 0
                text: qsTr("No copies in %1 yet.").arg(decolog.backupDir)
                color: Theme.textSecondary
                font.pixelSize: 12
            }
        }

        GlassButton {
            text: qsTr("Choose another file…")
            onClicked: otherFile.open()
        }

        // Cosa c'e' nella copia scelta.
        SectionTitle { text: qsTr("In this copy") }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: !root.details ? Theme.textSecondary : root.details.ok ? Theme.textPrimary : Theme.errorColor
            text: !root.selected.length ? qsTr("Choose a copy.")
                  : root.inspecting ? qsTr("Reading %1…").arg(root.selected)
                  : !root.details ? ""
                  : !root.details.ok ? qsTr("✗ It cannot be restored: %1").arg(root.details.problem)
                  : qsTr("✓ SQLite finds it sound · %n QSO, from %1 to %2 · %3", "", root.details.qsos)
                        .arg(root.details.first || "—").arg(root.details.last || "—").arg(root.details.size)
        }
        Text {
            Layout.fillWidth: true
            visible: !!root.details && root.details.ok
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: root.details && root.details.diff < 0 ? Theme.warningColor : Theme.textSecondary
            text: !root.details ? ""
                  : root.details.diff < 0
                    ? qsTr("%n QSO fewer than the log now: the ones logged after this copy will not be in the restored log.",
                           "", -root.details.diff)
                  : root.details.diff > 0 ? qsTr("%n QSO more than the log now.", "", root.details.diff)
                  : qsTr("As many QSOs as the log now.")
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textSecondary
            font.pixelSize: 12
            text: qsTr("DecoDXLog restarts and puts the copy in place of the log before opening it. The log as it "
                       + "is now is saved first in the backup folder as decodxlog-before-restore-…, so nothing is "
                       + "lost: to go back, restore that one.")
        }

        Item { Layout.fillHeight: true }

        Text {
            Layout.fillWidth: true
            visible: root.message.length > 0
            text: root.message
            color: Theme.warningColor
            wrapMode: Text.Wrap
            font.pixelSize: 12
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            GlassButton {
                text: root.armed ? qsTr("Confirm: restore and restart") : qsTr("Restore and restart")
                tone: root.armed ? Theme.errorColor : Theme.warningColor
                filled: root.armed
                enabled: !!root.details && root.details.ok && !root.inspecting
                onClicked: {
                    if (!root.armed) {
                        root.armed = true
                        disarm.restart()
                        return
                    }
                    root.armed = false
                    root.message = decolog.restoreBackup(root.selected)
                }
            }
            GlassButton { text: qsTr("Close"); onClicked: root.close() }
        }
    }
}
