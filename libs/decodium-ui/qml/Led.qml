// decodium-ui — spia rotonda, con alone quando `glow` (collegato, in arrivo).
import QtQuick
import Decodium.UI

Item {
    id: root

    property color color: Theme.accentColor
    property int size: 8
    property bool glow: false
    property bool blinking: false

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.centerIn: parent
        visible: root.glow
        width: root.size * 2.2
        height: width
        radius: width / 2
        color: Qt.rgba(root.color.r, root.color.g, root.color.b, 0.22)
    }
    Rectangle {
        id: dot
        anchors.centerIn: parent
        width: root.size
        height: root.size
        radius: root.size / 2
        color: root.color
        SequentialAnimation on opacity {
            running: root.blinking
            loops: Animation.Infinite
            onStopped: dot.opacity = 1.0
            NumberAnimation { to: 0.35; duration: 450 }
            NumberAnimation { to: 1.0; duration: 450 }
        }
    }
}
