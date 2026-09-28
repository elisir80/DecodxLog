// DecoDXLog — l'aggiornamento: si guarda da solo se e' uscita una versione nuova.
//
// Il controllo e' automatico e discreto: una volta al giorno, in silenzio, e
// solo se c'e' davvero qualcosa di nuovo si fa vedere. Da li' in poi decide chi
// opera: DecoDXLog scarica l'installatore e lo lancia solo quando glielo si
// chiede. Durante un contest nessuno vuole un programma che si cambia sotto i
// piedi da solo.
#pragma once

#include "core/Updates.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>

namespace decolog::app {

class UpdateController : public QObject {
    Q_OBJECT
    // Il controllo automatico: acceso di fabbrica, si spegne dalle impostazioni.
    Q_PROPERTY(bool automatic READ automatic WRITE setAutomatic NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    // C'e' una versione piu' nuova di questa, e non e' stata messa da parte.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString notes READ notes NOTIFY changed)
    Q_PROPERTY(QString page READ page NOTIFY changed)
    Q_PROPERTY(QString releaseRepository READ releaseRepository NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString lastCheck READ lastCheck NOTIFY changed)
    Q_PROPERTY(bool hasPackage READ hasPackage NOTIFY changed)
    // Il pacchetto sta nell'elenco firmato da chi pubblica: solo allora
    // «Aggiorna ora» c'e'. Senza firma si scarica a mano dalla pagina.
    Q_PROPERTY(bool verified READ verified NOTIFY changed)
    // "verified", "missing", "untrusted", "invalid": per il colore della riga.
    Q_PROPERTY(QString signatureState READ signatureState NOTIFY changed)
    Q_PROPERTY(QString signatureText READ signatureText NOTIFY changed)
    Q_PROPERTY(bool downloading READ downloading NOTIFY progressChanged)
    // Da 0 a 1 mentre scarica; -1 se non si sa quanto e' grande.
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString downloadSize READ downloadSize NOTIFY changed)

public:
    struct Context {
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
        // Da chiamare prima di lanciare l'installatore: chiude il programma.
        std::function<void()> quit;
    };

    explicit UpdateController(Context context, QObject* parent = nullptr);

    // Da chiamare quando il programma e' in piedi: fa il primo controllo dopo
    // qualche secondo, cosi' non rallenta l'avvio.
    void start();
    // Per le prove: si chiede a questo indirizzo invece che a GitHub.
    void overrideUrl(const QUrl& url) { m_fetcher.setUrl(url); }

    bool automatic() const { return m_automatic; }
    void setAutomatic(bool on);
    bool checking() const { return m_fetcher.busy(); }
    bool available() const;
    QString latestVersion() const { return m_info.version; }
    QString currentVersion() const;
    QString notes() const { return m_info.notes; }
    QString page() const { return m_info.page; }
    QString releaseRepository() const { return m_info.repository; }
    QString status() const { return m_status; }
    QString lastCheck() const;
    bool hasPackage() const { return m_info.valid && !m_info.package.isEmpty(); }
    bool verified() const { return hasPackage() && m_info.verified(); }
    QString signatureState() const;
    QString signatureText() const;
    bool downloading() const { return m_fetcher.downloading(); }
    double progress() const { return m_progress; }
    QString downloadSize() const;

    // Cerca adesso, che l'automatico sia acceso o spento.
    Q_INVOKABLE void checkNow();
    // Scarica solo il pacchetto adatto a questa macchina. Windows lancia il
    // setup, macOS apre il DMG e Linux aggiorna l'AppImage se e' possibile.
    Q_INVOKABLE void downloadAndInstall();
    Q_INVOKABLE void cancelDownload();
    // Questa versione non la voglio: non me la riproporre.
    Q_INVOKABLE void skipThisVersion();
    // La pagina della release nel browser, per chi preferisce fare da se'.
    Q_INVOKABLE void openPage();

signals:
    void changed();
    void progressChanged();
    // Una versione nuova, da far vedere: la finestra si apre solo su questo.
    void updateFound(const QString& version);

private:
    void runCheck(bool announce);
    void setStatus(const QString& text);

    Context m_ctx;
    core::UpdateFetcher m_fetcher;
    core::ReleaseInfo m_info;
    QTimer m_daily;
    QTimer m_first;
    bool m_automatic{true};
    QString m_status;
    QString m_skip;
    double m_progress{-1};
    bool m_replaceRunningAppImage{false};
};

} // namespace decolog::app
