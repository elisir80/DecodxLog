// decodium-ui — la fascia che titola un pannello: maniglia, pallino, titolo a
// spaziatura fissa e, a destra, gli strumenti del pannello. Alta quanto la
// densita' scelta.
import QtQuick
import QtQuick.Layouts
import Decodium.UI

Rectangle {
    id: root

    property alias text: label.text
    property color dotColor: Theme.accentColor
    property bool showDot: true
    property bool showHandle: true
    // Elementi subito dopo il titolo (es. una pillola LIVE) e in fondo a destra.
    property alias leading: leadingRow.data
    property alias tools: toolsRow.data

    implicitHeight: Theme.panelHeight
    height: implicitHeight
    color: Theme.panelHeader
    radius: 5

    // Solo gli angoli in alto arrotondati: sotto la fascia prosegue il pannello.
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: parent.radius
        color: parent.color
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.borderSoft
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        Text {
            visible: root.showHandle
            text: "⠿"
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSize
        }
        Rectangle {
            visible: root.showDot
            implicitWidth: 8
            implicitHeight: 8
            radius: 4
            color: root.dotColor
        }
        Text {
            id: label
            color: Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontSize
            font.bold: true
        }
        Row {
            id: leadingRow
            spacing: 6
            Layout.alignment: Qt.AlignVCenter
        }
        Item { Layout.fillWidth: true }
        Row {
            id: toolsRow
            spacing: 6
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
