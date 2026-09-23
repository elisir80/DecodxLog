// decodium-ui — la testata delle finestre grandi, al posto della barra di
// Windows: il titolo, e i tre comandi di sempre (riduci, ingrandisci, chiudi).
// Si sposta e si ingrandisce con WindowChrome, che le sta sopra.
import QtQuick
import QtQuick.Layouts
import Decodium.UI

Rectangle {
    id: root

    property var window: null
    // Il titolo della finestra senza il nome del programma davanti: si sa gia'
    // di stare in DecoDXLog, e cosi' c'e' posto per quello che conta.
    property string text: root.window ? String(root.window.title).replace(/^DecoDXLog\s*[—-]\s*/, "") : ""
    property color dotColor: Theme.primaryColor
    property bool minimizable: true

    implicitHeight: Theme.panelHeight
    color: Theme.panelHeader

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.borderSoft
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 8
        spacing: 8

        Rectangle {
            implicitWidth: 8
            implicitHeight: 8
            radius: 4
            color: root.dotColor
        }
        Text {
            id: label
            text: root.text
            Layout.fillWidth: true
            elide: Text.ElideRight
            color: Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontSize
            font.bold: true
        }
        PanelControl {
            visible: root.minimizable
            glyph: "–"
            hint: qsTr("Minimise")
            onClicked: if (root.window) root.window.showMinimized()
        }
        PanelControl {
            glyph: root.window && root.window.visibility === Window.Maximized ? "❐" : "▢"
            hint: qsTr("Maximise or restore")
            onClicked: {
                if (!root.window)
                    return
                if (root.window.visibility === Window.Maximized)
                    root.window.showNormal()
                else
                    root.window.showMaximized()
            }
        }
        PanelControl {
            glyph: "✕"
            hint: qsTr("Close")
            onClicked: if (root.window) root.window.close()
        }
    }
}
