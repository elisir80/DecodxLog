// DecoDXLog — l'orologio mondiale: il Sole, l'alba, il tramonto e la grayline.
//
// Tutto calcolato qui, senza chiedere niente a nessuno in rete: le formule
// sono quelle compatte dell'algoritmo di Vladimir Agafonkin (SunCalc), che
// sulle date di oggi sbagliano di meno di un minuto. Il punto subsolare dice
// dove il Sole sta a picco; l'altezza del Sole dice se in un posto e' giorno,
// crepuscolo o notte; le soglie (-0,833°, -6°, -12°, -18°) disegnano sulla
// mappa le fasce della notte e la grayline.
#pragma once

#include <QDateTime>
#include <QList>
#include <QPointF>
#include <QString>

#include <optional>

namespace decolog::core::worldclock {

// Soglie dell'altezza del Sole, in gradi.
inline constexpr double kSunset = -0.833;     // bordo del disco all'orizzonte, con la rifrazione
inline constexpr double kCivil = -6.0;
inline constexpr double kNautical = -12.0;
inline constexpr double kAstronomical = -18.0;

struct SubSolar {
    double lat{0};   // declinazione del Sole
    double lon{0};   // -180..180
};

// Dove il Sole sta a picco, in quell'istante.
SubSolar subSolar(const QDateTime& utc);

// Altezza del Sole sull'orizzonte (gradi) in un punto, con il punto subsolare
// gia' calcolato.
double sunAltitude(double lat, double lon, const SubSolar& sun);

// Gli orari del Sole di un giorno, in UTC. `valid` falso vuol dire che quella
// soglia quel giorno non si attraversa: `alwaysUp` (sole 24 h) oppure notte.
struct SunEvents {
    bool valid{false};
    bool alwaysUp{false};
    QDateTime rise;
    QDateTime set;
};
struct SunDay {
    QDateTime noon;          // mezzogiorno solare
    SunEvents sun;           // alba e tramonto (-0,833°)
    SunEvents civil;         // inizio e fine del crepuscolo civile (-6°)
};

// Il giorno solare di una data (quella locale del posto) in un punto.
SunDay sunDay(const QDate& localDate, double lat, double lon);

// Il confine di una soglia sulla mappa: per ogni longitudine da -180 a +180
// (passo `stepDeg`), la latitudine dove il Sole attraversa l'altezza `h`.
// Punti (lon, lat). La parte in ombra e' verso il polo `nightNorth`.
struct Terminator {
    QList<QPointF> boundary;
    bool nightNorth{false};
};
Terminator terminator(const SubSolar& sun, double h, double stepDeg = 2.0);

// Locatore Maidenhead a 4 caratteri.
QString locator4(double lat, double lon);

// "UTC+2", "UTC−4", "UTC±0", "UTC+5:30".
QString offsetLabel(int offsetSeconds);

} // namespace decolog::core::worldclock
