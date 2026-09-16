// DecoLog — il log: una riga per QSO, la piu' recente in alto.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    property int selectedRow: -1
    readonly property var model: decolog.qsoModel

    title: qsTr("Log")
    padding: 0

    headerTools: [
        Text {
            text: qsTr("%1 QSO").arg(root.model.count)
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontSize - 1
        }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StyledTextField {
            Layout.fillWidth: true
            Layout.maximumWidth: 340
            Layout.margins: 8
            mono: true
            placeholderText: qsTr("Filter: call, grid, band, mode")
            onTextChanged: {
                root.selectedRow = -1
                root.model.filterText = text
            }
        }

        HorizontalHeaderView {
            Layout.fillWidth: true
            syncView: table
            clip: true
            delegate: Rectangle {
                required property var display
                implicitHeight: Theme.rowHeight
                color: Theme.panelHeader
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    verticalAlignment: Text.AlignVCenter
                    text: parent.display
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSize - 1
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 0.8
                    elide: Text.ElideRight
                }
            }
        }

        TableView {
            id: table
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.model
            boundsBehavior: Flickable.StopAtBounds
            columnWidthProvider: function (column) { return root.model.columnWidthHint(column) }
            rowHeightProvider: function (row) { return Theme.rowHeight }
            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}

            Connections {
                target: Theme
                function onDensityChanged() { table.forceLayout() }
            }

            delegate: Rectangle {
                id: cell
                required property var display
                required property string columnKey
                required property bool isNew
                required property int row

                readonly property bool selected: row === root.selectedRow
                // Nominativi, frequenze, RST e orari a spaziatura fissa.
                readonly property bool mono: ["utc", "call", "freq", "rst_sent", "rst_rcvd", "grid", "band", "mode"].indexOf(columnKey) >= 0

                implicitHeight: Theme.rowHeight
                color: selected ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.28)
                     : isNew ? Theme.rowMatchBg
                     : row % 2 ? Qt.rgba(Theme.bgLight.r, Theme.bgLight.g, Theme.bgLight.b, 0.35)
                     : "transparent"

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 4
                    verticalAlignment: Text.AlignVCenter
                    text: cell.display
                    elide: Text.ElideRight
                    font.pixelSize: Theme.fontSize
                    font.family: cell.mono ? Theme.monoFamily : Qt.application.font.family
                    font.bold: cell.columnKey === "call"
                    color: cell.columnKey === "mode" ? Theme.secondaryColor
                         : cell.columnKey === "source" ? Theme.textSecondary
                         : Theme.textPrimary
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.selectedRow = cell.row
                        decolog.lookupCall = root.model.callAt(cell.row)
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: root.model.count === 0
                text: root.model.filterText.length ? qsTr("No QSO matches the filter.")
                                                   : qsTr("The log is empty. Log a QSO in Decodium, or import an ADIF file.")
                color: Theme.textSecondary
            }
        }
    }
}
