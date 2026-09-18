// DecoLog — le impostazioni che vanno e vengono dal Cloud.
//
// Il log di una stazione non e' solo l'elenco dei collegamenti: chi apre
// DecoLog sul secondo computer si deve ritrovare la stessa stazione — lo stesso
// tema, le stesse colonne, gli stessi filtri, le stesse fonti del cluster, le
// stesse porte. Qui dentro c'e' quel lavoro, fuori dall'interfaccia, cosi' si
// puo' provare per davvero.
//
// Viaggia tutto tranne quello che non e' una scelta di chi opera ma un fatto di
// questa macchina: vedi `isMachineOnly`.
#pragma once

#include <QLatin1String>
#include <QString>
#include <QVariantMap>

#include <functional>

class QSettings;

namespace decolog::core::cloudsettings {

// Il profilo attivo detto in un modo che vale su tutti i computer: il numero di
// riga cambia da un log all'altro, l'uuid no.
inline constexpr QLatin1String kActiveProfileUuid{"station/activeProfileUuid"};

// Vero per le impostazioni che restano dove sono state scritte.
bool isMachineOnly(const QString& key);

// Le impostazioni non sono tutte testo: un filtro salvato e' un QVariant
// serializzato da Qt. Quello che JSON sa dire passa com'e' — e si legge anche
// sulla pagina del Cloud; il resto viaggia impacchettato, senza perdere niente.
QVariant pack(const QVariant& value);
QVariant unpack(const QVariant& value);

// Tutto quello che c'e' da mandare. `uuidForProfile` traduce il numero di riga
// del profilo attivo nel suo uuid; se torna vuoto, il profilo attivo non viene
// detto.
QVariantMap collect(QSettings& settings,
                    const std::function<QString(qint64)>& uuidForProfile = {});

// Scrive quello che e' arrivato e dice quante impostazioni sono cambiate.
// `dryRun` conta senza scrivere: serve alle prove da riga di comando, che non
// devono toccare la stazione vera. `profileForUuid` fa la traduzione inversa.
int apply(QSettings& settings, const QVariantMap& values, bool dryRun = false,
          const std::function<qint64(const QString&)>& profileForUuid = {});

// Una firma stabile: le chiavi in ordine, il JSON compatto, e l'impronta.
QString fingerprint(const QVariantMap& values);

} // namespace decolog::core::cloudsettings
