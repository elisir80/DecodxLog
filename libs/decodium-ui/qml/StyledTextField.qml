// decodium-ui — campo di testo. `mono` per nominativi, frequenze, RST e orari.
import QtQuick
import QtQuick.Controls
import Decodium.UI

TextField {
    id: root

    property bool mono: false
    property bool uppercase: false

    implicitHeight: Math.max(26, Theme.rowHeight + 4)
    font.pixelSize: Theme.fontSize
    font.family: mono ? Theme.monoFamily : Qt.application.font.family
    font.capitalization: uppercase ? Font.AllUppercase : Font.MixedCase
    color: Theme.textPrimary
    placeholderTextColor: Theme.textSecondary
    selectionColor: Theme.primaryColor
    selectedTextColor: Theme.bgDeep
    leftPadding: 8
    rightPadding: 8

    background: Rectangle {
        radius: 6
        color: Theme.bgMedium
        border.width: 1
        border.color: root.activeFocus ? Theme.primaryColor : Theme.glassBorder
    }
}
