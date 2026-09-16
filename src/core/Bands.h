// DecoLog — bande ADIF ricavate dalla frequenza.
#pragma once

#include <QString>
#include <QStringList>

namespace decolog::core::bands {

// Nome ADIF in minuscolo ("40m", "70cm"), o stringa vuota fuori banda.
QString fromMhz(double mhz);

// Le bande nell'ordine in cui le vuole leggere un operatore: dalla piu' bassa.
QStringList all();

} // namespace decolog::core::bands
