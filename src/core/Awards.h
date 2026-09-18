// DecoLog — award calcolati dal log: DXCC, WAZ, WAS, WPX, locatori, IOTA, POTA,
// SOTA, WWFF e FT2.
//
// Nessuna tabella in piu' nel database: gli award si ricalcolano dai QSO, cosi'
// una correzione a un QSO si vede subito ovunque. Per ogni elemento (un'entita',
// una zona, uno stato...) si tiene su quali bande e' stato lavorato e su quali
// confermato, secondo le conferme che l'operatore accetta (LoTW e cartolina per
// default, come ARRL per il DXCC).
#pragma once

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>

namespace decolog::core {

class LogDatabase;

namespace awards {

// Il prefisso WPX di un nominativo secondo le regole CQ WPX:
// N8BJQ → N8, EA8/OH2XX → EA8, W1AW/4 → W4, LX/DL1ABC → LX0.
QString wpxPrefix(const QString& callsign);

// I 50 stati USA con il nome, per WAS.
const QMap<QString, QString>& usStates();

// I continenti dell'IARU con il nome, per il WAC. L'Antartide c'e' — chi l'ha
// lavorata vuole vederla — ma il traguardo del diploma resta sei.
const QMap<QString, QString>& continents();

// Le 47 prefetture giapponesi con il nome, per il WAJA. La chiave e' il numero
// a due cifre come lo scrive ADIF ("01" Hokkaido … "47" Okinawa).
const QMap<QString, QString>& japanPrefectures();

// La prefettura da come la scrivono i log: "12", "JA12", "12 Chiba".
QString japanPrefecture(const QString& state);

// Il distretto giapponese (AJD) dalla cifra del nominativo: JA1 → 1, JA0 → 0.
// Vuoto se il nominativo non e' giapponese o non si capisce.
QString japanDistrict(const QString& callsign);

// Il numero JARL che sta nel campo CNTY: le citta' (JCC) hanno quattro cifre,
// sei se sono quartieri di una citta' designata; i distretti (JCG) ne hanno
// cinque. Le prime due cifre sono sempre la prefettura, da 01 a 47. Si accetta
// come lo scrivono i log — "1001", "10-01", "JCC 1001" — e torna vuoto se il
// numero non sta in piedi.
QString japanJarlCode(const QString& county);

// Vero se quel numero e' di un distretto (JCG), cioe' di cinque cifre.
bool isJapanGun(const QString& jarlCode);

} // namespace awards

struct AwardFilter {
    QString band;               // vuoto = tutte
    QString modeGroup;          // "", "FT2", "FT8", "DIGITAL", "CW", "PHONE"
    bool confirmLotw{true};
    bool confirmCard{true};
    bool confirmEqsl{false};
    qint64 stationProfileId{0}; // 0 = tutti i profili
    QString tag;                // etichetta dei QSO che contano, vuota = tutti
};

struct BandTotal {
    QString band;
    int worked{0};              // elementi lavorati su questa banda
    int confirmed{0};
};

struct AwardItem {
    QString key;                // "291", "14", "CA", "IU8", "JN71", "EU-025"...
    QString name;               // nome leggibile, se c'e'
    QSet<QString> bandsWorked;
    QSet<QString> bandsConfirmed;
    int qsoCount{0};
    QDateTime first;
    QDateTime last;
    qint64 firstQsoId{0};
    QString firstCall;
    bool confirmed() const { return !bandsConfirmed.isEmpty(); }
};

struct AwardResult {
    QString id;                 // "dxcc", "waz"...
    QString title;
    int target{0};              // traguardo del diploma base (0 = nessuno)
    int total{0};               // elementi esistenti, se il numero e' fisso (40 zone, 50 stati)
    QList<AwardItem> items;     // ordinati per chiave
    int worked() const { return static_cast<int>(items.size()); }
    int confirmed() const;
    // Per banda, nell'ordine dato: quanti elementi lavorati e confermati. La somma
    // dei confermati e' il conteggio dei "band slot" (DXCC Challenge).
    QList<BandTotal> bandTotals(const QStringList& bands) const;
};

class AwardCalculator {
public:
    // Nome di un'entita' DXCC dal numero (lo fornisce il cty.csv).
    using DxccName = std::function<QString(int dxcc)>;

    explicit AwardCalculator(DxccName dxccName = {});

    static QStringList awardIds();
    // Tutti gli award in una passata sul log.
    QList<AwardResult> compute(const LogDatabase& db, const AwardFilter& filter) const;

private:
    DxccName m_dxccName;
};

} // namespace decolog::core
