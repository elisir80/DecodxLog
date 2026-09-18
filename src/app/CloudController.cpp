#include "app/CloudController.h"

#include "core/CredentialStore.h"
#include "core/LogDatabase.h"

#include <QDateTime>
#include <QSettings>
#include <QSysInfo>

namespace decolog::app {

using namespace decolog::core;

namespace {

// Quanti QSO per volta: un blocco che sta in un secondo di rete.
constexpr int kBatch = 200;

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
    connect(&m_sync, &CloudSync::pulled, this, [this](const QVariantList& qsos, qint64 cursor, bool more) {
        int written = 0;
        for (const QVariant& value : qsos) {
            const auto result = m_ctx.db->applyRemote(value.toMap());
            if (result == LogDatabase::RemoteResult::Inserted
                || result == LogDatabase::RemoteResult::Updated
                || result == LogDatabase::RemoteResult::Deleted) {
                ++written;
            }
        }
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
    connect(&m_sync, &CloudSync::pushed, this, [this](const QVariantList& results, qint64 cursor) {
        applyPushResults(results);
        m_cursor = qMax(m_cursor, cursor);
        m_ctx.db->setSyncState(accountKey(), {{QStringLiteral("lastPush"), nowLabel()},
                                              {QStringLiteral("lastError"), QString()}});
        if (m_ctx.logChanged)
            m_ctx.logChanged();
        // Se ne restano altri in coda si continua, altrimenti si chiude il giro.
        if (queued() > 0) {
            startPush();
            return;
        }
        m_lastSync = nowLabel();
        QSettings().setValue(QStringLiteral("cloud/lastSync"), m_lastSync);
        finish(tr("Cloud: up to date"), QStringLiteral("success"));
        m_sync.status();
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
        m_ctx.db->setSyncState(accountKey(), {{QStringLiteral("lastError"), error.message}});
        finish(error.retryLater ? tr("Cloud: not reachable, will retry (%1)").arg(error.message)
                                : tr("Cloud: %1").arg(error.message),
               error.retryLater ? QStringLiteral("warning") : QStringLiteral("error"));
    });

    // Sync a tempo: ogni cinque minuti, se c'e' qualcosa da fare.
    m_autoTimer.setInterval(5 * 60 * 1000);
    connect(&m_autoTimer, &QTimer::timeout, this, [this] {
        if (linked() && !m_busy)
            syncNow();
    });
    // I QSO che arrivano a raffica si mandano insieme poco dopo, non uno a uno.
    m_afterQso.setSingleShot(true);
    m_afterQso.setInterval(20'000);
    connect(&m_afterQso, &QTimer::timeout, this, [this] {
        if (linked() && !m_busy)
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
    loadToken();
    if (automatic && m_autoMode != QLatin1String("manual"))
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
    if (!m_ctx.credentials || !m_ctx.credentials->hasSecret(QStringLiteral("cloud")))
        return;
    m_ctx.credentials->readSecret(QStringLiteral("cloud"), [this](const QString& token, const QString& error) {
        if (token.isEmpty()) {
            m_status = tr("Cloud: token not readable (%1)").arg(error);
            emit changed();
            return;
        }
        m_token = token;
        m_sync.setToken(token);
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
    m_callsign = callsign;
    m_sync.setToken(token);
    QSettings().setValue(QStringLiteral("cloud/callsign"), callsign);
    if (m_ctx.credentials)
        m_ctx.credentials->save(QStringLiteral("cloud"), callsign, token);
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
    else
        m_autoTimer.start();
    emit changed();
}

void CloudController::signup(const QString& callsign, const QString& password)
{
    if (m_server.isEmpty()) {
        finish(tr("Cloud: set the server address first"), QStringLiteral("warning"));
        return;
    }
    m_busy = true;
    m_status = tr("Cloud: creating the account…");
    emit changed();
    m_sync.signup(callsign.trimmed().toUpper(), password);
}

void CloudController::login(const QString& callsign, const QString& password)
{
    if (m_server.isEmpty()) {
        finish(tr("Cloud: set the server address first"), QStringLiteral("warning"));
        return;
    }
    m_busy = true;
    m_status = tr("Cloud: signing in…");
    emit changed();
    m_sync.login(callsign.trimmed().toUpper(), password);
}

void CloudController::logout()
{
    m_token.clear();
    m_sync.setToken(QString());
    if (m_ctx.credentials)
        m_ctx.credentials->remove(QStringLiteral("cloud"));
    m_remote.clear();
    finish(tr("Cloud: this device is no longer linked"), QStringLiteral("info"));
}

// ── Il giro di sync ───────────────────────────────────────────────────────────

void CloudController::syncNow()
{
    if (!linked()) {
        // Il token puo' essere ancora nel portachiavi: la richiesta non si
        // butta via, parte appena arriva.
        if (m_ctx.credentials && m_ctx.credentials->hasSecret(QStringLiteral("cloud"))) {
            m_syncWhenReady = true;
            m_status = tr("Cloud: opening the keystore…");
            emit changed();
            return;
        }
        finish(tr("Cloud: not linked yet"), QStringLiteral("warning"));
        return;
    }
    if (m_busy)
        return;
    m_busy = true;
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
    m_batch = m_ctx.db->dirtyQsos(kBatch);
    if (m_batch.isEmpty()) {
        m_lastSync = nowLabel();
        QSettings().setValue(QStringLiteral("cloud/lastSync"), m_lastSync);
        finish(tr("Cloud: up to date"), QStringLiteral("success"));
        m_sync.status();
        return;
    }
    QVariantList payload;
    QList<qint64> sent;
    for (qint64 id : m_batch) {
        const QVariantMap record = m_ctx.db->syncRecord(id);
        if (record.isEmpty())
            continue;
        payload << record;
        sent << id;
    }
    m_batch = sent;
    if (payload.isEmpty()) {
        finish(tr("Cloud: nothing to send"), QStringLiteral("info"));
        return;
    }
    m_status = tr("Cloud: sending %n QSO…", nullptr, static_cast<int>(payload.size()));
    emit changed();
    m_sync.push(payload);
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

void CloudController::qsoLogged()
{
    if (linked() && m_autoMode == QLatin1String("qso"))
        m_afterQso.start();
}

} // namespace decolog::app
