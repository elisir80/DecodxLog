// DecoDXLog — c'e' una versione nuova?
//
// Le versioni stanno su GitHub, una release per volta con il suo pacchetto.
// Qui si chiede all'API qual e' l'ultima, si confrontano i numeri e si tiene da
// parte l'indirizzo dell'installatore. Scaricare e installare e' un passo
// avanti che decide chi opera: un programma che si cambia sotto i piedi mentre
// si sta lavorando in un contest non lo vuole nessuno.
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace decolog::core {

// Quello che si sa dell'ultima versione pubblicata.
struct ReleaseInfo {
    bool    valid{false};
    QString version;    // "1.2.0", senza la v davanti
    QString page;       // la pagina della release, da aprire nel browser
    QUrl    installer;  // DecoDXLog-1.2.0-setup.exe, se c'e'
    QUrl    archive;    // DecoDXLog-1.2.0-win64.zip, se c'e'
    QString notes;      // le note della release, come le ha scritte chi pubblica
    qint64  installerBytes{0};
};

namespace updates {

// -1, 0, +1 come strcmp: confronta 1.2.0 con 1.10.0 da numeri, non da lettere,
// cosi' 1.10 viene dopo 1.9 e non prima.
int compareVersions(const QString& a, const QString& b);

// Legge la risposta di api.github.com/repos/<tizio>/<progetto>/releases/latest.
ReleaseInfo parseRelease(const QByteArray& json);

} // namespace updates

// Chiede a GitHub qual e' l'ultima versione, e all'occorrenza scarica il
// pacchetto. Un errore non e' mai grave: si riprova al giro dopo.
class UpdateFetcher : public QObject {
    Q_OBJECT

public:
    explicit UpdateFetcher(QObject* parent = nullptr);

    void setUrl(const QUrl& url) { m_url = url; }
    bool busy() const { return m_busy; }
    void fetch();

    // Scarica il file in `path`. Emette progress() mentre va.
    void download(const QUrl& url, const QString& path);
    void cancelDownload();
    bool downloading() const { return m_download != nullptr; }

signals:
    void finished(const decolog::core::ReleaseInfo& info);
    void failed(const QString& error);
    void progress(qint64 done, qint64 total);
    void downloaded(const QString& path);
    void downloadFailed(const QString& error);

private:
    QNetworkAccessManager* m_net;
    QUrl m_url{QStringLiteral("https://api.github.com/repos/iu8lmc/DecoDXLog/releases/latest")};
    bool m_busy{false};
    QNetworkReply* m_download{nullptr};
    QString m_downloadPath;
};

} // namespace decolog::core
