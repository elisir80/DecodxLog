// DecoLog — la finestra principale.
//
// La stessa grammatica di Decodium: pannelli su SplitView, ridimensionabili, con
// le misure che restano da una sessione all'altra.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Decodium.UI

ApplicationWindow {
    id: window

    width: 1480
    height: 900
    minimumWidth: 980
    minimumHeight: 600
    visible: true
    title: "DecoLog " + decolog.version
    color: Theme.bgDeep

    font.pixelSize: Theme.fontSize

    Settings {
        id: layout
        category: "layout"
        property alias windowWidth: window.width
        property alias windowHeight: window.height
        property real leftWidth: 300
        property real rightWidth: 320
        property real bottomHeight: 210
    }

    SettingsDialog { id: settingsDialog }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            Layout.fillWidth: true
            onSettingsRequested: settingsDialog.open()
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8
            orientation: Qt.Horizontal
            handle: splitHandle

            SplitView {
                SplitView.fillWidth: true
                orientation: Qt.Vertical
                handle: splitHandle

                SplitView {
                    SplitView.fillHeight: true
                    orientation: Qt.Horizontal
                    handle: splitHandle

                    IncomingPanel {
                        SplitView.preferredWidth: layout.leftWidth
                        SplitView.minimumWidth: 240
                        onWidthChanged: if (width > 0) layout.leftWidth = width
                    }

                    LogTablePanel {
                        SplitView.fillWidth: true
                        SplitView.minimumWidth: 400
                    }
                }

                BottomPanel {
                    SplitView.preferredHeight: layout.bottomHeight
                    SplitView.minimumHeight: 120
                    onHeightChanged: if (height > 0) layout.bottomHeight = height
                }
            }

            CallInfoPanel {
                SplitView.preferredWidth: layout.rightWidth
                SplitView.minimumWidth: 240
                onWidthChanged: if (width > 0) layout.rightWidth = width
            }
        }

        StatusRail { Layout.fillWidth: true }
    }

    Component {
        id: splitHandle
        Rectangle {
            id: handleRoot
            implicitWidth: 8
            implicitHeight: 8
            color: "transparent"
            Rectangle {
                anchors.centerIn: parent
                width: handleRoot.width > handleRoot.height ? 36 : 2
                height: handleRoot.width > handleRoot.height ? 2 : 36
                radius: 1
                color: handleRoot.SplitHandle.pressed ? Theme.primaryColor
                     : handleRoot.SplitHandle.hovered ? Theme.textSecondary : Theme.borderSoft
            }
        }
    }
}
