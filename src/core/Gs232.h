// DecoDXLog — il protocollo seriale Yaesu GS-232 (G-450, G-550, G-800…).
//
// E' il dialetto che DecoRotor ha imparato per i rotori Yaesu (decorotor/gs232.py),
// portato qui per il gateway integrato. Rispetto al PRO.SIS.TEL non ci sono STX
// ne' identificativi d'asse: i comandi sono lettere ASCII chiuse da CR e le
// risposte sono righe chiuse da CR.
//
//     comando   ->  C <CR>             leggi l'azimut
//               ->  M290 <CR>          vai a 290 gradi
//               ->  S <CR>             stop
//     risposta  <-  +0290 <CR>         azimut 290 gradi    (GS-232A)
//               <-  +0290+0045 <CR>    azimut ed elevazione (C2)
//               <-  AZ=290 <CR>        lo stesso, nel dialetto GS-232B
//
// Le risposte si leggono con lo stesso tipo di quelle del PRO.SIS.TEL
// (prosistel::Reply), cosi' il gateway sceglie il dialetto in un punto solo.
#pragma once

#include "core/Prosistel.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace decolog::core::gs232 {

// Il G-450 gira fino a 450 gradi: oltre, il comando non avrebbe senso.
inline constexpr int kMaxAzimuth = 450;

// Un comando: lettere ASCII e CR.
QByteArray encode(const QString& command);
// `C<CR>` — legge l'azimut corrente.
QByteArray queryPosition();
// `M290<CR>` — porta il rotore ai gradi indicati (interi, tre cifre). Fuori
// dal campo 0..450 si resta al limite piu' vicino: meglio andare a fondo
// corsa che non andare.
QByteArray gotoAngle(double degrees);
// `S<CR>` — ferma la rotazione (il GS-232 non distingue stop dolce e rapido).
QByteArray stop();
// La riga di risposta alla `C`; nullopt se non e' una risposta. Il GS-232 non
// dice se gira: lo stato e' sempre «fermo» e il gateway lo deduce dal bersaglio.
std::optional<prosistel::Reply> decode(const QByteArray& frame);

} // namespace decolog::core::gs232
