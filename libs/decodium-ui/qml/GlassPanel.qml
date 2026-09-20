// decodium-ui — la superficie su cui poggia ogni pannello, con la sua testata.
import QtQuick
import Decodium.UI

Rectangle {
    id: root

    property string title: ""
    // Il pallino colorato accanto al titolo: dice a colpo d'occhio che pannello e'.
    property color dotColor: Theme.accentColor
    property bool showDot: true
    // Contenuto a destra della testata: contatori, pulsanti piccoli.
    property alias headerTools: header.tools
    property alias headerLeading: header.leading
    default property alias content: body.data
    property int padding: 10

    // La chiave del pannello: quando c'e', in testata compaiono i due comandi —
    // stacca in una finestra e chiudi — e chi ospita il pannello sa di chi si
    // tratta senza doverlo indovinare.
    property string panelKey: ""
    property bool detached: false
    property bool detachable: true
    property bool closable: true
    signal detachRequested()
    signal attachRequested()
    signal closeRequested()
    // Tasto destro sulla testata, e maniglia trascinata: chi ospita il
    // pannello decide cosa farne.
    signal menuRequested(real screenX, real screenY)
    signal moveStarted()
    signal moveMoved(real screenX, real screenY)
    signal moveEnded(real screenX, real screenY)

    color: Theme.panelColor
    border.color: Theme.glassBorder
    border.width: 1
    radius: 6
    clip: true

    PanelHeader {
        id: header
        visible: root.title.length > 0 || root.headerLeading.length > 0
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 }
        text: root.title
        dotColor: root.dotColor
        showDot: root.showDot
        onMenuRequested: (x, y) => root.menuRequested(x, y)
        onDragStarted: root.moveStarted()
        onDragMoved: (x, y) => root.moveMoved(x, y)
        onDragEnded: (x, y) => root.moveEnded(x, y)

        controls: [
            PanelControl {
                visible: root.panelKey.length > 0 && root.detachable
                glyph: root.detached ? "↩" : "⤢"
                hint: root.detached ? qsTr("Put it back in the main window")
                                    : qsTr("Detach it into its own window")
                onClicked: root.detached ? root.attachRequested() : root.detachRequested()
            },
            PanelControl {
                visible: root.panelKey.length > 0 && root.closable
                glyph: "✕"
                hint: qsTr("Close this panel — it comes back from Panels in the top bar")
                onClicked: root.closeRequested()
            }
        ]
    }

    Item {
        id: body
        anchors {
            left: parent.left; right: parent.right; bottom: parent.bottom
            top: header.visible ? header.bottom : parent.top
            margins: root.padding
        }
    }
}
