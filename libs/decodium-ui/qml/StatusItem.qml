// decodium-ui — una coppia etichetta/valore sulla barra di stato.
import QtQuick
import Decodium.UI

Row {
    property string label: ""
    property string value: ""
    property color  valueColor: Theme.textPrimary
    property bool   boldValue: true

    spacing: 5

    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: label
        color: Theme.textPrimary
        font.pixelSize: 12
        font.family: Theme.monoFamily
    }
    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: value
        color: valueColor
        font.pixelSize: 12
        font.family: Theme.monoFamily
        font.bold: boldValue
    }
}
