// DecoDXLog — il display del control box: angolo a caratteri grandi, verso di
// rotazione, memoria corrispondente alla direzione attuale e stato in chiaro.
// Copia di `desktop/qml/DecoRotor/DisplayPanel.qml`.
import QtQuick
import QtQuick.Layouts

RotorGlass {
    id: panel

    RotorPalette { id: rt }

    readonly property var rotor: decolog.rotor
    readonly property var st: rotor.state
    readonly property bool hasPosition: st.connected === true
    readonly property real azTarget: st.azTarget !== undefined && st.azTarget >= 0 ? st.azTarget : -1
    readonly property real elTarget: st.elTarget !== undefined && st.elTarget >= 0 ? st.elTarget : -1

    // Se l'antenna e' ferma sopra una memoria, quella memoria ha un nome: e'
    // piu' parlante di un numero, esattamente come sul frontalino.
    readonly property string standingOn: {
        if (!panel.hasPosition)
            return ""
        const memories = rotor.presets
        for (let i = 0; i < memories.length; ++i) {
            const gap = Math.abs(((memories[i].az - (st.az || 0) + 540) % 360) - 180)
            if (gap <= 3.0)
                return memories[i].name
        }
        return ""
    }

    readonly property string activity: {
        if (!st.connected)
            return qsTr("control box assente")
        if (st.moving && panel.azTarget >= 0)
            return qsTr("in rotazione verso %1°").arg(panel.azTarget.toFixed(1))
        if (st.moving)
            return qsTr("in rotazione")
        return qsTr("fermo")
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: rt.spacing

            ColumnLayout {
                Layout.alignment: Qt.AlignBottom
                spacing: 6

                Rectangle {
                    Layout.preferredWidth: 168
                    Layout.preferredHeight: 40
                    radius: 8
                    color: rt.bgElevated
                    border.color: panel.standingOn.length > 0 ? rt.accent : rt.borderSoft
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 16
                        text: panel.standingOn.length > 0 ? panel.standingOn.toUpperCase()
                                                          : qsTr("DIREZIONE LIBERA")
                        color: panel.standingOn.length > 0 ? rt.textPrimary : rt.textDim
                        font.pixelSize: rt.fontBody
                        font.bold: true
                        font.letterSpacing: 1.0
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }

                Text {
                    Layout.preferredWidth: 168
                    text: panel.activity
                    color: !panel.st.connected ? rt.danger
                         : panel.st.moving ? rt.warning
                         : rt.textDim
                    font.pixelSize: rt.fontSmall
                    wrapMode: Text.WordWrap
                }
            }

            Item { Layout.fillWidth: true }

            RotorReadout {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                label: qsTr("AZIMUT")
                value: panel.st.az || 0
                target: panel.azTarget
                valid: panel.hasPosition
                moving: panel.st.moving === true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: rt.spacing

            RotorSense {
                Layout.fillWidth: true
                Layout.minimumHeight: implicitHeight
                Layout.alignment: Qt.AlignBottom
                sense: panel.rotor.rotationSense
                moving: panel.st.moving === true
            }

            RotorReadout {
                Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                visible: panel.st.hasEl === true
                label: qsTr("ELEVAZIONE")
                value: panel.st.el || 0
                target: panel.elTarget
                valid: panel.st.hasEl === true
                moving: panel.st.moving === true
                digitSize: 34
            }
        }
    }
}
