#include "core/LogDatabase.h"

#include "core/Bands.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimeZone>
#include <QUuid>
#include <QVariant>
#include <algorithm>
#include <array>

namespace decolog::core {

namespace {

enum class Kind { Text, Upper, Lower, Real, Integer };

struct Column {
    const char* adif;
    const char* column;
    Kind kind;
};

// I campi ADIF che hanno una colonna. CALL, BAND e MODE sono obbligatori; date e
// ore sono trattate a parte perche' due campi ADIF diventano una colonna sola.
constexpr std::array kColumns{
    Column{"CALL", "call", Kind::Upper},
    Column{"BAND", "band", Kind::Lower},
    Column{"BAND_RX", "band_rx", Kind::Lower},
    Column{"FREQ", "freq", Kind::Real},
    Column{"FREQ_RX", "freq_rx", Kind::Real},
    Column{"MODE", "mode", Kind::Upper},
    Column{"SUBMODE", "submode", Kind::Upper},
    Column{"RST_SENT", "rst_sent", Kind::Text},
    Column{"RST_RCVD", "rst_rcvd", Kind::Text},
    Column{"GRIDSQUARE", "gridsquare", Kind::Text},
    Column{"NAME", "name", Kind::Text},
    Column{"QTH", "qth", Kind::Text},
    Column{"COUNTRY", "country", Kind::Text},
    Column{"DXCC", "dxcc", Kind::Integer},
    Column{"CQZ", "cqz", Kind::Integer},
    Column{"ITUZ", "ituz", Kind::Integer},
    Column{"CONT", "cont", Kind::Text},
    Column{"STATE", "state", Kind::Text},
    Column{"CNTY", "cnty", Kind::Text},
    Column{"IOTA", "iota", Kind::Text},
    Column{"SOTA_REF", "sota_ref", Kind::Text},
    Column{"POTA_REF", "pota_ref", Kind::Text},
    Column{"WWFF_REF", "wwff_ref", Kind::Text},
    Column{"PROP_MODE", "prop_mode", Kind::Text},
    Column{"SAT_NAME", "sat_name", Kind::Text},
    Column{"TX_PWR", "tx_pwr", Kind::Real},
    Column{"COMMENT", "comment", Kind::Text},
    Column{"NOTES", "notes", Kind::Text},
};

// Campi ADIF degli stati QSL, un servizio per riga di qsl_status. Club Log non
// ha un "ricevuto" in ADIF.
struct QslFields {
    const char* service;
    const char* sent;
    const char* sentDate;
    const char* rcvd;
    const char* rcvdDate;
};

constexpr std::array kQslFields{
    QslFields{"lotw", "LOTW_QSL_SENT", "LOTW_QSLSDATE", "LOTW_QSL_RCVD", "LOTW_QSLRDATE"},
    QslFields{"qrz", "QRZCOM_QSO_UPLOAD_STATUS", "QRZCOM_QSO_UPLOAD_DATE",
              "QRZCOM_QSO_DOWNLOAD_STATUS", "QRZCOM_QSO_DOWNLOAD_DATE"},
    QslFields{"clublog", "CLUBLOG_QSO_UPLOAD_STATUS", "CLUBLOG_QSO_UPLOAD_DATE", "", ""},
    QslFields{"eqsl", "EQSL_QSL_SENT", "EQSL_QSLSDATE", "EQSL_QSL_RCVD", "EQSL_QSLRDATE"},
    QslFields{"card", "QSL_SENT", "QSLSDATE", "QSL_RCVD", "QSLRDATE"},
};

const Column* columnFor(const QString& adifName)
{
    for (const auto& c : kColumns) {
        if (adifName == QLatin1String(c.adif))
            return &c;
    }
    return nullptr;
}

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QDateTime parseIso(const QString& iso)
{
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    dt.setTimeZone(QTimeZone::UTC);
    return dt;
}

// Il valore nella forma che va in colonna. nullopt = non rappresentabile (es.
// DXCC non numerico): il campo resta intatto in adif_extra.
std::optional<QVariant> toColumnValue(const Column& c, const QString& value)
{
    switch (c.kind) {
    case Kind::Text:
        return QVariant(value);
    case Kind::Upper:
        return QVariant(value.trimmed().toUpper());
    case Kind::Lower:
        return QVariant(value.trimmed().toLower());
    case Kind::Real: {
        bool ok = false;
        const double v = value.trimmed().toDouble(&ok);
        if (!ok)
            return std::nullopt;
        return QVariant(v);
    }
    case Kind::Integer: {
        bool ok = false;
        const qlonglong v = value.trimmed().toLongLong(&ok);
        if (!ok)
            return std::nullopt;
        return QVariant(v);
    }
    }
    return std::nullopt;
}

QString fromColumnValue(const Column& c, const QVariant& v)
{
    if (v.isNull())
        return {};
    switch (c.kind) {
    case Kind::Real:
        // Frequenze in MHz con 6 decimali; le potenze senza zeri inutili.
        if (qstrcmp(c.column, "tx_pwr") == 0)
            return QString::number(v.toDouble());
        return QString::number(v.toDouble(), 'f', 6);
    case Kind::Integer:
        return QString::number(v.toLongLong());
    default:
        return v.toString();
    }
}

void addDateTime(AdifRecord& r, const QString& iso, const char* dateField, const char* timeField)
{
    if (iso.isEmpty())
        return;
    const QDateTime dt = parseIso(iso);
    if (!dt.isValid())
        return;
    r.set(QLatin1String(dateField), dt.toString(QStringLiteral("yyyyMMdd")));
    r.set(QLatin1String(timeField), dt.toString(QStringLiteral("HHmmss")));
}

bool isStatusFlag(const QString& v)
{
    return v.size() == 1 && v.at(0).isLetter();
}

// Il modo come lo legge un operatore: FT2 e non MFSK, ma SSB e non USB.
QString displayMode(const QString& mode, const QString& submode)
{
    return submode.isEmpty() || mode == QLatin1String("SSB") ? mode : submode;
}

AdifRecord recordFromSnapshot(const QString& json)
{
    AdifRecord r;
    const QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();
    const QJsonArray fields = obj.value(QStringLiteral("fields")).toArray();
    for (const auto& f : fields) {
        const QJsonArray pair = f.toArray();
        if (pair.size() == 2)
            r.set(pair.at(0).toString(), pair.at(1).toString());
    }
    return r;
}

} // namespace

// Un record ADIF gia' tradotto in colonne, stati QSL e campi extra.
struct LogDatabase::Prepared {
    QString call;
    QString band;
    QString mode;
    QString submode;
    QString on;
    QString off;
    QList<QPair<QString, QVariant>> columns;   // solo le colonne ADIF di kColumns
    QString extraJson;
    QList<QslState> qsl;
};

LogDatabase::LogDatabase()
    : m_connectionName(QStringLiteral("decolog-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

LogDatabase::~LogDatabase()
{
    close();
}

QSqlDatabase LogDatabase::connection() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

bool LogDatabase::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName) && connection().isOpen();
}

bool LogDatabase::open(const QString& path)
{
    close();
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(path);
    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }
    m_path = path;

    QSqlQuery q(db);
    // Per connessione, non per file: vanno ripetute a ogni apertura.
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    q.exec(QStringLiteral("PRAGMA busy_timeout = 3000"));

    if (!applySchema()) {
        close();
        return false;
    }
    return true;
}

void LogDatabase::close()
{
    if (!QSqlDatabase::contains(m_connectionName))
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

int LogDatabase::schemaVersion() const
{
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral("SELECT MAX(version) FROM schema_version")) && q.next())
        return q.value(0).toInt();
    return 0;
}

void LogDatabase::setDedupWindows(int digitalSeconds, int manualSeconds)
{
    m_dedupDigital = qMax(0, digitalSeconds);
    m_dedupManual = qMax(0, manualSeconds);
}

bool LogDatabase::applySchema()
{
    QSqlDatabase db = connection();
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type='table' AND name='schema_version'"))
        && q.next()) {
        return true;
    }

    QFile file(QStringLiteral(":/decolog/schema.sql"));
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("schema.sql missing from resources");
        return false;
    }

    // Via i commenti, poi un'istruzione per ';'. Lo schema non ha trigger, quindi
    // non ci sono ';' dentro le istruzioni.
    QString sql;
    for (const QString& line : QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'))) {
        const qsizetype comment = line.indexOf(QLatin1String("--"));
        sql += (comment >= 0 ? line.left(comment) : line) + QLatin1Char('\n');
    }

    QStringList pragmas;
    QStringList statements;
    for (const QString& raw : sql.split(QLatin1Char(';'))) {
        const QString stmt = raw.trimmed();
        if (stmt.isEmpty())
            continue;
        (stmt.startsWith(QLatin1String("PRAGMA"), Qt::CaseInsensitive) ? pragmas : statements) << stmt;
    }

    // journal_mode non si puo' cambiare dentro una transazione.
    for (const QString& p : pragmas)
        q.exec(p);

    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    for (const QString& stmt : statements) {
        if (!q.exec(stmt)) {
            m_lastError = q.lastError().text() + QStringLiteral(" in: ") + stmt.left(80);
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

QString LogDatabase::isoFromAdif(const QString& date, const QString& time)
{
    const QDate d = QDate::fromString(date.trimmed(), QStringLiteral("yyyyMMdd"));
    if (!d.isValid())
        return {};
    const QString t = time.trimmed();
    QTime tm;
    if (t.size() == 6)
        tm = QTime::fromString(t, QStringLiteral("HHmmss"));
    else if (t.size() == 4)
        tm = QTime::fromString(t, QStringLiteral("HHmm"));
    if (!tm.isValid())
        return {};
    return QDateTime(d, tm, QTimeZone::UTC).toString(Qt::ISODate);
}

std::optional<qint64> LogDatabase::findDuplicate(const QString& call, const QString& band,
                                                 const QString& mode, const QString& submode,
                                                 const QDateTime& on, int windowSeconds) const
{
    // Il formato ISO a lunghezza fissa si confronta come stringa: l'indice
    // idx_qso_dedup lavora senza conversioni.
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT id FROM qso WHERE deleted = 0 AND call = ? AND band = ? AND mode = ? "
        "AND IFNULL(submode, '') = ? AND qso_datetime_on BETWEEN ? AND ? LIMIT 1"));
    q.addBindValue(call);
    q.addBindValue(band);
    q.addBindValue(mode);
    q.addBindValue(submode);
    q.addBindValue(on.addSecs(-windowSeconds).toString(Qt::ISODate));
    q.addBindValue(on.addSecs(windowSeconds).toString(Qt::ISODate));
    if (q.exec() && q.next())
        return q.value(0).toLongLong();
    return std::nullopt;
}

std::optional<LogDatabase::Prepared> LogDatabase::prepare(const AdifRecord& input, InsertResult& error) const
{
    AdifRecord record = input;
    adif::normalizeMode(record);

    // La banda si ricava dalla frequenza quando manca: molti log la omettono.
    if (record.value(QStringLiteral("BAND")).isEmpty()) {
        bool ok = false;
        const double mhz = record.value(QStringLiteral("FREQ")).toDouble(&ok);
        if (ok)
            record.set(QStringLiteral("BAND"), bands::fromMhz(mhz));
    }

    Prepared p;
    p.call = record.value(QStringLiteral("CALL")).trimmed().toUpper();
    p.band = record.value(QStringLiteral("BAND")).trimmed().toLower();
    p.mode = record.value(QStringLiteral("MODE")).trimmed().toUpper();
    p.submode = record.value(QStringLiteral("SUBMODE")).trimmed().toUpper();
    p.on = isoFromAdif(record.value(QStringLiteral("QSO_DATE")), record.value(QStringLiteral("TIME_ON")));
    p.off = isoFromAdif(record.value(QStringLiteral("QSO_DATE_OFF")), record.value(QStringLiteral("TIME_OFF")));

    QStringList missing;
    if (p.call.isEmpty()) missing << QStringLiteral("CALL");
    if (p.on.isEmpty())   missing << QStringLiteral("QSO_DATE/TIME_ON");
    if (p.band.isEmpty()) missing << QStringLiteral("BAND/FREQ");
    if (p.mode.isEmpty()) missing << QStringLiteral("MODE");
    if (!missing.isEmpty()) {
        error.status = InsertResult::Status::Invalid;
        error.message = QStringLiteral("%1: missing %2")
                            .arg(p.call.isEmpty() ? QStringLiteral("?") : p.call,
                                 missing.join(QStringLiteral(", ")));
        return std::nullopt;
    }

    // Stati QSL: in tabella solo quelli diversi da "N", che e' il valore
    // predefinito. Un "N" esplicito resta in adif_extra, cosi' l'export
    // restituisce esattamente quello che e' entrato.
    QSet<QString> consumed;
    for (const auto& f : kQslFields) {
        QslState st;
        st.service = QLatin1String(f.service);
        bool any = false;
        const QString sent = record.value(QLatin1String(f.sent)).trimmed().toUpper();
        if (isStatusFlag(sent) && sent != QLatin1String("N")) {
            st.sent = sent;
            consumed << QLatin1String(f.sent);
            const QString date = record.value(QLatin1String(f.sentDate));
            if (!date.isEmpty()) {
                st.sentDate = date;
                consumed << QLatin1String(f.sentDate);
            }
            any = true;
        }
        if (*f.rcvd) {
            const QString rcvd = record.value(QLatin1String(f.rcvd)).trimmed().toUpper();
            if (isStatusFlag(rcvd) && rcvd != QLatin1String("N")) {
                st.rcvd = rcvd;
                consumed << QLatin1String(f.rcvd);
                const QString date = record.value(QLatin1String(f.rcvdDate));
                if (!date.isEmpty()) {
                    st.rcvdDate = date;
                    consumed << QLatin1String(f.rcvdDate);
                }
                any = true;
            }
        }
        if (any)
            p.qsl << st;
    }

    QJsonObject extra;
    QSet<QString> used;
    for (const auto& f : record.fields()) {
        // Date e ore valide stanno nelle colonne; se non si leggono restano come
        // sono, cosi' l'export non le perde.
        if (f.name == QLatin1String("QSO_DATE") || f.name == QLatin1String("TIME_ON"))
            continue;
        if ((f.name == QLatin1String("QSO_DATE_OFF") || f.name == QLatin1String("TIME_OFF")) && !p.off.isEmpty())
            continue;
        if (consumed.contains(f.name))
            continue;
        const Column* c = columnFor(f.name);
        const auto value = c ? toColumnValue(*c, f.value) : std::nullopt;
        if (c && value && !used.contains(f.name)) {
            used << f.name;
            p.columns.append({QLatin1String(c->column), *value});
        } else {
            extra.insert(f.name, f.value);
        }
    }
    if (!extra.isEmpty())
        p.extraJson = QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
    return p;
}

bool LogDatabase::writeQsl(qint64 qsoId, const QList<QslState>& states, bool keepRemote)
{
    QSqlDatabase db = connection();
    QHash<QString, QPair<QString, QString>> remote;
    if (keepRemote) {
        QSqlQuery r(db);
        r.prepare(QStringLiteral("SELECT service, remote_id, last_error FROM qsl_status WHERE qso_id = ?"));
        r.addBindValue(qsoId);
        if (r.exec()) {
            while (r.next())
                remote.insert(r.value(0).toString(), {r.value(1).toString(), r.value(2).toString()});
        }
    }

    QSqlQuery del(db);
    del.prepare(QStringLiteral("DELETE FROM qsl_status WHERE qso_id = ?"));
    del.addBindValue(qsoId);
    if (!del.exec())
        return false;

    QSet<QString> written;
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO qsl_status (qso_id, service, sent, sent_date, rcvd, rcvd_date, remote_id, last_error) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    auto nullable = [](const QString& s) { return s.isEmpty() ? QVariant() : QVariant(s); };
    for (const QslState& st : states) {
        const auto kept = remote.value(st.service);
        ins.addBindValue(qsoId);
        ins.addBindValue(st.service);
        ins.addBindValue(st.sent);
        ins.addBindValue(nullable(st.sentDate));
        ins.addBindValue(st.rcvd);
        ins.addBindValue(nullable(st.rcvdDate));
        ins.addBindValue(nullable(st.remoteId.isEmpty() ? kept.first : st.remoteId));
        ins.addBindValue(nullable(st.lastError.isEmpty() ? kept.second : st.lastError));
        if (!ins.exec())
            return false;
        written << st.service;
    }
    // Un errore di upload o un id remoto non si perdono solo perche' lo stato e'
    // tornato "N".
    for (auto it = remote.cbegin(); it != remote.cend(); ++it) {
        if (written.contains(it.key()) || (it.value().first.isEmpty() && it.value().second.isEmpty()))
            continue;
        ins.addBindValue(qsoId);
        ins.addBindValue(it.key());
        ins.addBindValue(QStringLiteral("N"));
        ins.addBindValue(QVariant());
        ins.addBindValue(QStringLiteral("N"));
        ins.addBindValue(QVariant());
        ins.addBindValue(nullable(it.value().first));
        ins.addBindValue(nullable(it.value().second));
        if (!ins.exec())
            return false;
    }
    return true;
}

InsertResult LogDatabase::insertQso(const AdifRecord& input, const QString& source,
                                    const QString& sourceApp, bool manual, qint64 stationProfileId)
{
    InsertResult result;
    if (!isOpen()) {
        result.message = QStringLiteral("database not open");
        return result;
    }

    const auto p = prepare(input, result);
    if (!p)
        return result;

    if (const auto dup = findDuplicate(p->call, p->band, p->mode, p->submode, parseIso(p->on),
                                       dedupWindowSeconds(manual))) {
        result.status = InsertResult::Status::Duplicate;
        result.id = *dup;
        result.message = QStringLiteral("%1 %2 %3: already in log")
                             .arg(p->call, p->band, displayMode(p->mode, p->submode));
        return result;
    }

    const QString now = nowIso();
    QStringList columns{QStringLiteral("uuid"), QStringLiteral("qso_datetime_on"),
                        QStringLiteral("qso_datetime_off"), QStringLiteral("source"),
                        QStringLiteral("source_app"), QStringLiteral("created_at"),
                        QStringLiteral("updated_at"), QStringLiteral("station_profile_id"),
                        QStringLiteral("adif_extra")};
    QVariantList values{QUuid::createUuid().toString(QUuid::WithoutBraces), p->on,
                        p->off.isEmpty() ? QVariant() : QVariant(p->off), source,
                        sourceApp.isEmpty() ? QVariant() : QVariant(sourceApp), now, now,
                        stationProfileId > 0 ? QVariant(stationProfileId) : QVariant(),
                        p->extraJson.isEmpty() ? QVariant() : QVariant(p->extraJson)};
    for (const auto& [column, value] : p->columns) {
        columns << column;
        values << value;
    }

    QSqlDatabase db = connection();
    const bool ownTransaction = db.transaction();
    QSqlQuery q(db);
    QStringList marks;
    marks.fill(QStringLiteral("?"), columns.size());
    q.prepare(QStringLiteral("INSERT INTO qso (%1) VALUES (%2)")
                  .arg(columns.join(QLatin1Char(',')), marks.join(QLatin1Char(','))));
    for (const auto& v : values)
        q.addBindValue(v);
    if (!q.exec()) {
        if (ownTransaction)
            db.rollback();
        result.status = InsertResult::Status::Error;
        result.message = q.lastError().text();
        m_lastError = result.message;
        return result;
    }
    result.id = q.lastInsertId().toLongLong();
    if (!p->qsl.isEmpty() && !writeQsl(result.id, p->qsl, false)) {
        if (ownTransaction)
            db.rollback();
        result.status = InsertResult::Status::Error;
        result.message = QStringLiteral("cannot write QSL status");
        return result;
    }
    if (ownTransaction)
        db.commit();
    result.status = InsertResult::Status::Inserted;
    return result;
}

QString LogDatabase::snapshotJson(qint64 id) const
{
    const auto r = record(id);
    const auto m = meta(id);
    if (!r || !m)
        return {};
    QJsonArray fields;
    for (const auto& f : r->fields())
        fields.append(QJsonArray{f.name, f.value});
    QJsonObject obj{
        {QStringLiteral("fields"), fields},
        {QStringLiteral("uuid"), m->uuid},
        {QStringLiteral("revision"), m->revision},
        {QStringLiteral("source"), m->source},
        {QStringLiteral("source_app"), m->sourceApp},
        {QStringLiteral("station_profile_id"), m->stationProfileId},
        {QStringLiteral("updated_at"), m->updatedAt},
    };
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

InsertResult LogDatabase::updateQso(qint64 id, const AdifRecord& input, qint64 stationProfileId,
                                    const QString& reason)
{
    InsertResult result;
    result.id = id;
    const auto m = meta(id);
    if (!m) {
        result.message = QStringLiteral("QSO %1 not found").arg(id);
        return result;
    }
    const auto p = prepare(input, result);
    if (!p)
        return result;

    QSqlDatabase db = connection();
    if (!db.transaction()) {
        result.message = db.lastError().text();
        return result;
    }

    QSqlQuery h(db);
    h.prepare(QStringLiteral(
        "INSERT INTO qso_history (qso_uuid, revision, snapshot, reason, recorded_at) VALUES (?, ?, ?, ?, ?)"));
    h.addBindValue(m->uuid);
    h.addBindValue(m->revision);
    h.addBindValue(snapshotJson(id));
    h.addBindValue(reason);
    h.addBindValue(nowIso());
    if (!h.exec()) {
        db.rollback();
        result.message = h.lastError().text();
        return result;
    }

    // Tutte le colonne ADIF: quelle che il record non ha piu' tornano NULL.
    QStringList sets{QStringLiteral("qso_datetime_on = ?"), QStringLiteral("qso_datetime_off = ?"),
                     QStringLiteral("adif_extra = ?"), QStringLiteral("revision = revision + 1"),
                     QStringLiteral("updated_at = ?"), QStringLiteral("dirty = 1")};
    QVariantList values{p->on, p->off.isEmpty() ? QVariant() : QVariant(p->off),
                        p->extraJson.isEmpty() ? QVariant() : QVariant(p->extraJson), nowIso()};
    for (const auto& c : kColumns) {
        const QLatin1String name(c.column);
        sets << name + QStringLiteral(" = ?");
        QVariant v;
        for (const auto& [column, value] : p->columns) {
            if (column == name) {
                v = value;
                break;
            }
        }
        values << v;
    }
    if (stationProfileId >= 0) {
        sets << QStringLiteral("station_profile_id = ?");
        values << (stationProfileId > 0 ? QVariant(stationProfileId) : QVariant());
    }

    QSqlQuery u(db);
    u.prepare(QStringLiteral("UPDATE qso SET %1 WHERE id = ?").arg(sets.join(QStringLiteral(", "))));
    for (const auto& v : values)
        u.addBindValue(v);
    u.addBindValue(id);
    if (!u.exec() || !writeQsl(id, p->qsl, true)) {
        result.message = u.lastError().text();
        db.rollback();
        return result;
    }
    if (!db.commit()) {
        result.message = db.lastError().text();
        return result;
    }
    result.status = InsertResult::Status::Inserted;
    return result;
}

bool LogDatabase::softDeleteQso(qint64 id)
{
    const auto m = meta(id);
    if (!m || m->deleted)
        return false;
    QSqlDatabase db = connection();
    db.transaction();
    QSqlQuery h(db);
    h.prepare(QStringLiteral(
        "INSERT INTO qso_history (qso_uuid, revision, snapshot, reason, recorded_at) VALUES (?, ?, ?, 'delete', ?)"));
    h.addBindValue(m->uuid);
    h.addBindValue(m->revision);
    h.addBindValue(snapshotJson(id));
    h.addBindValue(nowIso());
    QSqlQuery u(db);
    u.prepare(QStringLiteral(
        "UPDATE qso SET deleted = 1, dirty = 1, revision = revision + 1, updated_at = ? WHERE id = ?"));
    u.addBindValue(nowIso());
    u.addBindValue(id);
    if (!h.exec() || !u.exec()) {
        db.rollback();
        return false;
    }
    return db.commit();
}

std::optional<AdifRecord> LogDatabase::record(qint64 id) const
{
    QSqlQuery q(connection());
    q.prepare(QStringLiteral("SELECT * FROM qso WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return std::nullopt;

    const QSqlRecord row = q.record();
    AdifRecord r;
    r.set(QStringLiteral("CALL"), row.value(QStringLiteral("call")).toString());
    addDateTime(r, row.value(QStringLiteral("qso_datetime_on")).toString(), "QSO_DATE", "TIME_ON");
    addDateTime(r, row.value(QStringLiteral("qso_datetime_off")).toString(), "QSO_DATE_OFF", "TIME_OFF");
    for (const auto& c : kColumns)
        r.set(QLatin1String(c.adif), fromColumnValue(c, row.value(QLatin1String(c.column))));

    for (const QslState& st : qslStatus(id)) {
        for (const auto& f : kQslFields) {
            if (st.service != QLatin1String(f.service))
                continue;
            if (st.sent != QLatin1String("N"))
                r.set(QLatin1String(f.sent), st.sent);
            r.set(QLatin1String(f.sentDate), st.sentDate);
            if (*f.rcvd) {
                if (st.rcvd != QLatin1String("N"))
                    r.set(QLatin1String(f.rcvd), st.rcvd);
                r.set(QLatin1String(f.rcvdDate), st.rcvdDate);
            }
        }
    }

    const QString extra = row.value(QStringLiteral("adif_extra")).toString();
    if (!extra.isEmpty()) {
        const QJsonObject obj = QJsonDocument::fromJson(extra.toUtf8()).object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            r.set(it.key(), it.value().toString());
    }
    return r;
}

std::optional<QsoMeta> LogDatabase::meta(qint64 id) const
{
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT uuid, revision, source, source_app, created_at, updated_at, dirty, deleted, station_profile_id "
        "FROM qso WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return std::nullopt;
    QsoMeta m;
    m.uuid = q.value(0).toString();
    m.revision = q.value(1).toInt();
    m.source = q.value(2).toString();
    m.sourceApp = q.value(3).toString();
    m.createdAt = q.value(4).toString();
    m.updatedAt = q.value(5).toString();
    m.dirty = q.value(6).toBool();
    m.deleted = q.value(7).toBool();
    m.stationProfileId = q.value(8).toLongLong();
    return m;
}

QList<QslState> LogDatabase::qslStatus(qint64 id) const
{
    QList<QslState> out;
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT service, sent, sent_date, rcvd, rcvd_date, remote_id, last_error FROM qsl_status WHERE qso_id = ?"));
    q.addBindValue(id);
    if (!q.exec())
        return out;
    while (q.next()) {
        QslState st;
        st.service = q.value(0).toString();
        st.sent = q.value(1).toString();
        st.sentDate = q.value(2).toString();
        st.rcvd = q.value(3).toString();
        st.rcvdDate = q.value(4).toString();
        st.remoteId = q.value(5).toString();
        st.lastError = q.value(6).toString();
        out << st;
    }
    return out;
}

QList<HistoryEntry> LogDatabase::history(qint64 id) const
{
    QList<HistoryEntry> out;
    const auto m = meta(id);
    if (!m)
        return out;
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT id, revision, reason, recorded_at, snapshot FROM qso_history WHERE qso_uuid = ? "
        "ORDER BY revision DESC, id DESC"));
    q.addBindValue(m->uuid);
    if (!q.exec())
        return out;
    while (q.next()) {
        HistoryEntry e;
        e.id = q.value(0).toLongLong();
        e.revision = q.value(1).toInt();
        e.reason = q.value(2).toString();
        e.recordedAt = parseIso(q.value(3).toString());
        e.record = recordFromSnapshot(q.value(4).toString());
        out << e;
    }
    return out;
}

InsertResult LogDatabase::restoreRevision(qint64 id, qint64 historyId)
{
    for (const HistoryEntry& e : history(id)) {
        if (e.id == historyId)
            return updateQso(id, e.record, -1, QStringLiteral("edit"));
    }
    InsertResult r;
    r.message = QStringLiteral("revision not found");
    return r;
}

ImportResult LogDatabase::importAdif(const QByteArray& data, const QString& source, qint64 stationProfileId)
{
    ImportResult result;
    const AdifDocument doc = adif::parse(data);
    const QString programId = doc.header.value(QStringLiteral("PROGRAMID"));

    QSqlDatabase db = connection();
    // Una transazione sola: diecimila QSO in secondi invece che in minuti.
    const bool ownTransaction = db.transaction();
    for (const auto& rec : doc.records) {
        const InsertResult r = insertQso(rec, source, programId, false, stationProfileId);
        switch (r.status) {
        case InsertResult::Status::Inserted:  ++result.inserted; break;
        case InsertResult::Status::Duplicate: ++result.duplicates; break;
        case InsertResult::Status::Invalid:
        case InsertResult::Status::Error:
            ++result.invalid;
            if (result.errors.size() < 20)
                result.errors << r.message;
            break;
        }
    }
    if (ownTransaction)
        db.commit();
    return result;
}

QByteArray LogDatabase::exportAdif(const QString& programVersion) const
{
    QList<qint64> ids;
    QSqlQuery q(connection());
    q.exec(QStringLiteral("SELECT id FROM qso WHERE deleted = 0 ORDER BY qso_datetime_on, id"));
    while (q.next())
        ids << q.value(0).toLongLong();
    return exportAdif(ids, programVersion);
}

QByteArray LogDatabase::exportAdif(const QList<qint64>& ids, const QString& programVersion) const
{
    AdifDocument doc;
    doc.header.set(QStringLiteral("ADIF_VER"), QStringLiteral("3.1.5"));
    doc.header.set(QStringLiteral("PROGRAMID"), QStringLiteral("DecoLog"));
    doc.header.set(QStringLiteral("PROGRAMVERSION"), programVersion);
    doc.header.set(QStringLiteral("CREATED_TIMESTAMP"),
                   QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd HHmmss")));
    for (qint64 id : ids) {
        if (auto r = record(id))
            doc.records.append(*r);
    }
    return adif::writeDocument(doc);
}

int LogDatabase::qsoCount() const
{
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM qso WHERE deleted = 0")) && q.next())
        return q.value(0).toInt();
    return 0;
}

int LogDatabase::dirtyCount() const
{
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM qso WHERE dirty = 1")) && q.next())
        return q.value(0).toInt();
    return 0;
}

int LogDatabase::conflictCount() const
{
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM qso_history WHERE reason = 'conflict_lost'")) && q.next())
        return q.value(0).toInt();
    return 0;
}

WorkedBefore LogDatabase::workedBefore(const QString& call) const
{
    WorkedBefore wb;
    const QString c = call.trimmed().toUpper();
    if (c.isEmpty())
        return wb;

    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT band, mode, submode, qso_datetime_on, name, gridsquare, country, qth, dxcc, cqz, ituz, id, "
        "(SELECT rcvd FROM qsl_status s WHERE s.qso_id = qso.id AND s.service = 'lotw') "
        "FROM qso WHERE deleted = 0 AND call = ? ORDER BY qso_datetime_on DESC"));
    q.addBindValue(c);
    if (!q.exec())
        return wb;

    const QStringList order = bands::all();
    while (q.next()) {
        const QString band = q.value(0).toString();
        const QString mode = displayMode(q.value(1).toString(), q.value(2).toString());
        const QDateTime on = parseIso(q.value(3).toString());
        if (wb.count == 0) {
            wb.last = on;
            wb.lastBand = band;
            wb.lastMode = mode;
            wb.lastId = q.value(11).toLongLong();
        }
        if (wb.name.isEmpty()) wb.name = q.value(4).toString();
        if (wb.gridsquare.isEmpty()) wb.gridsquare = q.value(5).toString();
        if (wb.country.isEmpty()) wb.country = q.value(6).toString();
        if (wb.qth.isEmpty()) wb.qth = q.value(7).toString();
        if (wb.dxcc == 0) wb.dxcc = q.value(8).toInt();
        if (wb.cqz == 0) wb.cqz = q.value(9).toInt();
        if (wb.ituz == 0) wb.ituz = q.value(10).toInt();
        if (!wb.bands.contains(band)) wb.bands << band;
        if (!wb.modes.contains(mode)) wb.modes << mode;
        if (wb.recent.size() < 5)
            wb.recent << WorkedEntry{on, band, mode, q.value(12).toString()};
        ++wb.count;
    }
    std::sort(wb.bands.begin(), wb.bands.end(), [&order](const QString& a, const QString& b) {
        return order.indexOf(a) < order.indexOf(b);
    });
    return wb;
}

bool LogDatabase::isFirstFt2Dxcc(qint64 id) const
{
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT q.dxcc, (SELECT COUNT(*) FROM qso o WHERE o.deleted = 0 AND o.submode = 'FT2' "
        "AND o.dxcc = q.dxcc AND o.id <> q.id AND (o.qso_datetime_on < q.qso_datetime_on "
        "OR (o.qso_datetime_on = q.qso_datetime_on AND o.id < q.id))) "
        "FROM qso q WHERE q.id = ? AND q.submode = 'FT2'"));
    q.addBindValue(id);
    if (!q.exec() || !q.next() || q.value(0).isNull())
        return false;
    return q.value(1).toInt() == 0;
}

Ft2Award LogDatabase::ft2Award() const
{
    Ft2Award a;
    QSqlQuery q(connection());
    const QString ft2 = QStringLiteral("FROM qso WHERE deleted = 0 AND submode = 'FT2'");
    const QString lotw = QStringLiteral(
        " AND EXISTS (SELECT 1 FROM qsl_status s WHERE s.qso_id = qso.id AND s.service = 'lotw' AND s.rcvd = 'Y')");
    auto scalar = [&q](const QString& sql) {
        return q.exec(sql) && q.next() ? q.value(0).toInt() : 0;
    };
    a.qsos = scalar(QStringLiteral("SELECT COUNT(*) ") + ft2);
    a.dxccWorked = scalar(QStringLiteral("SELECT COUNT(DISTINCT dxcc) ") + ft2 + QStringLiteral(" AND dxcc > 0"));
    a.dxccConfirmed = scalar(QStringLiteral("SELECT COUNT(DISTINCT dxcc) ") + ft2 + QStringLiteral(" AND dxcc > 0") + lotw);
    const QString grid = QStringLiteral("SELECT COUNT(DISTINCT UPPER(SUBSTR(gridsquare, 1, 4))) ");
    const QString hasGrid = QStringLiteral(" AND LENGTH(gridsquare) >= 4");
    a.gridsWorked = scalar(grid + ft2 + hasGrid);
    a.gridsConfirmed = scalar(grid + ft2 + hasGrid + lotw);
    return a;
}

QList<CountRow> LogDatabase::countByBand() const
{
    QList<CountRow> out;
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral("SELECT band, COUNT(*) FROM qso WHERE deleted = 0 GROUP BY band"))) {
        while (q.next())
            out << CountRow{q.value(0).toString(), q.value(1).toInt()};
    }
    const QStringList order = bands::all();
    std::sort(out.begin(), out.end(), [&order](const CountRow& a, const CountRow& b) {
        return order.indexOf(a.key) < order.indexOf(b.key);
    });
    return out;
}

QList<CountRow> LogDatabase::countByMode() const
{
    QList<CountRow> out;
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral(
            "SELECT CASE WHEN IFNULL(submode, '') = '' OR mode = 'SSB' THEN mode ELSE submode END AS m, COUNT(*) AS n "
            "FROM qso WHERE deleted = 0 GROUP BY m ORDER BY n DESC"))) {
        while (q.next())
            out << CountRow{q.value(0).toString(), q.value(1).toInt()};
    }
    return out;
}

QList<QVariantMap> LogDatabase::qslSummary() const
{
    QList<QVariantMap> out;
    for (const auto& f : kQslFields) {
        QSqlQuery q(connection());
        q.prepare(QStringLiteral(
            "SELECT SUM(sent IN ('R','Q')), SUM(sent = 'Y'), SUM(rcvd = 'Y'), SUM(IFNULL(last_error, '') <> '') "
            "FROM qsl_status s JOIN qso ON qso.id = s.qso_id WHERE qso.deleted = 0 AND s.service = ?"));
        q.addBindValue(QLatin1String(f.service));
        QVariantMap row{{QStringLiteral("service"), QLatin1String(f.service)}};
        if (q.exec() && q.next()) {
            row[QStringLiteral("queued")] = q.value(0).toInt();
            row[QStringLiteral("sent")] = q.value(1).toInt();
            row[QStringLiteral("confirmed")] = q.value(2).toInt();
            row[QStringLiteral("errors")] = q.value(3).toInt();
        }
        out << row;
    }
    return out;
}

QStringList LogDatabase::workedGrids(int limit) const
{
    QStringList out;
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT DISTINCT UPPER(SUBSTR(gridsquare, 1, 4)) FROM qso WHERE deleted = 0 "
        "AND LENGTH(gridsquare) >= 4 LIMIT ?"));
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next())
            out << q.value(0).toString();
    }
    return out;
}

// ── Profili stazione ───────────────────────────────────────────────────────────

namespace {

StationProfile profileFromQuery(const QSqlQuery& q)
{
    StationProfile p;
    p.id = q.value(0).toLongLong();
    p.uuid = q.value(1).toString();
    p.name = q.value(2).toString();
    p.stationCallsign = q.value(3).toString();
    p.operatorCall = q.value(4).toString();
    p.myGridsquare = q.value(5).toString();
    p.myCqZone = q.value(6).toInt();
    p.myItuZone = q.value(7).toInt();
    p.myDxcc = q.value(8).toInt();
    p.myRig = q.value(9).toString();
    p.myAntenna = q.value(10).toString();
    p.defaultTxPwr = q.value(11).toDouble();
    p.lotwStationLocation = q.value(12).toString();
    p.isDefault = q.value(13).toBool();
    p.revision = q.value(14).toInt();
    p.deleted = q.value(15).toBool();
    p.dirty = q.value(16).toBool();
    p.qsoCount = q.value(17).toInt();
    return p;
}

const QString kProfileSelect = QStringLiteral(
    "SELECT id, uuid, name, station_callsign, operator, my_gridsquare, my_cq_zone, my_itu_zone, my_dxcc, "
    "my_rig, my_antenna, default_tx_pwr, lotw_station_loc, is_default, revision, deleted, dirty, "
    "(SELECT COUNT(*) FROM qso WHERE qso.station_profile_id = station_profile.id AND qso.deleted = 0) "
    "FROM station_profile");

} // namespace

QList<StationProfile> LogDatabase::stationProfiles(bool includeDeleted) const
{
    QList<StationProfile> out;
    QSqlQuery q(connection());
    const QString where = includeDeleted ? QString() : QStringLiteral(" WHERE deleted = 0");
    if (q.exec(kProfileSelect + where + QStringLiteral(" ORDER BY deleted, is_default DESC, name COLLATE NOCASE"))) {
        while (q.next())
            out << profileFromQuery(q);
    }
    return out;
}

std::optional<StationProfile> LogDatabase::stationProfile(qint64 id) const
{
    QSqlQuery q(connection());
    q.prepare(kProfileSelect + QStringLiteral(" WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return profileFromQuery(q);
    return std::nullopt;
}

qint64 LogDatabase::saveStationProfile(const StationProfile& p)
{
    QSqlDatabase db = connection();
    db.transaction();
    auto nullableInt = [](int v) { return v > 0 ? QVariant(v) : QVariant(); };
    auto nullableText = [](const QString& s) { return s.trimmed().isEmpty() ? QVariant() : QVariant(s.trimmed()); };

    // Un solo profilo predefinito alla volta.
    if (p.isDefault) {
        QSqlQuery clear(db);
        clear.prepare(QStringLiteral(
            "UPDATE station_profile SET is_default = 0, dirty = 1, revision = revision + 1, updated_at = ? "
            "WHERE is_default = 1 AND id <> ?"));
        clear.addBindValue(nowIso());
        clear.addBindValue(p.id);
        clear.exec();
    }

    QSqlQuery q(db);
    if (p.id <= 0) {
        q.prepare(QStringLiteral(
            "INSERT INTO station_profile (uuid, name, station_callsign, operator, my_gridsquare, my_cq_zone, "
            "my_itu_zone, my_dxcc, my_rig, my_antenna, default_tx_pwr, lotw_station_loc, is_default, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        q.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
    } else {
        q.prepare(QStringLiteral(
            "UPDATE station_profile SET name = ?, station_callsign = ?, operator = ?, my_gridsquare = ?, "
            "my_cq_zone = ?, my_itu_zone = ?, my_dxcc = ?, my_rig = ?, my_antenna = ?, default_tx_pwr = ?, "
            "lotw_station_loc = ?, is_default = ?, updated_at = ?, revision = revision + 1, dirty = 1 WHERE id = ?"));
    }
    q.addBindValue(p.name.trimmed());
    q.addBindValue(p.stationCallsign.trimmed().toUpper());
    q.addBindValue(nullableText(p.operatorCall.toUpper()));
    q.addBindValue(nullableText(p.myGridsquare));
    q.addBindValue(nullableInt(p.myCqZone));
    q.addBindValue(nullableInt(p.myItuZone));
    q.addBindValue(nullableInt(p.myDxcc));
    q.addBindValue(nullableText(p.myRig));
    q.addBindValue(nullableText(p.myAntenna));
    q.addBindValue(p.defaultTxPwr > 0 ? QVariant(p.defaultTxPwr) : QVariant());
    q.addBindValue(nullableText(p.lotwStationLocation));
    q.addBindValue(p.isDefault ? 1 : 0);
    q.addBindValue(nowIso());
    if (p.id > 0)
        q.addBindValue(p.id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        db.rollback();
        return 0;
    }
    db.commit();
    return p.id > 0 ? p.id : q.lastInsertId().toLongLong();
}

bool LogDatabase::deleteStationProfile(qint64 id)
{
    // I QSO restano legati al profilo: cancellarlo non deve cambiare il log.
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "UPDATE station_profile SET deleted = 1, is_default = 0, dirty = 1, revision = revision + 1, "
        "updated_at = ? WHERE id = ?"));
    q.addBindValue(nowIso());
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

qint64 LogDatabase::profileForCallsign(const QString& stationCallsign) const
{
    const QString c = stationCallsign.trimmed().toUpper();
    if (c.isEmpty())
        return 0;
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT id FROM station_profile WHERE deleted = 0 AND station_callsign = ? ORDER BY is_default DESC"));
    q.addBindValue(c);
    // Con piu' profili sullo stesso nominativo vince quello predefinito.
    if (q.exec() && q.next())
        return q.value(0).toLongLong();
    return 0;
}

// ── Impostazioni e manutenzione ────────────────────────────────────────────────

QString LogDatabase::setting(const QString& key, const QString& fallback) const
{
    QSqlQuery q(connection());
    q.prepare(QStringLiteral("SELECT value FROM app_setting WHERE key = ?"));
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return fallback;
}

void LogDatabase::setSetting(const QString& key, const QString& value)
{
    QSqlQuery q(connection());
    q.prepare(QStringLiteral("INSERT INTO app_setting (key, value) VALUES (?, ?) "
                             "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}

bool LogDatabase::backupTo(const QString& filePath)
{
    if (QFile::exists(filePath))
        QFile::remove(filePath);
    QSqlQuery q(connection());
    q.prepare(QStringLiteral("VACUUM INTO ?"));
    q.addBindValue(filePath);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

} // namespace decolog::core
