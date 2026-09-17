// decodium-ui — scelta esclusiva: cerchio con il punto dentro quando e' scelta.
import QtQuick
import QtQuick.Controls
import Decodium.UI

RadioButton {
    id: root

    implicitHeight: 22
    spacing: 6

    indicator: Rectangle {
        x: 0
        anchors.verticalCenter: parent.verticalCenter
        width: 14
        height: 14
        radius: 7
        color: "transparent"
        border.width: 1
        border.color: root.checked ? Theme.accentColor : Theme.glassBorder
        Rectangle {
            anchors.centerIn: parent
            width: 8
            height: 8
            radius: 4
            visible: root.checked
            color: Theme.accentColor
        }
    }

    contentItem: Text {
        leftPadding: root.indicator.width + root.spacing
        text: root.text
        color: root.checked ? Theme.textPrimary : Theme.textSecondary
        font.pixelSize: 12
        verticalAlignment: Text.AlignVCenter
    }
}
