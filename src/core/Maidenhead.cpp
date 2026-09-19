#include "core/Maidenhead.h"

#include <cmath>

namespace decolog::core::maidenhead {

namespace {

constexpr double kEarthRadiusKm = 6371.0;
constexpr double kPi = 3.14159265358979323846;

double rad(double deg) { return deg * kPi / 180.0; }
double deg(double rad) { return rad * 180.0 / kPi; }

} // namespace

std::optional<LatLon> toLatLon(const QString& locator)
{
    const QString l = locator.trimmed().toUpper();
    const qsizetype n = l.size();
    if (n < 2 || n > 8 || n % 2 != 0)
        return std::nullopt;

    auto inRange = [](QChar c, char lo, char hi) { return c.unicode() >= lo && c.unicode() <= hi; };
    if (!inRange(l[0], 'A', 'R') || !inRange(l[1], 'A', 'R'))
        return std::nullopt;

    double lon = (l[0].unicode() - 'A') * 20.0 - 180.0;
    double lat = (l[1].unicode() - 'A') * 10.0 - 90.0;
    double lonSize = 20.0;
    double latSize = 10.0;

    if (n >= 4) {
        if (!inRange(l[2], '0', '9') || !inRange(l[3], '0', '9'))
            return std::nullopt;
        lonSize = 2.0;
        latSize = 1.0;
        lon += (l[2].unicode() - '0') * lonSize;
        lat += (l[3].unicode() - '0') * latSize;
    }
    if (n >= 6) {
        if (!inRange(l[4], 'A', 'X') || !inRange(l[5], 'A', 'X'))
            return std::nullopt;
        lonSize /= 24.0;
        latSize /= 24.0;
        lon += (l[4].unicode() - 'A') * lonSize;
        lat += (l[5].unicode() - 'A') * latSize;
    }
    if (n == 8) {
        if (!inRange(l[6], '0', '9') || !inRange(l[7], '0', '9'))
            return std::nullopt;
        lonSize /= 10.0;
        latSize /= 10.0;
        lon += (l[6].unicode() - '0') * lonSize;
        lat += (l[7].unicode() - '0') * latSize;
    }
    return LatLon{lat + latSize / 2.0, lon + lonSize / 2.0};
}

QString fromLatLon(double lat, double lon, int characters)
{
    if (!(lat >= -90.0 && lat <= 90.0) || !(lon >= -180.0 && lon <= 180.0))
        return {};
    if (characters != 4 && characters != 6 && characters != 8)
        characters = 6;

    // Si conta dal punto opposto al meridiano di Greenwich e dal polo sud, come
    // vuole il Maidenhead: campo, quadrato, sotto-quadrato, quadratino.
    double x = std::min(lon + 180.0, 359.999999);
    double y = std::min(lat + 90.0, 179.999999);

    QString out;
    out += QChar(QLatin1Char('A').unicode() + static_cast<int>(x / 20.0));
    out += QChar(QLatin1Char('A').unicode() + static_cast<int>(y / 10.0));
    x = std::fmod(x, 20.0);
    y = std::fmod(y, 10.0);
    out += QChar(QLatin1Char('0').unicode() + static_cast<int>(x / 2.0));
    out += QChar(QLatin1Char('0').unicode() + static_cast<int>(y));
    if (characters >= 6) {
        x = std::fmod(x, 2.0);
        y = std::fmod(y, 1.0);
        out += QChar(QLatin1Char('A').unicode() + static_cast<int>(x / (2.0 / 24.0)));
        out += QChar(QLatin1Char('A').unicode() + static_cast<int>(y / (1.0 / 24.0)));
    }
    if (characters == 8) {
        x = std::fmod(x, 2.0 / 24.0);
        y = std::fmod(y, 1.0 / 24.0);
        out += QChar(QLatin1Char('0').unicode() + static_cast<int>(x / (2.0 / 240.0)));
        out += QChar(QLatin1Char('0').unicode() + static_cast<int>(y / (1.0 / 240.0)));
    }
    return out;
}

double distanceKm(const LatLon& a, const LatLon& b)
{
    const double dLat = rad(b.lat - a.lat);
    const double dLon = rad(b.lon - a.lon);
    const double h = std::sin(dLat / 2) * std::sin(dLat / 2)
                   + std::cos(rad(a.lat)) * std::cos(rad(b.lat)) * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2.0 * kEarthRadiusKm * std::asin(std::min(1.0, std::sqrt(h)));
}

double azimuthDeg(const LatLon& a, const LatLon& b)
{
    const double dLon = rad(b.lon - a.lon);
    const double y = std::sin(dLon) * std::cos(rad(b.lat));
    const double x = std::cos(rad(a.lat)) * std::sin(rad(b.lat))
                   - std::sin(rad(a.lat)) * std::cos(rad(b.lat)) * std::cos(dLon);
    return std::fmod(deg(std::atan2(y, x)) + 360.0, 360.0);
}

} // namespace decolog::core::maidenhead
