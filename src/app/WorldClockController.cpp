#include "app/WorldClockController.h"

#include "core/WorldClock.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include <cmath>

namespace decolog::app {

namespace wc = core::worldclock;

namespace {

struct Preset {
    const char* id;
    const char* name;
    const char* shortName;
    const char* zone;
    double lat;
    double lon;
};

// Le citta' che si possono mettere nell'orologio. Le prime sette sono quelle
// di serie.
const Preset kPresets[] = {
    {"nyc", QT_TRANSLATE_NOOP("WorldClock", "New York"), "NY", "America/New_York", 40.71, -74.01},
    {"lax", QT_TRANSLATE_NOOP("WorldClock", "Los Angeles"), "LA", "America/Los_Angeles", 34.05, -118.24},
    {"rio", QT_TRANSLATE_NOOP("WorldClock", "Rio de Janeiro"), "RIO", "America/Sao_Paulo", -22.91, -43.17},
    {"jnb", QT_TRANSLATE_NOOP("WorldClock", "Johannesburg"), "JNB", "Africa/Johannesburg", -26.20, 28.05},
    {"mow", QT_TRANSLATE_NOOP("WorldClock", "Moscow"), "MOW", "Europe/Moscow", 55.76, 37.62},
    {"tyo", QT_TRANSLATE_NOOP("WorldClock", "Tokyo"), "TYO", "Asia/Tokyo", 35.68, 139.69},
    {"syd", QT_TRANSLATE_NOOP("WorldClock", "Sydney"), "SYD", "Australia/Sydney", -33.87, 151.21},
    {"hnl", QT_TRANSLATE_NOOP("WorldClock", "Honolulu"), "HNL", "Pacific/Honolulu", 21.31, -157.86},
    {"anc", QT_TRANSLATE_NOOP("WorldClock", "Anchorage"), "ANC", "America/Anchorage", 61.22, -149.90},
    {"yvr", QT_TRANSLATE_NOOP("WorldClock", "Vancouver"), "YVR", "America/Vancouver", 49.28, -123.12},
    {"chi", QT_TRANSLATE_NOOP("WorldClock", "Chicago"), "CHI", "America/Chicago", 41.88, -87.63},
    {"mex", QT_TRANSLATE_NOOP("WorldClock", "Mexico City"), "MEX", "America/Mexico_City", 19.43, -99.13},
    {"lim", QT_TRANSLATE_NOOP("WorldClock", "Lima"), "LIM", "America/Lima", -12.05, -77.04},
    {"bue", QT_TRANSLATE_NOOP("WorldClock", "Buenos Aires"), "BUE", "America/Argentina/Buenos_Aires", -34.60, -58.38},
    {"rkv", QT_TRANSLATE_NOOP("WorldClock", "Reykjavik"), "REY", "Atlantic/Reykjavik", 64.15, -21.94},
    {"lon", QT_TRANSLATE_NOOP("WorldClock", "London"), "LON", "Europe/London", 51.51, -0.13},
    {"lis", QT_TRANSLATE_NOOP("WorldClock", "Lisbon"), "LIS", "Europe/Lisbon", 38.72, -9.14},
    {"cai", QT_TRANSLATE_NOOP("WorldClock", "Cairo"), "CAI", "Africa/Cairo", 30.04, 31.24},
    {"nbo", QT_TRANSLATE_NOOP("WorldClock", "Nairobi"), "NBO", "Africa/Nairobi", -1.29, 36.82},
    {"dxb", QT_TRANSLATE_NOOP("WorldClock", "Dubai"), "DXB", "Asia/Dubai", 25.20, 55.27},
    {"del", QT_TRANSLATE_NOOP("WorldClock", "New Delhi"), "DEL", "Asia/Kolkata", 28.61, 77.21},
    {"bkk", QT_TRANSLATE_NOOP("WorldClock", "Bangkok"), "BKK", "Asia/Bangkok", 13.76, 100.50},
    {"sin", QT_TRANSLATE_NOOP("WorldClock", "Singapore"), "SIN", "Asia/Singapore", 1.35, 103.82},
    {"pek", QT_TRANSLATE_NOOP("WorldClock", "Beijing"), "PEK", "Asia/Shanghai", 39.90, 116.40},
    {"sel", QT_TRANSLATE_NOOP("WorldClock", "Seoul"), "SEL", "Asia/Seoul", 37.57, 126.98},
    {"per", QT_TRANSLATE_NOOP("WorldClock", "Perth"), "PER", "Australia/Perth", -31.95, 115.86},
    {"akl", QT_TRANSLATE_NOOP("WorldClock", "Auckland"), "AKL", "Pacific/Auckland", -36.85, 174.76},
    {"ppt", QT_TRANSLATE_NOOP("WorldClock", "Papeete"), "PPT", "Pacific/Tahiti", -17.54, -149.57},
};
constexpr int kDefaults = 7;

const Preset* presetFor(const QString& id)
{
    for (const Preset& p : kPresets) {
        if (id == QLatin1String(p.id))
            return &p;
    }
    return nullptr;
}

QString degrees(double v, char plus, char minus, const QLocale& locale)
{
    return QStringLiteral("%1°%2").arg(locale.toString(std::abs(v), 'f', 1)).arg(QLatin1Char(v < 0 ? minus : plus));
}

QString duration(qint64 seconds)
{
    const qint64 minutes = (seconds + 30) / 60;
    return QCoreApplication::translate("WorldClock", "%1 h %2 min").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

WorldClockController::WorldClockController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    const QString language = QSettings().value(QStringLiteral("ui/language"), QStringLiteral("auto")).toString();
    m_locale = language == QLatin1String("auto") ? QLocale::system() : QLocale(language);
    load();
    m_timer.setInterval(1000);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &WorldClockController::update);
    m_timer.start();
    update();
}

void WorldClockController::load()
{
    QSettings s;
    m_cities.clear();
    QStringList ids = s.value(QStringLiteral("worldClock/cities")).toStringList();
    if (ids.isEmpty()) {
        for (int i = 0; i < kDefaults; ++i)
            ids << QLatin1String(kPresets[i].id);
    }
    for (const QString& id : std::as_const(ids)) {
        if (const Preset* p = presetFor(id))
            m_cities << City{QLatin1String(p->id), QCoreApplication::translate("WorldClock", p->name),
                             QLatin1String(p->shortName), QLatin1String(p->zone), p->lat, p->lon, false};
    }
    m_footer = s.value(QStringLiteral("worldClock/footer"), QStringList{QStringLiteral("nyc"), QStringLiteral("tyo")})
                   .toStringList();
    m_selected = s.value(QStringLiteral("worldClock/selected"), QStringLiteral("home")).toString();
}

void WorldClockController::save()
{
    QSettings s;
    QStringList ids;
    for (const City& c : std::as_const(m_cities))
        ids << c.id;
    s.setValue(QStringLiteral("worldClock/cities"), ids);
    s.setValue(QStringLiteral("worldClock/footer"), m_footer);
    s.setValue(QStringLiteral("worldClock/selected"), m_selected);
}

QList<WorldClockController::City> WorldClockController::allCities() const
{
    QList<City> out;
    // Il QTH, se la stazione ha un locatore: sempre per primo, con il fuso di
    // questo computer.
    const QVariantMap pos = m_ctx.stationPosition ? m_ctx.stationPosition() : QVariantMap();
    if (pos.contains(QStringLiteral("lat"))) {
        const QString grid = m_ctx.stationGrid ? m_ctx.stationGrid().trimmed().toUpper() : QString();
        out << City{QStringLiteral("home"), QCoreApplication::translate("WorldClock", "Station QTH"), grid,
                    QString::fromUtf8(QTimeZone::systemTimeZoneId()), pos.value(QStringLiteral("lat")).toDouble(),
                    pos.value(QStringLiteral("lon")).toDouble(), true};
    }
    out << m_cities;
    return out;
}

QString WorldClockController::clock(const QDateTime& t, bool seconds) const
{
    return t.toString(seconds ? QStringLiteral("HH:mm:ss") : QStringLiteral("HH:mm"));
}

QString WorldClockController::shortDate(const QDate& d) const
{
    return m_locale.toString(d, QStringLiteral("ddd d MMM"));
}

QVariantMap WorldClockController::infoFor(const City& city) const
{
    const QTimeZone zone(city.zone.toUtf8());
    const QDateTime local = zone.isValid() ? m_now.toTimeZone(zone) : m_now.toLocalTime();
    const int offset = zone.isValid() ? zone.offsetFromUtc(m_now) : local.offsetFromUtc();
    const wc::SubSolar sun = wc::subSolar(m_now);
    const double altitude = wc::sunAltitude(city.lat, city.lon, sun);
    const QString state = altitude >= 0.0 ? QStringLiteral("day")
                        : altitude >= wc::kCivil ? QStringLiteral("grayline") : QStringLiteral("night");
    const wc::SunDay day = wc::sunDay(local.date(), city.lat, city.lon);
    auto localTime = [&](const QDateTime& t) {
        return t.isValid() ? clock(zone.isValid() ? t.toTimeZone(zone) : t.toLocalTime(), false) : QStringLiteral("—");
    };
    const qint64 dayShift = m_now.date().daysTo(local.date());

    QVariantMap out{
        {QStringLiteral("id"), city.id},
        {QStringLiteral("name"), city.name},
        {QStringLiteral("short"), city.shortName.isEmpty() ? QStringLiteral("QTH") : city.shortName},
        {QStringLiteral("home"), city.home},
        {QStringLiteral("lat"), city.lat},
        {QStringLiteral("lon"), city.lon},
        {QStringLiteral("locator"), city.home && !city.shortName.isEmpty() ? city.shortName : wc::locator4(city.lat, city.lon)},
        {QStringLiteral("time"), clock(local, false)},
        {QStringLiteral("seconds"), local.toString(QStringLiteral(":ss"))},
        {QStringLiteral("offset"), wc::offsetLabel(offset)},
        {QStringLiteral("date"), shortDate(local.date())},
        {QStringLiteral("dayShift"), dayShift == 0 ? QString()
                                     : QCoreApplication::translate("WorldClock", "(%1%2 d)")
                                           .arg(dayShift > 0 ? QStringLiteral("+") : QString(QChar(0x2212)))
                                           .arg(std::abs(dayShift))},
        {QStringLiteral("state"), state},
        {QStringLiteral("stateText"), state == QLatin1String("day") ? QCoreApplication::translate("WorldClock", "Day")
                                      : state == QLatin1String("grayline") ? QCoreApplication::translate("WorldClock", "Grayline")
                                                                           : QCoreApplication::translate("WorldClock", "Night")},
        {QStringLiteral("altitude"), altitude},
        {QStringLiteral("footer"), m_footer.contains(city.id)},
        {QStringLiteral("rise"), day.sun.valid ? localTime(day.sun.rise) : QStringLiteral("—")},
        {QStringLiteral("set"), day.sun.valid ? localTime(day.sun.set) : QStringLiteral("—")},
        {QStringLiteral("polar"), day.sun.valid ? QString()
                                  : day.sun.alwaysUp ? QCoreApplication::translate("WorldClock", "sun 24 h")
                                                     : QCoreApplication::translate("WorldClock", "polar night")},
    };
    return out;
}

void WorldClockController::update()
{
    if (!m_frozen)
        m_now = QDateTime::currentDateTimeUtc();
    m_cityInfo.clear();
    for (const City& c : allCities())
        m_cityInfo << infoFor(c);
    emit tick();
}

void WorldClockController::freezeAt(const QDateTime& utc)
{
    m_frozen = true;
    m_now = utc.toUTC();
    update();
}

void WorldClockController::setActive(bool on)
{
    if (on == m_timer.isActive())
        return;
    if (on) {
        m_timer.start();
        update();
    } else {
        m_timer.stop();
    }
    emit activeChanged();
}

QString WorldClockController::utcClock() const
{
    return clock(m_now, true);
}

QString WorldClockController::utcDate() const
{
    return m_locale.toString(m_now.date(), QStringLiteral("ddd d MMM yyyy"));
}

QVariantMap WorldClockController::subSolar() const
{
    const wc::SubSolar s = wc::subSolar(m_now);
    return {{QStringLiteral("lat"), s.lat},
            {QStringLiteral("lon"), s.lon},
            {QStringLiteral("text"), QStringLiteral("%1 %2").arg(degrees(s.lat, 'N', 'S', m_locale),
                                                                 degrees(s.lon, 'E', 'W', m_locale))}};
}

QVariantMap WorldClockController::home() const
{
    for (const QVariant& v : m_cityInfo) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("home")).toBool())
            return m;
    }
    return {};
}

QVariantList WorldClockController::footerCities() const
{
    QVariantList out;
    for (const QString& id : m_footer) {
        for (const QVariant& v : m_cityInfo) {
            if (v.toMap().value(QStringLiteral("id")).toString() == id)
                out << v;
        }
    }
    return out;
}

QVariantMap WorldClockController::detail() const
{
    const QList<City> all = allCities();
    const City* city = nullptr;
    for (const City& c : all) {
        if (c.id == m_selected)
            city = &c;
    }
    if (!city && !all.isEmpty())
        city = &all.first();
    if (!city)
        return {};
    QVariantMap out = infoFor(*city);
    const QTimeZone zone(city->zone.toUtf8());
    const QDateTime local = zone.isValid() ? m_now.toTimeZone(zone) : m_now.toLocalTime();
    const wc::SunDay day = wc::sunDay(local.date(), city->lat, city->lon);
    auto both = [&](const QDateTime& t) {
        return t.isValid() ? QVariantMap{{QStringLiteral("local"), clock(zone.isValid() ? t.toTimeZone(zone) : t.toLocalTime(), false)},
                                         {QStringLiteral("utc"), clock(t.toUTC(), false) + QStringLiteral(" UTC")}}
                           : QVariantMap{{QStringLiteral("local"), QStringLiteral("—")}, {QStringLiteral("utc"), QString()}};
    };
    const QString polar = out.value(QStringLiteral("polar")).toString();
    auto tile = [](const QString& label, const QString& value, const QString& sub) {
        return QVariantMap{{QStringLiteral("label"), label}, {QStringLiteral("value"), value}, {QStringLiteral("sub"), sub}};
    };
    const QVariantMap rise = both(day.sun.valid ? day.sun.rise : QDateTime());
    const QVariantMap set = both(day.sun.valid ? day.sun.set : QDateTime());
    const QVariantMap noon = both(day.noon);
    const QString length = day.sun.valid ? duration(day.sun.rise.secsTo(day.sun.set))
                         : day.sun.alwaysUp ? QStringLiteral("24 h") : QStringLiteral("0 h");
    auto span = [&](const QDateTime& a, const QDateTime& b) {
        if (!a.isValid() || !b.isValid())
            return tile(QString(), QStringLiteral("—"), QString());
        const QVariantMap ta = both(a);
        const QVariantMap tb = both(b);
        return tile(QString(),
                    ta.value(QStringLiteral("local")).toString() + QStringLiteral(" → ") + tb.value(QStringLiteral("local")).toString(),
                    clock(a.toUTC(), false) + QStringLiteral(" → ") + clock(b.toUTC(), false) + QStringLiteral(" UTC"));
    };
    QVariantMap morning = span(day.civil.valid && day.sun.valid ? day.civil.rise : QDateTime(), day.sun.valid ? day.sun.rise : QDateTime());
    morning[QStringLiteral("label")] = QCoreApplication::translate("WorldClock", "Morning grayline");
    QVariantMap evening = span(day.sun.valid ? day.sun.set : QDateTime(), day.civil.valid && day.sun.valid ? day.civil.set : QDateTime());
    evening[QStringLiteral("label")] = QCoreApplication::translate("WorldClock", "Evening grayline");
    const double altitude = out.value(QStringLiteral("altitude")).toDouble();

    out.insert(QStringLiteral("coords"), QStringLiteral("%1 %2").arg(degrees(city->lat, 'N', 'S', m_locale),
                                                                      degrees(city->lon, 'E', 'W', m_locale)));
    out.insert(QStringLiteral("tiles"), QVariantList{
        tile(QCoreApplication::translate("WorldClock", "Sunrise"),
             polar.isEmpty() ? rise.value(QStringLiteral("local")).toString() : polar, rise.value(QStringLiteral("utc")).toString()),
        tile(QCoreApplication::translate("WorldClock", "Sunset"),
             polar.isEmpty() ? set.value(QStringLiteral("local")).toString() : polar, set.value(QStringLiteral("utc")).toString()),
        tile(QCoreApplication::translate("WorldClock", "Solar noon"), noon.value(QStringLiteral("local")).toString(),
             noon.value(QStringLiteral("utc")).toString()),
        tile(QCoreApplication::translate("WorldClock", "Day length"), length, polar),
        morning,
        evening,
        tile(QCoreApplication::translate("WorldClock", "Sun altitude"),
             m_locale.toString(altitude, 'f', 1) + QStringLiteral("°"),
             altitude >= 0 ? QCoreApplication::translate("WorldClock", "above the horizon")
                           : QCoreApplication::translate("WorldClock", "below the horizon")),
        tile(QCoreApplication::translate("WorldClock", "Local time"), clock(local, true),
             out.value(QStringLiteral("offset")).toString() + QStringLiteral(" · ") + out.value(QStringLiteral("date")).toString()),
    });
    return out;
}

void WorldClockController::setSelected(const QString& id)
{
    if (id == m_selected)
        return;
    m_selected = id;
    save();
    emit selectionChanged();
    emit tick();
}

QVariantList WorldClockController::catalog() const
{
    QVariantList out;
    for (const Preset& p : kPresets) {
        bool present = false;
        for (const City& c : m_cities)
            present = present || c.id == QLatin1String(p.id);
        out << QVariantMap{{QStringLiteral("id"), QLatin1String(p.id)},
                           {QStringLiteral("name"), QCoreApplication::translate("WorldClock", p.name)},
                           {QStringLiteral("short"), QLatin1String(p.shortName)},
                           {QStringLiteral("present"), present}};
    }
    return out;
}

void WorldClockController::addCity(const QString& id)
{
    const Preset* p = presetFor(id);
    if (!p)
        return;
    for (const City& c : std::as_const(m_cities)) {
        if (c.id == id)
            return;
    }
    m_cities << City{QLatin1String(p->id), QCoreApplication::translate("WorldClock", p->name),
                     QLatin1String(p->shortName), QLatin1String(p->zone), p->lat, p->lon, false};
    save();
    emit citiesChanged();
    update();
}

void WorldClockController::removeCity(const QString& id)
{
    for (qsizetype i = 0; i < m_cities.size(); ++i) {
        if (m_cities.at(i).id == id) {
            m_cities.removeAt(i);
            m_footer.removeAll(id);
            if (m_selected == id)
                m_selected = QStringLiteral("home");
            save();
            emit citiesChanged();
            emit selectionChanged();
            update();
            return;
        }
    }
}

void WorldClockController::toggleFooter(const QString& id)
{
    if (id == QLatin1String("home"))
        return;
    if (m_footer.contains(id)) {
        m_footer.removeAll(id);
    } else {
        m_footer << id;
        while (m_footer.size() > 2)
            m_footer.removeFirst();
    }
    save();
    emit citiesChanged();
    update();
}

void WorldClockController::resetCities()
{
    QSettings s;
    s.remove(QStringLiteral("worldClock"));
    load();
    emit citiesChanged();
    emit selectionChanged();
    update();
}

} // namespace decolog::app
