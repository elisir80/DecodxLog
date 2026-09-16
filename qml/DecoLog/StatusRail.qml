// DecoLog — la barra di stato: totali, coda di sync, ricezione, tema.
import QtQuick
import QtQuick.Layouts
import Decodium.UI

Rectangle {
    implicitHeight: 26
    color: Theme.bgMedium

    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 1
        color: Theme.glassBorder
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 20

        StatusItem { label: "QSO"; value: decolog.qsoCount }
        StatusItem {
            label: "SYNC"
            value: qsTr("%1 queued").arg(decolog.dirtyCount)
            valueColor: Theme.textSecondary
        }
        StatusItem { label: "BACKUP"; value: "—"; valueColor: Theme.textSecondary }
        StatusItem {
            label: "UDP"
            value: decolog.listening ? decolog.udpPort : qsTr("closed")
            valueColor: decolog.listening ? Theme.textPrimary : Theme.errorColor
        }
        Item { Layout.fillWidth: true }
        Text {
            Layout.maximumWidth: 420
            text: decolog.databasePath
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSize - 2
            elide: Text.ElideMiddle
        }
        StatusItem {
            label: "THEME"
            value: Theme.currentTheme + " · " + Theme.density
            valueColor: Theme.textSecondary
        }
        Text {
            text: "v" + decolog.version
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSize - 2
        }
    }
}
