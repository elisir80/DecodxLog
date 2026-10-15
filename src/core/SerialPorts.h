#pragma once

#include <QString>
#include <QStringList>

namespace decolog::core {

// Porte seriali reali viste da Qt, senza quelle riservate a un altro programma
// (vedi reservedSerialPorts). Su macOS il percorso completo /dev/cu.* e'
// quello giusto da passare a hamlib; non si deve scansionare /dev perche' ttys*
// sono pseudo-terminali del sistema, non cavi CAT.
QStringList availableSerialPorts();

// Il nome com'e' per confrontarlo: senza spazi, senza il prefisso \\.\ e, su
// Windows, in maiuscolo ("com5" e "COM5" sono la stessa porta).
QString normalizedPortName(const QString& port);

// Le porte che DecoDXLog non apre mai, perche' sono di un altro programma:
// quelle che l'operatore ha scritto (serial/reserved) e, finche' e' acceso
// serial/leaveDecodium (di serie si'), quella su cui Decodium tiene il CAT.
// Una seriale la apre un programma solo: se DecoDXLog la prende prima, Decodium
// resta senza radio. Vale per il CAT, il PTT, il tasto CW, il rotore e la SO2R.
QStringList reservedSerialPorts();
bool isSerialPortReserved(const QString& port);

// Le porte scritte dall'operatore, e l'interruttore «lascia stare quella di Decodium».
QStringList userReservedSerialPorts();
void setUserReservedSerialPorts(const QStringList& ports);
bool leaveDecodiumPort();
void setLeaveDecodiumPort(bool on);

// La porta su cui Decodium tiene il CAT (Decodium3.ini, [Transceiver]/serialPort),
// o vuoto se non la si trova.
QString decodiumCatPort();

// Gli estremi nascosti delle coppie di porte virtuali (com0com li chiama CNCA0,
// CNCB0 ...): l'altra meta' e' di chi ha creato la coppia, DecoPort per primo.
bool isHiddenVirtualPort(const QString& port);

// In che ordine si cerca la radio: la porta di adesso, poi le virtuali condivise,
// poi le altre. Mai le riservate ne' gli estremi nascosti delle coppie virtuali.
QStringList serialProbeOrder(const QString& current, const QStringList& all, const QStringList& virtualPorts);

// Le porte virtuali visibili (una coppia com0com: DecoPort ne crea una per ogni
// radio che condivide, nel dialetto nativo della radio). Si usano come una
// seriale qualunque: stesso modello della radio, stessa velocita'.
QStringList virtualSerialPorts();

} // namespace decolog::core
