#include "app/SolarController.h"

#include "core/LogDatabase.h"
#include "core/Maidenhead.h"
#include "core/Propagation.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSqlQuery>

namespace decolog::app {

using namespace decolog::core;

QVariantMap SolarController::pathForecast(const QVariant& target) const
{
    QVariantMap out;
    double lat = 0.0;
    double lon = 0.0;
    bool have = false;
    if (target.metaType().id() == QMetaType::QString) {
        if (const auto p = maidenhead::toLatLon(target.toString().trimmed())) {
            lat = p->lat;
            lon = p->lon;
            have = true;
        }
    } else {
        const QVariantMap m = target.toMap();
        if (m.contains(QStringLiteral("lat")) && m.contains(QStringLiteral("lon"))) {
            lat = m.value(QStringLiteral("lat")).toDouble();
            lon = m.value(QStringLiteral("lon")).toDouble();
            have = true;
        }
    }
    const QVariantMap home = m_ctx.stationPosition ? m_ctx.stationPosition() : QVariantMap();
    if (!home.contains(QStringLiteral("lat"))) {
        out.insert(QStringLiteral("reason"), tr("Set your locator in the station profile."));
        return out;
    }
    if (!have) {
        out.insert(QStringLiteral("reason"), tr("No position for the DX: type a locator or look up a callsign."));
        return out;
    }
    propagation::Input in;
    in.fromLat = home.value(QStringLiteral("lat")).toDouble();
    in.fromLon = home.value(QStringLiteral("lon")).toDouble();
    in.toLat = lat;
    in.toLon = lon;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    in.date = now.date();
    // Senza dati del Sole si usa un valore medio: meglio una stima che niente.
    in.solarFlux = m_data.valid && m_data.solarFlux > 0 ? m_data.solarFlux : 120;
    in.sunspots = m_data.valid && m_data.sunspots > 0 ? m_data.sunspots : -1;
    in.kIndex = m_data.valid ? m_data.kIndex : 2;
    const propagation::Forecast f = propagation::forecast(in);
    out.insert(QStringLiteral("valid"), f.valid);
    out.insert(QStringLiteral("distanceKm"), qRound(f.distanceKm));
    out.insert(QStringLiteral("azimuth"), f.azimuth);
    out.insert(QStringLiteral("hops"), f.hops);
    out.insert(QStringLiteral("solarFlux"), in.solarFlux);
    out.insert(QStringLiteral("estimatedSun"), !m_data.valid);
    out.insert(QStringLiteral("currentHour"), now.time().hour());
    QStringList names;
    for (const auto& b : propagation::bands())
        names << b.name;
    out.insert(QStringLiteral("bands"), names);
    QVariantList hours;
    for (const auto& h : f.hours) {
        QVariantList q;
        for (int v : h.quality)
            q << v;
        hours << QVariantMap{{QStringLiteral("hour"), h.hourUtc},
                             {QStringLiteral("muf"), qRound(h.mufMhz * 10.0) / 10.0},
                             {QStringLiteral("luf"), qRound(h.lufMhz * 10.0) / 10.0},
                             {QStringLiteral("quality"), q}};
    }
    out.insert(QStringLiteral("hours"), hours);
    return out;
}

namespace {

constexpr const char* kHistoryKey = "solar.history";
// Un campione all'ora per tre settimane: piu' che basta per vedere una tendenza.
constexpr int kMaxSamples = 520;

} // namespace

SolarController::SolarController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    QSettings s;
    m_automatic = s.value(QStringLiteral("solar/automatic"), true).toBool();
    m_interval = qBound(15, s.value(QStringLiteral("solar/intervalMinutes"), 60).toInt(), 360);

    connect(&m_fetcher, &SolarFetcher::finished, this, [this](const SolarData& data) {
        m_data = data;
        m_status = tr("Solar data of %1").arg(data.updated);
        remember(data);
        emit changed();
    });
    connect(&m_fetcher, &SolarFetcher::failed, this, [this](const QString& error) {
        m_status = error;
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("PROP"), error, QStringLiteral("warning"));
        emit changed();
    });

    m_timer.setInterval(m_interval * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_automatic)
            m_fetcher.fetch();
    });
}

void SolarController::start()
{
    loadHistory();
    if (m_automatic) {
        refresh();
        m_timer.start();
    }
    emit changed();
}

void SolarController::setAutomatic(bool automatic)
{
    if (automatic == m_automatic)
        return;
    m_automatic = automatic;
    QSettings().setValue(QStringLiteral("solar/automatic"), automatic);
    if (automatic) {
        m_timer.start();
        refresh();
    } else {
        m_timer.stop();
    }
    emit changed();
}

void SolarController::setIntervalMinutes(int minutes)
{
    const int value = qBound(15, minutes, 360);
    if (value == m_interval)
        return;
    m_interval = value;
    QSettings().setValue(QStringLiteral("solar/intervalMinutes"), value);
    m_timer.setInterval(value * 60 * 1000);
    emit changed();
}

void SolarController::refresh()
{
    if (m_fetcher.busy())
        return;
    m_status = tr("Asking for the solar data…");
    m_fetcher.fetch();
    emit changed();
}

void SolarController::injectXml(const QByteArray& xml)
{
    const SolarData data = solar::parse(xml);
    if (!data.valid) {
        m_status = tr("The solar data cannot be read");
        emit changed();
        return;
    }
    m_data = data;
    m_status = tr("Solar data of %1").arg(data.updated);
    remember(data);
    emit changed();
}

// ── Storico ───────────────────────────────────────────────────────────────────

void SolarController::loadHistory()
{
    m_history.clear();
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return;
    const QString raw = m_ctx.db->setting(QLatin1String(kHistoryKey));
    if (raw.isEmpty())
        return;
    const QJsonArray array = QJsonDocument::fromJson(raw.toUtf8()).array();
    for (const QJsonValue& value : array)
        m_history << value.toObject().toVariantMap();
}

void SolarController::remember(const SolarData& data)
{
    if (!data.valid || !m_ctx.db || !m_ctx.db->isOpen())
        return;
    const QDateTime now = data.fetchedAt.isValid() ? data.fetchedAt : QDateTime::currentDateTimeUtc();
    const QString hour = now.toString(QStringLiteral("yyyy-MM-ddTHH:00:00"));
    // Un campione all'ora: se in quell'ora c'e' gia', si aggiorna.
    if (!m_history.isEmpty()
        && m_history.last().toMap().value(QStringLiteral("t")).toString() == hour) {
        m_history.removeLast();
    }
    m_history << QVariantMap{{QStringLiteral("t"), hour},
                             {QStringLiteral("sfi"), data.solarFlux},
                             {QStringLiteral("a"), data.aIndex},
                             {QStringLiteral("k"), data.kIndex},
                             {QStringLiteral("sunspots"), data.sunspots}};
    while (m_history.size() > kMaxSamples)
        m_history.removeFirst();
    saveHistory();
}

void SolarController::saveHistory()
{
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return;
    QJsonArray array;
    for (const QVariant& v : m_history)
        array.append(QJsonObject::fromVariantMap(v.toMap()));
    m_ctx.db->setSetting(QLatin1String(kHistoryKey),
                         QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

QVariantList SolarController::history(int days) const
{
    const QString from = QDateTime::currentDateTimeUtc().addDays(-qMax(1, days)).toString(Qt::ISODate);
    QVariantList out;
    for (const QVariant& v : m_history) {
        if (v.toMap().value(QStringLiteral("t")).toString() >= from)
            out << v;
    }
    return out;
}

QVariantList SolarController::qsoAgainstFlux(int days) const
{
    const int span = qBound(3, days, 60);
    // SFI medio del giorno, dai campioni tenuti.
    QHash<QString, QPair<int, int>> flux;      // giorno -> somma, quanti
    for (const QVariant& v : m_history) {
        const QVariantMap row = v.toMap();
        const QString day = row.value(QStringLiteral("t")).toString().left(10);
        auto& entry = flux[day];
        entry.first += row.value(QStringLiteral("sfi")).toInt();
        entry.second += 1;
    }

    QHash<QString, int> qsos;
    if (m_ctx.db && m_ctx.db->isOpen()) {
        QSqlQuery q(m_ctx.db->connection());
        q.prepare(QStringLiteral(
            "SELECT substr(qso_datetime_on, 1, 10) AS day, COUNT(*) FROM qso "
            "WHERE deleted = 0 AND qso_datetime_on >= ? GROUP BY day"));
        q.addBindValue(QDateTime::currentDateTimeUtc().addDays(-span).toString(Qt::ISODate));
        if (q.exec()) {
            while (q.next())
                qsos.insert(q.value(0).toString(), q.value(1).toInt());
        }
    }

    QVariantList out;
    const QDate today = QDateTime::currentDateTimeUtc().date();
    for (int i = span - 1; i >= 0; --i) {
        const QString day = today.addDays(-i).toString(Qt::ISODate);
        const auto entry = flux.value(day);
        out << QVariantMap{{QStringLiteral("day"), day},
                           {QStringLiteral("qso"), qsos.value(day, 0)},
                           {QStringLiteral("sfi"), entry.second > 0 ? entry.first / entry.second : 0}};
    }
    return out;
}

} // namespace decolog::app
