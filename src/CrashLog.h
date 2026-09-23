// DecoDXLog — il biglietto lasciato quando il programma cade.
//
// Windows, quando un programma si chiude per un errore, segna nel suo registro
// solo il punto esatto dove e' caduto: una riga, che non dice chi ce l'ha
// portato. Qui, se succede, si scrive in un file accanto al log la strada
// intera — le funzioni una dentro l'altra, come indirizzi da tradurre dopo con
// l'eseguibile di quella versione — e cosa stava facendo il programma.
#pragma once

#include <QString>

namespace decolog::crashlog {

// Da chiamare una volta, presto: `folder` e' dove finiscono i file crash-*.txt.
void install(const QString& folder, const QString& version);

// Cosa sta facendo il programma adesso, in poche parole: finisce nel biglietto
// se cade. Solo testo statico (una stringa letterale), niente allocazioni.
void setStage(const char* stage);

} // namespace decolog::crashlog
