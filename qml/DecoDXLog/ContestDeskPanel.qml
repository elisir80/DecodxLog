// DecoDXLog — il banco del contest: il comando di tutte le finestre.
//
// In gara le finestre sono otto e vanno dove servono a chi opera, non dove
// abbiamo deciso noi. Di qui si accende e si spegne ognuna, si sceglie una
// disposizione di partenza, si tengono tutte davanti alla finestra grande, e si
// arriva alle cose che servono solo in contest: il Cabrillo e l'invio del log.
//
// Resta piccola e sempre in vista: e' la pulsantiera, non una finestra da
// guardare.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    // Il comando va alla finestra principale, che e' l'unica che sa dove sono
    // le altre: "toggle", "arrange", "ontop", "export", "submit".
    signal deskCommand(string what, string arg)

    property int revision: 0
    readonly property var act: decolog.activation
    readonly property var session: { revision; return act.state }
    readonly property var scoring: { revision; return act.score() }
    // Quali finestre sono aperte adesso: la finestra principale la tiene
    // aggiornata.
    property var openPanels: []
    property bool allOnTop: true

    Connections {
        target: decolog.activation
        function onChanged() { root.revision++ }
    }

    readonly property var deskPanels: [
        { key: "contest", label: qsTr("QSO entry") },
        { key: "cluster", label: qsTr("Cluster") },
        { key: "logbook", label: qsTr("Logbook") },
        { key: "callinfo", label: qsTr("Callsign card") },
        { key: "rate", label: qsTr("Rate") },
        { key: "score", label: qsTr("Score") },
        { key: "map", label: qsTr("Map") },
        { key: "cw", label: qsTr("CW") },
    ]

    // La stessa pasticca dei filtri del cluster: accesa vuol dire aperta.
    component Chip: Rectangle {
        id: chip
        property string label
        property bool on: false
        property color tone: Theme.primaryColor
        signal toggled()
        implicitHeight: 22
        implicitWidth: chipText.implicitWidth + 14
        radius: 4
        color: on ? Qt.rgba(tone.r, tone.g, tone.b, 0.22)
                  : chipArea.containsMouse ? Theme.glassOverlay : "transparent"
        border.width: 1
        border.color: on ? tone : Theme.glassBorder
        Text {
            id: chipText
            anchors.centerIn: parent
            text: chip.label
            color: chip.on ? chip.tone : Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
            font.bold: chip.on
        }
        MouseArea {
            id: chipArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: chip.toggled()
        }
    }

    title: qsTr("Contest desk")
    dotColor: root.act.active ? Theme.accentColor : Theme.textSecondary

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // ── Le finestre: una alla volta, accese o spente ────────────────────
        Flow {
            Layout.fillWidth: true
            spacing: 4
            Repeater {
                model: root.deskPanels
                Chip {
                    required property var modelData
                    label: modelData.label
                    on: root.openPanels.indexOf(modelData.key) >= 0
                    onToggled: root.deskCommand("toggle", modelData.key)
                }
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSoft }

        // ── Come stanno messe ───────────────────────────────────────────────
        Text {
            text: qsTr("Layout")
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
        }
        Flow {
            Layout.fillWidth: true
            spacing: 4
            GlassButton {
                // Tre colonne: cluster, lavoro, conti. E' la disposizione dei
                // programmi da contest, ed e' quella che regge uno schermo solo.
                text: qsTr("Columns")
                buttonHeight: 24
                onClicked: root.deskCommand("arrange", "columns")
            }
            GlassButton {
                // Tutto attorno all'inserimento, che e' dove stanno gli occhi.
                text: qsTr("Centred")
                buttonHeight: 24
                onClicked: root.deskCommand("arrange", "centred")
            }
            GlassButton {
                text: qsTr("Two screens")
                buttonHeight: 24
                onClicked: root.deskCommand("arrange", "two")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ToggleSwitch {
                checked: root.allOnTop
                onToggled: root.deskCommand("ontop", checked ? "1" : "0")
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Always in front, never minimised")
                color: Theme.textPrimary
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSoft }

        // ── Quello che serve solo in gara ───────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            GlassButton {
                text: qsTr("Cabrillo…")
                tone: Theme.primaryColor
                buttonHeight: 24
                onClicked: root.deskCommand("export", "")
            }
            GlassButton {
                // Ogni contest ha il suo posto dove si manda il log: il
                // programma ci porta, con il Cabrillo gia' scritto.
                text: qsTr("Send the log…")
                tone: Theme.accentColor
                buttonHeight: 24
                enabled: root.scoring.valid
                onClicked: root.deskCommand("submit", "")
            }
            Item { Layout.fillWidth: true }
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: root.act.active
                  ? (root.scoring.valid
                     ? qsTr("%1 · %2 QSO · %3 points · %4 mult")
                           .arg(root.session.title || "")
                           .arg(root.session.qsoCount || 0)
                           .arg(root.scoring.points || 0)
                           .arg(root.scoring.multipliers || 0)
                     : qsTr("%1 · %2 QSO").arg(root.session.title || "").arg(root.session.qsoCount || 0))
                  : qsTr("No session open: Contest → Start session.")
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
        }
        Item { Layout.fillHeight: true }
    }
}
