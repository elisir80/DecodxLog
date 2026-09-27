// DecoDXLog — il riquadro degli avvisi DX: in alto a destra, sopra a tutto,
// se ne vedono fino a quattro. Un clic porta la radio sullo spot, la ✕ lo
// chiude; da soli spariscono dopo venti secondi.
import QtQuick
import QtQuick.Layouts
import Decodium.UI

Item {
    id: root

    property int lifetimeMs: 20000
    readonly property int count: toasts.count

    function show(title, text, spotKey) {
        // Lo stesso spot non si accumula: si rinfresca quello che c'e'.
        for (let i = 0; i < toasts.count; ++i) {
            if (toasts.get(i).spotKey === spotKey) {
                toasts.remove(i)
                break
            }
        }
        toasts.insert(0, { title: title, text: text, spotKey: spotKey, born: Date.now() })
        while (toasts.count > 4)
            toasts.remove(toasts.count - 1)
    }

    ListModel { id: toasts }

    Timer {
        interval: 1000
        repeat: true
        running: toasts.count > 0
        onTriggered: {
            const now = Date.now()
            for (let i = toasts.count - 1; i >= 0; --i) {
                if (now - toasts.get(i).born > root.lifetimeMs)
                    toasts.remove(i)
            }
        }
    }

    Column {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        spacing: 8

        Repeater {
            model: toasts
            Rectangle {
                id: toast
                required property int index
                required property string title
                required property string text
                required property string spotKey
                width: 360
                height: body.implicitHeight + 20
                radius: 10
                color: Theme.panelColor
                border.color: Theme.warningColor
                border.width: 2

                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: {
                        decolog.cluster.tune(toast.spotKey)
                        toasts.remove(toast.index)
                    }
                }
                RowLayout {
                    id: body
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                    spacing: 10
                    Rectangle {
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 4
                        width: 10; height: 10; radius: 5
                        color: Theme.warningColor
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: toast.title
                            color: Theme.warningColor
                            font.family: Theme.monoFamily
                            font.pixelSize: 13
                            font.bold: true
                        }
                        Text {
                            Layout.fillWidth: true
                            text: toast.text
                            wrapMode: Text.Wrap
                            color: Theme.textPrimary
                            font.family: Theme.monoFamily
                            font.pixelSize: 12
                        }
                        Text {
                            text: qsTr("Click to tune the radio")
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            visible: hover.hovered
                        }
                    }
                    Text {
                        Layout.alignment: Qt.AlignTop
                        text: "✕"
                        color: Theme.textSecondary
                        font.pixelSize: 14
                        TapHandler { onTapped: toasts.remove(toast.index) }
                    }
                }
            }
        }
    }
}
