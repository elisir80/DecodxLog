// DecoDXLog — una finestra vera di Decodium dentro la lavagna del log.
//
// Full Spectrum e Signal RX come li disegna Decodium, con i suoi colori e le
// sue colonne: non una lista ricostruita ma la sua finestra, mostrata viva in
// un pannello che si sposta, si ridimensiona, si attacca ai bordi e si stacca
// come gli altri. Si sceglie cosa mostrare: una zona della finestra principale
// di Decodium (di partenza quella di Full Spectrum o di Signal RX), oppure una
// delle sue finestre staccate col tasto «Pop».
//
// La finestra di Decodium non viene toccata (resta sua): se ne mostra
// l'immagine, ritagliata. Deve esserci — non ridotta a icona — ma puo' stare
// coperta o su un altro schermo. I clic arrivano a Decodium solo se si accende
// l'interruttore «⇄».
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI
import DecoDXLog.Native

GlassPanel {
    id: root

    // "decfull" (Full Spectrum) o "decsig" (Signal RX): lo da' la lavagna.
    readonly property bool isSignal: panelKey === "decsig"
    property bool selecting: false

    // Per le schermate di prova.
    function showMenu(name) {
        if (name === "source") sourceMenu.popup()
        else if (name === "select") root.selecting = true
    }

    title: isSignal ? qsTr("Signal RX") : qsTr("Full Spectrum")
    dotColor: mirror.status === WindowMirror.Live ? Theme.accentColor : Theme.errorColor
    padding: 0

    component HeaderToggle: Item {
        id: toggle
        property string glyph: ""
        property string hint: ""
        property bool on: false
        signal clicked()
        implicitWidth: 18
        implicitHeight: 18
        Text {
            anchors.centerIn: parent
            text: toggle.glyph
            color: toggle.on ? Theme.accentColor : toggleArea.containsMouse ? Theme.primaryColor : Theme.textSecondary
            font.pixelSize: Theme.fontSize
            font.bold: toggle.on
        }
        MouseArea {
            id: toggleArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: toggle.clicked()
        }
        ToolTip.visible: toggleArea.containsMouse && toggle.hint.length > 0
        ToolTip.delay: 500
        ToolTip.text: toggle.hint
    }

    headerTools: [
        HeaderToggle {
            anchors.verticalCenter: parent.verticalCenter
            glyph: "▾"
            hint: qsTr("Choose the Decodium window to show")
            onClicked: sourceMenu.popup()
        },
        HeaderToggle {
            anchors.verticalCenter: parent.verticalCenter
            glyph: "▭"
            on: root.selecting
            hint: qsTr("Pick the area of the window to show")
            onClicked: root.selecting = !root.selecting
        },
        HeaderToggle {
            anchors.verticalCenter: parent.verticalCenter
            glyph: "⇄"
            on: mirror.forwardInput
            hint: mirror.forwardInput ? qsTr("Clicks go to Decodium: click to stop")
                                      : qsTr("Send your clicks to Decodium (a double click on a line answers it): off")
            onClicked: mirror.forwardInput = !mirror.forwardInput
        },
        HeaderToggle {
            anchors.verticalCenter: parent.verticalCenter
            visible: mirror.target.length > 0 && mirror.target !== "@main"
            glyph: "⤡"
            on: mirror.matchSize
            hint: qsTr("Resize the Decodium window to fit this panel, so the text stays sharp")
            onClicked: mirror.matchSize = !mirror.matchSize
        }
    ]

    WindowMirror {
        id: mirror
        anchors.fill: parent
        settingsKey: root.panelKey
        showAll: root.selecting
        onStatusChanged: if (status !== WindowMirror.Live) root.selecting = false
    }

    // I clic verso Decodium. Il doppio clic di Qt Quick nasce da un secondo
    // «premuto» marcato: a Decodium arriva come il doppio clic di Windows.
    MouseArea {
        anchors.fill: parent
        enabled: mirror.forwardInput && mirror.status === WindowMirror.Live && !root.selecting
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onPressed: (m) => mirror.send((m.flags & Qt.MouseEventCreatedDoubleClick) ? "double" : "press",
                                      m.x, m.y, m.button === Qt.RightButton ? 2 : 1, 0)
        onReleased: (m) => mirror.send("release", m.x, m.y, m.button === Qt.RightButton ? 2 : 1, 0)
        onWheel: (w) => mirror.send("wheel", w.x, w.y, 0, w.angleDelta.y)
    }

    // Scegliere la zona: si vede la finestra intera e si traccia un rettangolo.
    Item {
        anchors.fill: parent
        visible: root.selecting
        Rectangle { anchors.fill: parent; color: Qt.rgba(0, 0, 0, 0.25) }
        Rectangle {
            id: band
            visible: area.pressed
            x: Math.min(area.start.x, area.current.x)
            y: Math.min(area.start.y, area.current.y)
            width: Math.abs(area.current.x - area.start.x)
            height: Math.abs(area.current.y - area.start.y)
            color: Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.22)
            border.color: Theme.primaryColor
            border.width: 2
        }
        Rectangle {
            anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 6 }
            width: hintText.implicitWidth + 20
            height: hintText.implicitHeight + 10
            radius: 4
            color: Theme.panelColor
            border.color: Theme.primaryColor
            Text {
                id: hintText
                anchors.centerIn: parent
                text: qsTr("Drag over the part of the Decodium window you want to see · Esc cancels")
                color: Theme.textPrimary
                font.pixelSize: 11
            }
        }
        MouseArea {
            id: area
            anchors.fill: parent
            property point start
            property point current
            cursorShape: Qt.CrossCursor
            onPressed: (m) => { start = Qt.point(m.x, m.y); current = start }
            onPositionChanged: (m) => { if (pressed) current = Qt.point(m.x, m.y) }
            onReleased: {
                const r = Qt.rect(Math.min(start.x, current.x), Math.min(start.y, current.y),
                                  Math.abs(current.x - start.x), Math.abs(current.y - start.y))
                if (r.width > 8 && r.height > 8)
                    mirror.selectRegion(r)
                root.selecting = false
            }
        }
        focus: visible
        Keys.onEscapePressed: root.selecting = false
    }

    // Quando non si vede niente, si dice perche'.
    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - 32
        spacing: 10
        visible: mirror.status !== WindowMirror.Live
    Text {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: Theme.textSecondary
        font.pixelSize: 12
        text: mirror.status === WindowMirror.Unsupported ? qsTr("This panel works only on Windows.")
            : mirror.status === WindowMirror.NoTarget ? qsTr("Choose the Decodium window to show with ▾ in the header.")
            : mirror.status === WindowMirror.Minimized ? qsTr("The Decodium window is minimized, and a minimized window cannot be copied. It can stay covered by other windows or on another screen.")
            : mirror.target === "@main" ? qsTr("Decodium is not open. Start it: its window shows up here.")
            : qsTr("The window \"%1\" is not open. In Decodium press Pop on the list to detach it, or pick another window with ▾.").arg(mirror.target)
    }
    GlassButton {
        Layout.alignment: Qt.AlignHCenter
        visible: mirror.status === WindowMirror.Minimized
        text: qsTr("Bring Decodium back, behind the other windows")
        onClicked: mirror.restoreSource()
    }
    }

    StyledMenu {
        id: sourceMenu
        property var list: []
        onAboutToShow: list = mirror.windows()
        Instantiator {
            model: sourceMenu.list
            delegate: StyledMenuItem {
                required property var modelData
                text: modelData.label + "  (" + modelData.width + "×" + modelData.height + ")"
                onTriggered: {
                    mirror.target = modelData.target
                    if (!modelData.main)
                        mirror.region = Qt.rect(0, 0, 1, 1)
                }
            }
            onObjectAdded: (index, object) => sourceMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => sourceMenu.removeItem(object)
        }
        StyledMenuItem {
            text: qsTr("No Decodium window is open")
            enabled: false
            visible: sourceMenu.list.length === 0
            height: visible ? implicitHeight : 0
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
        StyledMenuItem { text: qsTr("Area of Full Spectrum in the main window"); onTriggered: mirror.applyPreset("decfull") }
        StyledMenuItem { text: qsTr("Area of Signal RX in the main window"); onTriggered: mirror.applyPreset("decsig") }
        StyledMenuItem { text: qsTr("The whole main window"); onTriggered: mirror.applyPreset("window") }
        StyledMenuItem { text: qsTr("Pick the area…"); onTriggered: root.selecting = true }
    }
}
