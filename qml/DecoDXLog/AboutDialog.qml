// DecoDXLog — la finestra "Informazioni": cos'e' questo programma, chi l'ha
// fatto, con che licenza e dove si trova il codice. Poche righe, tutte vere.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

DialogFrame {
    id: root

    readonly property var info: decolog.about

    title: qsTr("About DecoDXLog")
    dialogKey: "about"
    width: 560
    height: 540

    // Il testo piccolo di spiegazione.
    component Note: Text {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Theme.textSecondary
        font.pixelSize: 12
    }

    // Una riga di quelle a due colonne: etichetta a sinistra, valore a destra.
    component Row_: RowLayout {
        property string label: ""
        property string value: ""
        property bool copiable: false
        Layout.fillWidth: true
        spacing: 10
        Text {
            Layout.preferredWidth: 130
            text: label
            color: Theme.textSecondary
            font.pixelSize: 12
        }
        Text {
            Layout.fillWidth: true
            text: value
            color: Theme.textPrimary
            font.family: copiable ? Theme.monoFamily : root.font.family
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }

    body: ColumnLayout {
        spacing: 12
        // Un po' d'aria attorno: il testo attaccato al bordo non si legge.
        anchors.margins: 16

        // L'icona e il nome, come sulla scatola.
        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            Image {
                source: "qrc:/decolog/decodxlog.png"
                sourceSize.width: 72
                sourceSize.height: 72
                Layout.preferredWidth: 72
                Layout.preferredHeight: 72
            }
            ColumnLayout {
                spacing: 2
                Text {
                    textFormat: Text.StyledText
                    text: "DECO<font color=\"" + Theme.secondaryColor + "\">DX</font>LOG"
                    color: Theme.primaryColor
                    font.pixelSize: 24
                    font.weight: Font.ExtraBold
                    font.letterSpacing: 0.8
                }
                Text {
                    text: qsTr("version %1").arg(root.info.version)
                    color: Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: 12
                }
                Text {
                    text: qsTr("The station logbook of the Decodium family")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSoft }

        SectionTitle { text: qsTr("Who made it") }
        Row_ { label: qsTr("Developer"); value: root.info.author }
        Row_ { label: qsTr("Email"); value: root.info.email; copiable: true }
        Row_ { label: qsTr("Home"); value: root.info.home; copiable: true }

        SectionTitle { text: qsTr("This copy") }
        Row_ { label: qsTr("QSO in the log"); value: Number(root.info.qsoCount).toLocaleString(Qt.locale(), "f", 0) }
        Row_ { label: qsTr("Built with"); value: "Qt " + root.info.qt }
        Row_ { label: qsTr("Built on"); value: root.info.built }
        Row_ { label: qsTr("Licence"); value: root.info.license }

        Note {
            Layout.fillWidth: true
            text: qsTr("Free software: you can use it, study it, change it and pass it on, sources "
                       + "included. The log is yours and stays on your computer — a SQLite file that "
                       + "opens even without us.")
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            GlassButton {
                text: qsTr("Copy these details")
                onClicked: {
                    detailsForClipboard.text = "DecoDXLog " + root.info.version
                                             + "\n" + root.info.author + " <" + root.info.email + ">"
                                             + "\n" + root.info.home
                                             + "\nQt " + root.info.qt + " — " + root.info.built
                                             + "\n" + root.info.license
                    detailsForClipboard.selectAll()
                    detailsForClipboard.copy()
                    copied.visible = true
                }
            }
            Pill {
                id: copied
                visible: false
                text: qsTr("copied")
                tone: Theme.accentColor
            }
            Item { Layout.fillWidth: true }
            GlassButton { text: qsTr("Close"); onClicked: root.close() }
        }
    }

    // Serve solo per passare dagli appunti: non si vede.
    TextEdit {
        id: detailsForClipboard
        visible: false
    }
}
