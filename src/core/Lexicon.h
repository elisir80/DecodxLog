// Portato da iu8lmc/Morsecodium (src/decode/): correzione lessicale delle parole CW.
#pragma once
#include <QString>

// Correzione lessicale a livello parola per il traffico CW: Q-code, prosegni,
// abbreviazioni ham e parole comuni dei QSO. Una parola decodificata che non
// e' nel lessico viene confrontata con le voci usando una distanza di edit
// "Morse-aware" (sostituire una lettera con una dal pattern Morse vicino costa
// meno: e' l'errore che il decoder fa davvero, E<->I, N<->D, S<->H...).
// Si corregge SOLO con un vincitore netto entro il budget di un errore:
// meglio lasciare una parola strana che inventarne una sbagliata.
namespace lexicon {

// Correzione di `word` (maiuscola), o `word` stessa se e' gia' plausibile,
// ambigua o fuori dominio (cifre = rapporti/nominativi, troppo corta/lunga).
QString correct(const QString &word);

// true se la parola e' una voce nota: lessico oppure sigla protetta (IARU).
bool contains(const QString &word);

} // namespace lexicon
