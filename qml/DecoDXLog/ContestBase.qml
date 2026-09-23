// DecoDXLog — la finestra principale in modalita' contest: la base del banco.
//
// In gara la finestra grande non mostra il log di tutti i giorni: niente
// pannello del nuovo QSO, niente schede di diplomi e statistiche. Sta sotto, e
// sopra di lei stanno le finestre della gara. Non ha scritte ne' pulsanti: tutto
// quello che serve sta nella barra in alto, sotto Contest Mode, e qui resta
// spazio libero per le finestre.
import QtQuick
import Decodium.UI

Rectangle {
    id: root

    color: Theme.bgDeep

    // Sotto c'e' la disposizione di tutti i giorni: i clic e la rotella si
    // fermano qui, invece di arrivare ai pannelli coperti.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = true
    }

    // Una griglia leggera sullo sfondo: si capisce che e' un piano di lavoro,
    // non una finestra rimasta vuota per sbaglio.
    Canvas {
        anchors.fill: parent
        opacity: 0.35
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = Theme.borderSoft
            ctx.lineWidth = 1
            for (let x = 0; x < width; x += 48) {
                ctx.beginPath(); ctx.moveTo(x + 0.5, 0); ctx.lineTo(x + 0.5, height); ctx.stroke()
            }
            for (let y = 0; y < height; y += 48) {
                ctx.beginPath(); ctx.moveTo(0, y + 0.5); ctx.lineTo(width, y + 0.5); ctx.stroke()
            }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }
}
