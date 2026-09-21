#include "core/LogDatabase.h"

#include "core/Modes.h"

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
#include <limits>

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
    Column{"SAT_MODE", "sat_mode", Kind::Text},
    Column{"TX_PWR", "tx_pwr", Kind::Real},
    Column{"COMMENT", "comment", Kind::Text},
    Column{"NOTES", "notes", Kind::Text},
    Column{"APP_DECOLOG_TAGS", "tags", Kind::Text},
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
        return migrate();
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

// Un log creato da una versione precedente si porta avanti un passo alla volta,
// ogni passo nella sua transazione. Mai all'indietro: una versione vecchia di
// DecoDXLog che apre un log nuovo trova solo colonne in piu'.
bool LogDatabase::migrate()
{
    QSqlDatabase db = connection();
    const int version = schemaVersion();
    struct Step {
        int to;
        QStringList statements;
    };
    const QList<Step> steps{
        {2, {QStringLiteral("ALTER TABLE qso ADD COLUMN tags TEXT")}},
        {3, {QStringLiteral("ALTER TABLE qsl_status ADD COLUMN via TEXT")}},
        {4, {QStringLiteral("ALTER TABLE qso ADD COLUMN sat_mode TEXT")}},
    };
    for (const Step& step : steps) {
        if (version >= step.to)
            continue;
        if (!db.transaction()) {
            m_lastError = db.lastError().text();
            return false;
        }
        QSqlQuery q(db);
        for (const QString& stmt : step.statements + QStringList{
                 QStringLiteral("INSERT INTO schema_version (version) VALUES (%1)").arg(step.to)}) {
            if (!q.exec(stmt)) {
                m_lastError = q.lastError().text() + QStringLiteral(" in migration to v%1").arg(step.to);
                db.rollback();
                return false;
            }
        }
        if (!db.commit()) {
            m_lastError = db.lastError().text();
            return false;
        }
    }
    return true;
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

    if (record.contains(QStringLiteral("APP_DECOLOG_TAGS")))
        record.set(QStringLiteral("APP_DECOLOG_TAGS"), joinTags(splitTags(record.value(QStringLiteral("APP_DECOLOG_TAGS")))));

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
        if (st.service == QLatin1String("card")) {
            const QString via = record.value(QStringLiteral("QSL_SENT_VIA")).trimmed().toUpper();
            if (!via.isEmpty()) {
                st.via = via;
                consumed << QStringLiteral("QSL_SENT_VIA");
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
        "INSERT INTO qsl_status (qso_id, service, sent, sent_date, rcvd, rcvd_date, remote_id, last_error, via) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
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
        ins.addBindValue(nullable(st.via));
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
        ins.addBindValue(QVariant());
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

    // Dentro una transazione gia' aperta (un import, una correzione in blocco)
    // si lavora in quella; altrimenti se ne apre una propria.
    QSqlDatabase db = connection();
    const bool ownTransaction = db.transaction();

    QSqlQuery h(db);
    h.prepare(QStringLiteral(
        "INSERT INTO qso_history (qso_uuid, revision, snapshot, reason, recorded_at) VALUES (?, ?, ?, ?, ?)"));
    h.addBindValue(m->uuid);
    h.addBindValue(m->revision);
    h.addBindValue(snapshotJson(id));
    h.addBindValue(reason);
    h.addBindValue(nowIso());
    if (!h.exec()) {
        if (ownTransaction)
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
        if (ownTransaction)
            db.rollback();
        return result;
    }
    if (ownTransaction && !db.commit()) {
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
        // Le cartacee portano anche la via: bureau, diretta o elettronica.
        if (st.service == QLatin1String("card") && !st.via.isEmpty())
            r.set(QStringLiteral("QSL_SENT_VIA"), st.via);
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
        "SELECT service, sent, sent_date, rcvd, rcvd_date, remote_id, last_error, via "
        "FROM qsl_status WHERE qso_id = ?"));
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
        st.via = q.value(7).toString();
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

ConfirmationResult LogDatabase::applyConfirmation(const QString& service, const AdifRecord& c, int windowSeconds)
{
    ConfirmationResult res;
    const QslFields* fields = nullptr;
    for (const auto& f : kQslFields) {
        if (service == QLatin1String(f.service) && *f.rcvd)
            fields = &f;
    }
    if (!fields) {
        res.status = ConfirmationResult::Status::Error;
        res.message = QStringLiteral("%1 has no confirmations").arg(service);
        return res;
    }

    const QString call = c.value(QStringLiteral("CALL")).trimmed().toUpper();
    QString band = c.value(QStringLiteral("BAND")).trimmed().toLower();
    if (band.isEmpty()) {
        bool ok = false;
        const double mhz = c.value(QStringLiteral("FREQ")).toDouble(&ok);
        if (ok)
            band = bands::fromMhz(mhz);
    }
    const QString iso = isoFromAdif(c.value(QStringLiteral("QSO_DATE")), c.value(QStringLiteral("TIME_ON")));
    const QString group = adif::modeGroup(c.value(QStringLiteral("MODE")), c.value(QStringLiteral("SUBMODE")));
    res.message = QStringLiteral("%1 %2 %3 %4").arg(call, band, c.value(QStringLiteral("MODE")), iso.left(16));
    if (call.isEmpty() || band.isEmpty() || iso.isEmpty()) {
        res.status = ConfirmationResult::Status::Invalid;
        return res;
    }

    const QDateTime on = parseIso(iso);
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT id, mode, IFNULL(submode, ''), qso_datetime_on, "
        "(SELECT rcvd FROM qsl_status s WHERE s.qso_id = qso.id AND s.service = ?) "
        "FROM qso WHERE deleted = 0 AND call = ? AND band = ? AND qso_datetime_on BETWEEN ? AND ?"));
    q.addBindValue(service);
    q.addBindValue(call);
    q.addBindValue(band);
    q.addBindValue(on.addSecs(-windowSeconds).toString(Qt::ISODate));
    q.addBindValue(on.addSecs(windowSeconds).toString(Qt::ISODate));
    if (!q.exec()) {
        res.status = ConfirmationResult::Status::Error;
        res.message = q.lastError().text();
        return res;
    }
    // Il QSO piu' vicino nel tempo fra quelli dello stesso gruppo di modi.
    qint64 best = 0;
    qint64 bestDistance = std::numeric_limits<qint64>::max();
    bool bestConfirmed = false;
    while (q.next()) {
        if (!group.isEmpty() && adif::modeGroup(q.value(1).toString(), q.value(2).toString()) != group)
            continue;
        const qint64 distance = qAbs(parseIso(q.value(3).toString()).secsTo(on));
        if (distance < bestDistance) {
            best = q.value(0).toLongLong();
            bestDistance = distance;
            bestConfirmed = q.value(4).toString() == QLatin1String("Y");
        }
    }
    if (best == 0)
        return res;
    res.id = best;
    if (bestConfirmed) {
        res.status = ConfirmationResult::Status::AlreadyConfirmed;
        return res;
    }

    auto r = record(best);
    if (!r) {
        res.status = ConfirmationResult::Status::Error;
        return res;
    }
    r->set(QLatin1String(fields->rcvd), QStringLiteral("Y"));
    QString date = c.value(QStringLiteral("QSLRDATE")).trimmed().remove(QLatin1Char('-')).left(8);
    if (!QDate::fromString(date, QStringLiteral("yyyyMMdd")).isValid())
        date = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd"));
    r->set(QLatin1String(fields->rcvdDate), date);
    // Confermato vuol dire che il QSO e' arrivato al servizio.
    const QString sent = r->value(QLatin1String(fields->sent)).toUpper();
    if (sent.isEmpty() || sent == QLatin1String("N") || sent == QLatin1String("R") || sent == QLatin1String("Q"))
        r->set(QLatin1String(fields->sent), QStringLiteral("Y"));

    // I dettagli del corrispondente: solo dove il log non ha niente. Un locatore
    // a quattro caratteri si allunga se la conferma dice lo stesso quadrato.
    const QString grid = r->value(QStringLiteral("GRIDSQUARE")).trimmed().toUpper();
    const QString confirmedGrid = c.value(QStringLiteral("GRIDSQUARE")).trimmed().toUpper();
    if (confirmedGrid.size() >= 4 && (grid.isEmpty() || (confirmedGrid.size() > grid.size() && confirmedGrid.startsWith(grid))))
        r->set(QStringLiteral("GRIDSQUARE"), confirmedGrid);
    for (const char* name : {"DXCC", "COUNTRY", "CQZ", "ITUZ", "STATE", "CNTY", "IOTA"}) {
        const QString field = QLatin1String(name);
        const QString value = c.value(field).trimmed();
        if (r->value(field).trimmed().isEmpty() && !value.isEmpty())
            r->set(field, value);
    }

    const InsertResult u = updateQso(best, *r, -1, service);
    if (u.status != InsertResult::Status::Inserted) {
        res.status = ConfirmationResult::Status::Error;
        res.message = u.message;
        return res;
    }
    res.status = ConfirmationResult::Status::Confirmed;
    return res;
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
    doc.header.set(QStringLiteral("PROGRAMID"), QStringLiteral("DecoDXLog"));
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
        "(SELECT rcvd FROM qsl_status s WHERE s.qso_id = qso.id AND s.service = 'lotw'), IFNULL(state, '') "
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
        if (wb.state.isEmpty()) wb.state = q.value(13).toString();
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

LogDatabase::DxccWorked LogDatabase::dxccWorked(int dxcc) const
{
    DxccWorked w;
    if (dxcc <= 0)
        return w;
    QSqlQuery q(connection());
    q.prepare(QStringLiteral("SELECT band, mode, submode FROM qso WHERE deleted = 0 AND dxcc = ?"));
    q.addBindValue(dxcc);
    if (!q.exec())
        return w;
    while (q.next()) {
        const QString band = q.value(0).toString();
        const QString mode = displayMode(q.value(1).toString(), q.value(2).toString());
        if (!w.bands.contains(band)) w.bands << band;
        if (!w.modes.contains(mode)) w.modes << mode;
        ++w.count;
    }
    return w;
}

namespace {

// Le caselle della griglia: una query sola, gia' raggruppata dal database, con
// le conferme accanto. Il gruppo del modo (CW, fonia, digitale) lo decide
// modes::groupFor, che e' lo stesso criterio dei diplomi.
QList<LogDatabase::BandModeSlot> slotsFrom(QSqlQuery& q)
{
    QHash<QString, LogDatabase::BandModeSlot> byKey;
    QStringList order;
    while (q.next()) {
        const QString band = q.value(0).toString();
        const QString group = modes::groupFor(displayMode(q.value(1).toString(), q.value(2).toString()));
        const QString key = band + QLatin1Char('|') + group;
        if (!byKey.contains(key)) {
            LogDatabase::BandModeSlot slot;
            slot.band = band;
            slot.group = group;
            byKey.insert(key, slot);
            order << key;
        }
        LogDatabase::BandModeSlot& slot = byKey[key];
        slot.count += q.value(3).toInt();
        slot.lotw    = slot.lotw    || q.value(4).toInt() > 0;
        slot.eqsl    = slot.eqsl    || q.value(5).toInt() > 0;
        slot.card    = slot.card    || q.value(6).toInt() > 0;
        slot.clublog = slot.clublog || q.value(7).toInt() > 0;
        slot.qrz     = slot.qrz     || q.value(8).toInt() > 0;
    }
    QList<LogDatabase::BandModeSlot> out;
    out.reserve(order.size());
    for (const QString& key : order)
        out << byKey.value(key);
    return out;
}

const char* kSlotColumns =
    "SELECT q.band, q.mode, q.submode, COUNT(DISTINCT q.id),"
    " MAX(CASE WHEN s.service = 'lotw'    AND s.rcvd = 'Y' THEN 1 ELSE 0 END),"
    " MAX(CASE WHEN s.service = 'eqsl'    AND s.rcvd = 'Y' THEN 1 ELSE 0 END),"
    " MAX(CASE WHEN s.service = 'card'    AND s.rcvd = 'Y' THEN 1 ELSE 0 END),"
    " MAX(CASE WHEN s.service = 'clublog' AND s.rcvd = 'Y' THEN 1 ELSE 0 END),"
    " MAX(CASE WHEN s.service = 'qrz'     AND s.rcvd = 'Y' THEN 1 ELSE 0 END)"
    " FROM qso q LEFT JOIN qsl_status s ON s.qso_id = q.id"
    " WHERE q.deleted = 0 AND ";

} // namespace

QList<LogDatabase::BandModeSlot> LogDatabase::bandModeSlotsForCall(const QString& call) const
{
    if (call.trimmed().isEmpty())
        return {};
    QSqlQuery q(connection());
    q.prepare(QLatin1String(kSlotColumns)
              + QStringLiteral("q.call = ? GROUP BY q.band, q.mode, q.submode"));
    q.addBindValue(call.trimmed().toUpper());
    if (!q.exec())
        return {};
    return slotsFrom(q);
}

QList<LogDatabase::BandModeSlot> LogDatabase::bandModeSlotsForDxcc(int dxcc) const
{
    if (dxcc <= 0)
        return {};
    QSqlQuery q(connection());
    q.prepare(QLatin1String(kSlotColumns)
              + QStringLiteral("q.dxcc = ? GROUP BY q.band, q.mode, q.submode"));
    q.addBindValue(dxcc);
    if (!q.exec())
        return {};
    return slotsFrom(q);
}

QJsonArray LogDatabase::workedRow(const QString& call, const QString& band, const QString& mode,
                                  const QString& submode, const QString& isoOn, const QString& grid, bool confirmed)
{
    return QJsonArray{call, band, displayMode(mode, submode),
                      isoOn.left(10).remove(QLatin1Char('-')), grid.left(4).toUpper(), confirmed ? 1 : 0};
}

QList<QJsonArray> LogDatabase::workedRows(bool confirmLotw, bool confirmCard, bool confirmEqsl) const
{
    QList<QJsonArray> rows;
    QSqlQuery q(connection());
    q.setForwardOnly(true);
    q.prepare(QStringLiteral(
        "SELECT call, band, mode, IFNULL(submode, ''), qso_datetime_on, IFNULL(gridsquare, ''), "
        "EXISTS (SELECT 1 FROM qsl_status s WHERE s.qso_id = qso.id AND s.rcvd = 'Y' AND ("
        "  (s.service = 'lotw' AND ?) OR (s.service = 'card' AND ?) OR (s.service = 'eqsl' AND ?))) "
        "FROM qso WHERE deleted = 0 ORDER BY qso_datetime_on"));
    q.addBindValue(confirmLotw ? 1 : 0);
    q.addBindValue(confirmCard ? 1 : 0);
    q.addBindValue(confirmEqsl ? 1 : 0);
    if (!q.exec())
        return rows;
    while (q.next()) {
        rows << workedRow(q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),
                          q.value(3).toString(), q.value(4).toString(), q.value(5).toString(), q.value(6).toBool());
    }
    return rows;
}

int LogDatabase::markAllDirty()
{
    QSqlQuery q(connection());
    if (!q.exec(QStringLiteral("UPDATE qso SET dirty = 1")))
        return 0;
    return q.numRowsAffected();
}

QList<qint64> LogDatabase::idsMissingCallbookData(int limit) const
{
    QList<qint64> ids;
    QSqlQuery q(connection());
    QString sql = QStringLiteral(
        "SELECT id FROM qso WHERE deleted = 0 AND IFNULL(gridsquare, '') = '' "
        "ORDER BY qso_datetime_on DESC");
    if (limit > 0)
        sql += QStringLiteral(" LIMIT %1").arg(limit);
    if (q.exec(sql)) {
        while (q.next())
            ids << q.value(0).toLongLong();
    }
    return ids;
}

QList<qint64> LogDatabase::idsWithDamagedText() const
{
    QList<qint64> ids;
    QSqlQuery q(connection());
    // Un '<' o il carattere di sostituzione dentro nome, QTH, indirizzo o note:
    // in un QSO scritto bene non ci stanno.
    if (q.exec(QStringLiteral(
            "SELECT id FROM qso WHERE deleted = 0 AND ("
            "  IFNULL(name, '') LIKE '%<%' OR IFNULL(qth, '') LIKE '%<%'"
            "  OR IFNULL(comment, '') LIKE '%<%' OR IFNULL(notes, '') LIKE '%<%'"
            "  OR IFNULL(country, '') LIKE '%<%'"
            "  OR IFNULL(name, '') LIKE '%' || CHAR(65533) || '%'"
            "  OR IFNULL(qth, '') LIKE '%' || CHAR(65533) || '%'"
            "  OR IFNULL(comment, '') LIKE '%' || CHAR(65533) || '%'"
            "  OR IFNULL(notes, '') LIKE '%' || CHAR(65533) || '%')"
            " ORDER BY id"))) {
        while (q.next())
            ids << q.value(0).toLongLong();
    }
    return ids;
}

QList<qint64> LogDatabase::idsWithoutDxcc() const
{
    QList<qint64> ids;
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral("SELECT id FROM qso WHERE deleted = 0 AND (dxcc IS NULL OR dxcc = 0)"))) {
        while (q.next())
            ids << q.value(0).toLongLong();
    }
    return ids;
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

QList<CountRow> LogDatabase::countByDxcc() const
{
    QList<CountRow> out;
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral(
            "SELECT dxcc, COUNT(*) AS n FROM qso WHERE deleted = 0 AND dxcc > 0 GROUP BY dxcc ORDER BY n DESC, dxcc"))) {
        while (q.next())
            out << CountRow{q.value(0).toString(), q.value(1).toInt()};
    }
    return out;
}

namespace {

// Le condizioni comuni alle statistiche: modo (FT2 e' un sottomodo), banda, anno.
QString statsWhere(const StatsFilter& f, QVariantList& binds)
{
    QString where = QStringLiteral("deleted = 0");
    if (!f.mode.isEmpty()) {
        where += QStringLiteral(" AND (CASE WHEN IFNULL(submode, '') = '' OR mode = 'SSB' THEN mode ELSE submode END) = ?");
        binds << f.mode.toUpper();
    }
    if (!f.band.isEmpty()) {
        where += QStringLiteral(" AND band = ?");
        binds << f.band.toLower();
    }
    if (f.year > 0) {
        where += QStringLiteral(" AND SUBSTR(qso_datetime_on, 1, 4) = ?");
        binds << QString::number(f.year);
    }
    return where;
}

QList<CountRow> groupedCount(const QSqlDatabase& db, const QString& expression, const StatsFilter& filter,
                             const QString& order, int limit = 0)
{
    QList<CountRow> out;
    QVariantList binds;
    const QString where = statsWhere(filter, binds);
    QString sql = QStringLiteral("SELECT %1 AS k, COUNT(*) AS n FROM qso WHERE %2 AND k IS NOT NULL AND k <> '' "
                                 "GROUP BY k ORDER BY %3").arg(expression, where, order);
    if (limit > 0)
        sql += QStringLiteral(" LIMIT %1").arg(limit);
    QSqlQuery q(db);
    q.setForwardOnly(true);
    q.prepare(sql);
    for (const QVariant& b : binds)
        q.addBindValue(b);
    if (!q.exec())
        return out;
    while (q.next())
        out << CountRow{q.value(0).toString(), q.value(1).toInt()};
    return out;
}

} // namespace

QList<CountRow> LogDatabase::countByYear(const StatsFilter& filter) const
{
    return groupedCount(connection(), QStringLiteral("SUBSTR(qso_datetime_on, 1, 4)"), filter, QStringLiteral("k"));
}

QList<CountRow> LogDatabase::countByMonth(int months, const StatsFilter& filter) const
{
    QList<CountRow> all = groupedCount(connection(), QStringLiteral("SUBSTR(qso_datetime_on, 1, 7)"), filter,
                                       QStringLiteral("k DESC"), months);
    std::reverse(all.begin(), all.end());
    return all;
}

QList<CountRow> LogDatabase::countByHour(const StatsFilter& filter) const
{
    return groupedCount(connection(), QStringLiteral("SUBSTR(qso_datetime_on, 12, 2)"), filter, QStringLiteral("k"));
}

QList<CountRow> LogDatabase::countByContinent(const StatsFilter& filter) const
{
    return groupedCount(connection(), QStringLiteral("cont"), filter, QStringLiteral("n DESC"));
}

QList<CountRow> LogDatabase::countByBand(const StatsFilter& filter) const
{
    QList<CountRow> out = groupedCount(connection(), QStringLiteral("band"), filter, QStringLiteral("n DESC"));
    const QStringList order = bands::all();
    std::sort(out.begin(), out.end(), [&order](const CountRow& a, const CountRow& b) {
        return order.indexOf(a.key) < order.indexOf(b.key);
    });
    return out;
}

QList<CountRow> LogDatabase::countByMode(const StatsFilter& filter) const
{
    return groupedCount(connection(),
                        QStringLiteral("CASE WHEN IFNULL(submode, '') = '' OR mode = 'SSB' THEN mode ELSE submode END"),
                        filter, QStringLiteral("n DESC"));
}

QList<QVariantMap> LogDatabase::bandByHour(const StatsFilter& filter) const
{
    QList<QVariantMap> out;
    QVariantList binds;
    const QString where = statsWhere(filter, binds);
    QSqlQuery q(connection());
    q.setForwardOnly(true);
    q.prepare(QStringLiteral("SELECT band, CAST(SUBSTR(qso_datetime_on, 12, 2) AS INTEGER) AS h, COUNT(*) "
                             "FROM qso WHERE %1 AND band <> '' GROUP BY band, h").arg(where));
    for (const QVariant& b : binds)
        q.addBindValue(b);
    if (!q.exec())
        return out;
    while (q.next()) {
        out << QVariantMap{{QStringLiteral("band"), q.value(0).toString()},
                           {QStringLiteral("hour"), q.value(1).toInt()},
                           {QStringLiteral("count"), q.value(2).toInt()}};
    }
    return out;
}

QVariantMap LogDatabase::statsSummary(const StatsFilter& filter) const
{
    QVariantMap out;
    QVariantList binds;
    const QString where = statsWhere(filter, binds);
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*), COUNT(DISTINCT call), COUNT(DISTINCT CASE WHEN dxcc > 0 THEN dxcc END), "
        "MIN(qso_datetime_on), MAX(qso_datetime_on), "
        "COUNT(DISTINCT UPPER(SUBSTR(gridsquare, 1, 4))) FROM qso WHERE %1").arg(where));
    for (const QVariant& b : binds)
        q.addBindValue(b);
    if (q.exec() && q.next()) {
        out.insert(QStringLiteral("qsos"), q.value(0).toInt());
        out.insert(QStringLiteral("calls"), q.value(1).toInt());
        out.insert(QStringLiteral("dxcc"), q.value(2).toInt());
        out.insert(QStringLiteral("first"), q.value(3).toString().left(10));
        out.insert(QStringLiteral("last"), q.value(4).toString().left(10));
        out.insert(QStringLiteral("grids"), q.value(5).toInt());
    }

    // Il giorno con piu' QSO e l'ora piu' produttiva: sono quelli che si raccontano.
    const QList<CountRow> days = groupedCount(connection(), QStringLiteral("SUBSTR(qso_datetime_on, 1, 10)"),
                                              filter, QStringLiteral("n DESC"), 1);
    if (!days.isEmpty()) {
        out.insert(QStringLiteral("bestDay"), days.first().key);
        out.insert(QStringLiteral("bestDayCount"), days.first().count);
    }
    const QList<CountRow> hours = groupedCount(connection(), QStringLiteral("SUBSTR(qso_datetime_on, 12, 2)"),
                                               filter, QStringLiteral("n DESC"), 1);
    if (!hours.isEmpty()) {
        out.insert(QStringLiteral("bestHour"), hours.first().key);
        out.insert(QStringLiteral("bestHourCount"), hours.first().count);
    }
    return out;
}

QStringList LogDatabase::yearsInLog() const
{
    QStringList out;
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral("SELECT DISTINCT SUBSTR(qso_datetime_on, 1, 4) AS y FROM qso WHERE deleted = 0 "
                              "ORDER BY y DESC"))) {
        while (q.next())
            out << q.value(0).toString();
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

// ── Invio QSL ─────────────────────────────────────────────────────────────────

bool LogDatabase::setCardState(qint64 id, const QslState& state)
{
    QslState card = state;
    card.service = QStringLiteral("card");
    return writeQslState(id, card, true);
}

bool LogDatabase::setQslState(qint64 id, const QslState& st)
{
    return writeQslState(id, st, false);
}

bool LogDatabase::writeQslState(qint64 id, const QslState& st, bool includeReceived)
{
    QSqlDatabase db = connection();
    const bool ownTransaction = db.transaction();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO qsl_status (qso_id, service, sent, sent_date, rcvd, rcvd_date, remote_id, last_error, via) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(qso_id, service) DO UPDATE SET sent = excluded.sent, sent_date = excluded.sent_date, "
        "remote_id = COALESCE(excluded.remote_id, qsl_status.remote_id), last_error = excluded.last_error, "
        "via = COALESCE(excluded.via, qsl_status.via)")
        + (includeReceived ? QStringLiteral(", rcvd = excluded.rcvd, rcvd_date = excluded.rcvd_date")
                           : QString()));
    auto nullable = [](const QString& v) { return v.isEmpty() ? QVariant() : QVariant(v); };
    q.addBindValue(id);
    q.addBindValue(st.service);
    q.addBindValue(st.sent.isEmpty() ? QStringLiteral("N") : st.sent);
    q.addBindValue(nullable(st.sentDate));
    q.addBindValue(st.rcvd.isEmpty() ? QStringLiteral("N") : st.rcvd);
    q.addBindValue(nullable(st.rcvdDate));
    q.addBindValue(nullable(st.remoteId));
    q.addBindValue(nullable(st.lastError));
    q.addBindValue(nullable(st.via));
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        if (ownTransaction)
            db.rollback();
        return false;
    }
    // Il QSO e' cambiato per il cloud, ma non e' una revisione nuova.
    QSqlQuery u(db);
    u.prepare(QStringLiteral("UPDATE qso SET dirty = 1, updated_at = ? WHERE id = ?"));
    u.addBindValue(nowIso());
    u.addBindValue(id);
    u.exec();
    if (ownTransaction)
        return db.commit();
    return true;
}

QList<qint64> LogDatabase::qsosToUpload(const QString& service, int limit) const
{
    QList<qint64> ids;
    QSqlQuery q(connection());
    q.setForwardOnly(true);
    q.prepare(QStringLiteral(
        "SELECT qso.id FROM qso LEFT JOIN qsl_status s ON s.qso_id = qso.id AND s.service = ? "
        "WHERE qso.deleted = 0 AND (s.sent IS NULL OR s.sent IN ('N', 'R', 'Q')) "
        "ORDER BY qso.qso_datetime_on, qso.id") + (limit > 0 ? QStringLiteral(" LIMIT ?") : QString()));
    q.addBindValue(service);
    if (limit > 0)
        q.addBindValue(limit);
    if (!q.exec())
        return ids;
    while (q.next())
        ids << q.value(0).toLongLong();
    return ids;
}

int LogDatabase::uploadPendingCount(const QString& service) const
{
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM qso LEFT JOIN qsl_status s ON s.qso_id = qso.id AND s.service = ? "
        "WHERE qso.deleted = 0 AND (s.sent IS NULL OR s.sent IN ('N', 'R', 'Q'))"));
    q.addBindValue(service);
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

// ── Sync con DecoDXLog Cloud ────────────────────────────────────────────────────

QList<qint64> LogDatabase::dirtyQsos(int limit) const
{
    QList<qint64> ids;
    QSqlQuery q(connection());
    q.setForwardOnly(true);
    q.prepare(QStringLiteral("SELECT id FROM qso WHERE dirty = 1 ORDER BY updated_at, id")
              + (limit > 0 ? QStringLiteral(" LIMIT ?") : QString()));
    if (limit > 0)
        q.addBindValue(limit);
    if (!q.exec())
        return ids;
    while (q.next())
        ids << q.value(0).toLongLong();
    return ids;
}

qint64 LogDatabase::idForUuid(const QString& uuid) const
{
    QSqlQuery q(connection());
    q.prepare(QStringLiteral("SELECT id FROM qso WHERE uuid = ?"));
    q.addBindValue(uuid);
    return q.exec() && q.next() ? q.value(0).toLongLong() : 0;
}

QVariantMap LogDatabase::syncRecord(qint64 id) const
{
    const auto m = meta(id);
    const auto r = record(id);
    if (!m || !r)
        return {};

    QVariantMap fields;
    for (const auto& f : r->fields())
        fields.insert(f.name, f.value);

    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT call, band, mode, submode, qso_datetime_on FROM qso WHERE id = ?"));
    q.addBindValue(id);
    QString call;
    QString band;
    QString mode;
    QString submode;
    QString on;
    if (q.exec() && q.next()) {
        call = q.value(0).toString();
        band = q.value(1).toString();
        mode = q.value(2).toString();
        submode = q.value(3).toString();
        on = q.value(4).toString();
    }

    return QVariantMap{
        {QStringLiteral("uuid"), m->uuid},
        {QStringLiteral("revision"), m->revision},
        {QStringLiteral("deleted"), m->deleted},
        // L'ora scritta a mano e' approssimativa: il server allarga la finestra.
        {QStringLiteral("manual"), m->source == QLatin1String("manual")},
        {QStringLiteral("call"), call},
        {QStringLiteral("band"), band},
        {QStringLiteral("mode"), mode},
        {QStringLiteral("submode"), submode},
        {QStringLiteral("startedAt"), on},
        {QStringLiteral("fields"), fields},
    };
}

bool LogDatabase::markSynced(qint64 id, int revision)
{
    QSqlQuery q(connection());
    if (revision > 0) {
        q.prepare(QStringLiteral("UPDATE qso SET dirty = 0, revision = ? WHERE id = ?"));
        q.addBindValue(revision);
    } else {
        q.prepare(QStringLiteral("UPDATE qso SET dirty = 0 WHERE id = ?"));
    }
    q.addBindValue(id);
    return q.exec();
}

bool LogDatabase::adoptUuid(qint64 id, const QString& serverUuid)
{
    if (serverUuid.isEmpty())
        return false;
    const qint64 existing = idForUuid(serverUuid);
    if (existing == id)
        return markSynced(id, 0);
    if (existing > 0) {
        // Quel collegamento e' gia' qui sotto l'uuid del server: questo e' un
        // doppione locale, e si toglie di mezzo com'e' d'uso, in morbido.
        softDeleteQso(id);
        QSqlQuery q(connection());
        q.prepare(QStringLiteral("UPDATE qso SET dirty = 0 WHERE id = ?"));
        q.addBindValue(id);
        return q.exec();
    }
    QSqlQuery q(connection());
    q.prepare(QStringLiteral("UPDATE qso SET uuid = ?, dirty = 0 WHERE id = ?"));
    q.addBindValue(serverUuid);
    q.addBindValue(id);
    return q.exec();
}

LogDatabase::RemoteResult LogDatabase::applyRemote(const QVariantMap& remote)
{
    const QString uuid = remote.value(QStringLiteral("uuid")).toString();
    if (uuid.isEmpty())
        return RemoteResult::Failed;
    const int revision = remote.value(QStringLiteral("revision")).toInt();
    const bool deleted = remote.value(QStringLiteral("deleted")).toBool();

    AdifRecord record;
    const QVariantMap fields = remote.value(QStringLiteral("fields")).toMap();
    for (auto it = fields.cbegin(); it != fields.cend(); ++it)
        record.set(it.key(), it.value().toString());

    qint64 id = idForUuid(uuid);

    if (id > 0) {
        const auto m = meta(id);
        if (m && m->revision > revision) {
            // Qui c'e' gia' qualcosa di piu' nuovo: resta, e ripartira' in coda.
            return RemoteResult::Skipped;
        }
        if (m && m->revision == revision && !m->dirty && !deleted) {
            // Stessa revisione: per il protocollo e' lo stesso contenuto. Succede
            // al giro dopo una spinta, quando il server ci rimanda i nostri.
            return RemoteResult::Skipped;
        }
        if (m && m->dirty && m->revision >= revision) {
            // C'e' una modifica locale ancora da mandare, e il server non ne sa
            // di piu': non si sovrascrive. Parte lei, e il conflitto lo decide
            // il server, che conserva la versione che perde.
            return RemoteResult::Skipped;
        }
        if (deleted) {
            if (m && m->deleted)
                return RemoteResult::Skipped;
            softDeleteQso(id);
        } else {
            InsertResult updated = updateQso(id, record, -1, QStringLiteral("cloud"));
            if (updated.status != InsertResult::Status::Inserted)
                return RemoteResult::Failed;
        }
    } else {
        if (deleted)
            return RemoteResult::Skipped;   // non l'abbiamo mai avuto: niente da fare
        InsertResult inserted = insertQso(record, QStringLiteral("cloud"));
        if (inserted.status == InsertResult::Status::Duplicate && inserted.id > 0) {
            // Lo stesso collegamento c'era gia' con un altro uuid: ci si allinea
            // a quello del server, che e' il nome comune.
            id = inserted.id;
        } else if (inserted.status != InsertResult::Status::Inserted) {
            return RemoteResult::Failed;
        } else {
            id = inserted.id;
        }
    }

    // Quello che arriva dal Cloud non torna in coda, e porta con se' la sua
    // revisione: e' gia' quella del server.
    QSqlQuery q(connection());
    q.prepare(QStringLiteral("UPDATE qso SET uuid = ?, revision = ?, dirty = 0 WHERE id = ?"));
    q.addBindValue(uuid);
    q.addBindValue(revision > 0 ? revision : 1);
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return RemoteResult::Failed;
    }
    return deleted ? RemoteResult::Deleted
                   : (id > 0 ? RemoteResult::Updated : RemoteResult::Inserted);
}

QList<QVariantMap> LogDatabase::dirtyProfiles() const
{
    QList<QVariantMap> out;
    for (const StationProfile& p : stationProfiles(true)) {
        if (!p.dirty)
            continue;
        out << QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("profile")},
            {QStringLiteral("key"), p.uuid},
            {QStringLiteral("revision"), p.revision},
            {QStringLiteral("deleted"), p.deleted},
            {QStringLiteral("data"), QVariantMap{
                {QStringLiteral("name"), p.name},
                {QStringLiteral("stationCallsign"), p.stationCallsign},
                {QStringLiteral("operatorCall"), p.operatorCall},
                {QStringLiteral("myGridsquare"), p.myGridsquare},
                {QStringLiteral("myCqZone"), p.myCqZone},
                {QStringLiteral("myItuZone"), p.myItuZone},
                {QStringLiteral("myDxcc"), p.myDxcc},
                {QStringLiteral("myRig"), p.myRig},
                {QStringLiteral("myAntenna"), p.myAntenna},
                {QStringLiteral("defaultTxPwr"), p.defaultTxPwr},
                {QStringLiteral("lotwStationLocation"), p.lotwStationLocation},
                {QStringLiteral("isDefault"), p.isDefault},
            }},
        };
    }
    return out;
}

bool LogDatabase::markProfileSynced(const QString& uuid, int revision)
{
    QSqlQuery q(connection());
    if (revision > 0) {
        q.prepare(QStringLiteral("UPDATE station_profile SET dirty = 0, revision = ? WHERE uuid = ?"));
        q.addBindValue(revision);
    } else {
        q.prepare(QStringLiteral("UPDATE station_profile SET dirty = 0 WHERE uuid = ?"));
    }
    q.addBindValue(uuid);
    return q.exec();
}

LogDatabase::RemoteResult LogDatabase::applyRemoteProfile(const QVariantMap& document)
{
    const QString uuid = document.value(QStringLiteral("key")).toString();
    if (uuid.isEmpty())
        return RemoteResult::Failed;
    const int revision = document.value(QStringLiteral("revision")).toInt();
    const bool deleted = document.value(QStringLiteral("deleted")).toBool();
    const QVariantMap data = document.value(QStringLiteral("data")).toMap();

    QSqlQuery find(connection());
    find.prepare(QStringLiteral("SELECT id, revision, dirty FROM station_profile WHERE uuid = ?"));
    find.addBindValue(uuid);
    const bool exists = find.exec() && find.next();
    const qint64 id = exists ? find.value(0).toLongLong() : 0;
    const int localRevision = exists ? find.value(1).toInt() : 0;
    const bool localDirty = exists && find.value(2).toInt() != 0;

    if (exists) {
        if (localRevision > revision)
            return RemoteResult::Skipped;
        if (localRevision == revision && !localDirty)
            return RemoteResult::Skipped;   // e' lo stesso profilo
        if (localDirty && localRevision >= revision)
            return RemoteResult::Skipped;   // c'e' una modifica locale da mandare
    }

    QSqlQuery q(connection());
    if (exists) {
        q.prepare(QStringLiteral(
            "UPDATE station_profile SET name = ?, station_callsign = ?, operator = ?, my_gridsquare = ?, "
            "my_cq_zone = ?, my_itu_zone = ?, my_dxcc = ?, my_rig = ?, my_antenna = ?, default_tx_pwr = ?, "
            "lotw_station_loc = ?, is_default = ?, updated_at = ?, revision = ?, deleted = ?, dirty = 0 "
            "WHERE uuid = ?"));
    } else {
        q.prepare(QStringLiteral(
            "INSERT INTO station_profile (name, station_callsign, operator, my_gridsquare, my_cq_zone, "
            "my_itu_zone, my_dxcc, my_rig, my_antenna, default_tx_pwr, lotw_station_loc, is_default, "
            "updated_at, revision, deleted, dirty, uuid) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?)"));
    }
    auto text = [&data](const char* key) {
        const QString value = data.value(QLatin1String(key)).toString().trimmed();
        return value.isEmpty() ? QVariant() : QVariant(value);
    };
    auto number = [&data](const char* key) {
        const int value = data.value(QLatin1String(key)).toInt();
        return value > 0 ? QVariant(value) : QVariant();
    };
    q.addBindValue(data.value(QStringLiteral("name")).toString());
    q.addBindValue(data.value(QStringLiteral("stationCallsign")).toString().toUpper());
    q.addBindValue(text("operatorCall"));
    q.addBindValue(text("myGridsquare"));
    q.addBindValue(number("myCqZone"));
    q.addBindValue(number("myItuZone"));
    q.addBindValue(number("myDxcc"));
    q.addBindValue(text("myRig"));
    q.addBindValue(text("myAntenna"));
    const double power = data.value(QStringLiteral("defaultTxPwr")).toDouble();
    q.addBindValue(power > 0 ? QVariant(power) : QVariant());
    q.addBindValue(text("lotwStationLocation"));
    q.addBindValue(data.value(QStringLiteral("isDefault")).toBool() ? 1 : 0);
    q.addBindValue(nowIso());
    q.addBindValue(revision > 0 ? revision : 1);
    q.addBindValue(deleted ? 1 : 0);
    q.addBindValue(uuid);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return RemoteResult::Failed;
    }
    if (deleted)
        return RemoteResult::Deleted;
    return exists && id > 0 ? RemoteResult::Updated : RemoteResult::Inserted;
}

QVariantMap LogDatabase::syncState(const QString& account) const
{
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "SELECT device_id, pull_cursor, last_push_at, last_pull_at, last_error FROM sync_state WHERE account = ?"));
    q.addBindValue(account);
    if (!q.exec() || !q.next())
        return {};
    return QVariantMap{
        {QStringLiteral("device"), q.value(0).toString()},
        {QStringLiteral("cursor"), q.value(1).toString()},
        {QStringLiteral("lastPush"), q.value(2).toString()},
        {QStringLiteral("lastPull"), q.value(3).toString()},
        {QStringLiteral("lastError"), q.value(4).toString()},
    };
}

void LogDatabase::setSyncState(const QString& account, const QVariantMap& values)
{
    const QVariantMap before = syncState(account);
    auto pick = [&](const char* key) {
        const QString name = QLatin1String(key);
        return values.contains(name) ? values.value(name).toString() : before.value(name).toString();
    };
    QSqlQuery q(connection());
    q.prepare(QStringLiteral(
        "INSERT INTO sync_state (account, device_id, pull_cursor, last_push_at, last_pull_at, last_error) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(account) DO UPDATE SET device_id = excluded.device_id, "
        "pull_cursor = excluded.pull_cursor, last_push_at = excluded.last_push_at, "
        "last_pull_at = excluded.last_pull_at, last_error = excluded.last_error"));
    q.addBindValue(account);
    // device_id e' NOT NULL, e una QString vuota si lega come NULL: meglio un
    // nome qualsiasi che una riga che non entra.
    const QString device = pick("device");
    q.addBindValue(device.isEmpty() ? QStringLiteral("decolog") : device);
    q.addBindValue(pick("cursor"));
    q.addBindValue(pick("lastPush"));
    q.addBindValue(pick("lastPull"));
    q.addBindValue(pick("lastError"));
    if (!q.exec())
        m_lastError = q.lastError().text();
}

// ── QSL di carta ──────────────────────────────────────────────────────────────

QList<QVariantMap> LogDatabase::cardRows(const QString& state, int limit) const
{
    QString where = QStringLiteral("qso.deleted = 0");
    if (state == QLatin1String("queue"))
        where += QStringLiteral(" AND s.sent IN ('R', 'Q')");
    else if (state == QLatin1String("sent"))
        where += QStringLiteral(" AND s.sent = 'Y'");
    else if (state == QLatin1String("received"))
        where += QStringLiteral(" AND s.rcvd = 'Y'");
    else
        where += QStringLiteral(" AND (s.sent IN ('R', 'Q', 'Y') OR s.rcvd = 'Y')");

    QList<QVariantMap> out;
    QSqlQuery q(connection());
    q.setForwardOnly(true);
    q.prepare(QStringLiteral(
        "SELECT qso.id, qso.call, qso.qso_datetime_on, qso.band, qso.mode, qso.submode, qso.freq, "
        "qso.rst_sent, qso.name, qso.country, s.sent, s.sent_date, s.rcvd, s.rcvd_date, s.via "
        "FROM qsl_status s JOIN qso ON qso.id = s.qso_id "
        "WHERE s.service = 'card' AND ") + where
        + QStringLiteral(" ORDER BY qso.call, qso.qso_datetime_on")
        + (limit > 0 ? QStringLiteral(" LIMIT ?") : QString()));
    if (limit > 0)
        q.addBindValue(limit);
    if (!q.exec())
        return out;
    while (q.next()) {
        const QDateTime when = QDateTime::fromString(q.value(2).toString(), Qt::ISODate);
        const QString submode = q.value(5).toString();
        QVariantMap row{
            {QStringLiteral("id"), q.value(0).toLongLong()},
            {QStringLiteral("call"), q.value(1).toString()},
            {QStringLiteral("date"), when.toString(QStringLiteral("yyyy-MM-dd"))},
            {QStringLiteral("time"), when.toString(QStringLiteral("hhmm"))},
            {QStringLiteral("band"), q.value(3).toString()},
            {QStringLiteral("mode"), submode.isEmpty() ? q.value(4).toString() : submode},
            {QStringLiteral("freq"), q.value(6).toString()},
            {QStringLiteral("rst"), q.value(7).toString()},
            {QStringLiteral("name"), q.value(8).toString()},
            {QStringLiteral("country"), q.value(9).toString()},
            {QStringLiteral("sent"), q.value(10).toString()},
            {QStringLiteral("sentDate"), q.value(11).toString()},
            {QStringLiteral("rcvd"), q.value(12).toString()},
            {QStringLiteral("rcvdDate"), q.value(13).toString()},
            {QStringLiteral("via"), q.value(14).toString()},
        };
        out << row;
    }
    return out;
}

// ── Etichette ─────────────────────────────────────────────────────────────────

QStringList LogDatabase::splitTags(const QString& tags)
{
    QStringList out;
    for (const QString& raw : tags.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString tag = raw.simplified();
        if (tag.isEmpty())
            continue;
        const bool seen = std::any_of(out.cbegin(), out.cend(), [&tag](const QString& t) {
            return t.compare(tag, Qt::CaseInsensitive) == 0;
        });
        if (!seen)
            out << tag;
    }
    return out;
}

QString LogDatabase::joinTags(const QStringList& tags)
{
    return tags.join(QLatin1Char(','));
}

QList<CountRow> LogDatabase::tagCounts() const
{
    QHash<QString, CountRow> counts;
    QSqlQuery q(connection());
    q.setForwardOnly(true);
    if (q.exec(QStringLiteral("SELECT tags FROM qso WHERE deleted = 0 AND IFNULL(tags, '') <> ''"))) {
        while (q.next()) {
            for (const QString& tag : splitTags(q.value(0).toString())) {
                CountRow& row = counts[tag.toLower()];
                if (row.key.isEmpty())
                    row.key = tag;
                ++row.count;
            }
        }
    }
    QList<CountRow> out = counts.values();
    std::sort(out.begin(), out.end(), [](const CountRow& a, const CountRow& b) {
        return a.count != b.count ? a.count > b.count : a.key.compare(b.key, Qt::CaseInsensitive) < 0;
    });
    return out;
}

int LogDatabase::setTag(const QList<qint64>& ids, const QString& tag, bool add)
{
    const QString clean = tag.simplified().remove(QLatin1Char(','));
    if (clean.isEmpty())
        return 0;
    int changed = 0;
    QSqlDatabase db = connection();
    const bool ownTransaction = db.transaction();
    for (qint64 id : ids) {
        auto r = record(id);
        if (!r)
            continue;
        QStringList tags = splitTags(r->value(QStringLiteral("APP_DECOLOG_TAGS")));
        const qsizetype at = std::find_if(tags.cbegin(), tags.cend(), [&clean](const QString& t) {
                                 return t.compare(clean, Qt::CaseInsensitive) == 0;
                             }) - tags.cbegin();
        const bool present = at < tags.size();
        if (add == present)
            continue;
        if (add)
            tags << clean;
        else
            tags.removeAt(at);
        r->set(QStringLiteral("APP_DECOLOG_TAGS"), joinTags(tags));
        if (updateQso(id, *r, -1, QStringLiteral("tag")).status == InsertResult::Status::Inserted)
            ++changed;
    }
    if (ownTransaction)
        db.commit();
    return changed;
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
