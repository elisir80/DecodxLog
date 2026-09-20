// DecoDXLog — il tasto di DecoRotor: normale, primario o di pericolo.
// Copia di `desktop/qml/DecoRotor/CommandButton.qml`.
import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    // 0 normale, 1 primario, 2 pericolo: gli stessi tre di DecoRotor.
    property int kind: 0

    RotorPalette { id: rt }

    readonly property color tint: kind === 1 ? rt.primary
                                : kind === 2 ? rt.danger
                                : rt.textSecondary

    implicitHeight: 38
    font.pixelSize: rt.fontBody
    font.bold: kind !== 0

    background: Rectangle {
        radius: 8
        color: control.kind === 0
               ? (control.down ? rt.bgHeader : rt.bgElevated)
               : Qt.rgba(control.tint.r, control.tint.g, control.tint.b, control.down ? 0.42 : 0.20)
        border.color: control.hovered || control.down ? control.tint : rt.borderSoft
        border.width: 1
        opacity: control.enabled ? 1 : 0.4
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.kind === 0 ? rt.textPrimary : control.tint
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: control.enabled ? 1 : 0.5
    }
}
