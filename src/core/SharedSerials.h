// DecoDXLog — i progressivi di una gara multi-operatore, uno solo per tutti.
//
// In una gara con un solo progressivo per la stazione (multi-single, o una
// sequenza unica per regolamento) i PC in rete non possono contare ognuno per
// conto suo: due QSO nello stesso minuto avrebbero lo stesso numero. Uno dei
// PC distribuisce i numeri — quello con l'identita' piu' piccola fra quelli
// presenti, cosi' se sparisce ne subentra un altro senza chiedere niente a
// nessuno — e gli altri ne chiedono uno in anticipo, per averlo pronto quando
// il QSO arriva. Senza rete si va avanti da soli: il piu' alto visto piu' uno.
#pragma once

#include <QString>
#include <QStringList>

namespace decolog::core {

class SharedSerials {
public:
    // Un progressivo visto in giro (un QSO della rete, uno dato a un altro).
    void note(int serial);
    // Il distributore: il prossimo numero, che da adesso e' preso.
    int serve();
    // Chi distribuisce: il piu' piccolo fra se' e gli altri presenti.
    static bool isServer(const QString& me, const QStringList& others);
    // Il numero per il QSO di adesso. `server`: questo PC distribuisce;
    // `online`: la rete c'e'. Con un numero avuto in anticipo si usa quello.
    int take(int localNext, bool server, bool online);
    // Un numero avuto dal distributore, per il prossimo QSO.
    void reserve(int serial);
    bool hasReserved() const { return m_reserved > 0; }
    int highest() const { return m_max; }

private:
    int m_max{0};
    int m_reserved{0};
    int m_next{0};
};

} // namespace decolog::core
