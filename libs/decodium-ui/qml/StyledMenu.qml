// decodium-ui — menu a comparsa coi colori del tema (quello di Qt resta grigio
// chiaro anche su Darkcodium). Le voci sono StyledMenuItem; i sottomenu
// ereditano lo stesso aspetto tramite il delegate.
import QtQuick
import QtQuick.Controls
import Decodium.UI

Menu {
    id: root

    padding: 4
    implicitWidth: Math.max(180, contentItem ? contentItem.implicitWidth + 8 : 180)

    delegate: StyledMenuItem {}

    background: Rectangle {
        implicitWidth: 180
        radius: 6
        color: Theme.bgMedium
        border.width: 1
        border.color: Theme.glassBorder
    }
}
