// DecoDXLog — la cassaforte delle credenziali che passa dal Cloud.
//
// Le password dei servizi (QRZ, LoTW, eQSL, Club Log, HamAlert...) stanno nel
// portachiavi del sistema, e li' restano. Ma chi apre DecoDXLog sul secondo
// computer non deve rimetterle a mano: viaggiano anche loro — chiuse.
//
// Il patto e' questo: **il server non le puo' leggere**. Si chiudono qui, con
// una chiave che nasce dalla password del Cloud (quella che l'utente scrive per
// entrare, e che il server conosce solo come impronta Argon2). Al server arriva
// un blocco di byte che senza quella password non si apre: niente password,
// niente credenziali, nemmeno per chi ha il database in mano.
//
// Come: PBKDF2-HMAC-SHA256 con 200.000 giri per la chiave, AES-256-GCM per il
// contenuto. Nessuna crittografia scritta a mano: e' OpenSSL, quello che sta
// sotto a HTTPS. Il sale non e' segreto (e non puo' esserlo: il secondo
// computer deve poterlo rifare avendo solo la password) ed e' legato al
// nominativo, cosi' due radioamatori con la stessa password hanno chiavi
// diverse.
#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace decolog::core::vault {

// Falso se DecoDXLog e' stato compilato senza OpenSSL: in quel caso le
// credenziali restano dove sono, e si dice.
bool available();

// La chiave di questo account. Vuota se OpenSSL non c'e'.
QByteArray deriveKey(const QString& password, const QString& callsign);

// Chiude: torna base64 di iv(12) + tag(16) + testo cifrato. Vuoto se non si
// puo' fare.
QString seal(const QByteArray& key, const QByteArray& plain);

// Apre. Niente se la chiave e' un'altra, o se qualcuno ha toccato il blocco:
// GCM se ne accorge.
std::optional<QByteArray> unseal(const QByteArray& key, const QString& sealed);

} // namespace decolog::core::vault
