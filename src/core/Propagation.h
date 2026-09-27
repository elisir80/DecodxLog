// DecoDXLog — previsione di propagazione HF su un percorso, ora per ora.
//
// Non e' VOACAP: e' un modello compatto, nello spirito di MINIMUF, che basta a
// rispondere alla domanda di tutti i giorni — "a che ora si apre il 15 m verso
// il Giappone?". Per ogni ora UTC si stima la foF2 nei punti di controllo del
// percorso (dove il segnale tocca lo strato F2) da quanto e' alto il Sole li' e
// dal numero di macchie; da li' la MUF del salto, e dall'assorbimento dello
// strato D di giorno la LUF. Una banda e' aperta se sta fra le due.
#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace decolog::core::propagation {

// Qualita' di una banda in un'ora.
enum Quality : int {
    Closed = 0,     // sopra la MUF o sotto la LUF
    Marginal = 1,   // vicino ai bordi: si passa a tratti
    Fair = 2,
    Good = 3,
};

struct Band {
    QString name;   // "20m"
    double mhz;     // la frequenza su cui si fanno i conti
};
// Le bande HF (e il 6 m, solo F2) nell'ordine dell'operatore.
const QList<Band>& bands();

struct Hour {
    int    hourUtc{0};
    double mufMhz{0.0};
    double lufMhz{0.0};
    QList<int> quality;     // una per banda, nell'ordine di bands()
};

struct Forecast {
    bool   valid{false};
    double distanceKm{0.0};
    int    azimuth{0};
    int    hops{1};
    QList<Hour> hours;      // 24, dalle 00 UTC
};

struct Input {
    double fromLat{0.0}, fromLon{0.0};
    double toLat{0.0}, toLon{0.0};
    QDate  date;            // il giorno (UTC) della previsione
    double sunspots{-1};    // numero di macchie; < 0 = ricavato dal flusso
    double solarFlux{0};    // SFI (10,7 cm)
    int    kIndex{0};
};

// La foF2 (MHz) in un punto a un'ora: esposta per i test.
double foF2(double lat, double lon, const QDateTime& utc, double sunspots);
// La previsione di tutta la giornata.
Forecast forecast(const Input& in);

} // namespace decolog::core::propagation
