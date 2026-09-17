// decodium-ui — voce di menu: testo a spaziatura fissa, spunta a sinistra per le
// voci selezionabili, freccia per i sottomenu.
import QtQuick
import QtQuick.Controls
import Decodium.UI

MenuItem {
    id: root

    implicitHeight: Theme.rowHeight + 4
    implicitWidth: Math.max(170, label.implicitWidth + 56)
    font.family: Theme.monoFamily
    font.pixelSize: 12
    opacity: enabled ? 1.0 : 0.45

    indicator: Text {
        x: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: root.checkable
        text: root.checked ? "✓" : ""
        color: Theme.accentColor
        font.pixelSize: 12
        font.bold: true
    }

    arrow: Text {
        x: root.width - width - 8
        anchors.verticalCenter: parent.verticalCenter
        visible: root.subMenu
        text: "▸"
        color: Theme.textSecondary
        font.pixelSize: 11
    }

    contentItem: Text {
        id: label
        leftPadding: 22
        rightPadding: 18
        text: root.text
        font: root.font
        color: root.highlighted ? Theme.primaryColor : Theme.textPrimary
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 4
        color: root.highlighted ? Theme.glassOverlay : "transparent"
        border.width: root.highlighted ? 1 : 0
        border.color: Theme.primaryColor
    }
}
