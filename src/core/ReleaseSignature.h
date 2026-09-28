// DecoDXLog — la firma delle versioni pubblicate.
//
// Un aggiornamento arriva da GitHub e poi si esegue: se qualcuno mettesse le
// mani sulla release, o sulla strada fra GitHub e la stazione, DecoDXLog
// installerebbe il suo programma. La dimensione del file non basta a
// accorgersene. Per questo chi pubblica firma, con una chiave che tiene solo
// lui, l'elenco dei file della release con il loro SHA-256:
//
//   decodxlog-release.json      {"files":[{"name","sha256","size"}],
//                                "format","repository","version"}
//   decodxlog-release.json.sig  {"key":"<id della chiave>","signature":"<base64>"}
//
// La firma e' Ed25519 (RFC 8032) sui byte esatti del .json. DecoDXLog conosce
// le chiavi pubbliche di chi pubblica, repository per repository, e installa
// solo un pacchetto che sta nell'elenco firmato con lo stesso SHA-256 e la
// stessa dimensione. Nell'elenco firmato stanno anche il repository e la
// versione: una firma vecchia attaccata a una release nuova non passa.
//
// La chiave segreta non sta mai nel programma ne' nel repository: la tiene
// chi pubblica, nel portachiavi del suo computer (strumento decodxlog-sign).
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace decolog::core::releasesig {

// Una chiave pubblica di cui DecoDXLog si fida, per un repository.
struct TrustedKey {
    QString repository;   // "iu8lmc/DecoDXLog"
    QByteArray publicKey; // 32 byte
};

// Le chiavi scritte nel programma. Un repository che non ne ha nessuna non
// puo' dare aggiornamenti da installare da soli: si scaricano a mano.
QList<TrustedKey> trustedKeys();

// Il nome corto di una chiave: i primi 8 byte del suo SHA-256, in esadecimale.
QString keyId(const QByteArray& publicKey);

// Un file della release, come sta nell'elenco firmato.
struct SignedFile {
    QString name;
    qint64 size{0};
    QByteArray sha256; // 32 byte
};

// Il risultato del controllo di un elenco firmato.
struct Manifest {
    enum class State {
        Missing,    // la release non ha l'elenco o la firma
        Untrusted,  // firmata con una chiave che per quel repository non si conosce
        Invalid,    // la firma non torna, o l'elenco e' di un'altra release
        Verified,
    };
    State state{State::Missing};
    QString repository;
    QString version;
    QString keyId;
    QList<SignedFile> files;

    bool verified() const { return state == State::Verified; }
    // Il file con quel nome, o uno vuoto (size 0, sha256 vuoto) se non c'e'.
    SignedFile file(const QString& name) const;
};

inline constexpr char kManifestName[] = "decodxlog-release.json";
inline constexpr char kSignatureName[] = "decodxlog-release.json.sig";

// L'elenco da firmare: JSON compatto, le chiavi in ordine alfabetico.
QByteArray buildManifest(const QString& repository, const QString& version,
                         const QList<SignedFile>& files);

// La firma dell'elenco, gia' nel formato del .sig. secretKey e' quella di
// Monocypher: 64 byte, il seme seguito dalla chiave pubblica.
QByteArray signManifest(const QByteArray& manifest, const QByteArray& secretKey);

// Controlla elenco e firma per la release di repository/version. Senza
// chiavi esplicite usa quelle scritte nel programma.
Manifest verify(const QByteArray& manifest, const QByteArray& signature,
                const QString& repository, const QString& version,
                const QList<TrustedKey>& keys);
Manifest verify(const QByteArray& manifest, const QByteArray& signature,
                const QString& repository, const QString& version);

// Una coppia di chiavi nuova, dal generatore casuale del sistema.
bool generateKeyPair(QByteArray* secretKey, QByteArray* publicKey);

// Ed25519 puro, per i vettori dell'RFC 8032 nei test.
QByteArray keyPairFromSeed(const QByteArray& seed, QByteArray* publicKey); // -> secret (64)
QByteArray ed25519Sign(const QByteArray& message, const QByteArray& secretKey);
bool ed25519Check(const QByteArray& signature, const QByteArray& publicKey, const QByteArray& message);

} // namespace decolog::core::releasesig
