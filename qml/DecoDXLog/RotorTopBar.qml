// DecoDXLog — la testata del posto di comando DecoRotor: marchio, stazione, spie.
// Copia di `desktop/qml/DecoRotor/TopBar.qml`; il tasto della luce accende e
// spegne il quadrante notturno dentro la finestra.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: bar

    property bool nightMode: true
    signal lightToggled()

    RotorPalette { id: rt; dark: bar.nightMode }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state

    implicitHeight: 58
    color: rt.bgHeader

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: rt.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: rt.padding
        anchors.rightMargin: rt.padding
        spacing: 18

        Rectangle {
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            radius: 8
            color: Qt.rgba(0.22, 0.74, 0.97, 0.16)
            border.color: rt.primary
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "◈"
                color: rt.primary
                font.pixelSize: 18
            }
        }

        ColumnLayout {
            spacing: 0

            Text {
                text: "DecoRotor"
                color: rt.textPrimary
                font.pixelSize: rt.fontTitle
                font.bold: true
                font.letterSpacing: 0.6
            }

            Text {
                readonly property string call: bar.st.callsign || ""
                readonly property string grid: bar.st.locator || ""
                text: call.length > 0 ? qsTr("%1 · %2").arg(call).arg(grid) : grid
                color: rt.textDim
                font.pixelSize: rt.fontSmall
            }
        }

        Item { Layout.fillWidth: true }

        RotorLed {
            label: bar.st.connected ? qsTr("CONTROL BOX %1").arg(bar.st.port || "")
                                    : qsTr("CONTROL BOX ASSENTE")
            colour: bar.st.connected ? rt.accent : rt.danger
            blinking: !bar.st.connected
        }

        RotorLed {
            label: bar.st.moving ? qsTr("IN ROTAZIONE") : qsTr("FERMO")
            colour: bar.st.moving ? rt.warning : rt.textDim
            blinking: bar.st.moving === true
        }

        RotorLed {
            label: qsTr("%n client", "", bar.st.clients || 0)
            colour: (bar.st.clients || 0) > 0 ? rt.primary : rt.textDim
        }

        Text {
            text: bar.st.modelLabel || ""
            color: rt.textSecondary
            font.pixelSize: rt.fontSmall
        }

        Rectangle {
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            radius: 8
            color: light.hovered ? rt.bgElevated : "transparent"
            border.color: rt.borderSoft
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: bar.nightMode ? "☾" : "☀"
                color: rt.textSecondary
                font.pixelSize: 16
            }

            HoverHandler {
                id: light
                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                onTapped: bar.lightToggled()
            }

            ToolTip.visible: light.hovered
            ToolTip.text: bar.nightMode ? qsTr("Passa al quadrante chiaro")
                                        : qsTr("Passa al quadrante notturno")
        }
    }
}
