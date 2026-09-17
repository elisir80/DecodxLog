#include "app/ActivationController.h"

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
    return m_session.exportFileName(call.isEmpty() ? QStringLiteral("DECOLOG") : call, day);
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

} // namespace decolog::app
