// DecoDXLog — la finestra riaperta dove uno schermo c'e' davvero.
//
// Misura e posizione di ogni finestra restano da una sessione all'altra, ed e'
// giusto: chi lavora con due monitor vuole ritrovare le cose dove le ha messe.
// Ma il secondo monitor a volte non c'e' piu' — staccato, spento, un portatile
// tornato a casa — e la finestra si riapriva a quelle coordinate, cioe' nel
// nulla: si apriva davvero, semplicemente non si vedeva da nessuna parte.
//
// Si mette una riga in ogni finestra:  OnScreen { target: root }
import QtQuick

QtObject {
    id: keeper

    property Window target: null
    // Quanto della finestra deve restare visibile perche' la si possa
    // riprendere col mouse: la barra del titolo e un pezzo di bordo.
    readonly property int margin: 80

    function screensRect() {
        const list = []
        const screens = Qt.application.screens
        for (let i = 0; i < screens.length; ++i) {
            list.push({x: screens[i].virtualX, y: screens[i].virtualY,
                       w: screens[i].width, h: screens[i].height})
        }
        return list
    }

    function isVisibleSomewhere() {
        if (!keeper.target)
            return true
        const screens = keeper.screensRect()
        for (let i = 0; i < screens.length; ++i) {
            const s = screens[i]
            const overlapX = Math.min(keeper.target.x + keeper.target.width, s.x + s.w) - Math.max(keeper.target.x, s.x)
            const overlapY = Math.min(keeper.target.y + keeper.target.height, s.y + s.h) - Math.max(keeper.target.y, s.y)
            if (overlapX >= keeper.margin && overlapY >= keeper.margin)
                return true
        }
        return false
    }

    function bringBack() {
        if (!keeper.target || keeper.isVisibleSomewhere())
            return
        const screens = keeper.screensRect()
        if (screens.length === 0)
            return
        const s = screens[0]
        // In mezzo allo schermo principale, e non piu' grande di lui.
        keeper.target.width = Math.min(keeper.target.width, s.w - 40)
        keeper.target.height = Math.min(keeper.target.height, s.h - 60)
        keeper.target.x = s.x + Math.max(0, (s.w - keeper.target.width) / 2)
        keeper.target.y = s.y + Math.max(0, (s.h - keeper.target.height) / 2)
    }

    Component.onCompleted: keeper.bringBack()
}
