// DecoLog — l'elenco completo delle memorie del gateway.
// Copia di `desktop/qml/DecoRotor/PresetPanel.qml`.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RotorGlass {
    id: panel

    title: qsTr("MEMORIE")

    RotorPalette { id: rt }

    readonly property var rotor: decolog.rotor

    function store() {
        if (nameField.text.trim().length === 0)
            return
        panel.rotor.savePresetHere(nameField.text)
        nameField.clear()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: list

                model: panel.rotor.presets
                spacing: 4
                reuseItems: true

                delegate: Rectangle {
                    id: row

                    required property var modelData

                    width: ListView.view.width
                    height: 40
                    radius: 8
                    color: hover.hovered ? rt.bgHeader : rt.bgElevated
                    border.color: rt.borderSoft
                    border.width: 1

                    HoverHandler { id: hover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 6
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: row.modelData.name
                            color: rt.textPrimary
                            font.pixelSize: rt.fontBody
                            elide: Text.ElideRight
                        }

                        Text {
                            // Un asse assente arriva come null dal gateway e
                            // come undefined una volta convertito: valgono
                            // entrambi "questa memoria non ha elevazione".
                            readonly property bool hasElevation:
                                row.modelData.el !== null && row.modelData.el !== undefined

                            text: !hasElevation
                                  ? row.modelData.az.toFixed(1) + "°"
                                  : qsTr("%1° / %2°").arg(row.modelData.az.toFixed(1))
                                                     .arg(row.modelData.el.toFixed(1))
                            color: rt.textSecondary
                            font.pixelSize: rt.fontSmall
                            font.family: rt.monoFamily
                        }

                        RotorButton {
                            Layout.preferredWidth: 64
                            Layout.preferredHeight: 28
                            text: qsTr("VAI")
                            kind: 1
                            onClicked: panel.rotor.recallPreset(row.modelData.name)
                        }

                        RotorButton {
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 28
                            text: "×"
                            onClicked: panel.rotor.deletePreset(row.modelData.name)
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: panel.rotor.presets.length === 0
            text: qsTr("Nessuna memoria: dai un nome alla direzione attuale e salvala.")
            color: rt.textDim
            font.pixelSize: rt.fontSmall
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            RotorField {
                id: nameField

                Layout.fillWidth: true
                placeholderText: qsTr("Nome della memoria")
                onAccepted: panel.store()
            }

            RotorButton {
                Layout.preferredWidth: 110
                text: qsTr("SALVA QUI")
                enabled: panel.rotor.state.connected === true && nameField.text.trim().length > 0
                onClicked: panel.store()
            }
        }
    }
}
