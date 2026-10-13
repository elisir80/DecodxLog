// Portato da iu8lmc/Morsecodium (src/decode/), decodificatore CW neurale CTC. Attivo solo con DECODXLOG_CTC_ONNX.
#pragma once
#include <QChar>
#include <QString>
#include <QHash>

// Tabelle di codifica/decodifica Morse internazionale (ITU) + alcuni segni
// di servizio e lettere accentate, allineate al progetto Python originale.
namespace morse {

// Restituisce la mappa codice("..-") -> carattere. Costruita una sola volta.
const QHash<QString, QChar> &codeToChar();

// Restituisce la mappa carattere -> codice.
const QHash<QChar, QString> &charToCode();

// Decodifica un singolo simbolo Morse (es. "-.-." -> 'C').
// Se sconosciuto restituisce il carattere di fallback (default '_').
QChar decode(const QString &code, QChar fallback = QLatin1Char('_'));

// Decodifica "fuzzy": come decode(), ma se il codice non e' in tabella prova
// le varianti a distanza di edit 1 (un elemento dit<->dah classificato male,
// oppure un elemento spurio in testa/coda da click di rumore) e sceglie il
// candidato piu' frequente in CW. Fallback se nessuna variante e' plausibile.
QChar decodeFuzzy(const QString &code, QChar fallback = QLatin1Char('_'));

// Codifica un carattere (case-insensitive) nella sua sequenza di punti/linee.
QString encode(QChar c);

} // namespace morse
