// DecoLog — il log su SQLite.
//
// Le colonne hanno i nomi ADIF (db/schema.sql). Tutto quello che arriva in ADIF e
// non ha una colonna finisce in qso.adif_extra come JSON: un import seguito da
// un export restituisce gli stessi campi con gli stessi valori. Gli stati QSL
// (LoTW, eQSL, cartolina, Club Log, QRZ) vanno nella tabella qsl_status e
// tornano campi ADIF all'export.
#pragma once

#include "core/Adif.h"

#include <QDateTime>
#include <QJsonArray>
#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <optional>

namespace decolog::core {

struct InsertResult {
    enum class Status { Inserted, Duplicate, Invalid, Error };
    Status  status{Status::Error};
    qint64  id{0};          // la riga scritta, o quella gia' presente se Duplicate
    QString message;
};

struct ImportResult {
    int inserted{0};
    int duplicates{0};
    int invalid{0};
    QStringList errors;     // i primi problemi, per mostrarli all'operatore
};

struct ConfirmationResult {
    enum class Status { Confirmed, AlreadyConfirmed, NotFound, Invalid, Error };
    Status  status{Status::NotFound};
    qint64  id{0};
    QString message;
};

struct WorkedEntry {
    QDateTime on;
    QString   band;
    QString   mode;         // il sottomodo se c'e': FT2, non MFSK
    QString   lotwRcvd;
};

struct WorkedBefore {
    int         count{0};
    QStringList bands;
    QStringList modes;
    QDateTime   last;
    QString     lastBand;
    QString     lastMode;
    QString     name;
    QString     qth;
    QString     gridsquare;
    QString     country;
    int         dxcc{0};
    int         cqz{0};
    int         ituz{0};
    qint64      lastId{0};
    QList<WorkedEntry> recent;   // le ultime cinque
};

struct QslState {
    QString service;        // lotw | qrz | clublog | eqsl | card
    QString sent{QStringLiteral("N")};
    QString sentDate;       // yyyyMMdd come in ADIF
    QString rcvd{QStringLiteral("N")};
    QString rcvdDate;
    QString remoteId;
    QString lastError;
    QString via;            // solo le cartacee: B bureau, D diretta, E elettronica
};

struct StationProfile {
    qint64  id{0};
    QString uuid;
    QString name;
    QString stationCallsign;
    QString operatorCall;
    QString myGridsquare;
    int     myCqZone{0};
    int     myItuZone{0};
    int     myDxcc{0};
    QString myRig;
    QString myAntenna;
    double  defaultTxPwr{0.0};
    QString lotwStationLocation;
    bool    isDefault{false};
    bool    deleted{false};
    bool    dirty{true};
    int     revision{1};
    int     qsoCount{0};
};

struct HistoryEntry {
    qint64    id{0};
    int       revision{0};
    QString   reason;
    QDateTime recordedAt;
    AdifRecord record;
};

struct QsoMeta {
    QString uuid;
    int     revision{0};
    QString source;
    QString sourceApp;
    QString createdAt;
    QString updatedAt;
    bool    dirty{false};
    bool    deleted{false};
    qint64  stationProfileId{0};
};

struct CountRow {
    QString key;
    int     count{0};
};

// Su cosa contare le statistiche: un modo (FT2, FT8, CW, SSB...), un anno, una
// banda. Vuoto o zero vuol dire "tutto".
struct StatsFilter {
    QString mode;
    QString band;
    int     year{0};
};

struct Ft2Award {
    int qsos{0};
    int dxccWorked{0};
    int dxccConfirmed{0};   // confermati su LoTW
    int gridsWorked{0};
    int gridsConfirmed{0};
};

class LogDatabase {
public:
    LogDatabase();
    ~LogDatabase();
    LogDatabase(const LogDatabase&) = delete;
    LogDatabase& operator=(const LogDatabase&) = delete;

    // Crea il file e lo schema se mancano. ":memory:" per i test.
    bool open(const QString& path);
    void close();
    bool isOpen() const;
    QString path() const { return m_path; }
    QString lastError() const { return m_lastError; }
    int schemaVersion() const;

    QSqlDatabase connection() const;

    // Finestra dei duplicati, in secondi: per i QSO dai programmi digitali e per
    // quelli scritti a mano (l'ora a mano e' approssimativa).
    void setDedupWindows(int digitalSeconds, int manualSeconds);
    int dedupWindowSeconds(bool manual) const { return manual ? m_dedupManual : m_dedupDigital; }

    // ── QSO ──────────────────────────────────────────────────────────────────
    InsertResult insertQso(const AdifRecord& record,
                           const QString& source,
                           const QString& sourceApp = {},
                           bool manual = false,
                           qint64 stationProfileId = 0);

    // Riscrive un QSO: la versione precedente va in qso_history, revision sale di
    // uno, dirty torna a 1. `stationProfileId` < 0 lascia il profilo com'e'.
    InsertResult updateQso(qint64 id, const AdifRecord& record,
                           qint64 stationProfileId = -1,
                           const QString& reason = QStringLiteral("edit"));

    // Cancellazione morbida: la riga resta, marcata deleted, e lo storico la
    // conserva.
    bool softDeleteQso(qint64 id);

    // Il record ADIF completo di un QSO: colonne, stati QSL e adif_extra.
    std::optional<AdifRecord> record(qint64 id) const;
    std::optional<QsoMeta> meta(qint64 id) const;
    QList<QslState> qslStatus(qint64 id) const;
    QList<HistoryEntry> history(qint64 id) const;
    // Rimette in vigore una versione dello storico (diventa una nuova revisione).
    InsertResult restoreRevision(qint64 id, qint64 historyId);

    // Una conferma arrivata da un servizio ("lotw", "qrz", "eqsl"): il record ha
    // CALL, BAND (o FREQ), MODE/SUBMODE, QSO_DATE, TIME_ON e QSLRDATE, piu' i
    // dettagli che il servizio conosce (GRIDSQUARE, CQZ, ITUZ, DXCC, STATE, CNTY,
    // IOTA). Il QSO si cerca per nominativo, banda, gruppo di modi e ora entro
    // `windowSeconds` (LoTW tollera mezz'ora); diventa confermato con una nuova
    // revisione, e i dettagli riempiono solo i campi vuoti.
    ConfirmationResult applyConfirmation(const QString& service, const AdifRecord& confirmation,
                                         int windowSeconds = 1800);

    ImportResult importAdif(const QByteArray& data, const QString& source = QStringLiteral("import"),
                            qint64 stationProfileId = 0);
    QByteArray   exportAdif(const QString& programVersion = {}) const;
    QByteArray   exportAdif(const QList<qint64>& ids, const QString& programVersion = {}) const;

    int qsoCount() const;
    int dirtyCount() const;
    int conflictCount() const;
    WorkedBefore workedBefore(const QString& call) const;

    // Bande e modi (FT2, non MFSK) in cui un'entita' DXCC e' gia' stata lavorata.
    struct DxccWorked { int count{0}; QStringList bands; QStringList modes; };
    DxccWorked dxccWorked(int dxcc) const;
    // Tutti i QSO in forma compatta per DecoLink: [call, banda, modo, data yyyyMMdd,
    // locatore a 4, confermato 0/1], con le conferme accettate indicate.
    QList<QJsonArray> workedRows(bool confirmLotw, bool confirmCard, bool confirmEqsl) const;
    static QJsonArray workedRow(const QString& call, const QString& band, const QString& mode,
                                const QString& submode, const QString& isoOn, const QString& grid, bool confirmed);
    // QSO senza numero DXCC, per completarli dal cty.csv.
    QList<qint64> idsWithoutDxcc() const;

    // E' il primo QSO FT2 con il suo DXCC, in ordine di tempo?
    bool isFirstFt2Dxcc(qint64 id) const;
    Ft2Award ft2Award() const;
    QList<CountRow> countByBand() const;
    QList<CountRow> countByMode() const;
    // QSO per numero DXCC (chiave = numero), dal piu' lavorato.
    QList<CountRow> countByDxcc() const;
    // Statistiche nel tempo: chiave "2026", "2026-09", "00".."23", continente.
    QList<CountRow> countByYear(const StatsFilter& filter = {}) const;
    QList<CountRow> countByMonth(int months, const StatsFilter& filter = {}) const;
    QList<CountRow> countByHour(const StatsFilter& filter = {}) const;
    QList<CountRow> countByContinent(const StatsFilter& filter = {}) const;
    QList<CountRow> countByBand(const StatsFilter& filter) const;
    QList<CountRow> countByMode(const StatsFilter& filter) const;
    // Banda per ora UTC, per la mappa di calore: [{band, hour, count}].
    QList<QVariantMap> bandByHour(const StatsFilter& filter = {}) const;
    // Totali: QSO, nominativi, entita', primo e ultimo QSO, giorno e ora migliori.
    QVariantMap statsSummary(const StatsFilter& filter = {}) const;
    // Gli anni presenti nel log, dal piu' recente.
    QStringList yearsInLog() const;
    // Per servizio: in coda (R/Q), inviati (Y), confermati (rcvd Y).
    QList<QVariantMap> qslSummary() const;
    // Locatori a quattro caratteri gia' lavorati, per la mappa.
    QStringList workedGrids(int limit = 4000) const;

    // ── Invio QSL ────────────────────────────────────────────────────────────
    // Lo stato di un servizio su un QSO, senza creare una revisione: l'invio di una
    // QSL non cambia il QSO, cambia quello che se ne e' fatto.
    bool setQslState(qint64 id, const QslState& state);
    bool writeQslState(qint64 id, const QslState& state, bool includeReceived);
    // I QSO ancora da mandare a un servizio (nessuna riga, o "N"/"R"/"Q"), dal piu'
    // vecchio. `limit` 0 = tutti.
    QList<qint64> qsosToUpload(const QString& service, int limit = 0) const;
    int uploadPendingCount(const QString& service) const;

    // ── Sync con DecoLog Cloud ───────────────────────────────────────────────
    // I QSO ancora da mandare, dal piu' vecchio. `limit` 0 = tutti.
    QList<qint64> dirtyQsos(int limit = 0) const;
    // Il QSO come lo vuole il Cloud: uuid, revisione, le chiavi del confronto e
    // tutti i campi ADIF dentro "fields".
    QVariantMap syncRecord(qint64 id) const;
    qint64 idForUuid(const QString& uuid) const;
    // Il server l'ha preso: esce dalla coda, con la revisione che dice lui
    // (che puo' essere piu' alta, se ha risolto un conflitto).
    bool markSynced(qint64 id, int revision);
    // Il server dice che quel collegamento da lui sta sotto un altro uuid: ci si
    // allinea, o si toglie di mezzo il doppione se quell'uuid c'e' gia'.
    bool adoptUuid(qint64 id, const QString& serverUuid);
    // Un QSO che arriva dal Cloud: si scrive senza rimetterlo in coda.
    enum class RemoteResult { Inserted, Updated, Deleted, Skipped, Failed };
    RemoteResult applyRemote(const QVariantMap& record);
    // Il log non e' solo i QSO: i profili stazione viaggiano come documenti,
    // con la stessa regola di revisione.
    QList<QVariantMap> dirtyProfiles() const;
    bool markProfileSynced(const QString& uuid, int revision);
    RemoteResult applyRemoteProfile(const QVariantMap& document);

    // Dove siamo arrivati col sync di questo account.
    QVariantMap syncState(const QString& account) const;
    void setSyncState(const QString& account, const QVariantMap& values);

    // ── QSL di carta ─────────────────────────────────────────────────────────
    // La coda delle cartacee: `state` e' "queue" (da mandare), "sent", "received"
    // o "all". Ogni riga ha quello che serve a un'etichetta e alla tabella.
    QList<QVariantMap> cardRows(const QString& state, int limit = 0) const;
    // Lo stato di una cartacea, conferma compresa (setQslState non tocca il
    // ricevuto, perche' per gli altri servizi lo scrive il download).
    bool setCardState(qint64 id, const QslState& state);

    // ── Etichette ────────────────────────────────────────────────────────────
    // Le etichette di un QSO (attivazione, contest, evento, portatile) stanno in
    // APP_DECOLOG_TAGS, separate da virgola: tornano uguali in un export ADIF.
    static QStringList splitTags(const QString& tags);
    static QString joinTags(const QStringList& tags);
    // Tutte le etichette del log con quanti QSO le hanno, dalla piu' usata.
    QList<CountRow> tagCounts() const;
    // Aggiunge o toglie un'etichetta a piu' QSO; ognuno cambiato diventa una
    // nuova revisione. Restituisce quanti QSO sono cambiati.
    int setTag(const QList<qint64>& ids, const QString& tag, bool add);

    // ── Profili stazione ─────────────────────────────────────────────────────
    QList<StationProfile> stationProfiles(bool includeDeleted = true) const;
    std::optional<StationProfile> stationProfile(qint64 id) const;
    // Crea (id 0) o aggiorna. Restituisce l'id, 0 in caso d'errore.
    qint64 saveStationProfile(const StationProfile& profile);
    bool deleteStationProfile(qint64 id);
    // Il profilo che corrisponde a un nominativo di stazione (il predefinito, se piu' d'uno).
    qint64 profileForCallsign(const QString& stationCallsign) const;

    // ── Impostazioni e manutenzione ──────────────────────────────────────────
    QString setting(const QString& key, const QString& fallback = {}) const;
    void    setSetting(const QString& key, const QString& value);
    // Copia coerente del database, anche mentre e' aperto (VACUUM INTO).
    bool backupTo(const QString& filePath);

    // Data e ora ADIF (QSO_DATE + TIME_ON) in ISO-8601 UTC, come nello schema.
    static QString isoFromAdif(const QString& date, const QString& time);

private:
    struct Prepared;
    std::optional<Prepared> prepare(const AdifRecord& input, InsertResult& error) const;
    bool writeQsl(qint64 qsoId, const QList<QslState>& states, bool keepRemote);
    QString snapshotJson(qint64 id) const;
    bool applySchema();
    bool migrate();
    std::optional<qint64> findDuplicate(const QString& call, const QString& band,
                                        const QString& mode, const QString& submode,
                                        const QDateTime& on, int windowSeconds) const;

    QString m_connectionName;
    QString m_path;
    int m_dedupDigital{120};
    int m_dedupManual{600};
    mutable QString m_lastError;
};

} // namespace decolog::core
