// decodium-ui — la cornice di una finestra senza la barra di Windows.
//
// La barra del titolo di sistema sopra una finestra che ha gia' la sua testata
// e' spazio buttato: il titolo c'e' due volte. Le finestre di DecoDXLog la
// tolgono, e questa cornice rimette quello che la barra faceva: si prende la
// testata e si sposta la finestra (anche verso il bordo dello schermo, dove
// Windows la aggancia), doppio clic sulla testata la ingrandisce, e dai bordi
// si ridimensiona. Un filo di bordo dice quale finestra e' attiva.
import QtQuick
import Decodium.UI

Item {
    id: root

    // La finestra da muovere. `dragHeight` e' quanto e' alta la testata, dal
    // bordo in alto: li' si prende per spostare.
    property var window: null
    property real dragHeight: Theme.panelHeight + 4
    property int grip: 5
    property bool resizable: true

    anchors.fill: parent
    z: 10000

    function toggleMaximized() {
        if (!root.window)
            return
        if (root.window.visibility === Window.Maximized)
            root.window.showNormal()
        else
            root.window.showMaximized()
    }

    // Il bordo: uno solo, sottile, piu' acceso sulla finestra attiva.
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: root.window && root.window.active ? Theme.primaryColor : Theme.glassBorder
    }

    // La testata: i pulsanti che ci stanno dentro funzionano come prima, perche'
    // queste maniglie non si prendono il clic, solo il trascinamento.
    Item {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: root.dragHeight
        DragHandler {
            target: null
            onActiveChanged: if (active && root.window) root.window.startSystemMove()
        }
        TapHandler {
            onDoubleTapped: root.toggleMaximized()
        }
    }

    component Edge: MouseArea {
        property int edges: 0
        enabled: root.resizable && root.window && root.window.visibility !== Window.Maximized
        hoverEnabled: enabled
        acceptedButtons: Qt.LeftButton
        onPressed: if (root.window) root.window.startSystemResize(edges)
    }

    Edge { edges: Qt.LeftEdge; cursorShape: Qt.SizeHorCursor
           anchors { left: parent.left; top: parent.top; bottom: parent.bottom; topMargin: root.grip; bottomMargin: root.grip }
           width: root.grip }
    Edge { edges: Qt.RightEdge; cursorShape: Qt.SizeHorCursor
           anchors { right: parent.right; top: parent.top; bottom: parent.bottom; topMargin: root.grip; bottomMargin: root.grip }
           width: root.grip }
    Edge { edges: Qt.TopEdge; cursorShape: Qt.SizeVerCursor
           anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: root.grip; rightMargin: root.grip }
           height: root.grip }
    Edge { edges: Qt.BottomEdge; cursorShape: Qt.SizeVerCursor
           anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: root.grip; rightMargin: root.grip }
           height: root.grip }
    Edge { edges: Qt.LeftEdge | Qt.TopEdge; cursorShape: Qt.SizeFDiagCursor
           anchors { left: parent.left; top: parent.top } width: root.grip * 2; height: root.grip * 2 }
    Edge { edges: Qt.RightEdge | Qt.BottomEdge; cursorShape: Qt.SizeFDiagCursor
           anchors { right: parent.right; bottom: parent.bottom } width: root.grip * 2; height: root.grip * 2 }
    Edge { edges: Qt.RightEdge | Qt.TopEdge; cursorShape: Qt.SizeBDiagCursor
           anchors { right: parent.right; top: parent.top } width: root.grip * 2; height: root.grip * 2 }
    Edge { edges: Qt.LeftEdge | Qt.BottomEdge; cursorShape: Qt.SizeBDiagCursor
           anchors { left: parent.left; bottom: parent.bottom } width: root.grip * 2; height: root.grip * 2 }
}
