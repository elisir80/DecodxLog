// DecoDXLog — la finestra principale in modalita' contest: la base del banco.
//
// In gara la finestra grande non mostra il log di tutti i giorni: niente
// pannello del nuovo QSO, niente schede di diplomi e statistiche. Sta sotto, e
// sopra di lei stanno le finestre della gara, raggruppate. Qui resta solo
// quello che serve a ritrovarsi: la gara, i conti, e i tre pulsanti per
// rimettere a posto le finestre, riaprirle se si sono chiuse, o uscire.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Rectangle {
    id: root

    signal arrangeRequested()
    signal exitRequested()
    signal deskRequested()

    property int revision: 0
    readonly property var act: decolog.activation
    readonly property var session: { revision; return act.state }
    readonly property var scoring: { revision; return act.score() }

    Connections {
        target: decolog.activation
        function onChanged() { root.revision++ }
    }
    Connections {
        target: decolog
        function onLogChanged() { root.revision++ }
    }

    color: Theme.bgDeep

    // Sotto c'e' la disposizione di tutti i giorni: i clic e la rotella si
    // fermano qui, invece di arrivare ai pannelli coperti.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = true
    }

    // Una griglia leggera sullo sfondo: si capisce che e' un piano di lavoro,
    // non una finestra rimasta vuota per sbaglio.
    Canvas {
        anchors.fill: parent
        opacity: 0.35
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = Theme.borderSoft
            ctx.lineWidth = 1
            for (let x = 0; x < width; x += 48) {
                ctx.beginPath(); ctx.moveTo(x + 0.5, 0); ctx.lineTo(x + 0.5, height); ctx.stroke()
            }
            for (let y = 0; y < height; y += 48) {
                ctx.beginPath(); ctx.moveTo(0, y + 0.5); ctx.lineTo(width, y + 0.5); ctx.stroke()
            }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 10
        width: Math.min(560, root.width - 40)

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("CONTEST MODE")
            color: Theme.accentColor
            font.family: Theme.monoFamily
            font.pixelSize: 13
            font.bold: true
            font.letterSpacing: 3
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: root.session.title || qsTr("No session open")
            color: Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: 26
            font.bold: true
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            visible: root.scoring.valid
            text: qsTr("%1 QSO · %2 points · %3 mult · %4")
                      .arg(root.session.qsoCount || 0).arg(root.scoring.points || 0)
                      .arg(root.scoring.multipliers || 0).arg(root.scoring.score || 0)
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 14
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("The contest windows stay on top of this one and are minimised with it. Put "
                       + "them where you like: they stay there. Closed one by mistake? Open the desk again.")
            color: Theme.textSecondary
            font.pixelSize: 12
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8
            GlassButton {
                text: qsTr("Open the desk again")
                tone: Theme.accentColor
                onClicked: root.deskRequested()
            }
            GlassButton {
                text: qsTr("Arrange the windows")
                tone: Theme.primaryColor
                onClicked: root.arrangeRequested()
            }
            GlassButton {
                text: qsTr("Leave contest mode")
                tone: Theme.errorColor
                onClicked: root.exitRequested()
            }
        }
    }
}
