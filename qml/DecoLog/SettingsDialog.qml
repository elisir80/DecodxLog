// DecoLog — impostazioni: aspetto e ricezione UDP.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Dialog {
    id: root

    modal: true
    anchors.centerIn: Overlay.overlay
    width: 480
    title: qsTr("Settings")
    padding: 16

    background: Rectangle {
        color: Theme.panelColor
        border.color: Theme.glassBorder
        radius: 10
    }
    header: PanelHeader { text: root.title }

    onOpened: {
        portField.text = decolog.udpPort
        groupField.text = decolog.multicastGroup
    }

    onAccepted: {
        decolog.multicastGroup = groupField.text.trim()
        decolog.udpPort = parseInt(portField.text)
    }

    component Key: Text {
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSize - 1
    }

    contentItem: GridLayout {
        columns: 2
        columnSpacing: 12
        rowSpacing: 8

        Key { text: qsTr("Theme") }
        StyledComboBox {
            Layout.fillWidth: true
            model: Theme.availableThemes
            currentIndex: Theme.availableThemes.indexOf(Theme.currentTheme)
            onActivated: Theme.currentTheme = currentText
        }
        Key { text: qsTr("Accent (Darkcodium)") }
        StyledComboBox {
            Layout.fillWidth: true
            enabled: Theme.currentTheme === "Darkcodium"
            model: Theme.availableVariants
            currentIndex: Theme.availableVariants.indexOf(Theme.accentVariant)
            onActivated: Theme.accentVariant = currentText
        }
        Key { text: qsTr("Density") }
        StyledComboBox {
            Layout.fillWidth: true
            model: Theme.availableDensities
            currentIndex: Theme.availableDensities.indexOf(Theme.density)
            onActivated: Theme.density = currentText
        }

        Rectangle { Layout.columnSpan: 2; Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSoft }

        Key { text: qsTr("UDP port") }
        StyledTextField {
            id: portField
            Layout.fillWidth: true
            mono: true
            validator: IntValidator { bottom: 0; top: 65535 }
        }
        Key { text: qsTr("Multicast group") }
        StyledTextField {
            id: groupField
            Layout.fillWidth: true
            mono: true
            placeholderText: qsTr("empty = unicast")
        }
        Text {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: decolog.udpError.length ? Theme.errorColor : Theme.textSecondary
            font.pixelSize: Theme.fontSize - 2
            text: decolog.udpError.length ? decolog.udpError
                : qsTr("In Decodium set the UDP server to this address and port. A multicast group (e.g. 239.255.0.1) shares the stream with GridTracker or JTAlert.")
        }
    }

    footer: DialogButtonBox {
        alignment: Qt.AlignRight
        padding: 12
        background: Item {}
        GlassButton {
            text: qsTr("Apply")
            tone: Theme.accentColor
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
        GlassButton {
            text: qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
