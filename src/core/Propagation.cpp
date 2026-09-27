#include "core/Propagation.h"

#include "core/Maidenhead.h"
#include "core/WorldClock.h"

#include <QTimeZone>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace decolog::core::propagation {

namespace {

constexpr double kMaxHopKm = 4000.0;

double rad(double d) { return d * M_PI / 180.0; }
double deg(double r) { return r * 180.0 / M_PI; }

// Il punto a `fraction` del cerchio massimo fra due punti.
maidenhead::LatLon along(const maidenhead::LatLon& a, const maidenhead::LatLon& b, double fraction)
{
    const double lat1 = rad(a.lat), lon1 = rad(a.lon), lat2 = rad(b.lat), lon2 = rad(b.lon);
    const double d = 2.0 * std::asin(std::sqrt(std::pow(std::sin((lat2 - lat1) / 2.0), 2)
                                               + std::cos(lat1) * std::cos(lat2) * std::pow(std::sin((lon2 - lon1) / 2.0), 2)));
    if (d < 1e-9)
        return a;
    const double A = std::sin((1.0 - fraction) * d) / std::sin(d);
    const double B = std::sin(fraction * d) / std::sin(d);
    const double x = A * std::cos(lat1) * std::cos(lon1) + B * std::cos(lat2) * std::cos(lon2);
    const double y = A * std::cos(lat1) * std::sin(lon1) + B * std::cos(lat2) * std::sin(lon2);
    const double z = A * std::sin(lat1) + B * std::sin(lat2);
    return {deg(std::atan2(z, std::sqrt(x * x + y * y))), deg(std::atan2(y, x))};
}

// Il coseno dell'angolo zenitale del Sole (negativo di notte).
double cosZenith(double lat, double lon, const QDateTime& utc)
{
    const auto sun = worldclock::subSolar(utc);
    return std::sin(rad(worldclock::sunAltitude(lat, lon, sun)));
}

// Il fattore che porta dalla frequenza critica alla MUF di un salto lungo
// `hopKm`: 1 in verticale, circa 3 per un salto di 3000 km.
double obliquity(double hopKm)
{
    return 1.0 + 2.3 * (1.0 - std::exp(-hopKm / 1700.0));
}

} // namespace

const QList<Band>& bands()
{
    static const QList<Band> list{
        {QStringLiteral("160m"), 1.83}, {QStringLiteral("80m"), 3.6},   {QStringLiteral("60m"), 5.36},
        {QStringLiteral("40m"), 7.1},   {QStringLiteral("30m"), 10.12}, {QStringLiteral("20m"), 14.1},
        {QStringLiteral("17m"), 18.1},  {QStringLiteral("15m"), 21.2},  {QStringLiteral("12m"), 24.94},
        {QStringLiteral("10m"), 28.5},  {QStringLiteral("6m"), 50.15},
    };
    return list;
}

double foF2(double lat, double lon, const QDateTime& utc, double sunspots)
{
    const double r = std::clamp(sunspots, 0.0, 250.0);
    // Lo strato F2 non si spegne al tramonto: si scarica piano. Conta il Sole
    // di adesso e, un po' meno, quello di un'ora e mezza fa.
    const double now = cosZenith(lat, lon, utc);
    const double before = cosZenith(lat, lon, utc.addSecs(-90 * 60));
    const double c = std::max({now, 0.8 * before, 0.0});
    const double day = (5.5 + 0.045 * r) * std::pow(c, 0.35);
    // Di notte resta un fondo, piu' alto con tante macchie; piu' basso alle
    // latitudini alte.
    const double night = (2.8 + 0.018 * r) * (1.0 - 0.25 * std::min(1.0, std::abs(lat) / 90.0));
    return std::max(day, night);
}

Forecast forecast(const Input& in)
{
    Forecast out;
    const maidenhead::LatLon from{in.fromLat, in.fromLon};
    const maidenhead::LatLon to{in.toLat, in.toLon};
    out.distanceKm = maidenhead::distanceKm(from, to);
    out.azimuth = qRound(maidenhead::azimuthDeg(from, to));
    if (!in.date.isValid() || out.distanceKm < 1.0)
        return out;
    out.valid = true;

    double r = in.sunspots;
    if (r < 0)
        r = std::max(0.0, (in.solarFlux - 67.0) * 1.2);
    out.hops = std::max(1, int(std::ceil(out.distanceKm / kMaxHopKm)));
    const double hopKm = out.distanceKm / out.hops;

    // I punti di controllo: il centro per un salto solo, altrimenti a un
    // salto da ciascun capo (e il centro, che per i percorsi lunghi conta).
    QList<double> controls;
    if (out.hops == 1) {
        controls << 0.5;
    } else {
        const double f = (hopKm / 2.0) / out.distanceKm;
        controls << f << 0.5 << 1.0 - f;
    }
    // L'assorbimento dello strato D si paga due volte per salto, salendo e
    // scendendo: a un quarto e a tre quarti di ogni salto.
    QList<double> absorptionPoints;
    for (int i = 0; i < out.hops; ++i)
        absorptionPoints << (i + 0.25) / out.hops << (i + 0.75) / out.hops;

    const double stormFactor = in.kIndex >= 7 ? 0.75 : in.kIndex >= 5 ? 0.85 : in.kIndex >= 4 ? 0.93 : 1.0;
    const double fluxFactor = 1.0 + 0.004 * std::max(0.0, in.solarFlux - 70.0);

    for (int h = 0; h < 24; ++h) {
        const QDateTime t(in.date, QTime(h, 30), QTimeZone::UTC);
        Hour hour;
        hour.hourUtc = h;
        double muf = 1e9;
        for (double f : controls) {
            const auto p = along(from, to, f);
            muf = std::min(muf, foF2(p.lat, p.lon, t, r) * obliquity(hopKm));
        }
        // Le tempeste pesano di piu' se il percorso passa alto.
        double maxLat = 0.0;
        for (double f = 0.0; f <= 1.0; f += 0.1)
            maxLat = std::max(maxLat, std::abs(along(from, to, f).lat));
        if (maxLat > 55.0)
            muf *= stormFactor;
        hour.mufMhz = muf;

        double absorption = 0.0;
        for (double f : absorptionPoints) {
            const auto p = along(from, to, f);
            absorption += std::pow(std::max(0.0, cosZenith(p.lat, p.lon, t)), 0.75);
        }
        // LUF: sui 2 MHz di notte, fino a una decina di giorno su un percorso
        // lungo tutto illuminato.
        hour.lufMhz = 1.6 + 2.4 * fluxFactor * absorption;

        const double fot = 0.85 * muf;
        for (const Band& b : bands()) {
            int q = Closed;
            if (b.mhz <= muf && b.mhz >= hour.lufMhz) {
                if (b.mhz > fot)
                    q = Marginal;
                else if (b.mhz < hour.lufMhz * 1.25)
                    q = Fair;
                else
                    q = Good;
            }
            hour.quality << q;
        }
        out.hours << hour;
    }
    return out;
}

} // namespace decolog::core::propagation
