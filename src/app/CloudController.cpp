#include "app/CloudController.h"

#include "core/CloudSettings.h"
#include "core/CredentialStore.h"
#include "core/LogDatabase.h"
#include "core/SecretVault.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSysInfo>

namespace decolog::app {

using namespace decolog::core;

namespace {

// Quanti QSO per volta: un blocco che sta in un secondo di rete.
constexpr int kBatch = 200;

// Nel portachiavi, accanto al token: la chiave della cassaforte. Non e' un
// servizio con cui parlare, e' un attrezzo di DecoDXLog.
const QLatin1String kVaultService{"cloudvault"};


QString nowLabel()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

} // namespace

CloudController::CloudController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    QSettings s;
    m_server = s.value(QStringLiteral("cloud/server")).toString();
    m_callsign = s.value(QStringLiteral("cloud/callsign")).toString();
    m_autoMode = s.value(QStringLiteral("cloud/auto"), QStringLiteral("qso")).toString();
    m_sync.setServer(QUrl(m_server));
    // Il nome del dispositivo serve solo a riconoscerlo nella diagnostica.
    m_sync.setDevice(QSysInfo::machineHostName());

    connect(&m_sync, &CloudSync::loggedIn, this, [this](const QString& token, const QString& callsign) {
        saveToken(token, callsign);
        m_busy = false;
        finish(tr("Cloud: %1 connected").arg(callsign), QStringLiteral("success"));
        syncNow();
    });
    connect(&m_sync, &CloudSync::pulled, this,
            [this](const QVariantList& qsos, const QVariantList& docs, qint64 cursor, bool more) {
        int written = 0;
        for (const QVariant& value : qsos) {
            const auto result = m_ctx.db->applyRemote(value.toMap());
            if (result == LogDatabase::RemoteResult::Inserted
                || result == LogDatabase::RemoteResult::Updated
                || result == LogDatabase::RemoteResult::Deleted) {
                ++written;
            }
        }
        // I documenti: profili stazione e impostazioni. Il log di una stazione
        // non e' solo l'elenco dei QSO.
        int profiles = 0;
        int settingsChanged = 0;
        int secretsChanged = 0;
        for (const QVariant& value : docs) {
            const QVariantMap document = value.toMap();
            const QString kind = document.value(QStringLiteral("kind")).toString();
            if (kind == QLatin1String("profile")) {
                const auto result = m_ctx.db->applyRemoteProfile(document);
                if (result == LogDatabase::RemoteResult::Inserted
                    || result == LogDatabase::RemoteResult::Updated
                    || result == LogDatabase::RemoteResult::Deleted) {
                    ++profiles;
                }
            } else if (kind == QLatin1String("setting")) {
                if (applyRemoteSettings(document))
                    ++settingsChanged;
            } else if (kind == QLatin1String("secret")) {
                if (applyRemoteSecrets(document))
                    ++secretsChanged;
            }
        }
        if (profiles > 0) {
            note(tr("Cloud: %n station profile(s) updated", nullptr, profiles), QStringLiteral("success"));
            emit profilesChanged();
        }
        if (settingsChanged > 0) {
            note(tr("Cloud: settings updated from another device"), QStringLiteral("success"));
            emit settingsApplied();
        }
        if (secretsChanged > 0)
            note(tr("Cloud: %n service password(s) arrived", nullptr, secretsChanged),
                 QStringLiteral("success"));
        m_cursor = cursor;
        m_ctx.db->setSyncState(accountKey(), {{QStringLiteral("cursor"), QString::number(cursor)},
                                              {QStringLiteral("device"), m_sync.device()},
                                              {QStringLiteral("lastPull"), nowLabel()},
                                              {QStringLiteral("lastError"), QString()}});
        if (written > 0) {
            note(tr("Cloud: %n QSO arrived from another device", nullptr, written), QStringLiteral("success"));
            if (m_ctx.logChanged)
                m_ctx.logChanged();
        }
        if (more) {
            m_sync.pull(m_cursor);   // c'e' dell'altro: si continua
            return;
        }
        m_pulling = false;
        startPush();
    });
    connect(&m_sync, &CloudSync::pushed, this,
            [this](const QVariantList& results, const QVariantList& docResults, qint64 cursor) {
        applyPushResults(results);
        applyDocResults(docResults);
        m_cursor = qMax(m_cursor, cursor);
        m_ctx.db->setSyncState(accountKey(), {{QStringLiteral("lastPush"), nowLabel()},
                                              {QStringLiteral("lastError"), QString()}});
        if (m_ctx.logChanged)
            m_ctx.logChanged();
        // Se ne restano altri in coda si continua.
        if (queued() > 0) {
            startPush();
            return;
        }
        // Il cursore si e' fermato prima della nostra spinta: un ultimo pull lo
        // porta in pari (i nostri tornano indietro una volta sola e si saltano
        // da soli, avendo la stessa revisione).
        if (!m_cursorCaughtUp) {
            m_cursorCaughtUp = true;
            m_sync.pull(m_cursor);
            return;
        }
        m_lastSync = nowLabel();
        if (!m_ephemeral)
            QSettings().setValue(QStringLiteral("cloud/lastSync"), m_lastSync);
        finish(tr("Cloud: up to date"), QStringLiteral("success"));
        m_sync.status();
    });
    connect(&m_sync, &CloudSync::purged, this, [this](const QVariantMap& deleted) {
        // Il server e' vuoto: il cursore locale deve tornare a zero, altrimenti
        // si aspetterebbe di ritrovare roba che non c'e' piu'.
        m_cursor = 0;
        m_cursorCaughtUp = false;
        if (m_ctx.db) {
            m_ctx.db->setSyncState(accountKey(), {{QStringLiteral("cursor"), QStringLiteral("0")}});
            m_ctx.db->markAllDirty();
            // Le impronte dicevano "lassu' c'e' gia' questa roba": adesso non
            // c'e' piu' niente, quindi si dimenticano e riparte tutto.
            for (const char* key : {"cloud.settingsRevision", "cloud.secretsRevision",
                                    "cloud.secretsFingerprint"}) {
                m_ctx.db->setSetting(QLatin1String(key), QString());
            }
        }
        m_settingsSent.clear();
        m_secretsSent.clear();
        m_remote.clear();
        finish(tr("Cloud emptied: %1 QSO and %2 settings deleted. What is here stays, "
                  "and goes back up at the next sync.")
                   .arg(deleted.value(QStringLiteral("qsos")).toInt())
                   .arg(deleted.value(QStringLiteral("docs")).toInt()),
               QStringLiteral("warning"));
    });
    connect(&m_sync, &CloudSync::statusReady, this, [this](const QVariantMap& status) {
        m_remote = status;
        m_busy = false;
        emit changed();
    });
    connect(&m_sync, &CloudSync::failed, this, [this](const CloudError& error) {
        m_busy = false;
        m_pulling = false;
        m_batch.clear();
        if (error.unauthorized) {
            // Token scaduto o revocato: si chiede di rientrare, senza insistere.
            m_token.clear();
            finish(tr("Cloud: sign in again (%1)").arg(error.message), QStringLiteral("warning"));
            return;
        }
        if (error.forbidden) {
            // Il token va bene: e' l'account che ancora non puo'. Succede quando
            // la registrazione aspetta il via libera di chi tiene il servizio.
            // Buttare il token e chiedere di rientrare non servirebbe a niente:
            // si rientrerebbe e si tornerebbe qui. Si dice com'e' e si aspetta.
            m_ctx.db->setSyncState(accountKey(), {{QStringLiteral("lastError"), error.message}});
            finish(tr("Cloud: %1").arg(error.message), QStringLiteral("warning"));
            return;
        }
        m_ctx.db->setSyncState(accountKey(), {{QStringLiteral("lastError"), error.message}});
        finish(error.retryLater ? tr("Cloud: not reachable, will retry (%1)").arg(error.message)
                                : tr("Cloud: %1").arg(error.message),
               error.retryLater ? QStringLiteral("warning") : QStringLiteral("error"));
    });

    // Sync a tempo: ogni cinque minuti, se c'e' qualcosa da fare.
    m_autoTimer.setInterval(5 * 60 * 1000);
    connect(&m_autoTimer, &QTimer::timeout, this, [this] {
        if (!m_token.isEmpty() && !m_busy)
            syncNow();
    });
    // La frequenza di adesso: una volta ogni venti secondi basta e avanza per
    // chi guarda da lontano, e non fa rumore in rete.
    m_presenceTimer.setSingleShot(true);
    m_presenceTimer.setInterval(20'000);
    connect(&m_presenceTimer, &QTimer::timeout, this, [this] {
        if (m_token.isEmpty() || m_presence.isEmpty())
            return;
        if (m_presence == m_presenceSent)
            return;   // niente di nuovo da dire
        m_sync.reportPresence(m_presence);
        m_presenceSent = m_presence;
    });

    // I QSO che arrivano a raffica si mandano insieme poco dopo, non uno a uno.
    m_afterQso.setSingleShot(true);
    m_afterQso.setInterval(20'000);
    connect(&m_afterQso, &QTimer::timeout, this, [this] {
        if (!m_token.isEmpty() && !m_busy)
            syncNow();
    });

    m_lastSync = s.value(QStringLiteral("cloud/lastSync")).toString();
}

void CloudController::start(bool automatic)
{
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return;
    m_automatic = automatic;
    m_cursor = m_ctx.db->syncState(accountKey()).value(QStringLiteral("cursor")).toLongLong();
    m_storedCloudToken = !m_ephemeral
        && m_ctx.credentials
        && m_ctx.credentials->hasSecret(QStringLiteral("cloud"));
    if (m_storedCloudToken && m_status.isEmpty())
        m_status = tr("Cloud: linked — sync will unlock it when needed");
    if (automatic && m_autoMode != QLatin1String("manual") && !m_token.isEmpty())
        m_autoTimer.start();
    emit changed();
}

QString CloudController::accountKey() const
{
    return m_callsign.isEmpty() ? QStringLiteral("cloud") : m_callsign;
}

int CloudController::queued() const
{
    return m_ctx.db && m_ctx.db->isOpen() ? m_ctx.db->dirtyCount() : 0;
}

void CloudController::note(const QString& text, const QString& level)
{
    if (m_ctx.activity)
        m_ctx.activity(QStringLiteral("SYNC"), text, level);
}

void CloudController::finish(const QString& text, const QString& level)
{
    m_busy = false;
    m_status = text;
    note(text, level);
    emit changed();
}

// ── Credenziali ───────────────────────────────────────────────────────────────

void CloudController::loadToken()
{
    if (m_ephemeral)
        return;   // collegamento di passaggio: il token lo da' chi prova
    if (!m_ctx.credentials || !m_ctx.credentials->hasSecret(QStringLiteral("cloud"))) {
        m_storedCloudToken = false;
        return;
    }
    m_storedCloudToken = true;
    m_ctx.credentials->readSecret(QStringLiteral("cloud"), [this](const QString& token, const QString& error) {
        if (token.isEmpty()) {
            m_status = tr("Cloud: token not readable (%1)").arg(error);
            emit changed();
            return;
        }
        m_token = token;
        m_storedCloudToken = true;
        m_sync.setToken(token);
        loadVaultKey();
        if (m_automatic && m_autoMode != QLatin1String("manual") && !m_autoTimer.isActive())
            m_autoTimer.start();
        emit changed();
        // Un giro subito: quello che e' cambiato altrove arriva senza chiederlo,
        // oppure quello gia' chiesto mentre il token era per strada.
        if (m_syncWhenReady || (m_automatic && m_autoMode != QLatin1String("manual"))) {
            m_syncWhenReady = false;
            syncNow();
        }
    });
}

void CloudController::saveToken(const QString& token, const QString& callsign)
{
    m_token = token;
    m_storedCloudToken = true;
    m_callsign = callsign;
    m_sync.setToken(token);
    if (m_ephemeral) {
        emit changed();
        return;
    }
    QSettings().setValue(QStringLiteral("cloud/callsign"), callsign);
    if (m_ctx.credentials) {
        m_ctx.credentials->save(QStringLiteral("cloud"), callsign, token);
        // La chiave della cassaforte resta nel portachiavi come il token: cosi'
        // domani si riapre senza richiedere la password.
        if (!m_vaultKey.isEmpty()) {
            m_ctx.credentials->save(kVaultService, callsign,
                                    QString::fromLatin1(m_vaultKey.toBase64()));
        }
        readSecrets();
    }
    emit changed();
}

void CloudController::setServer(const QString& url)
{
    const QString value = url.trimmed();
    if (value == m_server)
        return;
    m_server = value;
    m_sync.setServer(QUrl(value));
    QSettings().setValue(QStringLiteral("cloud/server"), value);
    emit changed();
}

void CloudController::overrideServer(const QString& url)
{
    // Una prova non deve poter scollegare la stazione vera: da qui in poi il
    // token vive solo in memoria.
    m_ephemeral = true;
    m_token.clear();
    m_storedCloudToken = false;
    m_sync.setToken(QString());
    m_server = url.trimmed();
    m_sync.setServer(QUrl(m_server));
    emit changed();
}

void CloudController::setAutoMode(const QString& mode)
{
    if (mode == m_autoMode)
        return;
    m_autoMode = mode;
    QSettings().setValue(QStringLiteral("cloud/auto"), mode);
    if (mode == QLatin1String("manual"))
        m_autoTimer.stop();
    else if (!m_token.isEmpty())
        m_autoTimer.start();
    emit changed();
}

// Il server chiede otto caratteri di password. Se si manda lo stesso, torna un
// errore di validazione: meglio dirlo qui, con parole, che far viaggiare una
// richiesta gia' persa.
//
// Sul nominativo non c'e' regola: 9H1SR, VY2XT, 9H1SR/M, un indicativo speciale
// corto — nel mondo ce n'e' di ogni forma, e un logbook non e' chi decide quali
// esistono. Basta che non sia vuoto.
bool CloudController::credentialsLookSane(const QString& callsign, const QString& password)
{
    if (callsign.trimmed().isEmpty()) {
        finish(tr("Cloud: write your callsign"), QStringLiteral("warning"));
        return false;
    }
    if (password.size() < 8) {
        finish(tr("Cloud: the password must be at least 8 characters"), QStringLiteral("warning"));
        return false;
    }
    return true;
}

void CloudController::signup(const QString& callsign, const QString& password)
{
    if (m_server.isEmpty()) {
        finish(tr("Cloud: set the server address first"), QStringLiteral("warning"));
        return;
    }
    if (!credentialsLookSane(callsign, password))
        return;
    m_busy = true;
    m_status = tr("Cloud: creating the account…");
    emit changed();
    m_callsign = callsign.trimmed().toUpper();
    // La password non si tiene: serve solo a fare la chiave della cassaforte.
    makeVaultKey(password);
    m_sync.signup(m_callsign, password);
}

void CloudController::login(const QString& callsign, const QString& password)
{
    if (m_server.isEmpty()) {
        finish(tr("Cloud: set the server address first"), QStringLiteral("warning"));
        return;
    }
    if (!credentialsLookSane(callsign, password))
        return;
    m_busy = true;
    m_status = tr("Cloud: signing in…");
    emit changed();
    m_callsign = callsign.trimmed().toUpper();
    makeVaultKey(password);
    m_sync.login(m_callsign, password);
}

void CloudController::purgeCloud(const QString& confirm)
{
    if (m_token.isEmpty()) {
        finish(tr("Cloud: sign in first"), QStringLiteral("warning"));
        return;
    }
    if (confirm.trimmed() != QLatin1String("DELETE")) {
        finish(tr("Cloud: nothing deleted — you have to write DELETE"), QStringLiteral("warning"));
        return;
    }
    m_busy = true;
    m_status = tr("Cloud: emptying…");
    emit changed();
    m_sync.purge(QStringLiteral("DELETE"));
}

void CloudController::logout()
{
    m_token.clear();
    m_storedCloudToken = false;
    m_sync.setToken(QString());
    // Staccare il dispositivo vuol dire anche buttare la chiave: le credenziali
    // dei servizi restano nel portachiavi, ma il blocco sul server non si apre
    // piu' da qui finche' non si rientra.
    m_vaultKey.clear();
    m_secrets.clear();
    if (m_ctx.credentials) {
        m_ctx.credentials->remove(QStringLiteral("cloud"));
        m_ctx.credentials->remove(kVaultService);
    }
    m_remote.clear();
    finish(tr("Cloud: this device is no longer linked"), QStringLiteral("info"));
}

// ── Il giro di sync ───────────────────────────────────────────────────────────

void CloudController::syncNow()
{
    if (m_token.isEmpty() && m_storedCloudToken) {
        // Il token e' salvato, ma volutamente non viene letto allo startup:
        // si apre il portachiavi solo quando l'utente chiede davvero il sync.
        m_syncWhenReady = true;
        m_status = tr("Cloud: opening the keystore…");
        emit changed();
        loadToken();
        return;
    }
    if (!linked()) {
        finish(tr("Cloud: not linked yet"), QStringLiteral("warning"));
        return;
    }
    if (m_busy)
        return;
    m_busy = true;
    m_cursorCaughtUp = false;
    m_status = tr("Cloud: syncing…");
    emit changed();
    startPull();
}

void CloudController::startPull()
{
    m_pulling = true;
    m_sync.pull(m_cursor);
}

void CloudController::startPush()
{
    if (!m_ctx.db)
        return;
    // Preparare i QSO da mandare costa: ognuno e' una lettura dal log piu' la
    // costruzione del record. Facendolo tutto in un colpo, con la coda piena,
    // la finestra restava ferma per secondi — e a chi opera sembra il
    // programma piantato. Si prepara a fette, tornando in mezzo a servire
    // l'interfaccia: ci si mette lo stesso tempo, ma il programma resta vivo.
    m_batch = m_ctx.db->dirtyQsos(kBatch);
    m_toPrepare = m_batch;
    m_prepared.clear();
    m_preparedIds.clear();
    prepareSomeAndPush();
}

void CloudController::prepareSomeAndPush()
{
    if (!m_ctx.db)
        return;
    // Quanti per fetta: abbastanza da non perdere tempo in giri a vuoto, pochi
    // abbastanza da non far ballare l'interfaccia.
    constexpr int kSlice = 20;
    for (int n = 0; n < kSlice && !m_toPrepare.isEmpty(); ++n) {
        const qint64 id = m_toPrepare.takeFirst();
        const QVariantMap record = m_ctx.db->syncRecord(id);
        if (record.isEmpty())
            continue;
        m_prepared << record;
        m_preparedIds << id;
    }
    if (!m_toPrepare.isEmpty()) {
        // Il resto al prossimo giro: prima lasciamo disegnare.
        QTimer::singleShot(0, this, &CloudController::prepareSomeAndPush);
        return;
    }

    const QVariantList payload = m_prepared;
    QList<qint64> sent = m_preparedIds;
    m_prepared.clear();
    m_preparedIds.clear();
    m_batch = sent;
    // Anche senza QSO in coda ci puo' essere da mandare: un profilo cambiato,
    // il tema, un filtro salvato. Il log non e' solo l'elenco dei collegamenti.
    const QVariantList docs = pendingDocs();
    if (payload.isEmpty() && docs.isEmpty()) {
        m_lastSync = nowLabel();
        if (!m_ephemeral)
            QSettings().setValue(QStringLiteral("cloud/lastSync"), m_lastSync);
        finish(tr("Cloud: up to date"), QStringLiteral("success"));
        m_sync.status();
        return;
    }
    m_status = payload.isEmpty()
        ? tr("Cloud: sending the station settings…")
        : tr("Cloud: sending %n QSO…", nullptr, static_cast<int>(payload.size()));
    emit changed();
    m_sync.push(payload, docs);
}

void CloudController::applyPushResults(const QVariantList& results)
{
    if (!m_ctx.db)
        return;
    int conflicts = 0;
    int duplicates = 0;
    for (const QVariant& value : results) {
        const QVariantMap result = value.toMap();
        const QString uuid = result.value(QStringLiteral("uuid")).toString();
        const QString status = result.value(QStringLiteral("status")).toString();
        const qint64 id = m_ctx.db->idForUuid(uuid);
        if (id <= 0)
            continue;

        if (status == QLatin1String("duplicate")) {
            m_ctx.db->adoptUuid(id, result.value(QStringLiteral("serverUuid")).toString());
            ++duplicates;
            continue;
        }
        if (status == QLatin1String("stale")) {
            // Il server ne sa di piu': la sua versione arrivera' col prossimo
            // pull, questo esce dalla coda per non insistere.
            m_ctx.db->markSynced(id, 0);
            continue;
        }
        if (status == QLatin1String("conflict"))
            ++conflicts;
        m_ctx.db->markSynced(id, result.value(QStringLiteral("revision")).toInt());
    }
    if (conflicts > 0) {
        note(tr("Cloud: %n conflict(s) resolved, the other version is in the history", nullptr, conflicts),
             QStringLiteral("warning"));
    }
    if (duplicates > 0)
        note(tr("Cloud: %n duplicate(s) recognised", nullptr, duplicates), QStringLiteral("info"));
}

QVariantList CloudController::pendingDocs()
{
    QVariantList docs;
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return docs;

    // I profili stazione con una modifica ancora da mandare.
    for (const QVariantMap& profile : m_ctx.db->dirtyProfiles())
        docs << profile;

    // Le credenziali dei servizi, chiuse: il server ne vede solo i byte.
    const QVariantMap secrets = sealedSecrets();
    if (!secrets.isEmpty())
        docs << secrets;

    // Le impostazioni: un documento solo, con la sua revisione. Si manda quando
    // e' cambiato davvero qualcosa, non a ogni giro. L'impronta si segna solo
    // quando il server conferma: un server piu' vecchio, che i documenti non li
    // conosce, non deve farcele dare per mandate.
    m_settingsSent.clear();
    const QVariantMap current = localSettings();
    const QString fingerprint = settingsFingerprint(current);
    const QString known = m_ctx.db->setting(QStringLiteral("cloud.settingsFingerprint"));
    if (fingerprint != known) {
        const int revision = qMax(1, m_ctx.db->setting(QStringLiteral("cloud.settingsRevision")).toInt() + 1);
        m_settingsSent = fingerprint;
        docs << QVariantMap{{QStringLiteral("kind"), QStringLiteral("setting")},
                            {QStringLiteral("key"), QStringLiteral("station")},
                            {QStringLiteral("revision"), revision},
                            {QStringLiteral("data"), current}};
    }
    return docs;
}

void CloudController::applyDocResults(const QVariantList& results)
{
    if (!m_ctx.db)
        return;
    for (const QVariant& value : results) {
        const QVariantMap result = value.toMap();
        const QString kind = result.value(QStringLiteral("kind")).toString();
        const QString key = result.value(QStringLiteral("key")).toString();
        const QString status = result.value(QStringLiteral("status")).toString();
        const int revision = result.value(QStringLiteral("revision")).toInt();
        if (kind == QLatin1String("profile")) {
            m_ctx.db->markProfileSynced(key, status == QLatin1String("stale") ? 0 : revision);
        } else if (kind == QLatin1String("setting") && revision > 0) {
            m_ctx.db->setSetting(QStringLiteral("cloud.settingsRevision"), QString::number(revision));
            // "stale" vuol dire che il server ne aveva una piu' avanti: l'impronta
            // resta quella vecchia, cosi' al giro dopo si riprova piu' in alto.
            if (status != QLatin1String("stale") && !m_settingsSent.isEmpty()) {
                m_ctx.db->setSetting(QStringLiteral("cloud.settingsFingerprint"), m_settingsSent);
                m_settingsSent.clear();
            }
        } else if (kind == QLatin1String("secret") && revision > 0) {
            m_ctx.db->setSetting(QStringLiteral("cloud.secretsRevision"), QString::number(revision));
            if (status != QLatin1String("stale") && !m_secretsSent.isEmpty()) {
                m_ctx.db->setSetting(QStringLiteral("cloud.secretsFingerprint"), m_secretsSent);
                m_secretsSent.clear();
            }
        }
    }
}

QVariantMap CloudController::localSettings() const
{
    QSettings s;
    return cloudsettings::collect(s, [this](qint64 id) -> QString {
        if (!m_ctx.db || !m_ctx.db->isOpen())
            return {};
        const auto profile = m_ctx.db->stationProfile(id);
        return profile ? profile->uuid : QString();
    });
}

qint64 CloudController::profileIdForUuid(const QString& uuid) const
{
    if (uuid.isEmpty() || !m_ctx.db || !m_ctx.db->isOpen())
        return 0;
    for (const core::StationProfile& p : m_ctx.db->stationProfiles(false)) {
        if (p.uuid == uuid)
            return p.id;
    }
    return 0;
}

QString CloudController::settingsFingerprint(const QVariantMap& values)
{
    return cloudsettings::fingerprint(values);
}

bool CloudController::applyRemoteSettings(const QVariantMap& document)
{
    if (!m_ctx.db)
        return false;
    const int revision = document.value(QStringLiteral("revision")).toInt();
    const int known = m_ctx.db->setting(QStringLiteral("cloud.settingsRevision")).toInt();
    if (revision <= known)
        return false;   // le nostre sono uguali o piu' nuove

    QSettings s;
    // Un collegamento di passaggio (le prove da riga di comando) non deve
    // riscrivere le impostazioni della stazione vera: si conta e basta.
    const int written = cloudsettings::apply(
        s, document.value(QStringLiteral("data")).toMap(), m_ephemeral,
        [this](const QString& uuid) { return profileIdForUuid(uuid); });

    m_ctx.db->setSetting(QStringLiteral("cloud.settingsRevision"), QString::number(revision));
    m_ctx.db->setSetting(QStringLiteral("cloud.settingsFingerprint"), settingsFingerprint(localSettings()));
    return written > 0;
}

// ── La cassaforte dei servizi ─────────────────────────────────────────────────
//
// Le password di QRZ, LoTW, eQSL, Club Log, HamQTH, HamAlert stanno nel
// portachiavi del sistema. Perche' anche il secondo computer le abbia senza
// riscriverle a mano, viaggiano — ma chiuse qui dentro, con una chiave che
// nasce dalla password del Cloud. Al server arriva un blocco di byte che senza
// quella password non si apre: e' l'unico modo onesto di mandarle.

bool CloudController::syncSecrets() const
{
    return QSettings().value(QStringLiteral("cloud/syncSecrets"), true).toBool();
}

void CloudController::setSyncSecrets(bool on)
{
    if (on == syncSecrets())
        return;
    QSettings().setValue(QStringLiteral("cloud/syncSecrets"), on);
    if (on)
        readSecrets();
    else
        m_secrets.clear();
    emit changed();
}

bool CloudController::vaultAvailable() const
{
    return core::vault::available();
}

void CloudController::unlockVault(const QString& password)
{
    if (!linked()) {
        finish(tr("Cloud: sign in first"), QStringLiteral("warning"));
        return;
    }
    makeVaultKey(password);
    if (m_vaultKey.isEmpty()) {
        finish(tr("Cloud: the vault did not open"), QStringLiteral("warning"));
        return;
    }
    // La chiave e' giusta se quello che c'e' sul server si apre; se sul server
    // non c'e' ancora niente, e' il primo dispositivo e va bene cosi'.
    if (m_ctx.credentials) {
        m_ctx.credentials->save(kVaultService, m_callsign,
                                QString::fromLatin1(m_vaultKey.toBase64()));
    }
    readSecrets();
    emit changed();
    note(tr("Cloud: vault open on this device"), QStringLiteral("success"));
    syncNow();
}

void CloudController::makeVaultKey(const QString& password)
{
    // Un collegamento di passaggio (le prove da riga di comando) non apre la
    // cassaforte della stazione vera: senza chiave, dal portachiavi non si
    // legge niente e non parte niente.
    if (m_ephemeral || password.isEmpty() || !core::vault::available())
        return;
    m_vaultKey = core::vault::deriveKey(password, m_callsign);
}

void CloudController::loadVaultKey()
{
    if (m_ephemeral || !core::vault::available())
        return;
    if (!m_ctx.credentials || !m_ctx.credentials->hasSecret(kVaultService))
        return;
    m_ctx.credentials->readSecret(kVaultService, [this](const QString& stored, const QString&) {
        if (stored.isEmpty())
            return;
        m_vaultKey = QByteArray::fromBase64(stored.toLatin1());
        readSecrets();
        emit changed();
    });
}

void CloudController::readSecrets()
{
    if (!m_ctx.credentials || m_vaultKey.isEmpty() || !syncSecrets())
        return;
    for (const core::CredentialService& service : core::CredentialStore::knownServices()) {
        // Il token di questo dispositivo e la chiave della cassaforte non
        // viaggiano: il primo e' di questa macchina, la seconda si rifa' dalla
        // password.
        if (service.id == QLatin1String("cloud") || service.id == kVaultService)
            continue;
        if (!m_ctx.credentials->hasSecret(service.id))
            continue;
        const QString account = m_ctx.credentials->account(service.id);
        m_ctx.credentials->readSecret(service.id, [this, id = service.id, account](const QString& secret,
                                                                                   const QString&) {
            if (secret.isEmpty())
                return;
            m_secrets.insert(id, QVariantMap{{QStringLiteral("account"), account},
                                             {QStringLiteral("secret"), secret}});
        });
    }
}

QVariantMap CloudController::sealedSecrets()
{
    m_secretsSent.clear();
    if (m_secrets.isEmpty() || m_vaultKey.isEmpty() || !m_ctx.db || !syncSecrets())
        return {};

    const QByteArray plain = QJsonDocument(QJsonObject::fromVariantMap(m_secrets))
                                 .toJson(QJsonDocument::Compact);
    // L'impronta e' del contenuto, non del blocco chiuso: ogni chiusura ha il
    // suo nonce e sarebbe diversa ogni volta.
    const QString fingerprint = QString::fromLatin1(
        QCryptographicHash::hash(plain, QCryptographicHash::Sha256).toHex());
    if (fingerprint == m_ctx.db->setting(QStringLiteral("cloud.secretsFingerprint")))
        return {};

    const QString sealed = core::vault::seal(m_vaultKey, plain);
    if (sealed.isEmpty())
        return {};

    m_secretsSent = fingerprint;
    const int revision = qMax(1, m_ctx.db->setting(QStringLiteral("cloud.secretsRevision")).toInt() + 1);
    return QVariantMap{{QStringLiteral("kind"), QStringLiteral("secret")},
                       {QStringLiteral("key"), QStringLiteral("vault")},
                       {QStringLiteral("revision"), revision},
                       {QStringLiteral("data"), QVariantMap{{QStringLiteral("alg"),
                                                             QStringLiteral("aes-256-gcm")},
                                                            {QStringLiteral("sealed"), sealed}}}};
}

bool CloudController::applyRemoteSecrets(const QVariantMap& document)
{
    if (!m_ctx.db || !m_ctx.credentials)
        return false;
    const int revision = document.value(QStringLiteral("revision")).toInt();
    const int known = m_ctx.db->setting(QStringLiteral("cloud.secretsRevision")).toInt();
    if (revision <= known)
        return false;

    if (m_vaultKey.isEmpty()) {
        note(tr("Cloud: the service passwords are waiting for you to sign in on this device"),
             QStringLiteral("info"));
        return false;
    }
    const auto plain = core::vault::unseal(
        m_vaultKey, document.value(QStringLiteral("data")).toMap()
                        .value(QStringLiteral("sealed")).toString());
    if (!plain) {
        // Chiave sbagliata (la password del Cloud e' cambiata) o blocco toccato:
        // non si indovina, si dice.
        note(tr("Cloud: the service passwords did not open with this password"),
             QStringLiteral("warning"));
        return false;
    }

    const QVariantMap arrived = QJsonDocument::fromJson(*plain).object().toVariantMap();
    int written = 0;
    for (auto it = arrived.cbegin(); it != arrived.cend(); ++it) {
        const QVariantMap entry = it.value().toMap();
        const QString account = entry.value(QStringLiteral("account")).toString();
        const QString secret = entry.value(QStringLiteral("secret")).toString();
        if (secret.isEmpty())
            continue;
        const QVariantMap mine = m_secrets.value(it.key()).toMap();
        if (mine.value(QStringLiteral("account")) == account
            && mine.value(QStringLiteral("secret")) == secret) {
            continue;
        }
        // Le prove da riga di comando non toccano il portachiavi vero.
        if (!m_ephemeral)
            m_ctx.credentials->save(it.key(), account, secret);
        m_secrets.insert(it.key(), entry);
        ++written;
    }

    m_ctx.db->setSetting(QStringLiteral("cloud.secretsRevision"), QString::number(revision));
    m_ctx.db->setSetting(QStringLiteral("cloud.secretsFingerprint"),
                         QString::fromLatin1(QCryptographicHash::hash(*plain,
                                                                      QCryptographicHash::Sha256).toHex()));
    return written > 0;
}

void CloudController::qsoLogged()
{
    if (!m_token.isEmpty() && m_autoMode == QLatin1String("qso"))
        m_afterQso.start();
}

// ── Dov'e' la stazione adesso ─────────────────────────────────────────────────
//
// La frequenza cambia a ogni giro di VFO: mandarla come si manda un QSO
// significherebbe svegliare gli altri dispositivi cento volte al minuto. Va per
// la sua strada — un POST che non aspetta risposta, niente cursore, niente
// storia — e con misura: al massimo ogni venti secondi, ma subito se cambia
// qualcosa che si vede.

void CloudController::clientStateChanged(const QVariantMap& state)
{
    // Vale anche per un collegamento di passaggio: la frequenza non si scrive da
    // nessuna parte qui, va al server che si sta provando e basta.
    if (m_token.isEmpty())
        return;
    m_presence = state;

    auto same = [this](const char* key) {
        return m_presence.value(QLatin1String(key)) == m_presenceSent.value(QLatin1String(key));
    };
    const bool worthSaying = m_presenceSent.isEmpty()
        || !same("band") || !same("mode") || !same("transmitting") || !same("dxCall");

    if (worthSaying) {
        m_presenceTimer.stop();
        m_sync.reportPresence(m_presence);
        m_presenceSent = m_presence;
        // Da qui in poi, per venti secondi, il resto aspetta.
        m_presenceTimer.start();
        return;
    }
    if (!m_presenceTimer.isActive())
        m_presenceTimer.start();
}

} // namespace decolog::app
