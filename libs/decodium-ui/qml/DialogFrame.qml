// decodium-ui — cornice delle finestre di dialogo: la stessa testata dei pannelli,
// con un'informazione a destra e la ✕ per chiudere.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Dialog {
    id: root

    property color dotColor: Theme.primaryColor
    property string info: ""

    modal: true
    anchors.centerIn: Overlay.overlay
    padding: 0
    topPadding: 0
    closePolicy: Popup.CloseOnEscape

    background: Rectangle {
        color: Theme.panelColor
        border.color: Theme.glassBorder
        border.width: 1
        radius: 6
    }

    header: PanelHeader {
        text: root.title
        dotColor: root.dotColor
        showHandle: false
        tools: [
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.info.length > 0
                text: root.info
                color: Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: 11
            },
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "✕"
                color: closeArea.containsMouse ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: Theme.fontSize + 1
                leftPadding: 8
                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    anchors.margins: -6
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.reject()
                }
            }
        ]
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, Theme.isLightTheme ? 0.25 : 0.55)
    }
}
