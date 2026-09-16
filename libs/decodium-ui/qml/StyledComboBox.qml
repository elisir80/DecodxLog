// decodium-ui — elenco a discesa coi colori del tema.
import QtQuick
import QtQuick.Controls
import Decodium.UI

ComboBox {
    id: root

    implicitHeight: Math.max(26, Theme.rowHeight + 4)
    font.pixelSize: Theme.fontSize

    contentItem: Text {
        leftPadding: 8
        rightPadding: root.indicator.width + 4
        text: root.displayText
        font: root.font
        color: Theme.textPrimary
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: root.width - width - 8
        anchors.verticalCenter: parent.verticalCenter
        text: "▾"
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSize
    }

    background: Rectangle {
        radius: 6
        color: Theme.bgMedium
        border.width: 1
        border.color: root.activeFocus || root.popup.visible ? Theme.primaryColor : Theme.glassBorder
    }

    delegate: ItemDelegate {
        required property var modelData
        required property int index
        width: root.width
        height: Theme.rowHeight + 2
        highlighted: root.highlightedIndex === index
        contentItem: Text {
            text: root.textRole ? modelData[root.textRole] : modelData
            font: root.font
            color: Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: highlighted ? Qt.rgba(Theme.primaryColor.r, Theme.primaryColor.g,
                                         Theme.primaryColor.b, 0.25)
                               : "transparent"
        }
    }

    popup: Popup {
        y: root.height + 2
        width: root.width
        implicitHeight: Math.min(contentItem.implicitHeight + 4, 320)
        padding: 2
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
        }
        background: Rectangle {
            radius: 6
            color: Theme.bgMedium
            border.color: Theme.glassBorder
        }
    }
}
