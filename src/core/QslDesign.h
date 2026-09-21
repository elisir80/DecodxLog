// DecoDXLog — la cartolina QSL: il modello e i campi da scriverci sopra.
//
// Una QSL di carta si stampa su una cartolina gia' fatta — la propria, con la
// foto e il nominativo — dove restano dei riquadri vuoti da riempire a mano:
// il corrispondente, la data, l'ora, la banda, il modo, il rapporto. Qui si
// dice dove vanno quei riquadri, una volta sola, e poi il programma li riempie
// da solo per tutti i QSO che si vogliono.
//
// Le posizioni non sono in pixel ma in frazione del modello (0..1), e i corpi
// dei caratteri in millesimi della sua altezza: cambiare la cartolina con una
// scansione piu' grande non sposta piu' niente. Il disegno e' lo stesso sullo
// schermo, nel PNG e nel PDF, perche' lo fa sempre QPainter.
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QImage;
class QPainter;
class QRectF;

namespace decolog::core::qsldesign {

// Un campo posato sulla cartolina.
struct Field {
    QString key;                        // quale dato ci va: vedi keys()
    QString text;                       // solo per key "text": quello che c'e' scritto
    double  x{0.5};                     // 0..1 sulla larghezza del modello
    double  y{0.5};                     // 0..1 sulla sua altezza
    int     size{40};                   // millesimi dell'altezza del modello
    bool    bold{true};
    QString color{QStringLiteral("#000000")};
    QString align{QStringLiteral("left")};   // left | center | right
    QString family;                     // vuoto: quello di sistema
};

// La cartolina: l'immagine di fondo e i campi.
struct Card {
    QString templatePath;
    QList<Field> fields;
};

// Le chiavi che si possono posare, nell'ordine in cui conviene proporle.
// L'etichetta da mostrare la mette chi chiama: qui dentro non si traduce.
QStringList keys();

// Il valore di un campo per quel QSO. `station` e' il profilo di stazione
// (call, grid, name, qth); serve per i campi "my…".
QString valueFor(const Field& field, const QVariantMap& qso, const QVariantMap& station);

QJsonObject toJson(const Card& card);
Card fromJson(const QJsonObject& object);

// Disegna una cartolina dentro `target`. `background` puo' essere nulla: allora
// resta il bianco, e i campi si vedono lo stesso.
void paint(QPainter& painter, const QRectF& target, const QImage& background,
           const Card& card, const QVariantMap& qso, const QVariantMap& station);

// Un PDF con le cartoline, `perPage` per foglio A4 (1, 2 o 4). Torna false e
// riempie `error` se il file non si puo' scrivere.
bool writePdf(const QString& path, const Card& card, const QList<QVariantMap>& qsos,
              const QVariantMap& station, int perPage, QString* error);

// Un PNG per QSO dentro `dir`, alla misura del modello. In `written` finiscono i
// file fatti davvero.
bool writePng(const QString& dir, const Card& card, const QList<QVariantMap>& qsos,
              const QVariantMap& station, QStringList* written, QString* error);

} // namespace decolog::core::qsldesign
