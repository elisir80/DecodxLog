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
    // QSO senza numero DXCC, per completarli dal cty.csv.
    QList<qint64> idsWithoutDxcc() const;

    // E' il primo QSO FT2 con il suo DXCC, in ordine di tempo?
    bool isFirstFt2Dxcc(qint64 id) const;
    Ft2Award ft2Award() const;
    QList<CountRow> countByBand() const;
    QList<CountRow> countByMode() const;
    // Per servizio: in coda (R/Q), inviati (Y), confermati (rcvd Y).
    QList<QVariantMap> qslSummary() const;
    // Locatori a quattro caratteri gia' lavorati, per la mappa.
    QStringList workedGrids(int limit = 4000) const;

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
