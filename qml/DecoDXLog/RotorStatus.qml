// DecoDXLog — la striscia di stato in fondo al posto di comando: l'errore, se
// c'e', e dove rispondono i tre servizi del gateway.
// Copia di `desktop/qml/DecoRotor/StatusStrip.qml`.
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: strip

    property bool nightMode: true

    RotorPalette { id: rt; dark: strip.nightMode }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state

    implicitHeight: 30
    color: rt.bgHeader

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: rt.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: rt.padding
        anchors.rightMargin: rt.padding
        spacing: 20

        Text {
            readonly property string problem: strip.st.error || ""
            text: problem.length > 0 ? problem : qsTr("ready")
            color: problem.length > 0 ? rt.danger : rt.textDim
            font.pixelSize: rt.fontSmall
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Repeater {
            model: [
                { caption: qsTr("app"), value: strip.rotor.wsEndpoint },
                { caption: qsTr("web"), value: strip.rotor.httpEndpoint },
                { caption: qsTr("rotctld"), value: strip.rotor.backend === "rotctld"
                                                   ? strip.rotor.wsEndpoint : ":4532" }
            ]

            Text {
                required property var modelData

                text: qsTr("%1 %2").arg(modelData.caption).arg(modelData.value)
                color: rt.textDim
                font.pixelSize: rt.fontSmall
                font.family: rt.monoFamily
            }
        }
    }
}
