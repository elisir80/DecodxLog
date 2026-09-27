// DecoDXLog — il keyer vocale: otto messaggi sui tasti F1-F8. Un clic li manda
// in aria (PTT compreso), il pallino rosso li registra dal microfono, la
// cartella prende un WAV gia' fatto. In fonia i tasti F1-F8 della tastiera
// fanno lo stesso; Esc ferma.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    readonly property var dvk: decolog.dvk
    property int importSlot: -1
    property string lastError: ""

    title: qsTr("Voice keyer")
    dotColor: root.dvk.playing >= 0 ? Theme.errorColor : root.dvk.recording >= 0 ? Theme.warningColor : Theme.accentColor
    padding: 8

    FileDialog {
        id: wavDialog
        title: qsTr("A WAV message")
        nameFilters: [qsTr("WAV audio (*.wav)")]
        onAccepted: {
            const error = root.dvk.importFile(root.importSlot, selectedFile)
            root.lastError = error
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        GridLayout {
            Layout.fillWidth: true
            columns: root.width > 520 ? 4 : 2
            columnSpacing: 6
            rowSpacing: 6
            Repeater {
                model: root.dvk.messages
                Rectangle {
                    id: slotBox
                    required property var modelData
                    readonly property bool isPlaying: root.dvk.playing === modelData.slot
                    readonly property bool isRecording: root.dvk.recording === modelData.slot
                    Layout.fillWidth: true
                    implicitHeight: 58
                    radius: 6
                    color: isPlaying ? Qt.rgba(Theme.errorColor.r, Theme.errorColor.g, Theme.errorColor.b, 0.25)
                         : isRecording ? Qt.rgba(Theme.warningColor.r, Theme.warningColor.g, Theme.warningColor.b, 0.25)
                         : Theme.bgMedium
                    border.color: modelData.present ? Theme.accentColor : Theme.borderSoft
                    opacity: modelData.present || isRecording ? 1 : 0.7
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: slotBox.modelData.key; color: Theme.textSecondary; font.family: Theme.monoFamily; font.pixelSize: 10 }
                            TextInput {
                                Layout.fillWidth: true
                                text: slotBox.modelData.label
                                color: Theme.textPrimary
                                font.family: Theme.monoFamily
                                font.pixelSize: 13
                                font.bold: true
                                selectByMouse: true
                                onEditingFinished: root.dvk.setLabel(slotBox.modelData.slot, text)
                            }
                        }
                        RowLayout {
                            spacing: 4
                            GlassButton {
                                text: slotBox.isPlaying ? "■" : "▶"
                                buttonHeight: 20
                                fontPixelSize: 10
                                enabled: slotBox.modelData.present && root.dvk.recording < 0
                                onClicked: slotBox.isPlaying ? root.dvk.stop() : root.dvk.play(slotBox.modelData.slot)
                            }
                            GlassButton {
                                text: slotBox.isRecording ? qsTr("stop") : "●"
                                tone: Theme.errorColor
                                buttonHeight: 20
                                fontPixelSize: 10
                                enabled: root.dvk.playing < 0 && (root.dvk.recording < 0 || slotBox.isRecording)
                                onClicked: slotBox.isRecording ? root.dvk.stopRecording() : root.dvk.startRecording(slotBox.modelData.slot)
                            }
                            GlassButton {
                                text: "📂"
                                buttonHeight: 20
                                fontPixelSize: 10
                                onClicked: { root.importSlot = slotBox.modelData.slot; wavDialog.open() }
                            }
                            Text {
                                visible: slotBox.modelData.present
                                text: slotBox.modelData.seconds.toFixed(1) + " s"
                                color: Theme.textSecondary
                                font.family: Theme.monoFamily
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Audio to the radio")
                StyledComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("System default")].concat(root.dvk.outputs)
                    currentIndex: Math.max(0, root.dvk.outputs.indexOf(root.dvk.output) + 1)
                    onActivated: root.dvk.output = currentIndex === 0 ? "" : currentText
                }
            }
            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Microphone")
                StyledComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("System default")].concat(root.dvk.inputs)
                    currentIndex: Math.max(0, root.dvk.inputs.indexOf(root.dvk.input) + 1)
                    onActivated: root.dvk.input = currentIndex === 0 ? "" : currentText
                }
            }
        }
        RowLayout {
            spacing: 12
            ToggleSwitch {
                text: qsTr("PTT from the radio")
                checked: root.dvk.usePtt
                onToggled: root.dvk.usePtt = checked
            }
            Text { text: qsTr("Repeat CQ every"); color: Theme.textSecondary; font.pixelSize: 12 }
            StyledComboBox {
                Layout.preferredWidth: 90
                readonly property var values: [0, 3, 5, 8, 10, 15]
                model: [qsTr("never"), "3 s", "5 s", "8 s", "10 s", "15 s"]
                currentIndex: Math.max(0, values.indexOf(root.dvk.repeatSeconds))
                onActivated: root.dvk.repeatSeconds = values[currentIndex]
            }
            GlassButton {
                text: qsTr("CQ loop")
                enabled: root.dvk.repeatSeconds > 0 && root.dvk.hasMessage(0)
                onClicked: root.dvk.playRepeating(0)
            }
            GlassButton {
                text: qsTr("Stop")
                tone: Theme.errorColor
                onClicked: root.dvk.stop()
            }
        }
        Text {
            visible: root.lastError.length > 0
            text: root.lastError
            color: Theme.warningColor
            font.pixelSize: 11
        }
        Item { Layout.fillHeight: true }
    }
}
