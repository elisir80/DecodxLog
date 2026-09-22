#include "app/ActivationController.h"

#include "core/Cabrillo.h"
#include "core/ContestRules.h"
#include "core/Contests.h"
#include "core/LogDatabase.h"
#include "core/Spots.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QTimeZone>

namespace decolog::app {

using namespace decolog::core;

namespace {

constexpr const char* kSetting = "activation.session";

} // namespace

ActivationController::ActivationController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
}

void ActivationController::load()
{
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return;
    const QString json = m_ctx.db->setting(QLatin1String(kSetting));
    if (!json.isEmpty())
        m_session = Activation::fromMap(QJsonDocument::fromJson(json.toUtf8()).object().toVariantMap());
    recount();
    emit changed();
}

void ActivationController::save()
{
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return;
    m_ctx.db->setSetting(QLatin1String(kSetting),
                         QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(m_session.toMap()))
                                               .toJson(QJsonDocument::Compact)));
}

QStringList ActivationController::kinds() const
{
    return {QStringLiteral("pota"), QStringLiteral("sota"), QStringLiteral("wwff"),
            QStringLiteral("iota"), QStringLiteral("contest"), QStringLiteral("free")};
}

QVariantMap ActivationController::state() const
{
    QVariantMap map = m_session.toMap();
    map.insert(QStringLiteral("qsoCount"), m_qsoCount);
    map.insert(QStringLiteral("uniqueCalls"), m_uniqueCalls);
    map.insert(QStringLiteral("perBand"), m_perBand);
    map.insert(QStringLiteral("lastQso"), m_lastQso);
    map.insert(QStringLiteral("elapsed"), elapsed());
    map.insert(QStringLiteral("fileName"), suggestedFileName());
    return map;
}

QString ActivationController::elapsed() const
{
    if (!m_session.active || !m_session.startedAt.isValid())
        return {};
    const qint64 minutes = m_session.startedAt.secsTo(QDateTime::currentDateTimeUtc()) / 60;
    return QStringLiteral("%1:%2").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

QString ActivationController::start(const QVariantMap& map)
{
    Activation session = Activation::fromMap(map);
    session.active = true;
    if (session.kind == Activation::Kind::None)
        return tr("Choose what kind of session it is");
    const bool needsReference = session.kind == Activation::Kind::Pota || session.kind == Activation::Kind::Sota
        || session.kind == Activation::Kind::Wwff || session.kind == Activation::Kind::Iota;
    if (needsReference && session.reference.isEmpty())
        return tr("A %1 activation needs its reference").arg(Activation::kindId(session.kind).toUpper());
    if (session.kind == Activation::Kind::Contest && session.contestId.isEmpty())
        return tr("A contest needs its name (CONTEST_ID)");

    if (session.myGrid.isEmpty() && m_ctx.stationGrid)
        session.myGrid = m_ctx.stationGrid();
    if (session.stationProfileId <= 0 && m_ctx.activeProfileId)
        session.stationProfileId = m_ctx.activeProfileId();
    if (session.tag.isEmpty())
        session.tag = session.defaultTag();
    session.startedAt = QDateTime::currentDateTimeUtc();
    m_session = session;
    save();
    recount();
    if (m_ctx.activity)
        m_ctx.activity(QStringLiteral("ACT"), tr("Session open: %1 · grid %2").arg(m_session.title(), m_session.myGrid),
                       QStringLiteral("success"));
    emit changed();
    return {};
}

void ActivationController::update(const QVariantMap& map)
{
    if (!m_session.active)
        return;
    const QDateTime started = m_session.startedAt;
    Activation session = Activation::fromMap(map);
    session.active = true;
    session.startedAt = started.isValid() ? started : QDateTime::currentDateTimeUtc();
    m_session = session;
    save();
    recount();
    emit changed();
}

void ActivationController::stop()
{
    if (!m_session.active)
        return;
    if (m_ctx.activity) {
        m_ctx.activity(QStringLiteral("ACT"),
                       tr("Session closed: %1 · %n QSO", nullptr, m_qsoCount).arg(m_session.title()),
                       m_qsoCount >= m_session.requiredQsos() ? QStringLiteral("success") : QStringLiteral("warning"));
    }
    m_session = Activation{};
    save();
    recount();
    emit changed();
}

void ActivationController::refresh()
{
    recount();
    emit changed();
}

void ActivationController::recount()
{
    m_qsoCount = 0;
    m_uniqueCalls = 0;
    m_perBand.clear();
    m_lastQso.clear();
    if (!m_session.active || !m_ctx.db || !m_ctx.db->isOpen() || !m_session.startedAt.isValid())
        return;

    QSqlQuery q(m_ctx.db->connection());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*), COUNT(DISTINCT call), MAX(qso_datetime_on) FROM qso "
        "WHERE deleted = 0 AND qso_datetime_on >= ?"));
    q.addBindValue(m_session.startedAt.toString(Qt::ISODate));
    if (q.exec() && q.next()) {
        m_qsoCount = q.value(0).toInt();
        m_uniqueCalls = q.value(1).toInt();
        const QDateTime last = QDateTime::fromString(q.value(2).toString(), Qt::ISODate);
        if (last.isValid())
            m_lastQso = last.toString(QStringLiteral("HH:mm"));
    }

    QSqlQuery bands(m_ctx.db->connection());
    bands.prepare(QStringLiteral(
        "SELECT band, CASE WHEN IFNULL(submode, '') = '' OR mode = 'SSB' THEN mode ELSE submode END AS m, COUNT(*) "
        "FROM qso WHERE deleted = 0 AND qso_datetime_on >= ? GROUP BY band, m ORDER BY COUNT(*) DESC"));
    bands.addBindValue(m_session.startedAt.toString(Qt::ISODate));
    if (bands.exec()) {
        while (bands.next()) {
            m_perBand << QVariantMap{{QStringLiteral("band"), bands.value(0).toString()},
                                     {QStringLiteral("mode"), bands.value(1).toString()},
                                     {QStringLiteral("count"), bands.value(2).toInt()}};
        }
    }
}

bool ActivationController::isDuplicate(const QString& call, const QString& band, const QString& mode) const
{
    if (!m_session.active || !m_ctx.db || !m_ctx.db->isOpen() || !m_session.startedAt.isValid())
        return false;
    QSqlQuery q(m_ctx.db->connection());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM qso WHERE deleted = 0 AND call = ? AND band = ? AND qso_datetime_on >= ? "
        "AND (CASE WHEN IFNULL(submode, '') = '' OR mode = 'SSB' THEN mode ELSE submode END) = ?"));
    q.addBindValue(call.trimmed().toUpper());
    q.addBindValue(band.trimmed().toLower());
    q.addBindValue(m_session.startedAt.toString(Qt::ISODate));
    q.addBindValue(mode.trimmed().toUpper());
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

void ActivationController::applyTo(AdifRecord& record) const
{
    m_session.applyTo(record, m_session.nextSerial);
}

void ActivationController::qsoLogged()
{
    if (!m_session.active)
        return;
    if (m_session.serialEnabled) {
        ++m_session.nextSerial;
        save();
    }
    recount();
    emit changed();
}

QVariantList ActivationController::qsoIds() const
{
    QVariantList ids;
    if (!m_session.active || !m_ctx.db || !m_ctx.db->isOpen() || !m_session.startedAt.isValid())
        return ids;
    QSqlQuery q(m_ctx.db->connection());
    q.prepare(QStringLiteral(
        "SELECT id FROM qso WHERE deleted = 0 AND qso_datetime_on >= ? ORDER BY qso_datetime_on, id"));
    q.addBindValue(m_session.startedAt.toString(Qt::ISODate));
    if (q.exec()) {
        while (q.next())
            ids << q.value(0).toLongLong();
    }
    return ids;
}

QString ActivationController::suggestedFileName() const
{
    if (!m_session.active)
        return {};
    const QString call = m_ctx.stationCall ? m_ctx.stationCall() : QString();
    const QDate day = m_session.startedAt.isValid() ? m_session.startedAt.date() : QDate::currentDate();
    return m_session.exportFileName(call.isEmpty() ? QStringLiteral("DECODXLOG") : call, day);
}

QVariantMap ActivationController::cabrilloDefaults() const
{
    const QString call = m_ctx.stationCall ? m_ctx.stationCall() : QString();
    return QVariantMap{
        {QStringLiteral("contest"), m_session.contestId.isEmpty() ? m_session.name : m_session.contestId},
        {QStringLiteral("callsign"), call},
        {QStringLiteral("gridLocator"), m_session.myGrid.isEmpty() && m_ctx.stationGrid ? m_ctx.stationGrid()
                                                                                        : m_session.myGrid},
        {QStringLiteral("operators"), call},
        {QStringLiteral("categoryOperator"), QStringLiteral("SINGLE-OP")},
        {QStringLiteral("categoryAssisted"), QStringLiteral("NON-ASSISTED")},
        {QStringLiteral("categoryBand"), m_session.band.isEmpty() ? QStringLiteral("ALL")
                                                                  : m_session.band.toUpper()},
        {QStringLiteral("categoryMode"), QStringLiteral("MIXED")},
        {QStringLiteral("categoryPower"), QStringLiteral("LOW")},
        {QStringLiteral("categoryTransmitter"), QStringLiteral("ONE")},
    };
}

QString ActivationController::exportCabrillo(const QUrl& url, const QVariantMap& info)
{
    const QVariantList ids = qsoIds();
    if (ids.isEmpty())
        return tr("No QSO in this session yet");

    auto text = [&info](const char* key) { return info.value(QLatin1String(key)).toString().trimmed(); };
    cabrillo::Info header;
    header.contest = text("contest");
    header.callsign = text("callsign");
    if (header.callsign.isEmpty() && m_ctx.stationCall)
        header.callsign = m_ctx.stationCall();
    if (!text("categoryOperator").isEmpty())
        header.categoryOperator = text("categoryOperator");
    if (!text("categoryAssisted").isEmpty())
        header.categoryAssisted = text("categoryAssisted");
    if (!text("categoryBand").isEmpty())
        header.categoryBand = text("categoryBand");
    if (!text("categoryMode").isEmpty())
        header.categoryMode = text("categoryMode");
    if (!text("categoryPower").isEmpty())
        header.categoryPower = text("categoryPower");
    if (!text("categoryTransmitter").isEmpty())
        header.categoryTransmitter = text("categoryTransmitter");
    header.categoryOverlay = text("categoryOverlay");
    header.gridLocator = text("gridLocator");
    header.location = text("location");
    header.club = text("club");
    header.name = text("name");
    header.email = text("email");
    header.operators = text("operators");
    header.claimedScore = info.value(QStringLiteral("claimedScore")).toLongLong();
    const QString address = info.value(QStringLiteral("address")).toString();
    if (!address.trimmed().isEmpty())
        header.address = address.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const QString soapbox = info.value(QStringLiteral("soapbox")).toString();
    if (!soapbox.trimmed().isEmpty())
        header.soapbox = soapbox.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    QList<AdifRecord> records;
    for (const QVariant& v : ids) {
        if (const auto record = m_ctx.db->record(v.toLongLong()))
            records << *record;
    }

    QString error;
    const QByteArray text_out = cabrillo::write(header, records, &error);
    if (text_out.isEmpty())
        return error.isEmpty() ? tr("Nothing to write") : error;

    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return tr("Cannot write %1").arg(path);
    file.write(text_out);
    file.close();
    if (m_ctx.activity) {
        m_ctx.activity(QStringLiteral("ACT"),
                       tr("Cabrillo written: %1 (%n QSO)", nullptr, static_cast<int>(records.size())).arg(path),
                       QStringLiteral("success"));
    }
    return {};
}

QVariantList ActivationController::recentQsos(int limit) const
{
    QVariantList out;
    if (!m_session.active || !m_ctx.db || !m_ctx.db->isOpen() || !m_session.startedAt.isValid())
        return out;
    QSqlQuery q(m_ctx.db->connection());
    q.setForwardOnly(true);
    q.prepare(QStringLiteral(
        "SELECT id, call, qso_datetime_on, band, "
        "CASE WHEN IFNULL(submode, '') = '' OR mode = 'SSB' THEN mode ELSE submode END, "
        "rst_sent, rst_rcvd, country FROM qso "
        "WHERE deleted = 0 AND qso_datetime_on >= ? ORDER BY qso_datetime_on DESC, id DESC LIMIT ?"));
    q.addBindValue(m_session.startedAt.toString(Qt::ISODate));
    q.addBindValue(qMax(1, limit));
    if (!q.exec())
        return out;
    while (q.next()) {
        const QDateTime when = QDateTime::fromString(q.value(2).toString(), Qt::ISODate);
        out << QVariantMap{{QStringLiteral("id"), q.value(0).toLongLong()},
                           {QStringLiteral("call"), q.value(1).toString()},
                           {QStringLiteral("time"), when.toString(QStringLiteral("hh:mm"))},
                           {QStringLiteral("band"), q.value(3).toString()},
                           {QStringLiteral("mode"), q.value(4).toString()},
                           {QStringLiteral("rstSent"), q.value(5).toString()},
                           {QStringLiteral("rstRcvd"), q.value(6).toString()},
                           {QStringLiteral("country"), q.value(7).toString()}};
    }
    return out;
}

QVariantMap ActivationController::rate() const
{
    QVariantMap out{{QStringLiteral("last10"), 0}, {QStringLiteral("last60"), 0},
                    {QStringLiteral("perHour10"), 0}, {QStringLiteral("perHour60"), 0},
                    {QStringLiteral("dxcc"), 0}, {QStringLiteral("grids"), 0}};
    if (!m_session.active || !m_ctx.db || !m_ctx.db->isOpen() || !m_session.startedAt.isValid())
        return out;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString start = m_session.startedAt.toString(Qt::ISODate);

    auto since = [this, &start](const QDateTime& from) {
        QSqlQuery q(m_ctx.db->connection());
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM qso WHERE deleted = 0 AND qso_datetime_on >= ? AND qso_datetime_on >= ?"));
        q.addBindValue(start);
        q.addBindValue(from.toString(Qt::ISODate));
        return q.exec() && q.next() ? q.value(0).toInt() : 0;
    };
    const int last10 = since(now.addSecs(-600));
    const int last60 = since(now.addSecs(-3600));
    out[QStringLiteral("last10")] = last10;
    out[QStringLiteral("last60")] = last60;
    // Il ritmo: quello che verrebbe fuori se si andasse avanti cosi' per un'ora.
    out[QStringLiteral("perHour10")] = last10 * 6;
    out[QStringLiteral("perHour60")] = last60;

    QSqlQuery mult(m_ctx.db->connection());
    mult.prepare(QStringLiteral(
        "SELECT COUNT(DISTINCT dxcc), COUNT(DISTINCT substr(gridsquare, 1, 4)) FROM qso "
        "WHERE deleted = 0 AND qso_datetime_on >= ?"));
    mult.addBindValue(start);
    if (mult.exec() && mult.next()) {
        out[QStringLiteral("dxcc")] = mult.value(0).toInt();
        out[QStringLiteral("grids")] = mult.value(1).toInt();
    }
    return out;
}

QString ActivationController::exportAdif(const QUrl& url)
{
    const QVariantList ids = qsoIds();
    if (ids.isEmpty())
        return tr("No QSO in this session yet");
    QList<qint64> list;
    for (const QVariant& v : ids)
        list << v.toLongLong();
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return file.errorString();
    file.write(m_ctx.db->exportAdif(list, QCoreApplication::applicationVersion()));
    if (m_ctx.activity)
        m_ctx.activity(QStringLiteral("ACT"), tr("%n QSO of the session → %1", nullptr, static_cast<int>(list.size())).arg(path),
                       QStringLiteral("success"));
    return {};
}

QVariantList ActivationController::contests(const QString& search) const
{
    QVariantList out;
    for (const core::Contest& c : core::contests::search(search)) {
        out << QVariantMap{
            {QStringLiteral("id"), c.id},
            {QStringLiteral("name"), c.name},
            // Quello che si legge nell'elenco: il nome, e l'id fra parentesi
            // perche' e' quello che finisce nel log.
            {QStringLiteral("label"), QStringLiteral("%1  (%2)").arg(c.name, c.id)},
        };
    }
    return out;
}

QString ActivationController::contestName(const QString& id) const
{
    return core::contests::nameFor(id);
}

QVariantMap ActivationController::score() const
{
    const core::ContestRules rules = core::contestrules::forId(m_session.contestId);
    QVariantMap out{
        {QStringLiteral("valid"), rules.valid},
        {QStringLiteral("contestId"), m_session.contestId},
        {QStringLiteral("exchangeLabel"), rules.valid ? rules.exchangeLabel : tr("Exchange")},
        {QStringLiteral("source"), rules.source},
        {QStringLiteral("points"), 0},
        {QStringLiteral("multipliers"), 0},
        {QStringLiteral("score"), 0},
        {QStringLiteral("bands"), QVariantList{}},
    };
    if (!rules.valid || !m_session.active || !m_ctx.db || !m_ctx.db->isOpen())
        return out;

    const core::ContestStation me = m_ctx.station ? m_ctx.station() : core::ContestStation{};
    int points = 0;
    QSet<QString> mults;
    // Per banda: quanti QSO, quanti punti, quanti moltiplicatori nuovi.
    QMap<QString, QVariantMap> perBand;
    QSet<QString> seenPerBand;

    for (const QVariant& v : qsoIds()) {
        const auto record = m_ctx.db->record(v.toLongLong());
        if (!record)
            continue;
        core::ContestQso qso;
        qso.call = record->value(QStringLiteral("CALL"));
        qso.band = record->value(QStringLiteral("BAND")).toLower();
        qso.mode = record->value(QStringLiteral("MODE")).toUpper();
        if (qso.mode == QLatin1String("MFSK") && !record->value(QStringLiteral("SUBMODE")).isEmpty())
            qso.mode = record->value(QStringLiteral("SUBMODE")).toUpper();
        qso.exchange = record->value(QStringLiteral("SRX_STRING"));
        if (qso.exchange.isEmpty())
            qso.exchange = record->value(QStringLiteral("SRX"));
        qso.dxcc = record->value(QStringLiteral("DXCC")).toInt();
        qso.continent = record->value(QStringLiteral("CONT")).toUpper();
        qso.cqZone = record->value(QStringLiteral("CQZ")).toInt();
        qso.ituZone = record->value(QStringLiteral("ITUZ")).toInt();
        // Un QSO scritto in fretta non ha il paese: lo si chiede al cty.csv,
        // invece di contare zero punti per un dato che si puo' sapere.
        if ((qso.dxcc == 0 || qso.continent.isEmpty()) && m_ctx.locate) {
            const core::ContestStation found = m_ctx.locate(qso.call);
            if (qso.dxcc == 0)
                qso.dxcc = found.dxcc;
            if (qso.continent.isEmpty())
                qso.continent = found.continent;
            if (qso.cqZone == 0)
                qso.cqZone = found.cqZone;
            if (qso.ituZone == 0)
                qso.ituZone = found.ituZone;
        }

        const int value = core::contestrules::points(rules, qso, me);
        points += value;

        QVariantMap& band = perBand[qso.band];
        band[QStringLiteral("band")] = qso.band;
        band[QStringLiteral("qsos")] = band.value(QStringLiteral("qsos")).toInt() + 1;
        band[QStringLiteral("points")] = band.value(QStringLiteral("points")).toInt() + value;

        for (const QString& key : core::contestrules::multipliers(rules, qso, me)) {
            if (mults.contains(key))
                continue;
            mults.insert(key);
            band[QStringLiteral("multipliers")] = band.value(QStringLiteral("multipliers")).toInt() + 1;
        }
    }

    QVariantList bands;
    for (const QVariantMap& band : std::as_const(perBand))
        bands << band;
    out[QStringLiteral("points")] = points;
    out[QStringLiteral("multipliers")] = mults.size();
    out[QStringLiteral("score")] = static_cast<qint64>(points) * mults.size();
    out[QStringLiteral("bands")] = bands;
    return out;
}

QString ActivationController::checkExchange(const QString& exchange) const
{
    return core::contestrules::checkExchange(core::contestrules::forId(m_session.contestId), exchange);
}

QVariantMap ActivationController::spotValue(const QString& call, const QString& band,
                                            const QString& mode) const
{
    QVariantMap out{{QStringLiteral("points"), 0},
                    {QStringLiteral("newMultiplier"), false},
                    {QStringLiteral("duplicate"), false},
                    {QStringLiteral("label"), QString()}};
    const core::ContestRules rules = core::contestrules::forId(m_session.contestId);
    if (!rules.valid || !m_session.active || call.trimmed().isEmpty())
        return out;

    const core::ContestStation me = m_ctx.station ? m_ctx.station() : core::ContestStation{};
    core::ContestQso qso;
    qso.call = call.trimmed().toUpper();
    qso.band = band.toLower();
    qso.mode = mode.toUpper();
    if (m_ctx.locate) {
        const core::ContestStation where = m_ctx.locate(qso.call);
        qso.dxcc = where.dxcc;
        qso.continent = where.continent;
        qso.cqZone = where.cqZone;
        qso.ituZone = where.ituZone;
    }
    // Lo scambio non si sa prima di averlo lavorato: per i contest che contano
    // zone o paesi basta il cty.csv, per gli altri (province, sezioni) il
    // moltiplicatore si sapra' solo a QSO fatto.
    out[QStringLiteral("points")] = core::contestrules::points(rules, qso, me);
    out[QStringLiteral("duplicate")] = isDuplicate(qso.call, qso.band, qso.mode);

    const QStringList keys = core::contestrules::multipliers(rules, qso, me);
    if (keys.isEmpty())
        return out;
    const QSet<QString> already = workedMultipliers();
    QStringList missing;
    for (const QString& key : keys) {
        if (!already.contains(key))
            missing << key.section(QLatin1Char('|'), 0, 0);
    }
    out[QStringLiteral("newMultiplier")] = !missing.isEmpty();
    out[QStringLiteral("label")] = missing.join(QStringLiteral(" · "));
    return out;
}

bool ActivationController::isCwContest() const
{
    if (!m_session.active)
        return false;
    if (m_session.mode.compare(QLatin1String("CW"), Qt::CaseInsensitive) == 0)
        return true;
    // Un contest che si fa solo in telegrafia lo dice nel suo identificativo.
    return m_session.contestId.endsWith(QLatin1String("-CW"), Qt::CaseInsensitive)
           || m_session.contestId.contains(QLatin1String("-CW-"), Qt::CaseInsensitive);
}

QSet<QString> ActivationController::workedMultipliers() const
{
    QSet<QString> out;
    const core::ContestRules rules = core::contestrules::forId(m_session.contestId);
    if (!rules.valid || !m_ctx.db || !m_ctx.db->isOpen())
        return out;
    const core::ContestStation me = m_ctx.station ? m_ctx.station() : core::ContestStation{};
    for (const QVariant& v : qsoIds()) {
        const auto record = m_ctx.db->record(v.toLongLong());
        if (!record)
            continue;
        core::ContestQso qso;
        qso.call = record->value(QStringLiteral("CALL"));
        qso.band = record->value(QStringLiteral("BAND")).toLower();
        qso.mode = record->value(QStringLiteral("MODE")).toUpper();
        qso.exchange = record->value(QStringLiteral("SRX_STRING"));
        if (qso.exchange.isEmpty())
            qso.exchange = record->value(QStringLiteral("SRX"));
        qso.dxcc = record->value(QStringLiteral("DXCC")).toInt();
        qso.continent = record->value(QStringLiteral("CONT")).toUpper();
        qso.cqZone = record->value(QStringLiteral("CQZ")).toInt();
        qso.ituZone = record->value(QStringLiteral("ITUZ")).toInt();
        if ((qso.dxcc == 0 || qso.continent.isEmpty()) && m_ctx.locate) {
            const core::ContestStation found = m_ctx.locate(qso.call);
            if (qso.dxcc == 0)
                qso.dxcc = found.dxcc;
            if (qso.continent.isEmpty())
                qso.continent = found.continent;
            if (qso.cqZone == 0)
                qso.cqZone = found.cqZone;
            if (qso.ituZone == 0)
                qso.ituZone = found.ituZone;
        }
        for (const QString& key : core::contestrules::multipliers(rules, qso, me))
            out.insert(key);
    }
    return out;
}

} // namespace decolog::app
