// DecoLog — il sync con DecoLog Cloud.
//
// Il log vero resta il file SQLite: il Cloud e' il posto dove i dispositivi si
// passano le modifiche. Un giro di sync fa due cose, in quest'ordine: prima
// prende quello che e' cambiato altrove (cosi' le revisioni locali partono gia'
// allineate), poi manda quello che e' in coda.
//
// Del Cloud, DecoLog tiene solo il token nel portachiavi: la password si scrive
// una volta e non resta da nessuna parte.
#pragma once

#include "core/CloudSync.h"

#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <functional>

namespace decolog::core {
class CredentialStore;
class LogDatabase;
}

namespace decolog::app {

class CloudController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString server READ server WRITE setServer NOTIFY changed)
    Q_PROPERTY(QString callsign READ callsign NOTIFY changed)
    Q_PROPERTY(bool linked READ linked NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString lastSync READ lastSync NOTIFY changed)
    Q_PROPERTY(int queued READ queued NOTIFY changed)
    Q_PROPERTY(QVariantMap remote READ remote NOTIFY changed)
    // "qso" dopo ogni QSO e ogni cinque minuti, "timer" solo a tempo, "manual".
    Q_PROPERTY(QString autoMode READ autoMode WRITE setAutoMode NOTIFY changed)

public:
    struct Context {
        core::LogDatabase* db{nullptr};
        core::CredentialStore* credentials{nullptr};
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
        std::function<void()> logChanged;
    };

    explicit CloudController(Context context, QObject* parent = nullptr);

    // A log aperto: rilegge il cursore e, se c'e' un token, si presenta.
    // `automatic` a false carica il token senza far partire niente: serve alle
    // schermate di prova, che non devono toccare il server di nessuno.
    void start(bool automatic = true);
    // Per le prove da riga di comando: il server solo per questa volta, senza
    // scrivere niente nelle impostazioni dell'operatore.
    void overrideServer(const QString& url);

    QString server() const { return m_server; }
    void setServer(const QString& url);
    QString callsign() const { return m_callsign; }
    bool linked() const { return !m_token.isEmpty(); }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QString lastSync() const { return m_lastSync; }
    int queued() const;
    QVariantMap remote() const { return m_remote; }
    QString autoMode() const { return m_autoMode; }
    void setAutoMode(const QString& mode);

    // Crea l'account sul server e si collega.
    Q_INVOKABLE void signup(const QString& callsign, const QString& password);
    // Si collega a un account che c'e' gia'.
    Q_INVOKABLE void login(const QString& callsign, const QString& password);
    // Stacca questo dispositivo: il token sparisce, il log resta.
    Q_INVOKABLE void logout();
    Q_INVOKABLE void syncNow();
    Q_INVOKABLE void refresh() { emit changed(); }

    // Un QSO e' stato scritto: se il sync e' automatico parte fra poco.
    void qsoLogged();

signals:
    void changed();

private:
    void note(const QString& text, const QString& level);
    void loadToken();
    void saveToken(const QString& token, const QString& callsign);
    void startPull();
    void startPush();
    void applyPushResults(const QVariantList& results);
    void finish(const QString& text, const QString& level);
    QString accountKey() const;

    Context m_ctx;
    core::CloudSync m_sync;
    QString m_server;
    QString m_callsign;
    QString m_token;
    QString m_status;
    QString m_lastSync;
    QString m_autoMode{QStringLiteral("qso")};
    QVariantMap m_remote;
    bool m_busy{false};
    bool m_automatic{true};
    // Sync chiesto mentre il token stava ancora uscendo dal portachiavi.
    bool m_syncWhenReady{false};
    bool m_pulling{false};
    // I QSO mandati nell'ultima spinta, nell'ordine: gli esiti tornano cosi'.
    QList<qint64> m_batch;
    qint64 m_cursor{0};
    QTimer m_autoTimer;
    QTimer m_afterQso;
};

} // namespace decolog::app
