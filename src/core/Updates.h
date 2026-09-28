// DecoDXLog — c'e' una versione nuova?
//
// Le versioni stanno su GitHub, una release per volta con il suo pacchetto.
// Qui si chiede all'API qual e' l'ultima, si confrontano i numeri e si tiene da
// parte l'indirizzo dell'installatore. Scaricare e installare e' un passo
// avanti che decide chi opera: un programma che si cambia sotto i piedi mentre
// si sta lavorando in un contest non lo vuole nessuno.
//
// E si installa solo quello che e' firmato (ReleaseSignature.h): il pacchetto
// deve stare nell'elenco firmato da chi pubblica, con lo stesso SHA-256. Una
// release senza firma si fa vedere, ma si scarica a mano dalla sua pagina.
#pragma once

#include "core/ReleaseSignature.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QObject>
#include <QFileDevice>
#include <QList>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;

namespace decolog::core {

// La macchina per cui si sta cercando l'aggiornamento. Tenerla esplicita rende
// possibile provare tutte le combinazioni dai test, anche su un solo sistema.
struct UpdateTarget {
    QString platform;      // windows, macos, linux
    QString architecture;  // x86_64, aarch64, ...
};

// Quello che si sa dell'ultima versione pubblicata *per questa macchina*.
struct ReleaseInfo {
    bool    valid{false};
    QString version;    // "1.2.0", senza la v davanti
    QString page;       // la pagina della release, da aprire nel browser
    QString repository; // proprietario/progetto da cui arriva questa release
    QUrl    package;    // solo il pacchetto corretto per OS e CPU correnti
    QString packageName;
    QString notes;      // le note della release, come le ha scritte chi pubblica
    qint64  packageBytes{0};
    // L'elenco firmato dei file e la sua firma, se la release li ha.
    QUrl    manifest;
    QUrl    signature;
    // Com'e' andato il controllo della firma, e cosa dice l'elenco firmato
    // del pacchetto: senza Verified non si installa niente.
    releasesig::Manifest::State signatureState{releasesig::Manifest::State::Missing};
    QByteArray packageSha256;
    QString signatureKey;

    bool verified() const
    {
        return signatureState == releasesig::Manifest::State::Verified && packageSha256.size() == 32;
    }
};

namespace updates {

// -1, 0, +1 come strcmp: confronta 1.2.0 con 1.10.0 da numeri, non da lettere,
// cosi' 1.10 viene dopo 1.9 e non prima.
int compareVersions(const QString& a, const QString& b);

// Identifica OS e CPU del processo attuale. L'architettura del processo e'
// importante: un DecoDXLog eseguito con Rosetta deve scaricare il DMG Intel.
UpdateTarget currentTarget();
QString normalizedArchitecture(QString architecture);

// Piu' alto e' meglio; zero significa "questo asset non si puo' usare".
int assetMatchScore(const QString& assetName, const UpdateTarget& target);

// Leggono sia /releases/latest (oggetto) sia /releases (array). Una release e'
// valida solo se include un pacchetto installabile per target: una release
// Windows non deve far comparire un avviso su macOS o Linux.
ReleaseInfo parseRelease(const QByteArray& json,
                         const UpdateTarget& target = currentTarget());
ReleaseInfo parseReleases(const QByteArray& json,
                          const UpdateTarget& target = currentTarget());

// Le release piu' nuove di questa, dalla piu' nuova alla piu' vecchia; a pari
// versione prima quella della sorgente che viene prima in `candidates`. Si
// prova a verificarle in quest'ordine: si propone la prima firmata, e solo se
// nessuna lo e' la piu' nuova, da scaricare a mano.
QList<ReleaseInfo> newerReleases(const QList<ReleaseInfo>& candidates, const QString& currentVersion);

// Mette nella release l'esito del controllo dell'elenco firmato: stato,
// chiave, e SHA-256 e dimensione del pacchetto. Un pacchetto che nell'elenco
// non c'e', o con un'altra dimensione, rende la release non valida.
void applyManifest(ReleaseInfo& info, const releasesig::Manifest& manifest);

} // namespace updates

// Chiede a GitHub qual e' l'ultima versione, e all'occorrenza scarica il
// pacchetto. Un errore non e' mai grave: si riprova al giro dopo.
class UpdateFetcher : public QObject {
    Q_OBJECT

public:
    explicit UpdateFetcher(QObject* parent = nullptr);

    // Solo per prove: interroga questo endpoint anziche' i due repository, e
    // si fida di queste chiavi invece che di quelle scritte nel programma.
    void setUrl(const QUrl& url) { m_overrideUrl = url; }
    void setTrustedKeys(const QList<releasesig::TrustedKey>& keys) { m_keys = keys; }
    bool busy() const { return m_busy; }
    void fetch(const QString& currentVersion);

    // Scarica il file in `path` in modo atomico. Emette progress() mentre va.
    // Il file resta solo se ha la dimensione e lo SHA-256 dell'elenco firmato:
    // altrimenti non si scrive niente, nemmeno sopra l'AppImage che gira. Le
    // permissions servono per rimpiazzare un'AppImage in Linux.
    void download(const QUrl& url, const QString& path, qint64 expectedBytes, const QByteArray& expectedSha256,
                  QFileDevice::Permissions permissions = QFileDevice::Permissions{});
    void cancelDownload();
    bool downloading() const { return m_download != nullptr; }

signals:
    void finished(const decolog::core::ReleaseInfo& info);
    void failed(const QString& error);
    void progress(qint64 done, qint64 total);
    void downloaded(const QString& path);
    void downloadFailed(const QString& error);

private:
    struct Source {
        QString repository;
        QUrl api;
        QUrl page;
    };

    void requestSource(int index);
    void sourcesDone();
    void verifyCandidate(int index);
    void finishWithoutUpdate();
    QNetworkReply* getSmall(const QUrl& url);
    QNetworkAccessManager* m_net;
    QUrl m_overrideUrl;
    QString m_currentVersion;
    UpdateTarget m_target;
    bool m_busy{false};
    QList<Source> m_sources;
    QList<ReleaseInfo> m_found;
    QList<ReleaseInfo> m_candidates;
    QString m_lastError;
    int m_answered{0};
    QList<releasesig::TrustedKey> m_keys;
    QNetworkReply* m_download{nullptr};
    QSaveFile* m_downloadFile{nullptr};
    QString m_downloadPath;
    qint64 m_downloadExpectedBytes{0};
    QByteArray m_downloadExpectedSha256;
    QCryptographicHash m_downloadHash{QCryptographicHash::Sha256};
    QFileDevice::Permissions m_downloadPermissions;
    bool m_setDownloadPermissions{false};
    bool m_downloadWriteFailed{false};
};

} // namespace decolog::core
