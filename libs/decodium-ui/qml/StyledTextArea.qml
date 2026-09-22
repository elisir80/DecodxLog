// decodium-ui — riquadro di testo su piu' righe, con i colori del tema.
//
// Come StyledTextField, ma per quello che non sta in una riga sola: il testo di
// una email, un commento lungo. Scorre da solo quando serve, e avvisa quando il
// testo cambia davvero — `editingFinished` non ce l'ha un TextArea, e chi lo usa
// vuole salvare quando si toglie il fuoco, non a ogni lettera.
import QtQuick
import QtQuick.Controls
import Decodium.UI

ScrollView {
    id: root

    property alias text: area.text
    property alias placeholderText: area.placeholderText
    property bool mono: false
    property bool readOnly: false

    signal editingFinished()

    implicitHeight: 100
    implicitWidth: 120
    clip: true
    ScrollBar.vertical: PanelScrollBar {}

    background: Rectangle {
        radius: 4
        color: Theme.bgMedium
        border.width: 1
        border.color: area.activeFocus ? Theme.primaryColor : Theme.glassBorder
    }

    TextArea {
        id: area
        readOnly: root.readOnly
        wrapMode: TextArea.Wrap
        selectByMouse: true
        font.pixelSize: Theme.fontSize
        font.family: root.mono ? Theme.monoFamily : Theme.uiFamily
        color: root.enabled ? Theme.textPrimary : Theme.textSecondary
        placeholderTextColor: Theme.textSecondary
        selectionColor: Theme.primaryColor
        selectedTextColor: Theme.bgDeep
        leftPadding: 8
        rightPadding: 8
        topPadding: 6
        background: null

        property string lastSaved: text
        onActiveFocusChanged: {
            if (activeFocus)
                return
            if (text === lastSaved)
                return
            lastSaved = text
            root.editingFinished()
        }
    }
}
