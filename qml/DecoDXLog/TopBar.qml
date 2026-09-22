// DecoDXLog — barra superiore: marchio, frequenza di Decodium, comandi, profilo
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
    signal activationRequested()
    signal profilesRequested()
    signal panelsRequested()
    signal aboutRequested()
    // Per le prove: apre il menu del marchio senza mouse.
    function openMainMenu() { mainMenu.popup(brandBlock, 0, brandBlock.height + 4) }
    // Idem per l'elenco dei modi, che di solito si apre cliccando la pillola.
    function openModeMenu() { modeMenu.popup(modePill, 0, modePill.height + 6) }
    signal logFolderRequested()
    signal quitRequested()
    // Quanti pannelli sono chiusi adesso: lo dice il pulsante, cosi' un pannello
    // sparito non e' un pannello perso.
    property int closedPanels: 0

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
        id: blockRoot
        default property alias content: blockRow.data
        property color outline: Theme.glassBorder
        property int hPadding: 12
        property alias spacing: blockRow.spacing
        // Un riquadro che si puo' premere tutto intero: serve al marchio, che
        // apre il menu. Il MouseArea sta qui e non fra il contenuto, perche'
        // li' dentro c'e' un layout e gli anchors non ci vanno.
        property bool clickable: false
        signal clicked()
        implicitHeight: 48
        implicitWidth: blockRow.implicitWidth + hPadding * 2
        radius: 6
        color: Theme.panelColor
        border.width: 1
        border.color: outline
        MouseArea {
            anchors.fill: parent
            enabled: blockRoot.clickable
            hoverEnabled: blockRoot.clickable
            cursorShape: Qt.PointingHandCursor
            onClicked: blockRoot.clicked()
        }
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
            id: brandBlock
            hPadding: 14

            // Tutto il riquadro del marchio apre il menu: le tre righette da
            // sole sono un bersaglio piccolo, e la gente ci clicca sopra il nome.
            clickable: true
            onClicked: mainMenu.opened ? mainMenu.close()
                                       : mainMenu.popup(brandBlock, 0, brandBlock.height + 4)

            StyledMenu {
                id: mainMenu
                StyledMenuItem { text: qsTr("About DecoDXLog…"); onTriggered: root.aboutRequested() }
                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
                StyledMenuItem { text: qsTr("Settings…"); onTriggered: root.setupRequested() }
                StyledMenuItem { text: qsTr("Station profiles…"); onTriggered: root.profilesRequested() }
                StyledMenuItem { text: qsTr("Panels…"); onTriggered: root.panelsRequested() }
                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
                StyledMenuItem { text: qsTr("Import ADIF…"); onTriggered: root.importRequested() }
                StyledMenuItem { text: qsTr("Export ADIF…"); onTriggered: root.exportRequested() }
                StyledMenuItem { text: qsTr("Open the log folder"); onTriggered: root.logFolderRequested() }
                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.borderSoft } }
                StyledMenuItem { text: qsTr("Quit"); onTriggered: root.quitRequested() }
            }

            Text {
                text: "≡"
                color: Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: 22
            }
            Column {
                // Il marchio, con il DX acceso come sull'icona.
                Text {
                    textFormat: Text.StyledText
                    text: "DECO<font color=\"" + Theme.secondaryColor + "\">DX</font>LOG"
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

        // Frequenza e modo di Decodium: DecoDXLog non tocca il CAT, li legge dallo Status.
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
            VfoDisplay {
                id: vfo
                // La radio quando c'e', perche' e' lo stato vero; Decodium
                // quando la radio non c'e'. Chi clicca uno spot deve vedere il
                // display muoversi con il VFO.
                mhz: decolog.shownFrequency.length ? parseFloat(decolog.shownFrequency) : 0
                tunable: decolog.rig.connected || decolog.clientConnected
                textColor: decolog.clientConnected || decolog.rig.connected
                           ? Theme.accentColor : Theme.textSecondary
                onTuned: (freq) => decolog.tuneTo(freq, "")
            }
            Text {
                text: "MHz"
                color: Theme.textSecondary
                font.pixelSize: 12
            }
            // Il modo: si clicca e si sceglie, CW e fonia in cima, i digitali
            // sotto. Quello che si sceglie va alla radio e a Decodium.
            Pill {
                id: modePill
                readonly property string shown: decolog.shownMode
                visible: shown.length > 0 || vfo.tunable
                text: shown.length ? shown : "···"
                tone: Theme.primaryColor
                rounded: false
                pillHeight: 26
                fontPixelSize: 12
                interactive: vfo.tunable
                onClicked: modeMenu.opened ? modeMenu.close()
                                           : modeMenu.popup(modePill, 0, modePill.height + 6)

                StyledMenu {
                    id: modeMenu
                    // L'elenco non cambia mentre il programma e' aperto: si
                    // chiede una volta sola, se no l'Instantiator rifa' tutte
                    // le voci ogni volta che qualcuno lo guarda.
                    readonly property var entries: decolog.operatingModes()

                    Instantiator {
                        model: modeMenu.entries
                        delegate: StyledMenuItem {
                            required property var modelData
                            text: modelData.name
                            checkable: true
                            checked: modePill.shown === modelData.name
                            onTriggered: decolog.tuneTo(vfo.mhz, modelData.name)
                        }
                        onObjectAdded: (index, object) => modeMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => modeMenu.removeItem(object)
                    }
                }
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
                readonly property var act: decolog.activation
                text: act.active
                      ? qsTr("%1 · %2/%3").arg(act.state.title).arg(act.qsoCount).arg(act.requiredQsos > 0 ? act.requiredQsos : act.qsoCount)
                      : qsTr("Activation")
                tone: !act.active ? "transparent"
                      : act.requiredQsos > 0 && act.qsoCount < act.requiredQsos ? Theme.warningColor : Theme.accentColor
                filled: act.active
                onClicked: root.activationRequested()
            }
            GlassButton {
                text: decolog.cluster.onlineCount > 0 ? qsTr("Cluster ●") : qsTr("Cluster")
                tone: decolog.cluster.onlineCount > 0 ? Theme.accentColor : "transparent"
                onClicked: root.clusterRequested()
            }
            GlassButton {
                text: root.closedPanels > 0 ? qsTr("Panels (%1 closed)").arg(root.closedPanels) : qsTr("Panels")
                tone: root.closedPanels > 0 ? Theme.warningColor : "transparent"
                onClicked: root.panelsRequested()
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

        // DecoDXLog Cloud: come sta il collegamento e cosa aspetta di partire.
        Block {
            Led {
                color: decolog.cloud.busy ? Theme.primaryColor
                     : decolog.cloud.linked ? Theme.accentColor
                     : decolog.cloud.server.length ? Theme.warningColor : Theme.textSecondary
            }
            Column {
                Text {
                    text: decolog.cloud.linked ? qsTr("Cloud %1").arg(decolog.cloud.callsign)
                        : decolog.cloud.server.length ? qsTr("Cloud not linked")
                        : qsTr("Cloud not configured")
                    color: Theme.textPrimary
                    font.family: Theme.monoFamily
                    font.pixelSize: 12
                    font.bold: true
                }
                Text {
                    text: decolog.cloud.busy ? qsTr("syncing…")
                        : decolog.cloud.queued > 0 ? qsTr("%1 queued").arg(decolog.cloud.queued)
                        : decolog.cloud.lastSync.length ? qsTr("synced %1").arg(decolog.cloud.lastSync)
                        : qsTr("%1 queued").arg(decolog.dirtyCount)
                    color: decolog.cloud.queued > 0 ? Theme.warningColor : Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 11
                }
            }
            GlassButton {
                text: decolog.cloud.busy ? qsTr("syncing…") : qsTr("Sync now")
                tone: Theme.primaryColor
                filled: true
                enabled: decolog.cloud.linked && !decolog.cloud.busy
                onClicked: decolog.cloud.syncNow()
                ToolTip.visible: hovered && !decolog.cloud.linked
                ToolTip.text: qsTr("Sign in from Setup → Sync & Cloud")
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
