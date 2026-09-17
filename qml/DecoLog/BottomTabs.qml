// DecoLog — il pannello inferiore a schede: award, statistiche, QSL, attività.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    // 0 Awards · 1 Statistics · 2 QSL Upload · 3 Activity log
    property int currentTab: 3
    signal awardRequested(string id)

    padding: 0
    headerLeading: [
        Row {
            spacing: 2
            Repeater {
                model: [qsTr("Awards"), qsTr("Statistics"), qsTr("QSL Upload"), qsTr("Activity log")]
                TabChip {
                    required property string modelData
                    required property int index
                    text: modelData
                    active: root.currentTab === index
                    onClicked: root.currentTab = index
                }
            }
        }
    ]
    headerTools: [
        GlassButton {
            visible: root.currentTab === 3
            text: qsTr("Clear")
            buttonHeight: 24
            fontPixelSize: 11
            onClicked: decolog.clearActivity()
        }
    ]
    showDot: false

    function categoryColor(category) {
        switch (category) {
        case "UDP": return Theme.accentColor
        case "SYNC": return Theme.secondaryColor
        case "LOTW": return Theme.primaryColor
        case "LOG": return Theme.primaryColor
        case "IMPORT":
        case "EXPORT": return Theme.secondaryColor
        default: return Theme.textSecondary
        }
    }

    component Cell: Text {
        property bool heading: false
        color: heading ? Theme.textSecondary : Theme.textPrimary
        font.family: Theme.monoFamily
        font.pixelSize: 12
        font.bold: !heading
    }

    component BarRow: RowLayout {
        property string key: ""
        property int count: 0
        property int maximum: 1
        property color barColor: Theme.primaryColor
        spacing: 8
        Text {
            Layout.preferredWidth: 52
            text: parent.key
            color: Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 8
            radius: 4
            color: Theme.bgMedium
            Rectangle {
                width: parent.width * (parent.parent.count / Math.max(1, parent.parent.maximum))
                height: parent.height
                radius: 4
                color: parent.parent.barColor
            }
        }
        Text {
            Layout.preferredWidth: 52
            horizontalAlignment: Text.AlignRight
            text: parent.count.toLocaleString(Qt.locale("en_US"), "f", 0)
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: 12
        }
    }

    StackLayout {
        anchors.fill: parent
        anchors.margins: 10
        anchors.topMargin: 6
        currentIndex: root.currentTab

        // ── Awards ──────────────────────────────────────────────────────────
        Flow {
            spacing: 8
            Repeater {
                model: decolog.awardSummary
                Rectangle {
                    id: tile
                    required property var modelData
                    width: 150
                    height: 58
                    radius: 5
                    color: tileArea.containsMouse ? Theme.glassOverlay : Theme.bgMedium
                    border.width: 1
                    border.color: modelData.id === "ft2" ? Theme.accentColor : Theme.borderSoft
                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 2
                        Text {
                            text: tile.modelData.title
                            color: tile.modelData.id === "ft2" ? Theme.accentColor : Theme.secondaryColor
                            font.family: Theme.monoFamily
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: tile.modelData.worked + (tile.modelData.total > 0 ? " / " + tile.modelData.total : "")
                            color: Theme.textPrimary
                            font.family: Theme.monoFamily
                            font.pixelSize: 15
                            font.bold: true
                        }
                        Text {
                            text: qsTr("%1 confirmed").arg(tile.modelData.confirmed)
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 10
                        }
                    }
                    MouseArea {
                        id: tileArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.awardRequested(tile.modelData.id)
                    }
                }
            }
        }

        // ── Statistiche ─────────────────────────────────────────────────────
        RowLayout {
            spacing: 24
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: 4
                SectionTitle { text: qsTr("By band") }
                Repeater {
                    model: decolog.bandStats.slice(0, 8)
                    BarRow {
                        required property var modelData
                        Layout.fillWidth: true
                        key: modelData.key
                        count: modelData.count
                        maximum: Math.max.apply(null, decolog.bandStats.map(r => r.count))
                        barColor: Theme.primaryColor
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: 4
                SectionTitle { text: qsTr("By mode") }
                Repeater {
                    model: decolog.modeStats.slice(0, 8)
                    BarRow {
                        required property var modelData
                        Layout.fillWidth: true
                        key: modelData.key
                        count: modelData.count
                        maximum: Math.max.apply(null, decolog.modeStats.map(r => r.count))
                        barColor: modelData.key === "FT2" ? Theme.accentColor : Theme.secondaryColor
                    }
                }
            }
        }

        // ── QSL Upload ──────────────────────────────────────────────────────
        ColumnLayout {
            spacing: 4
            RowLayout {
                spacing: 0
                Repeater {
                    model: [qsTr("Service"), qsTr("Queued"), qsTr("Sent"), qsTr("Confirmed"), qsTr("Errors")]
                    Text {
                        required property string modelData
                        required property int index
                        Layout.preferredWidth: index === 0 ? 110 : 100
                        text: modelData
                        color: Theme.secondaryColor
                        font.family: Theme.monoFamily
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }
            Repeater {
                model: decolog.qslSummary
                RowLayout {
                    required property var modelData
                    spacing: 0
                    Text { Layout.preferredWidth: 110; text: modelData.label; color: Theme.textPrimary; font.family: Theme.monoFamily; font.pixelSize: 12; font.bold: true }
                    Text { Layout.preferredWidth: 100; text: modelData.queued || 0; color: modelData.queued ? Theme.warningColor : Theme.textSecondary; font.family: Theme.monoFamily; font.pixelSize: 12 }
                    Text { Layout.preferredWidth: 100; text: modelData.sent || 0; color: Theme.textPrimary; font.family: Theme.monoFamily; font.pixelSize: 12 }
                    Text { Layout.preferredWidth: 100; text: modelData.confirmed || 0; color: modelData.confirmed ? Theme.accentColor : Theme.textSecondary; font.family: Theme.monoFamily; font.pixelSize: 12 }
                    Text { Layout.preferredWidth: 100; text: modelData.errors || 0; color: modelData.errors ? Theme.errorColor : Theme.textSecondary; font.family: Theme.monoFamily; font.pixelSize: 12 }
                }
            }
            RowLayout {
                Layout.topMargin: 6
                spacing: 8
                GlassButton {
                    text: decolog.lotwBusy ? qsTr("LoTW…") : qsTr("Sync LoTW")
                    tone: Theme.accentColor
                    buttonHeight: 24
                    fontPixelSize: 11
                    enabled: !decolog.lotwBusy
                    onClicked: decolog.syncLotw(false)
                }
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: decolog.lotwStatus.length ? decolog.lotwStatus
                          : decolog.lotwLastSync.length ? qsTr("LoTW last sync %1").arg(decolog.lotwLastSync)
                          : qsTr("LoTW confirmations are downloaded from Setup → QSL services. Uploads still go through TQSL.")
                    color: Theme.textSecondary
                    font.pixelSize: 11
                }
            }
            Item { Layout.fillHeight: true }
        }

        // ── Registro attività ───────────────────────────────────────────────
        ListView {
            clip: true
            model: decolog.activity
            ScrollBar.vertical: ScrollBar {}
            delegate: Text {
                required property var modelData
                width: ListView.view.width
                height: 22
                elide: Text.ElideRight
                textFormat: Text.StyledText
                font.family: Theme.monoFamily
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                color: modelData.level === "error" ? Theme.errorColor
                     : modelData.level === "warning" ? Theme.warningColor
                     : modelData.level === "highlight" && modelData.category === "LOTW" ? Theme.accentColor
                     : Theme.textPrimary
                text: "<font color=\"" + Theme.textSecondary + "\">" + modelData.time + "</font> "
                      + "<font color=\"" + root.categoryColor(modelData.category) + "\">" + modelData.category + "</font> "
                      + modelData.text.replace(/&/g, "&amp;").replace(/</g, "&lt;")
                          .replace(/( · new DXCC on FT2: .*)$/, "<font color=\"" + Theme.warningColor + "\">$1</font>")
            }
        }
    }
}
