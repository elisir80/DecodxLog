// DecoDXLog — un colore da scegliere: il quadratino, e con un clic la
// tavolozza di Decodium 4 piu' il codice da scrivere a mano (#RRGGBB o, per
// un fondo trasparente, #AARRGGBB).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Rectangle {
    id: root

    property string value: "#FFFFFF"
    // Le prime due cifre di #AARRGGBB, per i fondi: quanto si vede.
    property bool withAlpha: false
    signal picked(string value)

    readonly property var presets: [
        "#ff0000", "#ff6600", "#ffcc00", "#33cc33", "#00ccff", "#0066ff",
        "#9933ff", "#ff33cc", "#ffffff", "#cccccc", "#666666", "#000000"
    ]

    implicitWidth: 34
    implicitHeight: 22
    radius: 4
    color: value
    border.color: hover.hovered ? Theme.primaryColor : Theme.glassBorder
    border.width: 1

    // Il fondo a scacchi dice che un colore e' trasparente.
    Canvas {
        anchors.fill: parent
        anchors.margins: 1
        z: -1
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            for (let y = 0; y < height; y += 5)
                for (let x = 0; x < width; x += 5) {
                    ctx.fillStyle = ((x + y) / 5) % 2 === 0 ? "#555555" : "#999999"
                    ctx.fillRect(x, y, 5, 5)
                }
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: picker.open() }

    Popup {
        id: picker
        y: root.height + 4
        padding: 10
        popupType: Popup.Window
        onOpened: hexField.text = root.value
        background: Rectangle { color: Theme.panelColor; border.color: Theme.glassBorder; radius: 6 }
        contentItem: ColumnLayout {
            spacing: 8
            GridLayout {
                columns: 6
                columnSpacing: 4
                rowSpacing: 4
                Repeater {
                    model: root.presets
                    Rectangle {
                        required property string modelData
                        width: 24
                        height: 24
                        radius: 4
                        color: modelData
                        border.color: Theme.glassBorder
                        TapHandler {
                            onTapped: {
                                // Un fondo scelto dalla tavolozza resta leggero.
                                const v = root.withAlpha ? "#55" + modelData.slice(1) : modelData
                                root.picked(v.toUpperCase())
                                picker.close()
                            }
                        }
                    }
                }
            }
            RowLayout {
                spacing: 6
                StyledTextField {
                    id: hexField
                    Layout.preferredWidth: 110
                    placeholderText: root.withAlpha ? "#AARRGGBB" : "#RRGGBB"
                    onAccepted: apply.clicked()
                }
                GlassButton {
                    id: apply
                    text: qsTr("OK")
                    buttonHeight: 24
                    fontPixelSize: 11
                    onClicked: {
                        const v = hexField.text.trim()
                        if (/^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(v)) {
                            root.picked(v.toUpperCase())
                            picker.close()
                        }
                    }
                }
            }
        }
    }
}
