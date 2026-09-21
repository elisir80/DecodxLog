// DecoDXLog — l'andamento della posizione nel tempo: la traccia e' storia, non
// animazione, e si ridisegna quando arriva un blocco nuovo di campioni.
// Copia di `desktop/qml/DecoRotor/HistoryStrip.qml`.
import QtQuick
import QtQuick.Shapes

RotorGlass {
    id: strip

    title: qsTr("POSITION OVER TIME")

    RotorPalette { id: rt }

    readonly property var samples: decolog.rotor.history

    Item {
        anchors.fill: parent

        Repeater {
            model: [0, 90, 180, 270, 360]

            Item {
                required property int modelData

                anchors.left: parent.left
                anchors.right: parent.right
                y: parent.height * (1 - modelData / 360)
                height: 1

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: label.left
                    anchors.rightMargin: 6
                    height: 1
                    color: rt.borderSoft
                }

                Text {
                    id: label

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: parent.modelData + "°"
                    color: rt.textDim
                    font.pixelSize: 9
                    font.family: rt.monoFamily
                }
            }
        }

        Shape {
            id: plot

            anchors.fill: parent
            anchors.rightMargin: 30
            visible: strip.samples.length > 1

            ShapePath {
                strokeColor: rt.primary
                strokeWidth: 2
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap

                PathPolyline {
                    path: {
                        const points = []
                        const data = strip.samples
                        if (data.length < 2)
                            return points
                        const step = plot.width / (data.length - 1)
                        for (let i = 0; i < data.length; ++i) {
                            const az = data[i].az === null || data[i].az === undefined ? 0 : data[i].az
                            points.push(Qt.point(i * step, plot.height * (1 - az / 360)))
                        }
                        return points
                    }
                }
            }
        }

        Text {
            anchors.centerIn: parent
            visible: strip.samples.length <= 1
            text: qsTr("waiting for readings…")
            color: rt.textDim
            font.pixelSize: rt.fontSmall
        }
    }
}
