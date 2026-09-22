// DecoDXLog — il sync con DecoDXLog Cloud.
//
// Il log vero resta il file SQLite: il Cloud e' il posto dove i dispositivi si
// passano le modifiche. Un giro di sync fa due cose, in quest'ordine: prima
// prende quello che e' cambiato altrove (cosi' le revisioni locali partono gia'
// allineate), poi manda quello che e' in coda.
//
// Del Cloud, DecoDXLog tiene solo il token nel portachiavi: la password si scrive
// una volta e non resta da nessuna parte.
#pragma once

#include "core/CloudSync.h"

#include <QByteArray>
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
    // Le credenziali dei servizi viaggiano (chiuse) oppure restano qui.
    Q_PROPERTY(bool syncSecrets READ syncSecrets WRITE setSyncSecrets NOTIFY changed)
    // Falso se DecoDXLog e' stato compilato senza OpenSSL: allora non c'e'
    // cassaforte, e le credenziali non si muovono.
    Q_PROPERTY(bool vaultAvailable READ vaultAvailable CONSTANT)
    // La cassaforte e' aperta su questo dispositivo? La chiave nasce dalla
    // password del Cloud: chi si e' collegato prima che la cassaforte esistesse
    // ce l'ha chiusa, e basta dire la password una volta.
    Q_PROPERTY(bool vaultReady READ vaultReady NOTIFY changed)

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
    // Per le prove da riga di comando: il server solo per questa volta. Da qui
    // in poi il collegamento e' "di passaggio" — il token resta in memoria e
    // non tocca ne' il portachiavi ne' le impostazioni di chi usa il programma.
    void overrideServer(const QString& url);

    QString server() const { return m_server; }
    void setServer(const QString& url);
    QString callsign() const { return m_callsign; }
    bool linked() const { return !m_token.isEmpty() || m_storedCloudToken; }
    // Per chi deve chiedere qualcosa al Cloud che non e' il sync: l'invio
    // delle QSL. Vuoto quando non si e' collegati.
    QString token() const { return m_token; }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QString lastSync() const { return m_lastSync; }
    int queued() const;
    QVariantMap remote() const { return m_remote; }
    QString autoMode() const { return m_autoMode; }
    void setAutoMode(const QString& mode);
    bool syncSecrets() const;
    void setSyncSecrets(bool on);
    bool vaultAvailable() const;
    bool vaultReady() const { return !m_vaultKey.isEmpty(); }
    // Apre la cassaforte su questo dispositivo senza doversi scollegare: la
    // password serve a fare la chiave e non viene tenuta.
    Q_INVOKABLE void unlockVault(const QString& password);

    // Crea l'account sul server e si collega.
    Q_INVOKABLE void signup(const QString& callsign, const QString& password);
    // Si collega a un account che c'e' gia'.
    Q_INVOKABLE void login(const QString& callsign, const QString& password);
    // Stacca questo dispositivo: il token sparisce, il log resta.
    Q_INVOKABLE void logout();
    Q_INVOKABLE void syncNow();
    // Svuota il Cloud: QSO, storico, documenti e presenze di questo nominativo.
    // Vuole la parola DELETE scritta a mano; il log qui sul computer non si
    // tocca, e alla prossima sincronizzazione risale tutto da capo.
    Q_INVOKABLE void purgeCloud(const QString& confirm);
    Q_INVOKABLE void refresh() { emit changed(); }

    // Un QSO e' stato scritto: se il sync e' automatico parte fra poco.
    void qsoLogged();
    // Lo stato del client radio e' cambiato: la frequenza di adesso va al
    // Cloud, ma con misura (vedi `m_presenceTimer`).
    void clientStateChanged(const QVariantMap& state);

signals:
    void changed();
    // I profili stazione sono arrivati dal Cloud: la lista si rilegge.
    void profilesChanged();
    // Le impostazioni sono arrivate da un altro dispositivo: chi le mostra
    // (tema, colonne, filtri) si rilegge senza aspettare il riavvio.
    void settingsApplied();

private:
    void note(const QString& text, const QString& level);
    // Nominativo e password abbastanza lunghi per il server: altrimenti lo si
    // dice qui, invece di mandare una richiesta che tornera' indietro.
    bool credentialsLookSane(const QString& callsign, const QString& password);
    void loadToken();
    void saveToken(const QString& token, const QString& callsign);
    void startPull();
    void startPush();
    void applyPushResults(const QVariantList& results);
    void finish(const QString& text, const QString& level);
    QString accountKey() const;
    // Profili e impostazioni da mandare in questo giro.
    QVariantList pendingDocs();
    void applyDocResults(const QVariantList& results);
    // Tutte le impostazioni, meno quelle che parlano solo di questa macchina.
    QVariantMap localSettings() const;
    // Il profilo con questo uuid, sul computer dove siamo adesso.
    qint64 profileIdForUuid(const QString& uuid) const;
    // La cassaforte dei servizi: viaggia chiusa, e si chiude con una chiave che
    // nasce dalla password del Cloud. Il server ne vede solo i byte.
    void makeVaultKey(const QString& password);
    void loadVaultKey();
    void readSecrets();
    QVariantMap sealedSecrets();
    bool applyRemoteSecrets(const QVariantMap& document);
    static QString settingsFingerprint(const QVariantMap& values);
    bool applyRemoteSettings(const QVariantMap& document);

    Context m_ctx;
    core::CloudSync m_sync;
    QString m_server;
    QString m_callsign;
    QString m_token;
    QString m_status;
    QString m_lastSync;
    QString m_autoMode{QStringLiteral("qso")};
    QVariantMap m_remote;
    // L'impronta delle impostazioni gia' mandate in questo giro.
    QString m_settingsSent;
    // La chiave della cassaforte: si fa dalla password quando si entra, e poi
    // vive nel portachiavi come il token. Dal server non arriva mai.
    QByteArray m_vaultKey;
    // Le credenziali lette dal portachiavi, pronte per essere chiuse, e
    // l'impronta di quelle gia' mandate.
    QVariantMap m_secrets;
    QString m_secretsSent;
    bool m_busy{false};
    bool m_automatic{true};
    // Collegamento di passaggio (prove da riga di comando): niente portachiavi.
    bool m_ephemeral{false};
    // Il token esiste nel portachiavi, ma non e' stato ancora letto in questa
    // sessione: basta per mostrare il Cloud come collegato, non basta per
    // avviare sync automatici che aprirebbero il portachiavi allo startup.
    bool m_storedCloudToken{false};
    // Sync chiesto mentre il token stava ancora uscendo dal portachiavi.
    bool m_syncWhenReady{false};
    // Il cursore e' gia' stato riportato in pari dopo la spinta di questo giro?
    bool m_cursorCaughtUp{false};
    bool m_pulling{false};
    // I QSO mandati nell'ultima spinta, nell'ordine: gli esiti tornano cosi'.
    QList<qint64> m_batch;
    // La preparazione a fette: quello che resta da preparare e quello che e'
    // gia' pronto per partire.
    QList<qint64> m_toPrepare;
    QVariantList  m_prepared;
    QList<qint64> m_preparedIds;
    void prepareSomeAndPush();
    qint64 m_cursor{0};
    QTimer m_autoTimer;
    QTimer m_afterQso;
    // La frequenza di adesso: si manda al massimo ogni venti secondi, e subito
    // se cambia qualcosa che si vede (banda, modo, TX, nominativo lavorato).
    QTimer m_presenceTimer;
    QVariantMap m_presence;
    QVariantMap m_presenceSent;
};

} // namespace decolog::app
