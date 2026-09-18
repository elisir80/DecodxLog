// decodium-ui — campo di testo. `mono` per nominativi, frequenze, RST e orari.
import QtQuick
import QtQuick.Controls
import Decodium.UI

TextField {
    id: root

    property bool mono: true
    property bool uppercase: false
    property int fieldHeight: 30
    property color accentBorder: Theme.primaryColor

    implicitHeight: fieldHeight
    // Larghezza naturale piccola: nei layout decide lo spazio disponibile, non il
    // testo segnaposto, cosi' le colonne di una scheda restano uguali.
    implicitWidth: 60
    font.pixelSize: Theme.fontSize
    font.family: mono ? Theme.monoFamily : Theme.uiFamily
    font.capitalization: uppercase ? Font.AllUppercase : Font.MixedCase
    color: enabled ? Theme.textPrimary : Theme.textSecondary
    placeholderTextColor: Theme.textSecondary
    selectionColor: Theme.primaryColor
    selectedTextColor: Theme.bgDeep
    leftPadding: 8
    rightPadding: 8
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true

    background: Rectangle {
        radius: 4
        color: Theme.bgMedium
        border.width: 1
        border.color: root.activeFocus ? root.accentBorder : Theme.glassBorder
    }
}
