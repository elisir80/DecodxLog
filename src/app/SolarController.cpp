#include "app/SolarController.h"

#include "core/LogDatabase.h"
#include "core/Maidenhead.h"
#include "core/Propagation.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSqlQuery>
#include <QStandardPaths>

#include <algorithm>

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
    out.insert(QStringLiteral("engine"), QStringLiteral("model"));
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
    // VOACAP, se c'e' e si vuole: le bande fra 2 e 30 MHz (il 160 e il 6 m
    // restano al modello semplice), con la stazione delle impostazioni.
    const voacap::Result* v = nullptr;
    QList<int> voacapBands;
    if (f.valid && m_voacapEnabled && voacapAvailable()) {
        voacap::Request req;
        req.fromLat = in.fromLat;
        req.fromLon = in.fromLon;
        req.toLat = in.toLat;
        req.toLon = in.toLon;
        req.fromLabel = QStringLiteral("DECODXLOG");
        req.toLabel = QStringLiteral("DX");
        req.year = in.date.year();
        req.month = in.date.month();
        // Le macchie: quelle del giorno, o ricavate dal flusso come fa il modello.
        req.ssn = in.sunspots > 0 ? in.sunspots : std::max(0.0, (in.solarFlux - 67.0) * 1.2);
        req.powerWatts = m_voacapPower;
        req.txGainDbi = m_voacapGain;
        req.rxGainDbi = m_voacapGain;
        req.noise = m_voacapNoise;
        req.requiredSnr = voacap::requiredSnrFor(m_voacapMode);
        const auto& all = propagation::bands();
        for (int i = 0; i < all.size(); ++i) {
            if (all.at(i).mhz >= 2.0 && all.at(i).mhz <= 30.0 && req.mhz.size() < 11) {
                req.mhz << all.at(i).mhz;
                voacapBands << i;
            }
        }
        const QString key = req.key();
        const auto it = m_voacapResults.constFind(key);
        if (it == m_voacapResults.constEnd()) {
            if (!m_voacapAsked.contains(key)) {
                m_voacapAsked.insert(key);
                m_voacap->run(req);
            }
            out.insert(QStringLiteral("voacapPending"), true);
        } else if (!it->valid) {
            out.insert(QStringLiteral("voacapError"), it->error);
        } else {
            v = &*it;
            out.insert(QStringLiteral("engine"), QStringLiteral("voacap"));
        }
    }
    QVariantList hours;
    for (int hi = 0; hi < f.hours.size(); ++hi) {
        const auto& h = f.hours.at(hi);
        QVariantList q;
        QVariantList rel;
        QVariantList snr;
        for (int v2 : h.quality) {
            q << v2;
            rel << QVariant();
            snr << QVariant();
        }
        double muf = h.mufMhz;
        if (v && hi < v->hours.size()) {
            const voacap::HourResult& vh = v->hours.at(hi);
            muf = vh.muf;
            for (int k = 0; k < voacapBands.size() && k < vh.cells.size(); ++k) {
                const voacap::Cell& c = vh.cells.at(k);
                q[voacapBands.at(k)] = voacap::qualityOf(c);
                rel[voacapBands.at(k)] = qRound(c.rel * 100.0);
                snr[voacapBands.at(k)] = qRound(c.snr);
            }
        }
        hours << QVariantMap{{QStringLiteral("hour"), h.hourUtc},
                             {QStringLiteral("muf"), qRound(muf * 10.0) / 10.0},
                             {QStringLiteral("luf"), qRound(h.lufMhz * 10.0) / 10.0},
                             {QStringLiteral("quality"), q},
                             {QStringLiteral("rel"), rel},
                             {QStringLiteral("snr"), snr}};
    }
    out.insert(QStringLiteral("hours"), hours);
    return out;
}

void SolarController::saveVoacap(const char* key, const QVariant& value)
{
    QSettings().setValue(QStringLiteral("solar/voacap%1").arg(QLatin1String(key)), value);
    emit changed();
}

void SolarController::setVoacapEnabled(bool on)
{
    if (on == m_voacapEnabled)
        return;
    m_voacapEnabled = on;
    saveVoacap("Enabled", on);
}

void SolarController::setVoacapMode(const QString& mode)
{
    const QString clean = mode.trimmed().toUpper();
    if (clean.isEmpty() || clean == m_voacapMode)
        return;
    m_voacapMode = clean;
    saveVoacap("Mode", clean);
}

void SolarController::setVoacapPower(int watts)
{
    const int clean = std::clamp(watts, 1, 2000);
    if (clean == m_voacapPower)
        return;
    m_voacapPower = clean;
    saveVoacap("Power", clean);
}

void SolarController::setVoacapGain(double dbi)
{
    const double clean = std::clamp(dbi, -10.0, 25.0);
    if (qFuzzyCompare(clean, m_voacapGain))
        return;
    m_voacapGain = clean;
    saveVoacap("Gain", clean);
}

void SolarController::setVoacapNoise(int noise)
{
    const int clean = std::clamp(noise, 130, 165);
    if (clean == m_voacapNoise)
        return;
    m_voacapNoise = clean;
    saveVoacap("Noise", clean);
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
    m_voacapEnabled = s.value(QStringLiteral("solar/voacapEnabled"), true).toBool();
    m_voacapMode = s.value(QStringLiteral("solar/voacapMode"), QStringLiteral("FT8")).toString();
    m_voacapPower = std::clamp(s.value(QStringLiteral("solar/voacapPower"), 100).toInt(), 1, 2000);
    m_voacapGain = std::clamp(s.value(QStringLiteral("solar/voacapGain"), 2.0).toDouble(), -10.0, 25.0);
    m_voacapNoise = std::clamp(s.value(QStringLiteral("solar/voacapNoise"), 145).toInt(), 130, 165);

    // VOACAP lavora in una cartella sua, dove puo' scrivere: i dati del
    // pacchetto si copiano li' la prima volta.
    m_voacap = new voacap::Engine(this);
    m_voacap->setWorkDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                         + QStringLiteral("/voacap"));
    connect(m_voacap, &voacap::Engine::finished, this, [this](const QString& key, const voacap::Result& result) {
        m_voacapAsked.remove(key);
        // Poche decine di percorsi al massimo: i vecchi si lasciano andare.
        if (m_voacapResults.size() > 60)
            m_voacapResults.clear();
        m_voacapResults.insert(key, result);
        if (!result.valid && m_ctx.activity)
            m_ctx.activity(QStringLiteral("PROP"), tr("VOACAP: %1").arg(result.error), QStringLiteral("warning"));
        emit changed();
    });

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
