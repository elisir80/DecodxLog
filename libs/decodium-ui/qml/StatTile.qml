// decodium-ui — riquadro piccolo con un numero: distanza, ultima sincronizzazione...
import QtQuick
import Decodium.UI

Rectangle {
    id: root

    property string label: ""
    property string value: ""
    property color valueColor: Theme.textPrimary

    implicitHeight: column.implicitHeight + 12
    implicitWidth: column.implicitWidth + 16
    radius: 4
    color: Theme.bgMedium
    border.width: 1
    border.color: Theme.borderSoft

    Column {
        id: column
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1
        Text {
            text: root.label
            color: Theme.textSecondary
            font.pixelSize: 10
        }
        Text {
            text: root.value
            color: root.valueColor
            font.family: Theme.monoFamily
            font.pixelSize: 13
            font.bold: true
        }
    }
}
