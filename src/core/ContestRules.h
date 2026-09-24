// DecoDXLog — le regole dei contest: cosa si scambia, quanto vale un QSO e cosa
// fa moltiplicatore.
//
// Ogni contest ha le sue, e sono tutte prese dal regolamento vero, non da
// quello che ci si ricorda: un punteggio sbagliato non si vede guardando, si
// vede quando arriva la classifica. Le schede qui dentro citano il punto del
// regolamento da cui vengono, cosi' chi le rilegge fra un anno sa dove
// controllare se sono cambiate.
//
// Quello che non c'e' non si inventa: un contest senza scheda non ha punteggio
// e il programma lo dice, invece di dare un numero che non vuol dire niente.
#pragma once

#include <QList>
#include <QString>

namespace decolog::core {

// Il QSO come serve al conto: dove sta il corrispondente e cosa ha mandato.
struct ContestQso {
    QString band;        // "20m"
    QString mode;        // "CW", "SSB", "RTTY"...
    QString call;
    int     dxcc{0};
    QString continent;   // "EU", "NA"...
    int     cqZone{0};
    int     ituZone{0};
    QString exchange;    // lo scambio ricevuto: "14", "NA", "L01", "059"
};

// La propria stazione: serve per sapere se un QSO e' nel proprio paese, nel
// proprio continente, nella propria zona.
struct ContestStation {
    int     dxcc{0};
    QString continent;
    int     cqZone{0};
    int     ituZone{0};
};

struct ContestRules {
    // Cosa si scambia oltre al rapporto.
    enum class Exchange {
        None,        // solo il rapporto
        Serial,      // numero progressivo
        CqZone,      // zona CQ (1-40)
        ItuZone,     // zona ITU (1-90), o la sigla di una societa' IARU
        Province,    // sigla della provincia italiana
        AriSection,  // codice ASC della Sezione ARI: una lettera e due cifre
        Grid,        // locatore
    };

    QString  id;                  // CONTEST_ID ADIF
    Exchange exchange{Exchange::Serial};
    // Il nome del campo dello scambio ricevuto, gia' tradotto.
    QString  exchangeLabel;
    // Da dove vengono queste regole: si scrive nell'interfaccia.
    QString  source;
    // Dove si manda il log a gara finita. Vuoto quando non si sa.
    QString  submitUrl;
    // Entro quando: giorni dalla fine, 0 se il regolamento non lo dice.
    int      submitDays{0};
    bool     valid{false};        // falso quando il contest non ha una scheda
};

namespace contestrules {

// La scheda di un contest, o una scheda vuota (valid = false) se non c'e'.
ContestRules forId(const QString& contestId);

// Gli identificativi dei contest che hanno una scheda.
QStringList known();

// Quanto vale questo QSO. Zero e' una risposta buona: nel CQ WW un QSO con il
// proprio paese vale zero punti e conta lo stesso per i moltiplicatori.
int points(const ContestRules& rules, const ContestQso& qso, const ContestStation& me);

// La chiave del moltiplicatore, o vuota se questo QSO non ne porta. Due QSO con
// la stessa chiave sono lo stesso moltiplicatore: e' la chiave a dire se conta
// per banda, per modo, o una volta sola (il prefisso nel WPX).
// Un contest puo' darne piu' d'uno: nel CQ WW la zona e il paese.
QStringList multipliers(const ContestRules& rules, const ContestQso& qso, const ContestStation& me);

// Lo scambio ricevuto ha la forma che il contest vuole? Torna vuoto se va bene,
// altrimenti cosa c'e' che non va, gia' tradotto.
QString checkExchange(const ContestRules& rules, const QString& exchange);

// Il prefisso WPX di un nominativo sta in Awards: qui si dice solo che il WPX
// lo usa come moltiplicatore.

} // namespace contestrules
} // namespace decolog::core
