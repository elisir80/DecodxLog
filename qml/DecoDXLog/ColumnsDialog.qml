// DecoDXLog — le colonne di una tabella (il log, il cluster): quali, e in
// che ordine.
//
// Come nei log di stazione di una volta (Logger32 e gli altri): a sinistra le
// colonne che si vedono, nell'ordine, con le frecce per spostarle e la ✕ per
// toglierle; a destra tutte le altre, un clic e si aggiungono in fondo. Ci sono
// i campi ADIF piu' usati, e sotto si puo' scrivere il nome di un campo
// qualsiasi — anche quelli che un altro programma ha messo nel file.
//
// Le colonne si spostano anche trascinando l'intestazione nel log.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Popup {
    id: root

    // Chi tiene la disposizione e la salva: ha setLayout(lista),
    // moveColumn(da, a) e defaultLayout; il log anche resetWidths().
    property var panel: null
    // Le colonne mostrate, nell'ordine, e tutte quelle possibili
    // ([{key, title, field}]).
    property var shown: []
    property var all: []
    // Il log accetta un campo ADIF qualsiasi e ha le larghezze; il cluster no.
    property bool allowCustom: true
    property bool allowWidths: true

    // Una finestra sua: il log puo' essere un pannello stretto della lavagna.
    popupType: Popup.Window
    parent: Overlay.overlay
    anchors.centerIn: parent
    // Misura fissa: la lista delle colonne e' lunga e scorre, la finestra no.
    implicitWidth: 760
    implicitHeight: 560
    width: 760
    height: 560
    modal: true
    padding: 14
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: Theme.panelColor; border.color: Theme.glassBorder; radius: 6 }

    function titleOf(key) {
        for (const c of root.all)
            if (c.key === key)
                return c.title
        return key.startsWith("x:") ? key.slice(2) : key
    }
    function fieldOf(key) {
        for (const c of root.all)
            if (c.key === key)
                return c.field
        return ""
    }
    function add(key) {
        const list = root.shown.slice()
        if (list.indexOf(key) < 0)
            list.push(key)
        root.panel.setLayout(list)
    }
    function remove(key) {
        root.panel.setLayout(root.shown.filter(k => k !== key || k === "call"))
    }

    contentItem: ColumnLayout {
        spacing: 10

        Text {
            text: qsTr("COLUMNS")
            color: Theme.secondaryColor
            font.family: Theme.monoFamily
            font.pixelSize: 11
            font.bold: true
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textSecondary
            font.pixelSize: 11
            text: root.allowCustom
                  ? qsTr("On the left the columns you see, in order: ▲▼ move them, ✕ takes one away. On the right "
                         + "all the others, a click adds it at the end. Columns also move by dragging their "
                         + "header in the log. The same layout is used in contest mode.")
                  : qsTr("On the left the columns you see, in order: ▲▼ move them, ✕ takes one away. On the "
                         + "right all the others, a click adds it at the end. Columns also move by dragging "
                         + "their header.")
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // ── Quelle che si vedono, nell'ordine ───────────────────────────
            Rectangle {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                Layout.preferredHeight: 100
                color: Theme.bgMedium
                border.color: Theme.borderSoft
                radius: 4
                ListView {
                    id: shownList
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    model: root.shown
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: shownRow
                        required property string modelData
                        required property int index
                        width: ListView.view.width
                        height: 26
                        color: shownArea.containsMouse ? Theme.glassOverlay : "transparent"
                        radius: 3
                        MouseArea { id: shownArea; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 4
                            anchors.rightMargin: 4
                            spacing: 4
                            PanelControl {
                                glyph: "▲"
                                hint: qsTr("Move left")
                                opacity: shownRow.index > 0 ? 1 : 0.3
                                onClicked: root.panel.moveColumn(shownRow.index, shownRow.index - 1)
                            }
                            PanelControl {
                                glyph: "▼"
                                hint: qsTr("Move right")
                                opacity: shownRow.index < root.shown.length - 1 ? 1 : 0.3
                                onClicked: root.panel.moveColumn(shownRow.index, shownRow.index + 1)
                            }
                            Text {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: root.titleOf(shownRow.modelData)
                                color: Theme.textPrimary
                                font.pixelSize: 12
                            }
                            Text {
                                text: root.fieldOf(shownRow.modelData)
                                color: Theme.textSecondary
                                font.family: Theme.monoFamily
                                font.pixelSize: 10
                            }
                            PanelControl {
                                glyph: "✕"
                                visible: shownRow.modelData !== "call"
                                hint: qsTr("Hide this column")
                                onClicked: root.remove(shownRow.modelData)
                            }
                        }
                    }
                }
            }

            // ── Tutte le altre ──────────────────────────────────────────────
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6
                Text {
                    text: qsTr("Add a column")
                    color: Theme.textSecondary
                    font.pixelSize: 11
                }
                ScrollView {
                    id: addScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 100
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: addFlow.implicitHeight
                    Flow {
                        id: addFlow
                        width: addScroll.availableWidth
                        spacing: 6
                        Repeater {
                            model: root.all.filter(c => root.shown.indexOf(c.key) < 0)
                            delegate: GlassButton {
                                required property var modelData
                                text: modelData.title + (modelData.field && modelData.field !== modelData.title
                                                         ? "  ·  " + modelData.field : "")
                                buttonHeight: 24
                                fontPixelSize: 11
                                onClicked: root.add(modelData.key)
                            }
                        }
                    }
                }
                // Un campo ADIF qualsiasi, per nome.
                RowLayout {
                    Layout.fillWidth: true
                    visible: root.allowCustom
                    spacing: 6
                    StyledTextField {
                        id: customField
                        Layout.fillWidth: true
                        uppercase: true
                        placeholderText: qsTr("Any ADIF field, e.g. APP_LOGGER32_QSO_NUMBER")
                        onAccepted: addCustom.clicked()
                    }
                    GlassButton {
                        id: addCustom
                        text: qsTr("Add")
                        enabled: customField.text.trim().length > 0
                        onClicked: {
                            const name = customField.text.trim().toUpperCase().replace(/[^A-Z0-9_]/g, "")
                            if (name.length > 0)
                                root.add("x:" + name)
                            customField.text = ""
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            GlassButton {
                text: qsTr("Default columns")
                onClicked: root.panel.setLayout(root.panel.defaultLayout)
            }
            GlassButton {
                visible: root.allowWidths
                text: qsTr("Default widths")
                onClicked: root.panel.resetWidths()
            }
            Item { Layout.fillWidth: true }
            GlassButton {
                text: qsTr("Close")
                tone: Theme.primaryColor
                filled: true
                onClicked: root.close()
            }
        }
    }
}
