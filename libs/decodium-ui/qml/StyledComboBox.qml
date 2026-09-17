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
    font.family: mono ? Theme.monoFamily : Qt.application.font.family

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

    delegate: ItemDelegate {
        id: option
        required property var model
        required property int index
        width: root.width
        height: Theme.rowHeight + 2
        highlighted: root.highlightedIndex === index
        contentItem: Text {
            text: option.model[root.textRole] !== undefined ? option.model[root.textRole] : option.model.modelData
            font: root.font
            color: Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: option.highlighted ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g,
                                                Theme.primaryColor.b, 0.22)
                                      : "transparent"
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
