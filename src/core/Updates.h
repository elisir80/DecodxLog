// DecoDXLog — c'e' una versione nuova?
//
// Le versioni stanno su GitHub, una release per volta con il suo pacchetto.
// Qui si chiede all'API qual e' l'ultima, si confrontano i numeri e si tiene da
// parte l'indirizzo dell'installatore. Scaricare e installare e' un passo
// avanti che decide chi opera: un programma che si cambia sotto i piedi mentre
// si sta lavorando in un contest non lo vuole nessuno.
#pragma once

#include <QObject>
#include <QFileDevice>
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

// Il fork pubblicato e' la sorgente preferita. L'upstream e' consultato solo
// se il fork non offre un pacchetto piu' nuovo per questa macchina.
ReleaseInfo selectPreferredUpdate(const ReleaseInfo& primary,
                                  const ReleaseInfo& fallback,
                                  const QString& currentVersion);

} // namespace updates

// Chiede a GitHub qual e' l'ultima versione, e all'occorrenza scarica il
// pacchetto. Un errore non e' mai grave: si riprova al giro dopo.
class UpdateFetcher : public QObject {
    Q_OBJECT

public:
    explicit UpdateFetcher(QObject* parent = nullptr);

    // Solo per prove: interroga questo endpoint anziche' i due repository.
    void setUrl(const QUrl& url) { m_overrideUrl = url; }
    bool busy() const { return m_busy; }
    void fetch(const QString& currentVersion);

    // Scarica il file in `path` in modo atomico. Emette progress() mentre va;
    // expectedBytes viene dalla API GitHub e impedisce di installare un file
    // troncato. Le permissions servono per rimpiazzare un'AppImage in Linux.
    void download(const QUrl& url, const QString& path, qint64 expectedBytes = 0,
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

    void requestRelease(const Source& source, bool allowFallback);
    void finishWithoutUpdate();
    QNetworkAccessManager* m_net;
    QUrl m_overrideUrl;
    QString m_currentVersion;
    UpdateTarget m_target;
    bool m_busy{false};
    QNetworkReply* m_download{nullptr};
    QSaveFile* m_downloadFile{nullptr};
    QString m_downloadPath;
    qint64 m_downloadExpectedBytes{0};
    QFileDevice::Permissions m_downloadPermissions;
    bool m_setDownloadPermissions{false};
    bool m_downloadWriteFailed{false};
};

} // namespace decolog::core
