// DecoLog — spot del DX cluster: lettura, stato rispetto al log, filtri.
//
// Uno spot arriva da fonti diverse (nodi DX Spider/AR-Cluster/CC Cluster, Reverse
// Beacon Network, HamAlert, POTA) in formati diversi; qui diventa sempre la stessa
// struttura. Poi si arricchisce con quello che solo il log sa: e' un DXCC nuovo?
// una banda nuova per quell'entita'? l'ho gia' lavorato su questa banda? Un cluster
// che non conosce il log mostra tutto; questo mostra quello che serve.
#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <optional>

namespace decolog::core {

class Countries;
class LogDatabase;

struct Spot {
    QDateTime time;             // UTC
    QString dxCall;
    double  freqKhz{0.0};
    QString band;               // "20m"
    QString mode;               // FT8, FT4, FT2, CW, SSB, RTTY, DIGI, FM... vuoto se non si sa
    QString spotter;
    QString comment;
    QString source;             // "cluster", "rbn", "hamalert", "pota"
    QString sourceName;         // il nodo o il servizio, per l'operatore
    bool    hasSnr{false};
    int     snr{0};             // dB (RBN e skimmer)
    int     wpm{0};
    QString dxGrid;             // dal commento o dalla fonte
    QString potaRef;
    QString sotaRef;
    QString wwffRef;
    QString iotaRef;
    QString dxName;             // POTA: nome del parco

    bool isSkimmer() const { return spotter.contains(QLatin1Char('#')); }
};

// Stato dello spot rispetto al log: si combinano.
enum SpotStatus : int {
    StatusNewDxcc    = 1 << 0,  // entita' mai lavorata
    StatusNewBand    = 1 << 1,  // entita' mai lavorata su questa banda
    StatusNewMode    = 1 << 2,  // entita' mai lavorata in questo modo
    StatusNewSlot    = 1 << 3,  // entita' mai lavorata su questa banda in questo modo
    StatusNewCall    = 1 << 4,  // nominativo mai lavorato
    StatusWorkedBand = 1 << 5,  // nominativo gia' lavorato su questa banda (e modo, se noto)
    StatusUnconfirmed = 1 << 6, // entita' lavorata ma non ancora confermata
    StatusLotwUser   = 1 << 7,  // carica su LoTW
};

struct EnrichedSpot {
    Spot    spot;
    int     dxcc{0};
    QString entity;
    QString continent;
    int     cqZone{0};
    QString spotterContinent;
    int     status{0};
    double  distanceKm{-1.0};
    int     azimuth{-1};
    // Lo stesso nominativo sulla stessa banda e modo da piu' spotter diventa una riga.
    int     count{1};
    QStringList spotters;
    QDateTime firstSeen;

    QString key() const;        // call|band|mode
};

namespace spots {

// "DX de IK1XXX:   14074.0  JA1YYY   FT8 -12 dB 1234 Hz   1234Z JN12". `now` serve
// per la data: il cluster da' solo l'ora.
std::optional<Spot> parseDxLine(const QString& line, const QDateTime& now = QDateTime::currentDateTimeUtc());
// Una riga della risposta a SH/DX: "  14074.0 KH8WW  17-Sep-2026 1231Z FT8  <W4EU>".
std::optional<Spot> parseShowDxLine(const QString& line, const QDateTime& now = QDateTime::currentDateTimeUtc());
// Una riga JSON di HamAlert (telnet dopo "set/json").
std::optional<Spot> parseHamAlertJson(const QByteArray& line, const QDateTime& now = QDateTime::currentDateTimeUtc());
// La risposta di api.pota.app/spot/activator.
QList<Spot> parsePotaJson(const QByteArray& json);

// Il modo dal commento ("FT8 -12 dB", "CW 22 WPM", "USB") o, se non lo dice, dal
// piano di banda: le frequenze FT8/FT4/FT2 note, i segmenti CW e fonia IARU.
QString modeFor(double freqKhz, const QString& comment);
// Gruppo del modo per i confronti col log: FT2, FT8, FT4 restano se stessi,
// SSB/USB/LSB/AM/FM diventano PHONE.
QString modeKey(const QString& mode);
// Dove sintonizzare per uno spot: per FT8/FT4/FT2 la frequenza di chiamata (dial)
// e lo spostamento audio dello spot; per gli altri modi la frequenza stessa.
struct Tuning {
    double dialKhz{0.0};
    int audioHz{0};
};
Tuning tuningFor(double freqKhz, const QString& mode);
// Referenze POTA/SOTA/WWFF/IOTA e locatore scritti nel commento.
void extractReferences(Spot& spot);

} // namespace spots

// Quello che il log sa, in memoria per rispondere a migliaia di spot l'ora senza
// una query ciascuno. Si ricostruisce quando il log cambia.
class LogIndex {
public:
    void rebuild(const LogDatabase& db, bool confirmLotw, bool confirmCard, bool confirmEqsl);
    void clear();
    // Stato di uno spot la cui entita' e' `dxcc` (0 se sconosciuta).
    int status(const Spot& spot, int dxcc) const;
    int qsoCount() const { return m_qsos; }

private:
    QSet<QString> m_calls;          // CALL
    QSet<QString> m_callBand;       // CALL|band
    QSet<QString> m_callBandMode;   // CALL|band|modeKey
    QSet<int>     m_dxcc;
    QSet<QString> m_dxccBand;       // dxcc|band
    QSet<QString> m_dxccMode;       // dxcc|modeKey
    QSet<QString> m_dxccSlot;       // dxcc|band|modeKey
    QSet<int>     m_dxccConfirmed;
    int m_qsos{0};
};

struct SpotFilter {
    QSet<QString> bands;            // vuoto = tutte
    QSet<QString> modes;            // FT8 FT4 FT2 CW SSB RTTY DIGI
    QSet<QString> dxContinents;     // EU NA SA AS AF OC AN
    QSet<QString> spotterContinents;
    QSet<QString> sources;          // cluster rbn hamalert pota
    QSet<int>     dxcc;
    int  anyStatus{0};              // almeno uno di questi stati (0 = nessun vincolo)
    bool hideWorkedBand{false};     // nasconde chi e' gia' nel log su questa banda
    bool onlyActivations{false};    // POTA, SOTA, WWFF o IOTA
    bool onlyLotw{false};
    bool skimmers{true};            // spot da skimmer (spotter con '#')
    int  minSnr{-99};               // solo per gli spot con SNR
    int  maxAgeMinutes{30};
    QString calls;                  // "VP8*, 3Y0J, *BOUVET*": nominativi con * e ?
    QString text;                   // cerca in nominativo, entita', commento

    bool matches(const EnrichedSpot& spot, const QDateTime& now) const;
    bool isEmpty() const;
    QVariantMap toMap() const;
    static SpotFilter fromMap(const QVariantMap& map);
};

// Utenti LoTW con la data dell'ultimo upload (lotw-user-activity.csv di ARRL).
class LotwUsers {
public:
    int load(const QByteArray& csv);
    bool isEmpty() const { return m_lastUpload.isEmpty(); }
    int size() const { return static_cast<int>(m_lastUpload.size()); }
    // Ha caricato su LoTW negli ultimi `days` giorni?
    bool isActive(const QString& call, int days = 365, const QDate& today = QDate::currentDate()) const;

private:
    QHash<QString, QDate> m_lastUpload;
};

} // namespace decolog::core
