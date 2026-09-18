// DecoLog — la sessione di attivazione o di contest: si apre, si conta, si esporta.
// Finche' e' aperta, ogni QSO prende la referenza, il locatore del posto,
// l'etichetta e il numero progressivo, e i duplicati si contano dentro la sessione.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Decodium.UI

DialogFrame {
    id: root

    readonly property var act: decolog.activation
    readonly property var state: act.state
    property var draft: ({})

    function openDialog() {
        draft = act.active ? Object.assign({}, root.state)
                           : { kind: "pota", reference: "", name: "", contestId: "", myGrid: decolog.myGrid,
                               tag: "", band: "", mode: "", serialEnabled: false, nextSerial: 1 }
        errorText.text = ""
        open()
    }
    function set(key, value) {
        const d = Object.assign({}, root.draft)
        d[key] = value
        root.draft = d
    }
    readonly property var kindLabels: ({ pota: "POTA", sota: "SOTA", wwff: "WWFF", iota: "IOTA",
                                         contest: qsTr("Contest"), free: qsTr("Free session") })
    readonly property bool needsReference: ["pota", "sota", "wwff", "iota"].indexOf(draft.kind) >= 0

    title: act.active ? qsTr("Session · %1").arg(root.state.title) : qsTr("Activation / contest")
    dotColor: act.active ? Theme.accentColor : Theme.secondaryColor
    width: Math.min(760, parent ? parent.width - 60 : 760)
    info: act.active ? qsTr("open since %1 UTC · %2").arg(String(root.state.startedAt).substring(11, 16)).arg(root.state.elapsed)
                     : ""

    FileDialog {
        id: exportDialog
        title: qsTr("Export the session")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "adi"
        currentFile: "file:///" + root.state.fileName
        nameFilters: [qsTr("ADIF files (*.adi)")]
        onAccepted: errorText.text = root.act.exportAdif(selectedFile)
    }

    component Note: Text {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Theme.textSecondary
        font.pixelSize: 12
    }

    contentItem: ColumnLayout {
        anchors.margins: 14
        spacing: 12

        // ── Quello che sta succedendo ───────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            visible: root.act.active
            spacing: 8
            StatTile { Layout.fillWidth: true; label: qsTr("QSO"); value: root.act.qsoCount }
            StatTile { Layout.fillWidth: true; label: qsTr("Different calls"); value: root.act.uniqueCalls }
            StatTile { Layout.fillWidth: true; label: qsTr("Duration"); value: root.state.elapsed || "0:00" }
            StatTile { Layout.fillWidth: true; label: qsTr("Last QSO"); value: root.state.lastQso || "—" }
            StatTile {
                Layout.fillWidth: true
                visible: root.state.serialEnabled
                label: qsTr("Next number")
                value: root.act.nextSerial
            }
        }
        MeterBar {
            Layout.fillWidth: true
            visible: root.act.active && root.act.requiredQsos > 0
            label: root.act.qsoCount >= root.act.requiredQsos
                   ? qsTr("activation valid: %1 QSO").arg(root.act.qsoCount)
                   : qsTr("%1 QSO to go").arg(root.act.requiredQsos - root.act.qsoCount)
            valueText: qsTr("%1 / %2").arg(root.act.qsoCount).arg(root.act.requiredQsos)
            fraction: root.act.requiredQsos > 0 ? Math.min(1, root.act.qsoCount / root.act.requiredQsos) : 0
            barColor: root.act.qsoCount >= root.act.requiredQsos ? Theme.accentColor : Theme.warningColor
        }
        Flow {
            Layout.fillWidth: true
            visible: root.act.active
            spacing: 6
            Repeater {
                model: root.act.perBand
                Pill {
                    required property var modelData
                    text: "%1 %2 · %3".arg(modelData.band).arg(modelData.mode).arg(modelData.count)
                    tone: modelData.mode === "FT2" ? Theme.accentColor : Theme.secondaryColor
                }
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSoft; visible: root.act.active }

        // ── Come e' fatta la sessione ───────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            LabeledField {
                label: qsTr("Kind")
                StyledComboBox {
                    Layout.preferredWidth: 150
                    model: root.act.kinds.map(k => root.kindLabels[k])
                    currentIndex: Math.max(0, root.act.kinds.indexOf(root.draft.kind))
                    onActivated: root.set("kind", root.act.kinds[currentIndex])
                }
            }
            LabeledField {
                Layout.fillWidth: true
                visible: root.needsReference
                label: qsTr("Reference")
                StyledTextField {
                    Layout.fillWidth: true
                    uppercase: true
                    text: root.draft.reference || ""
                    placeholderText: root.draft.kind === "sota" ? "I/LM-001" : root.draft.kind === "wwff" ? "IFF-0123"
                                     : root.draft.kind === "iota" ? "EU-025" : "IT-1234"
                    onTextEdited: root.set("reference", text)
                }
            }
            LabeledField {
                Layout.fillWidth: true
                visible: root.draft.kind === "contest"
                label: qsTr("Contest (CONTEST_ID)")
                StyledTextField {
                    Layout.fillWidth: true
                    uppercase: true
                    text: root.draft.contestId || ""
                    placeholderText: "CQ-WW-SSB"
                    onTextEdited: root.set("contestId", text)
                }
            }
            LabeledField {
                Layout.fillWidth: true
                label: qsTr("Name")
                StyledTextField {
                    Layout.fillWidth: true
                    mono: false
                    text: root.draft.name || ""
                    onTextEdited: root.set("name", text)
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            LabeledField {
                label: qsTr("Grid of the place")
                StyledTextField {
                    Layout.preferredWidth: 120
                    uppercase: true
                    text: root.draft.myGrid || ""
                    placeholderText: decolog.myGrid
                    onTextEdited: root.set("myGrid", text)
                }
            }
            LabeledField {
                label: qsTr("Tag on every QSO")
                StyledTextField {
                    Layout.preferredWidth: 140
                    mono: false
                    text: root.draft.tag || ""
                    placeholderText: root.draft.kind === "contest" ? (root.draft.contestId || "contest").toLowerCase() : root.draft.kind
                    onTextEdited: root.set("tag", text)
                }
            }
            LabeledField {
                label: qsTr("Station profile")
                StyledComboBox {
                    Layout.preferredWidth: 180
                    readonly property var ids: {
                        const list = [0]
                        for (let i = 0; i < decolog.stationProfiles.count; ++i) {
                            const p = decolog.stationProfiles.get(i)
                            if (!p.deleted) list.push(p.id)
                        }
                        return list
                    }
                    model: ids.map(id => id === 0 ? qsTr("The active one") : decolog.stationProfiles.byId(id).name)
                    currentIndex: Math.max(0, ids.indexOf(root.draft.stationProfileId || 0))
                    onActivated: root.set("stationProfileId", ids[currentIndex])
                }
            }
            ToggleSwitch {
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: 4
                text: qsTr("Serial number")
                checked: !!root.draft.serialEnabled
                onToggled: root.set("serialEnabled", checked)
            }
            LabeledField {
                visible: !!root.draft.serialEnabled
                label: qsTr("Next")
                StyledTextField {
                    Layout.preferredWidth: 70
                    text: root.draft.nextSerial || 1
                    validator: IntValidator { bottom: 1; top: 99999 }
                    onEditingFinished: root.set("nextSerial", parseInt(text) || 1)
                }
            }
        }

        Note {
            text: root.draft.kind === "contest"
                  ? qsTr("The QSOs get CONTEST_ID and the serial number sent (STX). The number received goes in the New QSO panel.")
                  : qsTr("The QSOs get the activator fields (MY_SIG, MY_SIG_INFO, MY_SOTA_REF…), the grid of the place and the tag. "
                         + "A call already worked in this session on the same band and mode counts as a duplicate, whenever it was.")
        }
        Text {
            id: errorText
            Layout.fillWidth: true
            visible: text.length > 0
            wrapMode: Text.Wrap
            color: Theme.errorColor
            font.pixelSize: 12
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            GlassButton {
                visible: root.act.active
                text: qsTr("Export ADIF (%1)").arg(root.state.fileName || "")
                onClicked: exportDialog.open()
            }
            GlassButton {
                text: qsTr("Contest window (Ctrl+Shift+T)")
                tone: Theme.primaryColor
                onClicked: { window.openContest(); root.close() }
            }
            Item { Layout.fillWidth: true }
            GlassButton { text: qsTr("Close"); onClicked: root.close() }
            GlassButton {
                visible: root.act.active
                text: qsTr("Apply changes")
                onClicked: { root.act.update(root.draft); errorText.text = "" }
            }
            GlassButton {
                visible: root.act.active
                text: qsTr("End session")
                tone: Theme.errorColor
                onClicked: { root.act.stop(); root.close() }
            }
            GlassButton {
                visible: !root.act.active
                text: qsTr("Start session")
                tone: Theme.accentColor
                filled: true
                onClicked: {
                    errorText.text = root.act.start(root.draft)
                    if (errorText.text.length === 0)
                        root.close()
                }
            }
        }
    }
}
