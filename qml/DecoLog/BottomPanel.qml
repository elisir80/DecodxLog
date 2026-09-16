// DecoLog — il pannello inferiore a schede. Per ora vive il registro attivita';
// award, statistiche e upload QSL arrivano con le release 1.x.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Decodium.UI

GlassPanel {
    id: root

    padding: 0

    FileDialog {
        id: importDialog
        title: qsTr("Import ADIF")
        nameFilters: [qsTr("ADIF files (*.adi *.adif)"), qsTr("All files (*)")]
        onAccepted: decolog.importAdif(selectedFile)
    }
    FileDialog {
        id: exportDialog
        title: qsTr("Export ADIF")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "adi"
        nameFilters: [qsTr("ADIF files (*.adi)")]
        onAccepted: decolog.exportAdif(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.panelHeight
            color: Theme.panelHeader
            radius: 10
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 10
                color: parent.color
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 2

                Repeater {
                    model: [
                        { label: qsTr("Activity"), ready: true },
                        { label: qsTr("Awards"), ready: false },
                        { label: qsTr("Statistics"), ready: false },
                        { label: qsTr("QSL upload"), ready: false }
                    ]
                    delegate: Item {
                        id: tab
                        required property var modelData
                        required property int index
                        implicitWidth: tabText.implicitWidth + 22
                        Layout.fillHeight: true
                        opacity: modelData.ready ? 1.0 : 0.45

                        Text {
                            id: tabText
                            anchors.centerIn: parent
                            text: tab.modelData.label
                            color: tab.index === 0 ? Theme.textPrimary : Theme.textSecondary
                            font.bold: tab.index === 0
                            font.pixelSize: Theme.fontSize - 1
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: 1.0
                        }
                        Rectangle {
                            visible: tab.index === 0
                            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 6; rightMargin: 6; bottomMargin: 3 }
                            height: 2
                            color: Theme.primaryColor
                        }
                        HoverHandler { id: tabHover }
                        ToolTip.visible: !modelData.ready && tabHover.hovered
                        ToolTip.text: qsTr("Coming in 1.x")
                    }
                }

                Item { Layout.fillWidth: true }

                GlassButton { text: qsTr("Import ADIF…"); implicitHeight: Theme.panelHeight - 6; onClicked: importDialog.open() }
                GlassButton { text: qsTr("Export ADIF…"); implicitHeight: Theme.panelHeight - 6; onClicked: exportDialog.open() }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8
            clip: true
            model: decolog.activity
            ScrollBar.vertical: ScrollBar {}
            delegate: Row {
                id: line
                required property var modelData
                spacing: 10
                height: Theme.rowHeight - 4
                Text {
                    text: line.modelData.time
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: Theme.fontSize - 1
                }
                Text {
                    text: line.modelData.text
                    font.pixelSize: Theme.fontSize - 1
                    color: line.modelData.level === "error" ? Theme.errorColor
                         : line.modelData.level === "warning" ? Theme.warningColor
                         : line.modelData.level === "success" ? Theme.accentColor
                         : Theme.textPrimary
                }
            }
        }
    }
}
