#include "core/CredentialStore.h"

#include <QSettings>

#ifdef DECOLOG_HAVE_KEYCHAIN
#include <qt6keychain/keychain.h>
#endif

namespace decolog::core {

CredentialStore::CredentialStore(const QString& keychainService, QObject* parent)
    : QObject(parent)
    , m_keychainService(keychainService)
{
}

bool CredentialStore::compiledWithKeychain()
{
#ifdef DECOLOG_HAVE_KEYCHAIN
    return true;
#else
    return false;
#endif
}

QString CredentialStore::backend() const
{
#ifndef DECOLOG_HAVE_KEYCHAIN
    return tr("not available in this build");
#elif defined(Q_OS_WIN)
    return tr("Windows Credential Manager");
#elif defined(Q_OS_MACOS)
    return tr("macOS Keychain");
#else
    return tr("Secret Service (GNOME Keyring / KWallet)");
#endif
}

QList<CredentialService> CredentialStore::knownServices()
{
    return {
        {QStringLiteral("cloud"), QStringLiteral("DecoLog Cloud"), tr("Account"), tr("Token"),
         tr("Sync between devices (Phase 3)")},
        {QStringLiteral("qrz"), QStringLiteral("QRZ.com"), tr("Username"), tr("Password"),
         tr("Callbook lookups (XML subscription)")},
        {QStringLiteral("qrzlogbook"), QStringLiteral("QRZ Logbook"), tr("Callsign"), tr("API key"),
         tr("Upload and confirmations of the QRZ Logbook")},
        {QStringLiteral("lotw"), QStringLiteral("LoTW"), tr("Username"), tr("Password"),
         tr("Downloading confirmations; upload goes through the local TQSL")},
        {QStringLiteral("clublog"), QStringLiteral("Club Log"), tr("Email"), tr("App password"),
         tr("Real-time upload and OQRS")},
        {QStringLiteral("eqsl"), QStringLiteral("eQSL"), tr("Username"), tr("Password"),
         tr("Upload and eQSL confirmations")},
        {QStringLiteral("hamqth"), QStringLiteral("HamQTH"), tr("Username"), tr("Password"),
         tr("Free callbook lookups")},
        {QStringLiteral("hamalert"), QStringLiteral("HamAlert"), tr("Username"), tr("Password"),
         tr("Spots from your HamAlert triggers (telnet)")},
    };
}

QString CredentialStore::settingsKey(const QString& service, const char* field) const
{
    return QStringLiteral("credentials/%1/%2").arg(service, QLatin1String(field));
}

QString CredentialStore::account(const QString& service) const
{
    return QSettings().value(settingsKey(service, "account")).toString();
}

bool CredentialStore::hasSecret(const QString& service) const
{
    return QSettings().value(settingsKey(service, "stored"), false).toBool();
}

QVariantList CredentialStore::services() const
{
    QVariantList out;
    for (const CredentialService& s : knownServices()) {
        out << QVariantMap{
            {QStringLiteral("id"), s.id},
            {QStringLiteral("label"), s.label},
            {QStringLiteral("accountLabel"), s.accountLabel},
            {QStringLiteral("secretLabel"), s.secretLabel},
            {QStringLiteral("hint"), s.hint},
            {QStringLiteral("account"), account(s.id)},
            {QStringLiteral("stored"), hasSecret(s.id)},
            {QStringLiteral("busy"), m_busy.value(s.id, false)},
            {QStringLiteral("error"), m_errors.value(s.id)},
        };
    }
    return out;
}

void CredentialStore::setError(const QString& service, const QString& error)
{
    if (error.isEmpty())
        m_errors.remove(service);
    else
        m_errors.insert(service, error);
}

void CredentialStore::setBusy(const QString& service, bool busy)
{
    m_busy.insert(service, busy);
    emit changed();
}

void CredentialStore::save(const QString& service, const QString& accountName, const QString& secret)
{
    QSettings().setValue(settingsKey(service, "account"), accountName.trimmed());
    if (secret.isEmpty()) {
        setError(service, {});
        emit changed();
        emit finished(service, true, tr("Account saved"));
        return;
    }

#ifdef DECOLOG_HAVE_KEYCHAIN
    auto* job = new QKeychain::WritePasswordJob(m_keychainService, this);
    job->setAutoDelete(true);
    job->setKey(service);
    job->setTextData(secret);
    connect(job, &QKeychain::Job::finished, this, [this, service](QKeychain::Job* j) {
        const bool ok = j->error() == QKeychain::NoError;
        if (ok)
            QSettings().setValue(settingsKey(service, "stored"), true);
        setError(service, ok ? QString() : j->errorString());
        setBusy(service, false);
        emit finished(service, ok, ok ? tr("Stored in the system keystore") : j->errorString());
    });
    setBusy(service, true);
    job->start();
#else
    setError(service, tr("No system keystore in this build: the secret was not saved"));
    emit changed();
    emit finished(service, false, m_errors.value(service));
#endif
}

void CredentialStore::remove(const QString& service)
{
    QSettings s;
    s.remove(QStringLiteral("credentials/%1").arg(service));

#ifdef DECOLOG_HAVE_KEYCHAIN
    auto* job = new QKeychain::DeletePasswordJob(m_keychainService, this);
    job->setAutoDelete(true);
    job->setKey(service);
    connect(job, &QKeychain::Job::finished, this, [this, service](QKeychain::Job* j) {
        // Un segreto che non c'era gia' e' comunque tolto.
        const bool ok = j->error() == QKeychain::NoError || j->error() == QKeychain::EntryNotFound;
        setError(service, ok ? QString() : j->errorString());
        setBusy(service, false);
        emit finished(service, ok, ok ? tr("Removed") : j->errorString());
    });
    setBusy(service, true);
    job->start();
#else
    setError(service, {});
    emit changed();
    emit finished(service, true, tr("Removed"));
#endif
}

void CredentialStore::readSecret(const QString& service,
                                 std::function<void(const QString&, const QString&)> done)
{
#ifdef DECOLOG_HAVE_KEYCHAIN
    auto* job = new QKeychain::ReadPasswordJob(m_keychainService, this);
    job->setAutoDelete(true);
    job->setKey(service);
    connect(job, &QKeychain::Job::finished, this, [job, done](QKeychain::Job* j) {
        if (j->error() == QKeychain::NoError)
            done(job->textData(), {});
        else
            done({}, j->errorString());
    });
    job->start();
#else
    Q_UNUSED(service);
    done({}, tr("No system keystore in this build"));
#endif
}

void CredentialStore::verify(const QString& service)
{
    setBusy(service, true);
    readSecret(service, [this, service](const QString& secret, const QString& error) {
        const bool ok = error.isEmpty() && !secret.isEmpty();
        // Il portachiavi fa fede: se il segreto non c'e' piu', non lo si dichiara.
        QSettings().setValue(settingsKey(service, "stored"), ok);
        setError(service, ok ? QString() : error);
        setBusy(service, false);
        emit finished(service, ok, ok ? tr("Secret present in the keystore") : error);
    });
}

} // namespace decolog::core
