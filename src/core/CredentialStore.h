// DecoDXLog — credenziali dei servizi nel portachiavi del sistema.
//
// Password, chiavi API e token stanno solo nel portachiavi (Gestione credenziali
// su Windows, Portachiavi su macOS, Secret Service su Linux) tramite qtkeychain.
// Nel file delle impostazioni va soltanto quello che non e' segreto: il nome
// utente e il fatto che un segreto esiste. Se il portachiavi non c'e' (build
// senza qtkeychain, Linux senza Secret Service) non si salva niente altrove: un
// segreto in chiaro in un .ini e' peggio di nessun segreto.
#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <functional>

namespace decolog::core {

struct CredentialService {
    QString id;             // "qrz", "lotw"...
    QString label;          // "QRZ.com"
    QString accountLabel;   // "Username", "Email"...
    QString secretLabel;    // "Password", "API key"...
    QString hint;           // a cosa serve
};

class CredentialStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString backend READ backend CONSTANT)
    Q_PROPERTY(QVariantList services READ services NOTIFY changed)

public:
    // `keychainService` separa i segreti di DecoDXLog da quelli di altri programmi
    // (e quelli dei test da quelli veri).
    explicit CredentialStore(const QString& keychainService = QStringLiteral("DecoDXLog"),
                             QObject* parent = nullptr);

    static bool compiledWithKeychain();
    bool available() const { return compiledWithKeychain(); }
    QString backend() const;

    static QList<CredentialService> knownServices();

    // Per il QML: id, label, accountLabel, secretLabel, hint, account, stored,
    // busy, error.
    QVariantList services() const;

    QString account(const QString& service) const;
    bool hasSecret(const QString& service) const;

    // Salva nome utente (in QSettings) e segreto (nel portachiavi). Un segreto
    // vuoto lascia quello gia' salvato. Esito con finished().
    Q_INVOKABLE void save(const QString& service, const QString& account, const QString& secret);
    Q_INVOKABLE void remove(const QString& service);
    // Controlla che il segreto dichiarato ci sia davvero (il portachiavi puo'
    // essere stato svuotato a mano). Esito con finished().
    Q_INVOKABLE void verify(const QString& service);

    // Per chi usera' le credenziali (QRZ, LoTW...): il segreto arriva nella
    // callback, vuoto con errore se non c'e'.
    void readSecret(const QString& service,
                    std::function<void(const QString& secret, const QString& error)> done);

signals:
    void changed();
    void finished(const QString& service, bool ok, const QString& message);

private:
    QString settingsKey(const QString& service, const char* field) const;
    void setError(const QString& service, const QString& error);
    void setBusy(const QString& service, bool busy);

    QString m_keychainService;
    QHash<QString, QString> m_errors;
    QHash<QString, bool> m_busy;
    QHash<QString, QString> m_secretCache;
};

} // namespace decolog::core
