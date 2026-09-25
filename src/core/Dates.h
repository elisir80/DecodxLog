// DecoDXLog — le date come le legge chi usa il programma.
//
// Dentro (database, ADIF, file) le date restano ISO, anno-mese-giorno, che si
// ordinano da sole. Solo quando arrivano davanti a qualcuno prendono la forma
// della sua lingua: in italiano 25/09/2026, in tedesco 25.09.2026, in inglese
// e nelle lingue che partono dall'anno come prima, 2026-09-25. E quello che si
// scrive a mano si capisce in tutte e due le forme.
#pragma once

#include <QString>

namespace decolog::core::dates {

// La lingua dell'interfaccia ("it", "de", "zh_TW"…): si dice una volta
// all'avvio, come i file di traduzione.
void setLanguage(const QString& language);

// Il giorno viene per primo?
bool dayFirst();

// Il formato completo ("dd/MM/yyyy") e quello corto per le tabelle ("dd/MM/yy").
QString format();
QString shortFormat();

// Da ISO ("2026-09-25", anche con l'ora dopo) o ADIF ("20260925") alla forma
// della lingua; l'ora, se c'e', resta com'e'. Quello che non e' una data torna
// uguale.
QString show(const QString& text);
QString showShort(const QString& text);

// Quello che ha scritto l'operatore, in ISO: capisce la forma della lingua,
// ISO e ADIF. Vuoto se non e' una data.
QString read(const QString& text);

} // namespace decolog::core::dates
