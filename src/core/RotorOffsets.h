// DecoDXLog — le antenne del rotore, banda per banda.
//
// Sullo stesso palo ci sono antenne che non guardano tutte dove guarda il
// rotore: un dipolo rotativo dei 40 metri montato a croce rispetto alla
// direttiva dei 20, una direttiva dei 2 metri girata di 180 gradi. Per ogni
// banda si dice quale antenna si usa e di quanto e' girata: il quadrante
// mostra dove guarda davvero l'antenna in uso, e "punta a 45°" manda il
// rotore dove serve perche' sia quell'antenna a guardare a 45°.
#pragma once

#include <QList>
#include <QString>
#include <QVariantList>

namespace decolog::core::rotoroffsets {

struct Entry {
    QString band;      // "40m"
    QString antenna;   // "Dipolo rotativo"
    int offset{0};     // gradi: dove guarda l'antenna rispetto al rotore
};

// La tabella come la tengono le impostazioni (JSON) e come la usa il QML.
QList<Entry> fromJson(const QString& json);
QString toJson(const QList<Entry>& entries);
QVariantList toVariant(const QList<Entry>& entries);

// La riga della banda, o nullptr se per quella banda non c'e' niente (allora
// l'antenna guarda dove guarda il rotore).
const Entry* forBand(const QList<Entry>& entries, const QString& band);

// Dove guarda l'antenna, con il rotore a `rotorAz`.
double antennaAz(double rotorAz, int offset);
// Dove mandare il rotore perche' l'antenna guardi a `antennaAz`.
double rotorAz(double antennaAz, int offset);

} // namespace decolog::core::rotoroffsets
