// DecoDXLog — bande ADIF ricavate dalla frequenza.
#pragma once

#include <QString>
#include <QStringList>

namespace decolog::core::bands {

// Nome ADIF in minuscolo ("40m", "70cm"), o stringa vuota fuori banda.
QString fromMhz(double mhz);

// Le bande nell'ordine in cui le vuole leggere un operatore: dalla piu' bassa.
QStringList all();

// Dove andare quando si sceglie una banda e un modo (MHz): la frequenza di
// chiamata per i digitali, l'inizio del segmento CW o fonia per gli altri,
// secondo il piano IARU Regione 1. 0 se la banda non si conosce.
double defaultFrequency(const QString& band, const QString& mode);

} // namespace decolog::core::bands
