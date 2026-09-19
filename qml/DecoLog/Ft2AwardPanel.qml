// DecoLog — FT2 Award: DXCC e locatori lavorati in FT2, e quanti confermati.
//
// I traguardi (100 DXCC, 500 locatori) sono quelli del mockup: vanno allineati
// al regolamento dell'award quando sara' pubblicato.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    signal detailsRequested()

    readonly property int dxccTarget: 100
    readonly property int gridTarget: 500
    readonly property var award: decolog.ft2Award

    title: qsTr("FT2 Award")
    dotColor: Theme.accentColor
    implicitHeight: Theme.panelHeight + meters.implicitHeight + 22
    headerTools: [
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Details ▸")
            color: detailsArea.containsMouse ? Theme.primaryColor : Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 11
            MouseArea {
                id: detailsArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.detailsRequested()
            }
        }
    ]

    // Se il pannello viene stretto, le barre si scorrono invece di sparire.
    ScrollView {
        id: scroller
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
        id: meters
        width: scroller.availableWidth
        spacing: 8

        MeterBar {
            Layout.fillWidth: true
            label: qsTr("DXCC on FT2")
            valueText: (root.award.dxccWorked || 0) + " / " + root.dxccTarget
            fraction: (root.award.dxccWorked || 0) / root.dxccTarget
            barColor: Theme.accentColor
        }
        MeterBar {
            Layout.fillWidth: true
            label: qsTr("Grids on FT2")
            valueText: (root.award.gridsWorked || 0) + " / " + root.gridTarget
            fraction: (root.award.gridsWorked || 0) / root.gridTarget
            barColor: Theme.secondaryColor
        }
        MeterBar {
            Layout.fillWidth: true
            label: qsTr("Confirmed (LoTW)")
            valueText: String(root.award.dxccConfirmed || 0)
            fraction: (root.award.dxccConfirmed || 0) / root.dxccTarget
            barColor: Theme.primaryColor
        }
    }
}
}
