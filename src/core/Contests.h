// DecoDXLog — l'elenco dei contest.
//
// Gli identificativi sono quelli dell'enumerazione Contest_ID di ADIF 3.1.5:
// e' quello che va scritto nel campo CONTEST_ID perche' chi riceve il log lo
// riconosca. In fondo ci sono i contest della ARI che la tabella ADIF non ha:
// hanno un id nello stesso stile, e quello finisce nel log.
//
// La tabella e' generata dalla specifica ADIF, non scritta a mano: un id
// sbagliato non si vede guardando, si vede quando il log viene rifiutato.
#pragma once

#include <QList>
#include <QString>

namespace decolog::core {

struct Contest {
    QString id;      // CONTEST_ID ADIF: "CQ-WW-SSB"
    QString name;    // "CQ WW DX Contest (SSB)"
};

namespace contests {

// Tutti i contest, in ordine di identificativo.
const QList<Contest>& all();

// Quelli il cui id o nome contiene il testo cercato, senza badare a maiuscole
// e minuscole. Testo vuoto: tutti.
QList<Contest> search(const QString& text);

// Il nome di un id, vuoto se non e' uno di quelli conosciuti.
QString nameFor(const QString& id);

} // namespace contests
} // namespace decolog::core
