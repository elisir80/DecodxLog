// DecoLog — locatori Maidenhead: posizione, distanza e direzione.
#pragma once

#include <QString>
#include <optional>

namespace decolog::core::maidenhead {

struct LatLon {
    double lat{0.0};
    double lon{0.0};
};

// Centro del quadrato indicato (2, 4, 6 o 8 caratteri). nullopt se non valido.
std::optional<LatLon> toLatLon(const QString& locator);

// Distanza sul cerchio massimo, in km.
double distanceKm(const LatLon& from, const LatLon& to);

// Direzione iniziale da `from` verso `to`, in gradi da nord (0-360).
double azimuthDeg(const LatLon& from, const LatLon& to);

} // namespace decolog::core::maidenhead
