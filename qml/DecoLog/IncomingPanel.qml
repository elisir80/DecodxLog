// DecoLog — a sinistra: la scheda per i QSO a voce e quelli in arrivo da Decodium.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Decodium.UI

GlassPanel {
    id: root

    title: qsTr("New QSO")

    function pad(n) { return (n < 10 ? "0" : "") + n }

    function resetTime() {
        const d = new Date()
        dateField.text = d.getUTCFullYear() + "-" + pad(d.getUTCMonth() + 1) + "-" + pad(d.getUTCDate())
        timeField.text = pad(d.getUTCHours()) + ":" + pad(d.getUTCMinutes())
    }

    function submit() {
        const error = decolog.logManualQso({
            call: callField.text, date: dateField.text, time: timeField.text,
            freq: freqField.text, band: bandBox.currentIndex > 0 ? bandBox.currentText : "",
            mode: modeBox.editText, rst_sent: sentField.text, rst_rcvd: rcvdField.text,
            name: nameField.text, gridsquare: gridField.text, comment: commentField.text
        })
        formError.text = error
        if (error.length === 0) {
            callField.text = ""
            nameField.text = ""
            gridField.text = ""
            commentField.text = ""
            callField.forceActiveFocus()
        }
    }

    Component.onCompleted: resetTime()

    component SectionLabel: Text {
        font.pixelSize: Theme.fontSize - 2
        font.letterSpacing: 1.2
        font.bold: true
        font.capitalization: Font.AllUppercase
        color: Theme.textSecondary
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 6
            rowSpacing: 5

            StyledTextField {
                id: callField
                Layout.columnSpan: 2
                Layout.fillWidth: true
                mono: true
                uppercase: true
                font.pixelSize: Theme.fontSize + 4
                font.bold: true
                placeholderText: qsTr("Callsign")
                onTextChanged: decolog.lookupCall = text
                // L'ora si prende quando si comincia a scrivere il nominativo,
                // non quando la finestra e' stata aperta.
                onActiveFocusChanged: if (activeFocus && text.length === 0) root.resetTime()
                Keys.onReturnPressed: root.submit()
                Keys.onEnterPressed: root.submit()
            }
            StyledTextField { id: dateField; Layout.fillWidth: true; mono: true; placeholderText: "yyyy-mm-dd" }
            StyledTextField { id: timeField; Layout.fillWidth: true; mono: true; placeholderText: qsTr("HH:mm UTC") }
            StyledTextField {
                id: freqField
                Layout.fillWidth: true
                mono: true
                placeholderText: "MHz"
                text: decolog.dialFrequency
                onTextChanged: {
                    const i = bandBox.find(decolog.bandForFrequency(text))
                    if (i >= 0) bandBox.currentIndex = i
                }
            }
            StyledComboBox {
                id: bandBox
                Layout.fillWidth: true
                model: [qsTr("band")].concat(decolog.bands)
            }
            StyledComboBox {
                id: modeBox
                Layout.fillWidth: true
                editable: true
                model: ["SSB", "CW", "FM", "AM", "RTTY", "FT8", "FT4", "FT2", "PSK31"]
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                StyledTextField { id: sentField; Layout.fillWidth: true; mono: true; placeholderText: qsTr("Sent"); text: "59" }
                StyledTextField { id: rcvdField; Layout.fillWidth: true; mono: true; placeholderText: qsTr("Rcvd"); text: "59" }
            }
            StyledTextField { id: nameField; Layout.fillWidth: true; placeholderText: qsTr("Name") }
            StyledTextField { id: gridField; Layout.fillWidth: true; mono: true; uppercase: true; placeholderText: qsTr("Grid") }
            StyledTextField {
                id: commentField
                Layout.columnSpan: 2
                Layout.fillWidth: true
                placeholderText: qsTr("Comment")
                Keys.onReturnPressed: root.submit()
                Keys.onEnterPressed: root.submit()
            }
            Text {
                id: formError
                Layout.columnSpan: 2
                Layout.fillWidth: true
                visible: text.length > 0
                color: Theme.errorColor
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSize - 1
            }
            GlassButton {
                Layout.alignment: Qt.AlignRight
                Layout.columnSpan: 2
                text: qsTr("Log QSO")
                tone: Theme.accentColor
                enabled: callField.text.length > 2
                onClicked: root.submit()
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.borderSoft }

        SectionLabel { text: qsTr("From Decodium") }

        ListView {
            id: incomingList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 3
            model: decolog.incoming

            Text {
                anchors.centerIn: parent
                width: parent.width - 20
                visible: incomingList.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("QSOs logged in Decodium, WSJT-X or JTDX appear here.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSize - 1
            }

            delegate: Rectangle {
                id: item
                required property var modelData
                required property int index
                readonly property bool fresh: index === 0 && modelData.status === "logged"

                width: ListView.view.width
                height: Theme.rowHeight + 20
                radius: 6
                color: fresh ? Theme.rowMatchBg : Theme.glassOverlay
                border.width: fresh ? 1 : 0
                border.color: Theme.rowMatchBorder

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    Row {
                        spacing: 8
                        Text {
                            text: item.modelData.call
                            color: Theme.textPrimary
                            font.family: Theme.monoFamily
                            font.bold: true
                            font.pixelSize: Theme.fontSize + 1
                        }
                        Text {
                            anchors.bottom: parent.bottom
                            text: item.modelData.band + " " + item.modelData.mode
                            color: Theme.secondaryColor
                            font.family: Theme.monoFamily
                        }
                    }
                    Text {
                        text: item.modelData.time + "  " + item.modelData.rstSent + "/" + item.modelData.rstRcvd
                              + (item.modelData.grid ? "  " + item.modelData.grid : "")
                        color: Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: item.modelData.status === "logged" ? qsTr("logged")
                        : item.modelData.status === "duplicate" ? qsTr("duplicate") : qsTr("error")
                    color: item.modelData.status === "logged" ? Theme.accentColor
                         : item.modelData.status === "duplicate" ? Theme.warningColor : Theme.errorColor
                    font.pixelSize: Theme.fontSize - 2
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: decolog.lookupCall = item.modelData.call
                }
            }
        }
    }
}
