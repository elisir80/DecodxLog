// DecoLog — a destra: cosa sa il log del nominativo che si sta lavorando.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    readonly property var wb: decolog.workedBefore
    readonly property bool hasCall: decolog.lookupCall.length > 0

    title: qsTr("Call info")

    component Key: Text {
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSize - 2
        font.bold: true
        font.capitalization: Font.AllUppercase
    }
    component Value: Text {
        Layout.fillWidth: true
        color: Theme.textPrimary
        font.family: Theme.monoFamily
        wrapMode: Text.Wrap
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: root.hasCall ? decolog.lookupCall : qsTr("No callsign")
            color: root.hasCall ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontSize + 12
            font.bold: true
            elide: Text.ElideRight
        }

        // Nuovo o gia' lavorato: la prima domanda di ogni operatore.
        Rectangle {
            visible: root.hasCall
            Layout.fillWidth: true
            implicitHeight: badge.implicitHeight + 12
            radius: 6
            color: root.wb.count > 0 ? Theme.glassOverlay : Theme.rowMatchBg
            border.width: 1
            border.color: root.wb.count > 0 ? Theme.glassBorder : Theme.rowMatchBorder
            Text {
                id: badge
                anchors.centerIn: parent
                text: root.wb.count > 0 ? qsTr("Worked %n time(s)", "", root.wb.count)
                                        : qsTr("New station")
                color: root.wb.count > 0 ? Theme.textPrimary : Theme.accentColor
                font.bold: true
            }
        }

        GridLayout {
            visible: root.hasCall && root.wb.count > 0
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 6

            Key { text: qsTr("Name") }
            Value { text: root.wb.name || "—" }
            Key { text: qsTr("Grid") }
            Value { text: root.wb.gridsquare || "—" }
            Key { text: qsTr("Country") }
            Value { text: root.wb.country || "—" }
            Key { text: qsTr("Last") }
            Value { text: (root.wb.last || "") + "  " + (root.wb.lastBand || "") + " " + (root.wb.lastMode || "") }
            Key { text: qsTr("Bands") }
            Value { text: (root.wb.bands || []).join("  ") }
            Key { text: qsTr("Modes") }
            Value { text: (root.wb.modes || []).join("  ") }
        }

        Item { Layout.fillHeight: true }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Callbook, QSL status and the FT2 Award arrive with the 1.x releases.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSize - 2
        }
    }
}
