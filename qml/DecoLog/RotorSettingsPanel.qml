// DecoLog — stazione e sicurezza del gateway: quello che si cambia a caldo
// finisce subito nel suo config.json.
// Copia di `desktop/qml/DecoRotor/SettingsPanel.qml`.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RotorGlass {
    id: panel

    title: qsTr("STAZIONE E SICUREZZA")

    RotorPalette { id: rt }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state

    GridLayout {
        anchors.fill: parent
        columns: 2
        rowSpacing: 10
        columnSpacing: 14

        Text {
            text: qsTr("Nominativo")
            color: rt.textSecondary
            font.pixelSize: rt.fontBody
        }

        RotorField {
            Layout.fillWidth: true
            text: panel.st.callsign || ""
            font.capitalization: Font.AllUppercase
            onEditingFinished: panel.rotor.setSetting("callsign", text)
        }

        Text {
            text: qsTr("Locatore del QTH")
            color: rt.textSecondary
            font.pixelSize: rt.fontBody
        }

        RotorField {
            Layout.fillWidth: true
            text: panel.st.locator || ""
            font.capitalization: Font.AllUppercase
            onEditingFinished: panel.rotor.setSetting("my_locator", text)
        }

        Text {
            text: qsTr("Apertura del lobo: %1°").arg((panel.st.beamwidth || 45).toFixed(0))
            color: rt.textSecondary
            font.pixelSize: rt.fontBody
        }

        Slider {
            id: beamSlider

            Layout.fillWidth: true
            from: 5
            to: 180
            stepSize: 1
            value: panel.st.beamwidth || 45
            onMoved: panel.rotor.setSetting("beamwidth", value)

            background: Rectangle {
                x: beamSlider.leftPadding
                y: beamSlider.topPadding + beamSlider.availableHeight / 2 - height / 2
                width: beamSlider.availableWidth
                height: 4
                radius: 2
                color: rt.bgDeep

                Rectangle {
                    width: beamSlider.visualPosition * parent.width
                    height: parent.height
                    radius: parent.radius
                    color: rt.primary
                }
            }

            handle: Rectangle {
                x: beamSlider.leftPadding + beamSlider.visualPosition * (beamSlider.availableWidth - width)
                y: beamSlider.topPadding + beamSlider.availableHeight / 2 - height / 2
                width: 16
                height: 16
                radius: 8
                color: beamSlider.pressed ? rt.secondary : rt.primary
                border.color: rt.bgDeep
                border.width: 2
            }
        }

        Text {
            text: qsTr("Finecorsa azimut")
            color: rt.textSecondary
            font.pixelSize: rt.fontBody
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            RotorField {
                Layout.fillWidth: true
                text: (panel.st.azMin !== undefined ? panel.st.azMin : 0).toFixed(0)
                validator: DoubleValidator { bottom: -180; top: 720 }
                onEditingFinished: panel.rotor.setLimit("az_min", parseFloat(text))
            }

            Text {
                text: "→"
                color: rt.textDim
            }

            RotorField {
                Layout.fillWidth: true
                text: (panel.st.azMax !== undefined ? panel.st.azMax : 360).toFixed(0)
                validator: DoubleValidator { bottom: -180; top: 720 }
                onEditingFinished: panel.rotor.setLimit("az_max", parseFloat(text))
            }
        }

        Text {
            text: qsTr("Posizione di riposo")
            color: rt.textSecondary
            font.pixelSize: rt.fontBody
        }

        RotorField {
            Layout.fillWidth: true
            text: (panel.st.parkAz || 0).toFixed(0)
            validator: DoubleValidator { bottom: 0; top: 360 }
            onEditingFinished: panel.rotor.setSetting("park_az", parseFloat(text))
        }

        Text {
            text: qsTr("Stop se cade il collegamento")
            color: rt.textSecondary
            font.pixelSize: rt.fontBody
        }

        Switch {
            id: guardSwitch

            checked: panel.st.stopOnClientLoss === true
            onToggled: panel.rotor.setSetting("stop_on_client_loss", checked)

            indicator: Rectangle {
                implicitWidth: 44
                implicitHeight: 22
                x: guardSwitch.leftPadding
                y: guardSwitch.topPadding + guardSwitch.availableHeight / 2 - height / 2
                radius: 11
                color: guardSwitch.checked ? Qt.rgba(0.20, 0.83, 0.60, 0.30) : rt.bgDeep
                border.color: guardSwitch.checked ? rt.accent : rt.borderSoft
                border.width: 1

                Rectangle {
                    x: guardSwitch.checked ? parent.width - width - 3 : 3
                    y: 3
                    width: 16
                    height: 16
                    radius: 8
                    color: guardSwitch.checked ? rt.accent : rt.textDim

                    Behavior on x {
                        XAnimator { duration: 120 }
                    }
                }
            }

            contentItem: Text {
                text: guardSwitch.checked ? qsTr("attivo") : qsTr("disattivato")
                color: rt.textSecondary
                font.pixelSize: rt.fontSmall
                leftPadding: guardSwitch.indicator.width + 10
                verticalAlignment: Text.AlignVCenter
            }
        }

        Text {
            text: qsTr("Tolleranza di arrivo")
            color: rt.textSecondary
            font.pixelSize: rt.fontBody
        }

        RotorField {
            Layout.fillWidth: true
            text: (panel.st.tolerance || 1).toFixed(1)
            validator: DoubleValidator { bottom: 0.1; top: 10 }
            onEditingFinished: panel.rotor.setSetting("tolerance", parseFloat(text.replace(",", ".")))
        }

        Item {
            Layout.columnSpan: 2
            Layout.fillHeight: true
        }

        Text {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            text: qsTr("Porta seriale, modello e porta dei decode si cambiano all'avvio del gateway.")
            color: rt.textDim
            font.pixelSize: rt.fontSmall
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            text: qsTr("Le modifiche vengono salvate subito in config.json.")
            color: rt.textDim
            font.pixelSize: rt.fontSmall
            wrapMode: Text.WordWrap
        }
    }
}
