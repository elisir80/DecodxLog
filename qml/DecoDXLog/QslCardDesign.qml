// DecoDXLog — la cartolina QSL: si carica la propria, si posano i campi sopra.
//
// Una QSL di carta si stampa sulla cartolina che ci si e' fatti fare, dove
// restano i riquadri vuoti da riempire: il corrispondente, la data, l'ora, la
// banda, il modo, il rapporto. Qui si dice dove vanno quei riquadri — una volta
// sola, trascinandoli col mouse — e poi il programma li riempie da solo per
// tutti i QSO che si vogliono.
//
// Quello che si vede qui e' quello che si stampa: le posizioni sono frazioni
// del modello e i corpi millesimi della sua altezza, e a disegnare e' sempre lo
// stesso codice, sullo schermo come nel PDF.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Decodium.UI

ColumnLayout {
    id: root

    // Gli id scelti nella coda: vuoto vuol dire "tutta la coda".
    property var selectedIds: []

    readonly property var cards: decolog.cards
    readonly property var fields: cards.cardFields
    readonly property var sample: { cards.cardChanged; decolog.logChanged; return cards.sampleQso() }
    readonly property var station: cards.stationInfo()
    property int selected: -1

    spacing: 10

    function fieldValue(field) {
        const key = field.key
        if (key === "text") return field.text
        if (key.startsWith("my")) {
            const what = key.substring(2, 3).toLowerCase() + key.substring(3)
            return root.station[what] || ""
        }
        const date = root.sample.date || ""
        if (key === "day")   return date.length === 10 ? date.substring(8, 10) : ""
        if (key === "month") return date.length === 10 ? date.substring(5, 7) : ""
        if (key === "year")  return date.length === 10 ? date.substring(0, 4) : ""
        if (key === "monthName") {
            const names = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                           "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"]
            return date.length === 10 ? names[parseInt(date.substring(5, 7), 10) - 1] : ""
        }
        if (key === "time") {
            const t = root.sample.time || ""
            return t.length === 4 ? t.substring(0, 2) + ":" + t.substring(2) : t
        }
        if (key === "freq") {
            const mhz = parseFloat(root.sample.freq)
            return isNaN(mhz) || mhz <= 0 ? "" : mhz.toFixed(3)
        }
        return root.sample[key] || ""
    }

    // ── Il modello ──────────────────────────────────────────────────────────
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Text {
            text: qsTr("Card model")
            color: Theme.textSecondary
            font.pixelSize: 12
        }
        StyledTextField {
            Layout.fillWidth: true
            readOnly: true
            text: root.cards.cardTemplate || qsTr("no image yet")
        }
        Text {
            visible: root.cards.cardSize.width > 0
            text: root.cards.cardSize.width + " × " + root.cards.cardSize.height
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
        }
        GlassButton {
            text: qsTr("Load an image…")
            buttonHeight: 26
            fontPixelSize: 12
            onClicked: templateDialog.open()
        }
    }

    // ── La cartolina, con i campi sopra ─────────────────────────────────────
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 200
        color: Theme.bgMedium
        border.width: 1
        border.color: Theme.borderSoft
        radius: 4
        clip: true

        // La cartolina tiene le sue proporzioni e sta in mezzo: quello che si
        // vede e' la cartolina, non il riquadro che la contiene.
        Item {
            id: view
            readonly property real ratio: root.cards.cardSize.height > 0
                                          ? root.cards.cardSize.width / root.cards.cardSize.height
                                          : 148 / 105
            readonly property real maxW: parent.width - 24
            readonly property real maxH: parent.height - 24
            width: Math.min(maxW, maxH * ratio)
            height: width / ratio
            anchors.centerIn: parent

            Image {
                id: background
                anchors.fill: parent
                source: root.cards.cardTemplate.length
                        ? "file:///" + root.cards.cardTemplate.replace(/\\/g, "/") : ""
                fillMode: Image.Stretch
                smooth: true
                cache: false
            }
            Rectangle {
                anchors.fill: parent
                visible: background.status !== Image.Ready
                color: "white"
                border.width: 1
                border.color: Theme.borderColor
                Text {
                    anchors.centerIn: parent
                    width: parent.width * 0.7
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: qsTr("Load the image of your QSL card: a scan or the file the printer gave you. "
                               + "Then drop the fields on it and drag them where the boxes are.")
                    color: "#555555"
                    font.pixelSize: 13
                }
            }

            Repeater {
                model: root.fields
                Item {
                    id: chip
                    required property var modelData
                    required property int index
                    readonly property bool isSelected: root.selected === chip.index
                    // Dove si aggancia il testo: a sinistra, in mezzo o a destra
                    // del punto che si e' scelto.
                    readonly property real anchorOffset: modelData.align === "center" ? width / 2
                                                       : modelData.align === "right" ? width : 0
                    width: label.implicitWidth
                    height: label.implicitHeight
                    x: view.width * modelData.x - anchorOffset
                    y: view.height * modelData.y

                    Text {
                        id: label
                        // Il corpo e' in millesimi dell'altezza della cartolina,
                        // come nel disegno vero: cosi' l'anteprima non mente.
                        text: root.fieldValue(chip.modelData) || "—"
                        color: chip.modelData.color
                        font.pixelSize: Math.max(4, view.height * chip.modelData.size / 1000)
                        font.bold: chip.modelData.bold
                    }
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -2
                        color: "transparent"
                        border.width: 1
                        border.color: chip.isSelected ? Theme.primaryColor
                                    : dragger.containsMouse ? Theme.accentColor : "transparent"
                        radius: 2
                    }
                    MouseArea {
                        id: dragger
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.SizeAllCursor
                        drag.target: chip
                        drag.threshold: 0
                        onPressed: root.selected = chip.index
                        onReleased: {
                            root.cards.moveCardField(chip.index,
                                                     (chip.x + chip.anchorOffset) / view.width,
                                                     chip.y / view.height)
                        }
                    }
                }
            }
        }
    }

    // ── Cosa si posa, e com'e' fatto ────────────────────────────────────────
    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        GlassButton {
            text: qsTr("Add a field ▾")
            buttonHeight: 26
            fontPixelSize: 12
            onClicked: keyMenu.popup()
            StyledMenu {
                id: keyMenu
                Instantiator {
                    model: root.cards.cardKeys()
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.label
                        onTriggered: {
                            root.cards.addCardField(modelData.key)
                            root.selected = root.fields.length
                        }
                    }
                    onObjectAdded: (index, object) => keyMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => keyMenu.removeItem(object)
                }
            }
        }

        Text {
            visible: root.selected < 0 || root.selected >= root.fields.length
            Layout.fillWidth: true
            text: qsTr("Click a field on the card to change it.")
            color: Theme.textSecondary
            font.pixelSize: 11
        }

        // Le proprieta' del campo scelto.
        RowLayout {
            id: props
            readonly property var field: root.selected >= 0 && root.selected < root.fields.length
                                         ? root.fields[root.selected] : null
            visible: props.field !== null
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: props.field ? (props.field.key === "text" ? qsTr("Free text") : props.field.key) : ""
                color: Theme.primaryColor
                font.family: Theme.monoFamily
                font.pixelSize: 12
                font.bold: true
            }
            StyledTextField {
                visible: props.field && props.field.key === "text"
                Layout.preferredWidth: 160
                text: props.field ? props.field.text : ""
                onEditingFinished: root.cards.updateCardField(root.selected, { "text": text })
            }
            Text { text: qsTr("Size"); color: Theme.textSecondary; font.pixelSize: 11 }
            Slider {
                Layout.preferredWidth: 150
                from: 10
                to: 200
                stepSize: 1
                value: props.field ? props.field.size : 40
                onMoved: root.cards.updateCardField(root.selected, { "size": Math.round(value) })
            }
            ToggleSwitch {
                text: qsTr("Bold")
                checked: props.field ? props.field.bold : true
                onToggled: root.cards.updateCardField(root.selected, { "bold": checked })
            }
            StyledComboBox {
                Layout.preferredWidth: 110
                readonly property var codes: ["left", "center", "right"]
                model: [qsTr("left"), qsTr("centre"), qsTr("right")]
                currentIndex: props.field ? codes.indexOf(props.field.align) : 0
                onActivated: root.cards.updateCardField(root.selected, { "align": codes[currentIndex] })
            }
            Repeater {
                model: ["#000000", "#ffffff", "#c00000", "#0050c0"]
                Rectangle {
                    required property string modelData
                    width: 20
                    height: 20
                    radius: 3
                    color: modelData
                    border.width: 1
                    border.color: Theme.borderColor
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.cards.updateCardField(root.selected, { "color": modelData })
                    }
                }
            }
            GlassButton {
                text: qsTr("Remove")
                tone: Theme.errorColor
                buttonHeight: 24
                fontPixelSize: 11
                onClicked: { root.cards.removeCardField(root.selected); root.selected = -1 }
            }
        }
    }

    // ── Stampare ────────────────────────────────────────────────────────────
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Text {
            text: root.selectedIds.length > 0
                  ? qsTr("%n chosen QSO", "", root.selectedIds.length)
                  : qsTr("the whole queue")
            color: Theme.textSecondary
            font.pixelSize: 12
        }
        Item { Layout.fillWidth: true }
        LabeledField {
            label: qsTr("Cards per sheet")
            StyledComboBox {
                id: perPageBox
                Layout.preferredWidth: 90
                readonly property var values: [1, 2, 4]
                model: ["1", "2", "4"]
                currentIndex: 1
            }
        }
        GlassButton {
            Layout.alignment: Qt.AlignBottom
            text: qsTr("Write the PDF…")
            tone: Theme.primaryColor
            filled: true
            buttonHeight: 28
            onClicked: pdfDialog.open()
        }
        GlassButton {
            Layout.alignment: Qt.AlignBottom
            text: qsTr("One PNG each…")
            buttonHeight: 28
            onClicked: pngDialog.open()
        }
    }

    FileDialog {
        id: templateDialog
        title: qsTr("The image of your QSL card")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"), qsTr("All files (*)")]
        onAccepted: root.cards.setCardTemplate(selectedFile)
    }
    FileDialog {
        id: pdfDialog
        title: qsTr("QSL cards")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        currentFile: "file:decodxlog-qsl-cards.pdf"
        nameFilters: [qsTr("PDF files (*.pdf)")]
        onAccepted: root.cards.writeCardsPdf(selectedFile, root.selectedIds,
                                             perPageBox.values[perPageBox.currentIndex])
    }
    FolderDialog {
        id: pngDialog
        title: qsTr("Where to put the cards")
        onAccepted: root.cards.writeCardsPng(selectedFolder, root.selectedIds)
    }
}
