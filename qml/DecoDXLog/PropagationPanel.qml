// DecoDXLog — la propagazione: i numeri del Sole e come stanno le bande.
//
// I numeri da soli non dicono molto: quello che serve sapere e' se una banda e'
// buona adesso, di giorno e di notte, e se il ritmo dei propri QSO segue davvero
// il flusso solare. Le due cose stanno una accanto all'altra.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

Item {
    id: root

    property int revision: 0
    readonly property var solar: { revision; return decolog.solar.data }
    readonly property var days: { revision; return decolog.solar.qsoAgainstFlux(14) }

    Connections {
        target: decolog.solar
        function onChanged() { root.revision++ }
    }
    Connections {
        target: decolog
        function onLogChanged() { root.revision++ }
    }

    function toneFor(kind) {
        switch (kind) {
        case "good": return Theme.accentColor
        case "fair": return Theme.warningColor
        case "poor": return Theme.errorColor
        case "closed": return Theme.textSecondary
        default: return Theme.textSecondary
        }
    }
    function conditionLabel(text) {
        const t = String(text).toLowerCase()
        return t.indexOf("good") === 0 ? qsTr("good")
             : t.indexOf("fair") === 0 ? qsTr("fair")
             : t.indexOf("poor") === 0 ? qsTr("poor")
             : t.indexOf("closed") >= 0 ? qsTr("closed")
             : text
    }

    component Number: Rectangle {
        property string label: ""
        property string value: ""
        property color tone: Theme.textPrimary
        implicitWidth: 96
        implicitHeight: 50
        radius: 5
        color: Theme.bgMedium
        border.width: 1
        border.color: Theme.borderSoft
        Column {
            anchors.centerIn: parent
            spacing: 1
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: parent.parent.label
                color: Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: 10
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: parent.parent.value
                color: parent.parent.tone
                font.family: Theme.monoFamily
                font.pixelSize: 16
                font.bold: true
            }
        }
    }

    component Chip: Rectangle {
        property string text: ""
        property color tone: Theme.textSecondary
        implicitWidth: chipText.implicitWidth + 16
        implicitHeight: 20
        radius: 3
        color: Qt.alpha(tone, 0.16)
        border.width: 1
        border.color: tone
        Text {
            id: chipText
            anchors.centerIn: parent
            text: parent.text
            color: parent.tone
            font.family: Theme.monoFamily
            font.pixelSize: 11
            font.bold: true
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 10

            // ── I numeri ────────────────────────────────────────────────────
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Number {
                    label: qsTr("SFI")
                    value: String(root.solar.solarFlux || "—")
                    tone: (root.solar.solarFlux || 0) >= 120 ? Theme.accentColor : Theme.textPrimary
                }
                Number { label: qsTr("Sunspots"); value: String(root.solar.sunspots || "—") }
                Number {
                    label: qsTr("A index")
                    value: String(root.solar.aIndex !== undefined ? root.solar.aIndex : "—")
                    tone: (root.solar.aIndex || 0) >= 20 ? Theme.errorColor : Theme.textPrimary
                }
                Number {
                    label: qsTr("K index")
                    value: String(root.solar.kIndex !== undefined ? root.solar.kIndex : "—")
                    tone: (root.solar.kIndex || 0) >= 4 ? Theme.errorColor
                        : (root.solar.kIndex || 0) >= 3 ? Theme.warningColor : Theme.accentColor
                }
                Number { label: qsTr("Aurora"); value: String(root.solar.aurora !== undefined ? root.solar.aurora : "—") }
                Number { label: qsTr("X-ray"); value: root.solar.xray || "—" }
                Number { label: qsTr("Geomag"); value: root.solar.geomagField || "—" }
                Number { label: qsTr("Noise"); value: root.solar.signalNoise || "—" }
                Number { label: qsTr("Sol. wind"); value: root.solar.solarWind || "—" }
            }

            // ── Bande ───────────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 24

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: 4
                    SectionTitle { text: qsTr("HF · day") }
                    Repeater {
                        model: (root.solar.hf || []).filter(c => c.when === "day")
                        RowLayout {
                            required property var modelData
                            spacing: 8
                            Text {
                                Layout.preferredWidth: 80
                                text: modelData.band
                                color: Theme.textPrimary
                                font.family: Theme.monoFamily
                                font.pixelSize: 12
                            }
                            Chip {
                                text: root.conditionLabel(modelData.condition)
                                tone: root.toneFor(modelData["class"])
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: 4
                    SectionTitle { text: qsTr("HF · night") }
                    Repeater {
                        model: (root.solar.hf || []).filter(c => c.when === "night")
                        RowLayout {
                            required property var modelData
                            spacing: 8
                            Text {
                                Layout.preferredWidth: 80
                                text: modelData.band
                                color: Theme.textPrimary
                                font.family: Theme.monoFamily
                                font.pixelSize: 12
                            }
                            Chip {
                                text: root.conditionLabel(modelData.condition)
                                tone: root.toneFor(modelData["class"])
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    Layout.fillWidth: true
                    spacing: 4
                    SectionTitle { text: qsTr("VHF") }
                    Repeater {
                        model: root.solar.vhf || []
                        RowLayout {
                            required property var modelData
                            spacing: 8
                            Text {
                                Layout.preferredWidth: 150
                                text: modelData.band + " · " + modelData.when.replace(/_/g, " ")
                                color: Theme.textPrimary
                                font.family: Theme.monoFamily
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                            Chip {
                                text: root.conditionLabel(modelData.condition)
                                tone: root.toneFor(modelData["class"])
                            }
                        }
                    }
                }
            }

            // ── QSO e flusso solare, giorno per giorno ──────────────────────
            SectionTitle { text: qsTr("Your QSOs and the solar flux · last 14 days") }
            RowLayout {
                id: chart
                Layout.fillWidth: true
                Layout.preferredHeight: 110
                spacing: 4
                readonly property int maxQso: {
                    let m = 1
                    for (const d of root.days)
                        m = Math.max(m, d.qso)
                    return m
                }
                Repeater {
                    model: root.days
                    ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 2
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.qso > 0 ? modelData.qso : ""
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 10
                        }
                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignBottom
                            Layout.preferredWidth: Math.min(36, chart.width / Math.max(1, root.days.length) - 8)
                            Layout.preferredHeight: Math.max(2, 60 * modelData.qso / chart.maxQso)
                            radius: 2
                            color: Theme.primaryColor
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.sfi > 0 ? modelData.sfi : "·"
                            color: modelData.sfi >= 120 ? Theme.accentColor : Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 10
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.day.substring(8, 10)
                            color: Theme.textSecondary
                            font.family: Theme.monoFamily
                            font.pixelSize: 10
                        }
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("The bar is the QSOs of the day, the number under it the average solar flux. "
                           + "The flux is kept from when DecoDXLog started looking: the first days are empty.")
                color: Theme.textSecondary
                font.pixelSize: 11
            }

            // ── Fonte ───────────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                GlassButton {
                    text: decolog.solar.busy ? qsTr("asking…") : qsTr("Update now")
                    tone: Theme.primaryColor
                    buttonHeight: 24
                    fontPixelSize: 11
                    enabled: !decolog.solar.busy
                    onClicked: decolog.solar.refresh()
                }
                ToggleSwitch {
                    text: qsTr("by itself every %1 min").arg(decolog.solar.intervalMinutes)
                    checked: decolog.solar.automatic
                    onToggled: decolog.solar.automatic = checked
                }
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: root.solar.valid
                          ? qsTr("%1 · source %2").arg(root.solar.updated).arg(root.solar.source || "N0NBH")
                          : decolog.solar.status.length ? decolog.solar.status
                          : qsTr("No solar data yet.")
                    color: Theme.textSecondary
                    font.pixelSize: 11
                }
            }
            Item { Layout.fillHeight: true }
        }
    }
}
