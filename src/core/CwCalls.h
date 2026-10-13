// DecoDXLog — i nominativi dentro al testo CW decodificato.
//
// Il CW decodificato e' testo sporco: un nominativo vero ("K1ABC") sta in mezzo a parole,
// rapporti ("5NN"), bande ("40M") e lettere sbagliate. Qui si dice quali pezzi sono
// nominativi sul serio: la forma, e il contesto che li accompagna — «DE K1ABC», «CQ ... K1ABC»,
// oppure lo stesso nominativo ripetuto. Un nominativo che compare una volta sola, senza
// contesto, non si dichiara: meglio non dire niente che dire uno sbagliato.
#pragma once

#include <QString>

namespace decolog::core::cwcalls {

// Somiglia a un nominativo CW: la forma di un nominativo (lettere, una cifra, lettere) ma non una
// banda ("40M"), un ordinale o un rapporto.
bool isCall(const QString& token);

struct Found {
    QString call;     // vuoto se non se ne e' riconosciuto uno
    bool cq{false};   // sta chiamando CQ (il CQ viene prima del nominativo)
    int score{0};
};

// Il nominativo di chi trasmette, dal testo che ha mandato. `complete` dice se l'ultima parola e'
// finita (la stazione ha smesso): altrimenti l'ultima parola puo' essere un nominativo a meta' e
// non si guarda.
Found find(const QString& text, bool complete);

// Il testo come HTML per un riquadro di testo ricco: a capo, caratteri speciali, e ogni
// nominativo diventa un collegamento "call:K1ABC" nel colore dato (#rrggbb).
QString toHtml(const QString& text, const QString& linkColor);

} // namespace decolog::core::cwcalls
