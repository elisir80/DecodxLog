// DecoDXLog — come sta andando la gara: QSO, ritmo, ultimi collegati.
//
// Il ritmo e' la cosa che si guarda piu' spesso dopo il nominativo: dice se
// vale la pena restare su questa banda o cambiare. Sta in un pannello suo
// perche' in contest lo si tiene d'occhio mentre si scrive da un'altra parte.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    property int revision: 0
    readonly property var act: decolog.activation
    readonly property var session: { revision; return act.state }
    readonly property var rate: { revision; return act.rate() }
    readonly property var recent: { revision; return act.recentQsos(12) }

    Connections {
        target: decolog
        function onLogChanged() { root.revision++ }
    }
    Connections {
        target: decolog.activation
        function onChanged() { root.revision++ }
    }
    // Il ritmo scende da solo mentre non si lavora nessuno: se non si rilegge,
    // resta fermo sull'ultimo QSO e dice una cosa che non e' piu' vera.
    Timer {
        running: act.active
        interval: 15000
        repeat: true
        onTriggered: root.revision++
    }

    title: qsTr("How it is going")
    dotColor: act.active ? Theme.accentColor : Theme.textSecondary

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            rowSpacing: 6
            columnSpacing: 6
            Repeater {
                model: [
                    { label: qsTr("QSO"), value: String(root.session.qsoCount || 0), tone: Theme.primaryColor },
                    { label: qsTr("Calls"), value: String(root.session.uniqueCalls || 0), tone: Theme.textPrimary },
                    { label: qsTr("Duration"), value: root.session.elapsed || "0:00", tone: Theme.textPrimary },
                    { label: qsTr("Last 10 min"), value: String(root.rate.last10 || 0), tone: Theme.textPrimary },
                    { label: qsTr("QSO/h"), value: String(root.rate.perHour10 || 0), tone: Theme.accentColor },
                    { label: qsTr("Last hour"), value: String(root.rate.last60 || 0), tone: Theme.textPrimary },
                ]
                Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    radius: 5
                    color: Theme.bgDeep
                    border.color: Theme.borderSoft
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 0
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.label
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 9
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.value
                            color: modelData.tone
                            font.family: Theme.monoFamily
                            font.pixelSize: 18
                            font.bold: true
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Last QSOs")
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.recent
            ScrollBar.vertical: PanelScrollBar {}
            delegate: RowLayout {
                required property var modelData
                width: ListView.view.width
                height: 22
                spacing: 8
                Text {
                    Layout.preferredWidth: 44
                    text: modelData.time || ""
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
                Text {
                    Layout.fillWidth: true
                    text: modelData.call || ""
                    color: Theme.textPrimary
                    font.family: Theme.monoFamily
                    font.pixelSize: 12
                    font.bold: true
                }
                Text {
                    Layout.preferredWidth: 46
                    text: modelData.band || ""
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
                Text {
                    Layout.preferredWidth: 46
                    text: modelData.mode || ""
                    color: Theme.secondaryColor
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
            }
        }
    }
}
