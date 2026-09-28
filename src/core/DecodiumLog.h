// DecoDXLog — il log ADIF che Decodium 4 scrive da se'.
//
// Ogni QSO che Decodium registra va in due posti: nel pacchetto UDP per
// DecoDXLog e in decodium_log.adi. Se DecoDXLog era chiuso, o non riceveva
// (porta sbagliata, rete giu'), il QSO resta solo nel file: da li' si
// recupera, guardando solo la coda del file, quella dei QSO recenti.
#pragma once

#include "core/Adif.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace decolog::core::decodiumlog {

// Dove sta il log di Decodium, solo i file che esistono: prima quello attivo
// nelle impostazioni di Decodium (Decodium3.ini, [Logbooks] ActivePath), poi
// i posti dove Decodium lo mette da solo. Il primo e' quello da leggere.
QStringList candidates();

// Quando il QSO e' stato registrato, in UTC: la fine se c'e', se no l'inizio.
QDateTime loggedAt(const AdifRecord& record);

struct Tail {
    bool ok{false};
    QString error;
    QList<AdifRecord> records;   // registrati dopo `from` e fino a `to`, nell'ordine del file
    qint64 size{0};
};

// I QSO registrati dopo `from` (non valido = tutti) e fino a `to` compreso,
// leggendo il file dalla fine solo quanto serve: un pezzo, e se il QSO piu'
// vecchio del pezzo e' ancora dopo `from`, un pezzo quattro volte piu' grande.
// Un record a meta' (Decodium sta scrivendo) resta per la volta dopo.
Tail recent(const QString& path, const QDateTime& from, const QDateTime& to, qint64 firstChunk = 256 * 1024);

} // namespace decolog::core::decodiumlog
