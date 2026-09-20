// DecoDXLog — le QSL di carta: coda, etichette, PDF.
//
// Una QSL cartacea non e' un file da caricare: e' un pezzo di carta che va
// scritto, imbustato e spedito. Quello che il programma puo' fare e' tenere la
// coda di quelle da mandare e stampare le etichette adesive gia' compilate, che
// e' il lavoro noioso: una etichetta per corrispondente, con dentro fino a
// quattro QSO, cosi' una busta sola risponde a tutti i collegamenti fatti con
// quella stazione.
//
// Il PDF si scrive con QPdfWriter, che sta dentro QtGui: niente moduli in piu' e
// niente stampante di mezzo. Il foglio si sceglie fra quelli in commercio.
#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace decolog::core::qslcard {

// Un foglio di etichette adesive: misure in millimetri.
struct Sheet {
    QString id;
    QString label;          // come lo si legge in negozio
    int     columns{3};
    int     rows{8};
    double  width{70.0};    // etichetta
    double  height{36.0};
    double  left{0.0};      // margine del foglio
    double  top{4.5};
    double  hGap{0.0};      // spazio fra le etichette
    double  vGap{0.0};
};

// I fogli conosciuti; il primo e' quello predefinito.
QList<Sheet> sheets();
Sheet sheetById(const QString& id);

// Una riga di QSO dentro l'etichetta.
struct Line {
    QString date;       // yyyy-MM-dd
    QString time;       // hhmm
    QString band;
    QString mode;
    QString rst;
    QString freq;
};

// Una etichetta: il corrispondente e i QSO fatti con lui.
struct Label {
    QString call;
    QString via;        // QSL manager, se c'e'
    QList<Line> lines;
};

// Raggruppa i QSO per nominativo (fino a `perLabel` per etichetta, poi si passa
// alla successiva) tenendo l'ordine di data del primo QSO di ognuno.
QList<Label> group(const QList<QVariantMap>& qsos, int perLabel = 4);

// Scrive il PDF. `station` vuole almeno "call"; "grid", "name" e "message" sono
// facoltativi. Torna false e riempie `error` se il file non si puo' scrivere.
bool writePdf(const QString& path, const QList<Label>& labels, const Sheet& sheet,
              const QVariantMap& station, bool guides, QString* error);

} // namespace decolog::core::qslcard
