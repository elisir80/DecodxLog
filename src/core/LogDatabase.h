// DecoLog — il log su SQLite.
//
// Le colonne hanno i nomi ADIF (db/schema.sql). Tutto quello che arriva in ADIF e
// non ha una colonna finisce in qso.adif_extra come JSON: un import seguito da
// un export restituisce gli stessi campi con gli stessi valori.
#pragma once

#include "core/Adif.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <optional>

namespace decolog::core {

struct InsertResult {
    enum class Status { Inserted, Duplicate, Invalid, Error };
    Status  status{Status::Error};
    qint64  id{0};          // la riga inserita, o quella gia' presente se Duplicate
    QString message;
};

struct ImportResult {
    int inserted{0};
    int duplicates{0};
    int invalid{0};
    QStringList errors;     // i primi problemi, per mostrarli all'operatore
};

struct WorkedBefore {
    int         count{0};
    QStringList bands;
    QStringList modes;
    QDateTime   last;
    QString     lastBand;
    QString     lastMode;
    QString     name;
    QString     gridsquare;
    QString     country;
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

    // `manual` allarga la finestra dei duplicati a ±10 minuti: l'ora di un QSO
    // scritto a mano e' approssimativa, quella di Decodium no.
    InsertResult insertQso(const AdifRecord& record,
                           const QString& source,
                           const QString& sourceApp = {},
                           bool manual = false);

    // Il record ADIF completo di un QSO: colonne piu' adif_extra.
    std::optional<AdifRecord> record(qint64 id) const;

    ImportResult importAdif(const QByteArray& data, const QString& source = QStringLiteral("import"));
    QByteArray   exportAdif(const QString& programVersion = {}) const;

    int qsoCount() const;
    int dirtyCount() const;
    WorkedBefore workedBefore(const QString& call) const;

    // Data e ora ADIF (QSO_DATE + TIME_ON) in ISO-8601 UTC, come nello schema.
    static QString isoFromAdif(const QString& date, const QString& time);

private:
    bool applySchema();
    std::optional<qint64> findDuplicate(const QString& call, const QString& band,
                                        const QString& mode, const QString& submode,
                                        const QDateTime& on, int windowSeconds) const;

    QString m_connectionName;
    QString m_path;
    mutable QString m_lastError;
};

} // namespace decolog::core
