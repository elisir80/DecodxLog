// DecoDXLog — e' uscita una versione nuova.
//
// Si fa vedere solo quando c'e' davvero qualcosa di nuovo, e da li' in poi
// decide chi opera: si scarica e si installa, si va a leggere cosa c'e' di
// nuovo, si rimanda, o si mette da parte questa versione e non se ne parla
// piu'. Durante un contest nessuno vuole un programma che si cambia sotto i
// piedi da solo.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

DialogFrame {
    id: root

    readonly property var updates: decolog.updates

    title: qsTr("A new version is out")
    dialogKey: "update"
    width: 560
    height: 460

    body: ColumnLayout {
        spacing: 12
        anchors.margins: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            Image {
                source: "qrc:/decolog/decodxlog.png"
                sourceSize.width: 64
                sourceSize.height: 64
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
            }
            ColumnLayout {
                spacing: 2
                Text {
                    text: "DecoDXLog " + root.updates.latestVersion
                    color: Theme.primaryColor
                    font.pixelSize: 22
                    font.weight: Font.ExtraBold
                }
                Text {
                    text: qsTr("you have %1").arg(root.updates.currentVersion)
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 12
                }
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSoft }

        SectionTitle { text: qsTr("What is new") }

        // Le note della release, come le ha scritte chi pubblica.
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            TextArea {
                readOnly: true
                wrapMode: TextArea.Wrap
                text: root.updates.notes
                color: Theme.textSecondary
                font.pixelSize: 12
                background: null
            }
        }

        // Mentre scarica: quanto manca.
        ProgressBar {
            Layout.fillWidth: true
            visible: root.updates.downloading
            indeterminate: root.updates.progress < 0
            value: root.updates.progress < 0 ? 0 : root.updates.progress
        }

        Text {
            Layout.fillWidth: true
            visible: root.updates.status.length > 0
            text: root.updates.status
            color: Theme.textSecondary
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }

        // I pulsanti vanno a capo se non ci stanno: in italiano e in tedesco
        // «Salta questa versione» e' lungo, e in riga finiva mezzo fuori.
        Flow {
            Layout.fillWidth: true
            spacing: 8

            GlassButton {
                text: root.updates.downloadSize.length
                      ? qsTr("Update now (%1)").arg(root.updates.downloadSize)
                      : qsTr("Update now")
                tone: Theme.accentColor
                filled: true
                visible: root.updates.hasInstaller
                enabled: !root.updates.downloading
                onClicked: root.updates.downloadAndInstall()
            }
            GlassButton {
                text: qsTr("Stop")
                tone: Theme.errorColor
                visible: root.updates.downloading
                onClicked: root.updates.cancelDownload()
            }
            GlassButton {
                text: qsTr("Open the page")
                onClicked: root.updates.openPage()
            }
            GlassButton {
                text: qsTr("Later")
                onClicked: root.close()
            }
            GlassButton {
                text: qsTr("Skip this version")
                onClicked: { root.updates.skipThisVersion(); root.close() }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("«Update now» downloads the installer and opens it: DecoDXLog closes, because "
                       + "an installer cannot replace the files of a program that is running. The log "
                       + "and the settings stay where they are.")
            color: Theme.textSecondary
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
    }
}
