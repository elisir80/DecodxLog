// DecoDXLog — l'orologio mondiale nella barra in basso: mini-mappa, UTC coi
// secondi, il QTH con alba e tramonto, due citta' DX. Un pulsante vero: si
// apre con un clic, con Invio o con lo Spazio.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI
import DecoDXLog.Native

AbstractButton {
    id: root

    readonly property var clock: decolog.worldClock
    readonly property var home: clock.home
    readonly property bool hasHome: home && home.id !== undefined

    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Open the world clock")
    padding: 3
    leftPadding: 4
    rightPadding: 10
    implicitHeight: 42
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding

    background: Rectangle {
        radius: 8
        color: root.hovered || root.visualFocus ? Theme.panelHeader : Theme.bgDeep
        border.color: root.visualFocus ? Theme.secondaryColor : Theme.borderColor
        border.width: root.visualFocus ? 2 : 1
    }

    component Divider: Rectangle {
        Layout.preferredWidth: 1
        Layout.preferredHeight: 24
        color: Theme.borderColor
    }
    component Small: Text {
        color: Theme.textSecondary
        font.family: Theme.monoFamily
        font.pixelSize: 11
    }
    component Value: Text {
        color: Theme.textPrimary
        font.family: Theme.monoFamily
        font.pixelSize: 13
    }

    contentItem: RowLayout {
        spacing: 10

        // La mini-mappa: fasce della notte, grayline e il Sole.
        Rectangle {
            Layout.preferredWidth: 72
            Layout.preferredHeight: 36
            Layout.maximumHeight: 36
            Layout.alignment: Qt.AlignVCenter
            radius: 3
            clip: true
            color: "#0F1B2D"
            WorldMapItem {
                anchors.fill: parent
                detailed: false
                time: root.clock.now
                graylineColor: Theme.secondaryColor
            }
            Rectangle {
                readonly property var sun: root.clock.subSolar
                width: 5
                height: 5
                radius: 2.5
                color: "#FFE9A8"
                x: (sun.lon + 180) / 360 * parent.width - width / 2
                y: (90 - sun.lat) / 180 * parent.height - height / 2
            }
        }

        Small { text: "UTC" }
        Text {
            text: root.clock.utcClock
            color: Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: 16
        }

        Divider { visible: root.hasHome }
        Small { visible: root.hasHome; text: root.hasHome ? root.home.locator : "" }
        Value { visible: root.hasHome; text: root.hasHome ? root.home.time : "" }
        WorldClockIcon { visible: root.hasHome; kind: "sunrise"; color: Theme.textSecondary; width: 14; height: 14 }
        Value { visible: root.hasHome; text: root.hasHome ? (root.home.polar || root.home.rise) : "" }
        WorldClockIcon { visible: root.hasHome && !root.home.polar; kind: "sunset"; color: Theme.textSecondary; width: 14; height: 14 }
        Value { visible: root.hasHome && !root.home.polar; text: root.hasHome ? root.home.set : "" }

        Divider { visible: root.clock.footerCities.length > 0 }
        Repeater {
            model: root.clock.footerCities
            Row {
                required property var modelData
                spacing: 5
                Small { text: modelData.short; anchors.verticalCenter: parent.verticalCenter }
                Value { text: modelData.time }
            }
        }

        WorldClockIcon { kind: "expand"; color: Theme.secondaryColor; width: 14; height: 14 }
    }
}
