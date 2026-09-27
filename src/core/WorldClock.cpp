#include "core/WorldClock.h"

#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace decolog::core::worldclock {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRad = kPi / 180.0;
constexpr double kDayMs = 86400000.0;
constexpr double kJ1970 = 2440588.0;
constexpr double kJ2000 = 2451545.0;
constexpr double kObliquity = 23.4397 * kRad;
constexpr double kJ0 = 0.0009;

double toDays(double ms) { return ms / kDayMs - 0.5 + kJ1970 - kJ2000; }
double fromJulian(double j) { return (j + 0.5 - kJ1970) * kDayMs; }
double meanAnomaly(double d) { return kRad * (357.5291 + 0.98560028 * d); }
double eclipticLongitude(double m)
{
    const double c = kRad * (1.9148 * std::sin(m) + 0.02 * std::sin(2 * m) + 0.0003 * std::sin(3 * m));
    return m + c + kRad * 102.9372 + kPi;
}
double declination(double l) { return std::asin(std::sin(kObliquity) * std::sin(l)); }

double normalizeLon(double lon)
{
    double out = std::fmod(lon + 180.0, 360.0);
    if (out < 0)
        out += 360.0;
    return out - 180.0;
}

QDateTime fromMs(double ms)
{
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(std::llround(ms)), QTimeZone::UTC);
}

} // namespace

SubSolar subSolar(const QDateTime& utc)
{
    const double t = static_cast<double>(utc.toMSecsSinceEpoch());
    const double d = toDays(t);
    SubSolar s;
    s.lat = declination(eclipticLongitude(meanAnomaly(d))) / kRad;
    // Il mezzogiorno solare di Greenwich di quel giorno: da li' ci si sposta di
    // 15° all'ora verso ovest.
    const double n = std::round(d - kJ0);
    const double ds = kJ0 + n;
    const double m = meanAnomaly(ds);
    const double l = eclipticLongitude(m);
    const double noon = fromJulian(kJ2000 + ds + 0.0053 * std::sin(m) - 0.0069 * std::sin(2 * l));
    s.lon = normalizeLon(-((t - noon) / 3600000.0) * 15.0);
    return s;
}

double sunAltitude(double lat, double lon, const SubSolar& sun)
{
    const double phi = lat * kRad;
    const double delta = sun.lat * kRad;
    const double h = (lon - sun.lon) * kRad;
    const double v = std::sin(phi) * std::sin(delta) + std::cos(phi) * std::cos(delta) * std::cos(h);
    return std::asin(std::clamp(v, -1.0, 1.0)) / kRad;
}

SunDay sunDay(const QDate& localDate, double lat, double lon)
{
    // Mezzogiorno UTC della data, spostato del fuso "solare" del posto: cosi'
    // il ciclo trovato e' quello della data locale.
    const QDateTime base(localDate, QTime(12, 0), QTimeZone::UTC);
    const double ms = static_cast<double>(base.toMSecsSinceEpoch()) - lon / 15.0 * 3600000.0;
    const double lw = -lon * kRad;
    const double phi = lat * kRad;
    const double dd = toDays(ms);
    const double n = std::round(dd - kJ0 - lw / (2 * kPi));
    const double ds = kJ0 + lw / (2 * kPi) + n;
    const double m = meanAnomaly(ds);
    const double l = eclipticLongitude(m);
    const double delta = declination(l);
    const double jnoon = kJ2000 + ds + 0.0053 * std::sin(m) - 0.0069 * std::sin(2 * l);

    SunDay day;
    day.noon = fromMs(fromJulian(jnoon));
    auto events = [&](double h) {
        SunEvents e;
        const double c = (std::sin(h * kRad) - std::sin(phi) * std::sin(delta)) / (std::cos(phi) * std::cos(delta));
        if (c > 1.0) {
            e.alwaysUp = false;   // notte (polare): la soglia non si raggiunge
            return e;
        }
        if (c < -1.0) {
            e.alwaysUp = true;    // il Sole non scende sotto la soglia
            return e;
        }
        const double w = std::acos(c);
        const double jset = kJ2000 + kJ0 + (w + lw) / (2 * kPi) + n + 0.0053 * std::sin(m) - 0.0069 * std::sin(2 * l);
        const double jrise = jnoon - (jset - jnoon);
        e.valid = true;
        e.rise = fromMs(fromJulian(jrise));
        e.set = fromMs(fromJulian(jset));
        return e;
    };
    day.sun = events(kSunset);
    day.civil = events(kCivil);
    return day;
}

Terminator terminator(const SubSolar& sun, double h, double stepDeg)
{
    Terminator t;
    // Si parte dal polo illuminato e si scende finche' il Sole non va sotto la
    // soglia: li' e' il confine.
    const bool litNorth = sun.lat >= 0.0;
    t.nightNorth = !litNorth;
    const double start = litNorth ? 90.0 : -90.0;
    const double dir = litNorth ? -1.0 : 1.0;
    const double step = stepDeg > 0 ? stepDeg : 2.0;
    for (double lon = -180.0; lon <= 180.0 + 1e-9; lon += step) {
        double boundary = litNorth ? -90.0 : 90.0;   // nessun confine: colonna tutta in luce
        const double first = sunAltitude(start, lon, sun);
        if (first < h) {
            boundary = start;                          // gia' in ombra al polo
        } else {
            double prevLat = start;
            double prevAlt = first;
            for (int i = 1; i <= 180; ++i) {
                const double lat = start + dir * i;
                const double alt = sunAltitude(lat, lon, sun);
                if (alt < h) {
                    const double f = (prevAlt - h) / (prevAlt - alt);
                    boundary = prevLat + (lat - prevLat) * f;
                    break;
                }
                prevLat = lat;
                prevAlt = alt;
            }
        }
        t.boundary.append(QPointF(lon, boundary));
    }
    return t;
}

QString locator4(double lat, double lon)
{
    double l = lon + 180.0;
    double a = lat + 90.0;
    l = std::clamp(l, 0.0, 359.999);
    a = std::clamp(a, 0.0, 179.999);
    QString out;
    out += QChar(u'A' + static_cast<int>(l / 20.0));
    out += QChar(u'A' + static_cast<int>(a / 10.0));
    out += QChar(u'0' + static_cast<int>(std::fmod(l, 20.0) / 2.0));
    out += QChar(u'0' + static_cast<int>(std::fmod(a, 10.0)));
    return out;
}

QString offsetLabel(int offsetSeconds)
{
    if (offsetSeconds == 0)
        return QStringLiteral("UTC±0");
    const QChar sign = offsetSeconds > 0 ? QLatin1Char('+') : QChar(0x2212);
    const int abs = std::abs(offsetSeconds);
    const int h = abs / 3600;
    const int m = (abs % 3600) / 60;
    return m == 0 ? QStringLiteral("UTC%1%2").arg(sign).arg(h)
                  : QStringLiteral("UTC%1%2:%3").arg(sign).arg(h).arg(m, 2, 10, QLatin1Char('0'));
}

} // namespace decolog::core::worldclock
