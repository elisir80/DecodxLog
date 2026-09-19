// decodium-ui — elenco a discesa coi colori del tema.
import QtQuick
import QtQuick.Controls
import Decodium.UI

ComboBox {
    id: root

    property bool mono: true
    property int fieldHeight: 30

    implicitHeight: fieldHeight
    // Larghezza naturale piccola: nei layout decide lo spazio disponibile, non il
    // testo segnaposto, cosi' le colonne di una scheda restano uguali.
    implicitWidth: 60
    font.pixelSize: Theme.fontSize
    font.family: mono ? Theme.monoFamily : Theme.uiFamily

    contentItem: TextField {
        leftPadding: 8
        rightPadding: root.indicator.width + 6
        text: root.editable ? root.editText : root.displayText
        readOnly: !root.editable
        enabled: root.editable
        font: root.font
        color: root.enabled ? Theme.textPrimary : Theme.textSecondary
        selectionColor: Theme.primaryColor
        verticalAlignment: Text.AlignVCenter
        background: Item {}
        onTextEdited: if (root.editable) root.editText = text
        onAccepted: if (root.editable) root.accepted()
    }

    // La freccia: in un elenco modificabile il campo di testo si prende i clic, e
    // senza un appiglio qui la tendina non si aprirebbe piu' — si potrebbe solo
    // scrivere. Il tocco apre e chiude; con la tastiera resta il Giu' di ComboBox.
    indicator: Item {
        x: root.width - width - 6
        y: (root.height - height) / 2
        implicitWidth: arrow.implicitWidth + 12
        implicitHeight: root.height
        Text {
            id: arrow
            anchors.centerIn: parent
            text: "▾"
            color: root.popup.visible ? Theme.primaryColor : Theme.textSecondary
            font.pixelSize: Theme.fontSize
        }
        TapHandler {
            enabled: root.editable && root.enabled
            onSingleTapped: root.popup.visible ? root.popup.close() : root.popup.open()
        }
    }

    background: Rectangle {
        radius: 4
        color: Theme.bgMedium
        border.width: 1
        border.color: root.activeFocus || root.popup.visible ? Theme.primaryColor : Theme.glassBorder
    }

    // La riga della tendina la disegniamo noi, con un rettangolo e un testo.
    // Con ItemDelegate e le proprieta' "required" le righe non venivano create
    // per gli elenchi di stringhe semplici, e la tendina si apriva vuota: righe
    // che ci sono e non si leggono. Cosi' invece funzionano tutti gli elenchi.
    delegate: Rectangle {
        id: option
        readonly property int rowIndex: index
        readonly property var rowValue: typeof modelData !== "undefined" ? modelData : model
        readonly property bool chosen: root.highlightedIndex === rowIndex || root.currentIndex === rowIndex

        width: ListView.view ? ListView.view.width : root.width
        height: Theme.rowHeight + 2
        color: area.containsMouse || chosen
               ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g, Theme.primaryColor.b, 0.22)
               : "transparent"

        Text {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font: root.font
            color: Theme.textPrimary
            text: {
                const value = option.rowValue
                if (value === undefined || value === null)
                    return ""
                if (typeof value === "string")
                    return value
                if (root.textRole.length > 0 && value[root.textRole] !== undefined)
                    return value[root.textRole]
                if (value.display !== undefined)
                    return value.display
                return String(value)
            }
        }

        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onEntered: root.highlightedIndex = option.rowIndex
            onClicked: {
                root.currentIndex = option.rowIndex
                root.activated(option.rowIndex)
                root.popup.close()
            }
        }
    }

    popup: Popup {
        y: root.height + 2
        width: Math.max(root.width, 160)
        implicitHeight: Math.min(contentItem.implicitHeight + 4, 340)
        padding: 2
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            radius: 4
            color: Theme.bgMedium
            border.color: Theme.glassBorder
        }
    }
}
