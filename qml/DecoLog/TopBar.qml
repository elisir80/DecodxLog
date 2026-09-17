// DecoLog — barra superiore: marchio, frequenza di Decodium, comandi, profilo
// stazione, stato del cloud, ricerca.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Rectangle {
    id: root

    signal setupRequested()
    signal importRequested()
    signal exportRequested()
    signal awardsRequested()
    signal clusterRequested()
    signal profilesRequested()

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    readonly property var profiles: decolog.stationProfiles

    implicitHeight: 64
    color: Theme.bgMedium

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Theme.borderSoft
    }

    component Block: Rectangle {
        default property alias content: blockRow.data
        property color outline: Theme.glassBorder
        property int hPadding: 12
        property alias spacing: blockRow.spacing
        implicitHeight: 48
        implicitWidth: blockRow.implicitWidth + hPadding * 2
        radius: 6
        color: Theme.panelColor
        border.width: 1
        border.color: outline
        RowLayout {
            id: blockRow
            anchors.fill: parent
            anchors.leftMargin: parent.hPadding
            anchors.rightMargin: parent.hPadding
            spacing: 8
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        // Marchio e versione.
        Block {
            hPadding: 14
            Text {
                text: "≡"
                color: Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: 22
            }
            Column {
                Text {
                    text: "DECOLOG"
                    color: Theme.primaryColor
                    font.pixelSize: 15
                    font.weight: Font.ExtraBold
                    font.letterSpacing: 0.6
                }
                Text {
                    text: "v" + decolog.version
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
            }
        }

        // Frequenza e modo di Decodium: DecoLog non tocca il CAT, li legge dallo Status.
        Block {
            outline: decolog.clientConnected ? Theme.accentColor : Theme.glassBorder
            hPadding: 14
            Led {
                color: !decolog.listening ? Theme.errorColor
                     : decolog.transmitting ? Theme.warningColor
                     : decolog.clientConnected ? Theme.accentColor : Theme.textSecondary
                size: 10
                glow: decolog.clientConnected
                blinking: decolog.transmitting
            }
            Text {
                text: decolog.dialFrequency.length ? decolog.dialFrequency : "--.------"
                color: decolog.clientConnected ? Theme.accentColor : Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: 20
                font.bold: true
            }
            Text {
                text: "MHz"
                color: Theme.textSecondary
                font.pixelSize: 12
            }
            Pill {
                visible: decolog.currentMode.length > 0
                text: decolog.currentMode
                tone: Theme.primaryColor
                rounded: false
                pillHeight: 26
                fontPixelSize: 12
            }
        }

        Block {
            hPadding: 10
            spacing: 6
            GlassButton { text: qsTr("Setup"); tone: Theme.primaryColor; filled: true; onClicked: root.setupRequested() }
            GlassButton { text: qsTr("Import"); onClicked: root.importRequested() }
            GlassButton { text: qsTr("Export"); onClicked: root.exportRequested() }
            GlassButton { text: qsTr("Awards"); onClicked: root.awardsRequested() }
            GlassButton {
                text: decolog.cluster.onlineCount > 0 ? qsTr("Cluster ●") : qsTr("Cluster")
                tone: decolog.cluster.onlineCount > 0 ? Theme.accentColor : "transparent"
                onClicked: root.clusterRequested()
            }
        }

        // Il profilo con cui si scrivono i QSO nuovi.
        Block {
            Text {
                text: qsTr("Station")
                color: Theme.textSecondary
                font.pixelSize: 11
            }
            StyledComboBox {
                id: stationBox
                Layout.preferredWidth: 180
                model: root.profiles
                textRole: "name"
                valueRole: "profileId"
                displayText: root.profiles.activeProfile.name ? root.profiles.activeProfile.name : qsTr("No profile")
                currentIndex: root.profiles.rowForId(root.profiles.activeProfileId)
                onActivated: (index) => {
                    const p = root.profiles.get(index)
                    if (p.deleted)
                        root.profilesRequested()
                    else
                        root.profiles.activeProfileId = p.id
                }
                Connections {
                    target: root.profiles
                    function onActiveChanged() { stationBox.currentIndex = root.profiles.rowForId(root.profiles.activeProfileId) }
                }
            }
            GlassButton {
                text: "✎"
                minimumWidth: 30
                implicitWidth: 30
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Station profiles")
                onClicked: root.profilesRequested()
            }
        }

        Item { Layout.fillWidth: true }

        // DecoLog Cloud arriva in Fase 3: qui si vede cosa aspetta di partire.
        Block {
            Led {
                color: decolog.cloudServer.length ? Theme.warningColor : Theme.textSecondary
            }
            Column {
                Text {
                    text: decolog.cloudServer.length ? qsTr("Cloud offline") : qsTr("Cloud not configured")
                    color: Theme.textPrimary
                    font.family: Theme.monoFamily
                    font.pixelSize: 12
                    font.bold: true
                }
                Text {
                    text: qsTr("%1 queued").arg(decolog.dirtyCount)
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
            }
            GlassButton {
                text: qsTr("Sync now")
                tone: Theme.primaryColor
                filled: true
                enabled: false
                ToolTip.visible: hovered
                ToolTip.text: qsTr("DecoLog Cloud sync arrives in Phase 3")
            }
        }

        Block {
            hPadding: 10
            spacing: 6
            StyledTextField {
                id: searchField
                Layout.preferredWidth: 140
                placeholderText: qsTr("Search…")
                text: decolog.qsoModel.filterText
                onTextEdited: decolog.qsoModel.filterText = text
                Keys.onEscapePressed: { text = ""; decolog.qsoModel.filterText = "" }
            }
            GlassButton {
                text: "⌕"
                tone: Theme.secondaryColor
                minimumWidth: 30
                implicitWidth: 30
                buttonHeight: 30
                fontPixelSize: 14
                onClicked: root.focusSearch()
            }
        }
    }
}
