// DecoLog — il campo di testo di DecoRotor.
// Copia di `desktop/qml/DecoRotor/FieldInput.qml`.
import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: field

    RotorPalette { id: rt }

    implicitHeight: 38
    color: rt.textPrimary
    font.pixelSize: rt.fontBody
    font.family: rt.monoFamily
    placeholderTextColor: rt.textDim
    selectionColor: Qt.rgba(0.22, 0.74, 0.97, 0.35)
    selectedTextColor: rt.textPrimary
    leftPadding: 10
    rightPadding: 10

    background: Rectangle {
        radius: 8
        color: rt.bgDeep
        border.color: field.activeFocus ? rt.primary : rt.borderSoft
        border.width: 1
    }
}
