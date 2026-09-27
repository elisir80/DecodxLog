// DecoDXLog — i moltiplicatori della gara, banda per banda: una riga per zona,
// paese, provincia..., una colonna per banda, la casella accesa dove e' stato
// lavorato. Le zone e le province ci sono tutte, cosi' si vede cosa manca.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Decodium.UI

GlassPanel {
    id: root

    property int revision: 0
    readonly property var matrix: { revision; return decolog.activation.active ? decolog.activation.multiplierMatrix() : ({}) }
    readonly property var kinds: matrix.kinds || []
    readonly property var bands: matrix.bands || []
    readonly property var current: kinds.length ? kinds[Math.min(store.kind, kinds.length - 1)] : null

    Settings {
        id: store
        category: "contest/mults"
        property int kind: 0
        property bool onlyMissing: false
    }
    Connections {
        target: decolog
        function onLogChanged() { refresh.restart() }
    }
    Connections {
        target: decolog.activation
        function onChanged() { refresh.restart() }
    }
    Timer { id: refresh; interval: 400; onTriggered: root.revision++ }

    title: qsTr("Multipliers")
    dotColor: Theme.warningColor
    padding: 8

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        Text {
            visible: !decolog.activation.active
            text: qsTr("Open a contest session to see the multipliers.")
            color: Theme.textSecondary
            font.pixelSize: 12
        }
        RowLayout {
            Layout.fillWidth: true
            visible: root.kinds.length > 0
            spacing: 6
            Repeater {
                model: root.kinds
                GlassButton {
                    required property var modelData
                    required property int index
                    text: "%1 · %2".arg(modelData.label).arg(modelData.count)
                    buttonHeight: 22
                    fontPixelSize: 11
                    filled: index === Math.min(store.kind, root.kinds.length - 1)
                    onClicked: store.kind = index
                }
            }
            Item { Layout.fillWidth: true }
            ToggleSwitch {
                text: qsTr("Only missing")
                checked: store.onlyMissing
                onToggled: store.onlyMissing = checked
            }
        }

        // Le bande in testa.
        Row {
            visible: root.current !== null && root.current.perBand
            spacing: 2
            Item { width: 150; height: 16 }
            Repeater {
                model: root.bands
                Text {
                    required property string modelData
                    width: 42
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 10
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.current ? root.current.rows.filter(r => !store.onlyMissing
                                                               || (root.current.perBand ? root.bands.some(b => !r.worked[b]) : !r.any))
                                : []
            ScrollBar.vertical: PanelScrollBar {}
            delegate: Row {
                id: row
                required property var modelData
                height: 18
                spacing: 2
                Text {
                    width: 150
                    height: 18
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    text: row.modelData.value + (row.modelData.name ? " " + row.modelData.name : "")
                    color: row.modelData.any ? Theme.textPrimary : Theme.warningColor
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
                Repeater {
                    model: root.current && root.current.perBand ? root.bands : []
                    Rectangle {
                        required property string modelData
                        width: 42
                        height: 16
                        radius: 2
                        color: row.modelData.worked[modelData] ? Theme.accentColor : Theme.bgMedium
                        opacity: row.modelData.worked[modelData] ? 0.85 : 1
                    }
                }
            }
        }
    }
}
