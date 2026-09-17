// decodium-ui — menu a comparsa coi colori del tema (quello di Qt resta grigio
// chiaro anche su Darkcodium). Le voci sono StyledMenuItem; i sottomenu
// ereditano lo stesso aspetto tramite il delegate.
import QtQuick
import QtQuick.Controls
import Decodium.UI

Menu {
    id: root

    padding: 4
    // La voce piu' larga decide: il ListView del contenuto non lo sa.
    readonly property real widestItem: {
        let w = 0
        for (let i = 0; i < count; ++i) {
            const item = itemAt(i)
            if (item && item.visible)
                w = Math.max(w, item.implicitWidth)
        }
        return w
    }
    implicitWidth: Math.max(180, widestItem + leftPadding + rightPadding)

    delegate: StyledMenuItem {}

    background: Rectangle {
        implicitWidth: 180
        radius: 6
        color: Theme.bgMedium
        border.width: 1
        border.color: Theme.glassBorder
    }
}
