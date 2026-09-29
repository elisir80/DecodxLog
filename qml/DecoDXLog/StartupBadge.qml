// DecoDXLog — il biglietto d'avvio: nome, versione, con che cosa e' scritto,
// chi lo fa, e un caffe' per chi vuole offrirlo (come in Decodium 4).
//
// Copre la finestra per dieci secondi al massimo, poi sfuma; "Avvia", Esc o
// Invio lo chiudono subito. Il programma sotto intanto si carica: il biglietto
// non ferma niente. Nelle prove (--show, --grab) non compare, se non chiesto.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Rectangle {
    id: root

    property int duration: 10000
    readonly property string coffeeUrl: "https://buymeacoffee.com/iu8lmc"
    // Il giallo del caffe': su tema chiaro va scurito, se no non si legge.
    readonly property color coffeeTone: Theme.isLightTheme ? "#B07A00" : "#FFD740"
    readonly property color coffeeText: Theme.isLightTheme ? "#6B4A00" : "#FFE082"

    signal finished()

    function close() {
        if (!fadeOut.running)
            fadeOut.start()
    }

    color: Theme.bgDeep
    z: 9999
    focus: true
    opacity: 0
    Keys.onEscapePressed: root.close()
    Keys.onReturnPressed: root.close()
    Keys.onEnterPressed: root.close()

    // I clic non passano alla finestra sotto.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
    }

    NumberAnimation on opacity {
        from: 0; to: 1
        duration: 350
        easing.type: Easing.OutQuad
    }
    NumberAnimation {
        id: fadeOut
        target: root
        property: "opacity"
        to: 0
        duration: 400
        easing.type: Easing.InQuad
        onFinished: root.finished()
    }
    Timer {
        interval: root.duration
        running: true
        onTriggered: root.close()
    }

    // Un badge alla maniera di shields.io: a sinistra la voce, a destra il valore.
    component Shield: Row {
        id: shield
        property string key: ""
        property string value: ""
        property color tone: Theme.primaryColor
        height: 24
        Rectangle {
            width: keyText.implicitWidth + 18
            height: parent.height
            radius: 4
            color: Theme.isLightTheme ? "#555c66" : "#3a404a"
            // L'angolo destro dritto, attaccato al valore.
            Rectangle { anchors.right: parent.right; width: 4; height: parent.height; color: parent.color }
            Text {
                id: keyText
                anchors.centerIn: parent
                text: shield.key
                color: "#ffffff"
                font.pixelSize: 12
            }
        }
        Rectangle {
            width: valueText.implicitWidth + 18
            height: parent.height
            radius: 4
            color: shield.tone
            Rectangle { anchors.left: parent.left; width: 4; height: parent.height; color: parent.color }
            Text {
                id: valueText
                anchors.centerIn: parent
                text: shield.value
                color: "#ffffff"
                font.pixelSize: 12
                font.bold: true
            }
        }
    }

    // Una riga dei crediti: nome, nominativo, che cosa fa.
    component Credit: Row {
        property string name: ""
        property string call: ""
        property string role: ""
        spacing: 8
        Layout.alignment: Qt.AlignHCenter
        Text {
            text: parent.name
            color: Theme.textPrimary
            font.pixelSize: 14
            font.bold: true
        }
        Text {
            text: parent.call
            color: Theme.secondaryColor
            font.family: Theme.monoFamily
            font.pixelSize: 14
            font.bold: true
        }
        Text {
            text: "· " + parent.role
            color: Theme.textSecondary
            font.pixelSize: 13
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, column.implicitHeight + 60)
        interactive: contentHeight > height
        clip: true

        ColumnLayout {
            id: column
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(30, (parent.height - implicitHeight) / 2)
            spacing: 0

            // Il logo in un cerchio, col bordo che respira.
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 132
                implicitHeight: 132
                radius: 66
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.primaryColor }
                    GradientStop { position: 1.0; color: Theme.secondaryColor }
                }
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: "transparent"
                    border.color: Theme.secondaryColor
                    border.width: 3
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 900 }
                        NumberAnimation { to: 1.0; duration: 900 }
                    }
                }
                Image {
                    anchors.centerIn: parent
                    source: "qrc:/decolog/decodxlog.png"
                    sourceSize.width: 92
                    sourceSize.height: 92
                    width: 92
                    height: 92
                    smooth: true
                    mipmap: true
                }
            }

            Item { implicitHeight: 24 }

            Text {
                Layout.alignment: Qt.AlignHCenter
                textFormat: Text.StyledText
                text: "DECO<font color=\"" + Theme.secondaryColor + "\">DX</font>LOG"
                color: Theme.textPrimary
                font.pixelSize: 46
                font.weight: Font.ExtraBold
                font.letterSpacing: 6
            }

            Item { implicitHeight: 8 }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10
                Pill {
                    text: decolog.version
                    tone: Theme.secondaryColor
                    fontPixelSize: 12
                }
                Text {
                    text: qsTr("The station logbook of the Decodium family")
                    color: Theme.secondaryColor
                    font.pixelSize: 15
                    font.italic: true
                }
            }

            Item { implicitHeight: 18 }

            // Con che cosa e' fatto.
            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10
                Shield { key: qsTr("language"); value: "C++20"; tone: "#00599C" }
                Shield { key: qsTr("interface"); value: "Qt 6 · QML"; tone: "#2E9E48" }
                Shield { key: qsTr("licence"); value: "GPL-3.0"; tone: "#3572C6" }
            }

            Item { implicitHeight: 26 }

            // Chi lo fa.
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 6
                Credit { name: "Martino Merola"; call: "IU8LMC"; role: qsTr("author and developer") }
                Credit { name: "Salvatore Raccampo"; call: "9H1SR"; role: qsTr("developer") }
                Credit { name: "Filippo Ricci"; call: "G0YCE"; role: qsTr("support and social media manager") }
            }

            Item { implicitHeight: 30 }

            // Il tempo che resta prima di chiudersi da solo.
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 320
                implicitHeight: 3
                radius: 2
                color: Theme.borderSoft
                Rectangle {
                    height: parent.height
                    radius: parent.radius
                    color: Theme.secondaryColor
                    NumberAnimation on width {
                        from: 0; to: 320
                        duration: root.duration - 200
                        easing.type: Easing.InOutQuad
                    }
                }
            }

            Item { implicitHeight: 26 }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 14

                Rectangle {
                    implicitWidth: Math.max(210, coffeeLabel.implicitWidth + 36)
                    implicitHeight: 40
                    radius: 8
                    color: Qt.rgba(root.coffeeTone.r, root.coffeeTone.g, root.coffeeTone.b,
                                   coffeeMouse.containsMouse ? 0.32 : 0.16)
                    border.color: root.coffeeTone
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Text {
                        id: coffeeLabel
                        anchors.centerIn: parent
                        text: qsTr("☕  Buy me a coffee")
                        color: root.coffeeText
                        font.pixelSize: 14
                        font.bold: true
                    }
                    MouseArea {
                        id: coffeeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally(root.coffeeUrl)
                    }
                    ToolTip.visible: coffeeMouse.containsMouse
                    ToolTip.text: root.coffeeUrl
                }

                Rectangle {
                    implicitWidth: Math.max(150, startLabel.implicitWidth + 36)
                    implicitHeight: 40
                    radius: 8
                    color: Qt.rgba(Theme.secondaryColor.r, Theme.secondaryColor.g, Theme.secondaryColor.b,
                                   startMouse.containsMouse ? 0.45 : 0.26)
                    border.color: Theme.secondaryColor
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Text {
                        id: startLabel
                        anchors.centerIn: parent
                        text: qsTr("Start  ▶")
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.bold: true
                    }
                    MouseArea {
                        id: startMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }
            }

            Item { implicitHeight: 30 }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Free software under GPL-3.0 · sources on github.com/iu8lmc/DecoDXLog")
                color: Theme.textSecondary
                opacity: 0.7
                font.pixelSize: 11
            }
        }
    }
}
