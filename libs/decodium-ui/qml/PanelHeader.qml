// decodium-ui — la fascia che titola un pannello: maniglia, pallino, titolo a
// spaziatura fissa e, a destra, gli strumenti del pannello. Alta quanto la
// densita' scelta.
import QtQuick
import QtQuick.Layouts
import Decodium.UI

Rectangle {
    id: root

    property alias text: label.text
    property color dotColor: Theme.accentColor
    property bool showDot: true
    property bool showHandle: true
    // Elementi subito dopo il titolo (es. una pillola LIVE) e in fondo a destra.
    property alias leading: leadingRow.data
    property alias tools: toolsRow.data
    // I comandi del pannello (stacca, chiudi): stanno in fondo, dopo tutto il
    // resto, sempre nello stesso posto in ogni pannello.
    property alias controls: controlsRow.data
    // Nei pannelli DecoDXLog i comandi sono sempre a sinistra, accanto alla
    // testata, con gli stessi controlli circolari delle finestre staccate.
    readonly property bool trafficLightControls: true

    // Il tasto destro sulla testata apre il menu della disposizione; la
    // maniglia a sinistra si prende e si trascina per spostare il pannello.
    signal menuRequested(real screenX, real screenY)
    signal dragStarted()
    signal dragMoved(real screenX, real screenY)
    signal dragEnded(real screenX, real screenY)

    implicitHeight: Theme.panelHeight
    height: implicitHeight
    color: Theme.panelHeader
    radius: 5

    // Solo gli angoli in alto arrotondati: sotto la fascia prosegue il pannello.
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: parent.radius
        color: parent.color
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.borderSoft
    }

    // Sotto a tutto: prende solo il tasto destro, cosi' i pulsanti della
    // testata continuano a funzionare come prima.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: function (mouse) {
            const p = mapToGlobal(mouse.x, mouse.y)
            root.menuRequested(p.x, p.y)
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.trafficLightControls ? 18 + controlsRow.width : 10
        anchors.rightMargin: root.trafficLightControls ? 10 : 14 + controlsRow.width
        // Se la testata e' piu' stretta di quello che ci sta dentro, quello che
        // avanza si taglia: meglio un pulsante mozzato che due cose sovrapposte.
        clip: true
        spacing: 8

        // La maniglia: si prende di qui e si porta il pannello dove si vuole.
        Text {
            id: handle
            visible: root.showHandle
            text: "⠿"
            color: handleArea.drag.active ? Theme.primaryColor
                 : handleArea.containsMouse ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: Theme.fontSize

            MouseArea {
                id: handleArea
                anchors.fill: parent
                anchors.margins: -6
                hoverEnabled: true
                cursorShape: Qt.SizeAllCursor
                drag.target: null
                property bool moving: false
                onPressed: function (mouse) {
                    moving = true
                    root.dragStarted()
                }
                onPositionChanged: function (mouse) {
                    if (!moving)
                        return
                    const p = mapToGlobal(mouse.x, mouse.y)
                    root.dragMoved(p.x, p.y)
                }
                onReleased: function (mouse) {
                    if (!moving)
                        return
                    moving = false
                    const p = mapToGlobal(mouse.x, mouse.y)
                    root.dragEnded(p.x, p.y)
                }
                onCanceled: moving = false
            }
        }
        Rectangle {
            visible: root.showDot
            implicitWidth: 8
            implicitHeight: 8
            radius: 4
            color: root.dotColor
        }
        Text {
            id: label
            color: Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontSize
            font.bold: true
        }
        Row {
            id: leadingRow
            spacing: 6
            Layout.alignment: Qt.AlignVCenter
        }
        Item { Layout.fillWidth: true }
        Row {
            id: toolsRow
            spacing: 6
            Layout.alignment: Qt.AlignVCenter
        }
    }
    // I comandi del pannello stanno fuori dalla fila: se la testata e' piena di
    // schede e pulsanti, il resto si stringe ma stacca e chiudi restano dove
    // sono. Un comando che scappa fuori dal bordo e' un comando che non c'e'.
    Row {
        id: controlsRow
        spacing: root.trafficLightControls ? 7 : 2
        anchors.left: root.trafficLightControls ? parent.left : undefined
        anchors.leftMargin: root.trafficLightControls ? 10 : 0
        anchors.right: root.trafficLightControls ? undefined : parent.right
        anchors.rightMargin: root.trafficLightControls ? 0 : 8
        anchors.verticalCenter: parent.verticalCenter
    }

}
