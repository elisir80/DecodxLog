#include "app/QslController.h"

#include "core/CredentialStore.h"
#include "core/LogDatabase.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>
#include <array>

namespace decolog::app {

using namespace decolog::core;

namespace {

struct ServiceInfo {
    const char* id;
    const char* label;
    const char* credential;     // il servizio nel portachiavi
};

constexpr std::array<ServiceInfo, 4> kServices{{
    {"lotw", "LoTW", "lotw"},
    {"qrz", "QRZ Logbook", "qrzlogbook"},
    {"clublog", "Club Log", "clublog"},
    {"eqsl", "eQSL", "eqsl"},
}};

// LoTW accetta file grandi, ma un invio troppo lungo blocca tutto il resto.
constexpr int kLotwBatch = 500;
// Club Log preferisce blocchi ragionevoli a un log intero per volta.
constexpr int kClubLogBatch = 1000;

bool isBatchService(const QString& service)
{
    return service == QLatin1String("lotw") || service == QLatin1String("clublog");
}

QString tempAdifPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("decolog-lotw-upload.adi"));
}

} // namespace

QslController::QslController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    QSettings s;
    m_tqslPath = s.value(QStringLiteral("qsl/tqslPath")).toString();
    if (m_tqslPath.isEmpty())
        m_tqslPath = qsl::findTqsl();
    m_tqslLocation = s.value(QStringLiteral("qsl/tqslLocation")).toString();
    m_clubLogApiKey = s.value(QStringLiteral("qsl/clubLogApiKey")).toString();
    for (const auto& info : kServices)
        m_auto.insert(QLatin1String(info.id), s.value(QStringLiteral("qsl/auto/") + QLatin1String(info.id), false).toBool());
    m_tqsl.setProgram(m_tqslPath);

    connect(&m_tqsl, &TqslUploader::finished, this, &QslController::finishBatch);
    connect(&m_web, &WebQslUploader::finished, this, [this](const QslUploadResult& result) {
        if (m_batch.isEmpty())
            return;
        if (m_batchMode) {
            // Club Log ha risposto per tutto il blocco in una volta.
            finishBatch(result);
            return;
        }
        const qint64 id = m_batch.takeFirst();
        markSent(id, m_busyService, result);
        if (result.retryLater) {
            // Rete o servizio giu': si ferma qui, il resto resta in coda per dopo.
            note(tr("%1: stopped, %2").arg(m_busyService, result.message), QStringLiteral("warning"));
            finishBatch(result);
            return;
        }
        uploadNextWeb();
    });

    // I QSO che arrivano a raffica si mandano insieme poco dopo, non uno a uno.
    m_autoDelay.setSingleShot(true);
    m_autoDelay.setInterval(20'000);
    connect(&m_autoDelay, &QTimer::timeout, this, [this] {
        m_autoQueue.clear();
        for (const auto& info : kServices) {
            const QString id = QLatin1String(info.id);
            if (m_auto.value(id) && !busy())
                uploadPending(id, 0);
        }
    });
}

// ── Stato ─────────────────────────────────────────────────────────────────────

bool QslController::tqslReady() const
{
    return !m_tqslPath.isEmpty() && QFile::exists(m_tqslPath) && qsl::tqslHasCertificate();
}

QString QslController::tqslStatus() const
{
    if (m_tqslPath.isEmpty() || !QFile::exists(m_tqslPath))
        return tr("TQSL not found: install Trusted QSL or set its path here");
    if (!qsl::tqslHasCertificate())
        return tr("TQSL is installed but has no certificate: import your LoTW certificate in TQSL");
    const QStringList locations = tqslLocations();
    if (locations.isEmpty())
        return tr("TQSL has no station location: create one in TQSL (Station → Add location)");
    return tr("TQSL ready · %n station location(s)", nullptr, static_cast<int>(locations.size()));
}

QVariantList QslController::services() const
{
    QVariantList out;
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return out;
    const QList<QVariantMap> summary = m_ctx.db->qslSummary();
    for (const auto& info : kServices) {
        const QString id = QLatin1String(info.id);
        QVariantMap row{{QStringLiteral("id"), id},
                        {QStringLiteral("label"), QLatin1String(info.label)},
                        {QStringLiteral("pending"), m_ctx.db->uploadPendingCount(id)},
                        {QStringLiteral("auto"), m_auto.value(id)},
                        {QStringLiteral("busy"), m_busyService == id},
                        {QStringLiteral("lastResult"), m_lastResult.value(id)}};
        for (const QVariantMap& s : summary) {
            if (s.value(QStringLiteral("service")).toString() == id) {
                row.insert(QStringLiteral("sent"), s.value(QStringLiteral("sent")));
                row.insert(QStringLiteral("confirmed"), s.value(QStringLiteral("confirmed")));
                row.insert(QStringLiteral("errors"), s.value(QStringLiteral("errors")));
            }
        }
        const QString credential = QLatin1String(info.credential);
        const bool hasSecret = m_ctx.credentials && m_ctx.credentials->hasSecret(credential);
        bool ready = hasSecret;
        QString hint;
        if (id == QLatin1String("lotw")) {
            ready = tqslReady();
            hint = tqslStatus();
        } else if (id == QLatin1String("clublog")) {
            ready = hasSecret && !m_clubLogApiKey.isEmpty();
            hint = !hasSecret      ? tr("no credentials: Setup \u2192 QSL services")
                 : m_clubLogApiKey.isEmpty() ? tr("no API key: Setup \u2192 QSL services")
                                             : QString();
        } else if (!hasSecret) {
            hint = tr("no credentials: Setup \u2192 QSL services");
        }
        row.insert(QStringLiteral("ready"), ready);
        row.insert(QStringLiteral("hint"), hint);
        row.insert(QStringLiteral("credential"), credential);
        out << row;
    }
    return out;
}

void QslController::setTqslPath(const QString& path)
{
    if (path == m_tqslPath)
        return;
    m_tqslPath = path.trimmed();
    m_tqsl.setProgram(m_tqslPath);
    QSettings().setValue(QStringLiteral("qsl/tqslPath"), m_tqslPath);
    emit changed();
}

void QslController::setClubLogApiKey(const QString& key)
{
    if (key.trimmed() == m_clubLogApiKey)
        return;
    m_clubLogApiKey = key.trimmed();
    QSettings().setValue(QStringLiteral("qsl/clubLogApiKey"), m_clubLogApiKey);
    emit changed();
}

void QslController::setTqslLocation(const QString& location)
{
    if (location == m_tqslLocation)
        return;
    m_tqslLocation = location;
    QSettings().setValue(QStringLiteral("qsl/tqslLocation"), location);
    emit changed();
}

void QslController::setAutoUpload(const QString& service, bool automatic)
{
    if (m_auto.value(service) == automatic)
        return;
    m_auto.insert(service, automatic);
    QSettings().setValue(QStringLiteral("qsl/auto/") + service, automatic);
    emit changed();
}

bool QslController::autoUpload(const QString& service) const
{
    return m_auto.value(service);
}

void QslController::note(const QString& text, const QString& level)
{
    if (m_ctx.activity)
        m_ctx.activity(QStringLiteral("QSL"), text, level);
}

void QslController::readSecret(const QString& service, std::function<void(const QString&, const QString&)> done)
{
    if (!m_ctx.credentials) {
        done({}, tr("no keystore"));
        return;
    }
    m_ctx.credentials->readSecret(service, std::move(done));
}

// ── Invio ─────────────────────────────────────────────────────────────────────

void QslController::qsoLogged(qint64 id)
{
    const bool anyAuto = std::any_of(kServices.cbegin(), kServices.cend(),
                                     [this](const ServiceInfo& i) { return m_auto.value(QLatin1String(i.id)); });
    if (!anyAuto)
        return;
    m_autoQueue << id;
    m_autoDelay.start();
}

void QslController::uploadQso(qint64 id, const QString& service)
{
    if (busy() || !m_ctx.db)
        return;
    m_busyService = service;
    m_batch = {id};
    startNext();
}

int QslController::uploadQsos(const QVariantList& ids, const QString& service)
{
    if (busy() || !m_ctx.db || !m_ctx.db->isOpen() || ids.isEmpty())
        return 0;
    // Solo quelli che a quel servizio devono ancora andare: rimandare un QSO
    // gia' confermato non serve a nessuno e fa arrabbiare il servizio.
    const QList<qint64> pending = m_ctx.db->qsosToUpload(service, 0);
    const QSet<qint64> waiting(pending.cbegin(), pending.cend());
    QList<qint64> batch;
    for (const QVariant& value : ids) {
        const qint64 id = value.toLongLong();
        if (waiting.contains(id) && !batch.contains(id))
            batch << id;
    }
    if (batch.isEmpty()) {
        m_lastResult.insert(service, tr("nothing to send: they have already gone"));
        emit changed();
        return 0;
    }
    m_busyService = service;
    m_batch = batch;
    startNext();
    return static_cast<int>(batch.size());
}

void QslController::uploadPending(const QString& service, int limit)
{
    if (busy() || !m_ctx.db || !m_ctx.db->isOpen())
        return;
    int cap = limit;
    if (service == QLatin1String("lotw"))
        cap = limit > 0 ? qMin(limit, kLotwBatch) : kLotwBatch;
    else if (service == QLatin1String("clublog"))
        cap = limit > 0 ? qMin(limit, kClubLogBatch) : kClubLogBatch;
    m_batch = m_ctx.db->qsosToUpload(service, cap);
    if (m_batch.isEmpty()) {
        m_lastResult.insert(service, tr("nothing to send"));
        emit changed();
        return;
    }
    m_busyService = service;
    startNext();
}

void QslController::startNext()
{
    m_accepted = m_duplicates = m_rejected = 0;
    m_batchMode = isBatchService(m_busyService);
    emit changed();

    if (m_busyService == QLatin1String("lotw")) {
        const QString path = tempAdifPath();
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QslUploadResult failed;
            failed.message = tr("Cannot write %1").arg(path);
            finishBatch(failed);
            return;
        }
        file.write(m_ctx.db->exportAdif(m_batch, QCoreApplication::applicationVersion()));
        file.close();
        m_adifFile = path;
        const QString location = m_tqslLocation.isEmpty() && m_ctx.stationLocation ? m_ctx.stationLocation()
                                                                                   : m_tqslLocation;
        note(tr("LoTW: sending %n QSO with TQSL…", nullptr, static_cast<int>(m_batch.size())), QStringLiteral("info"));
        m_tqsl.upload(path, location, static_cast<int>(m_batch.size()));
        return;
    }

    if (m_busyService == QLatin1String("clublog")) {
        const QString call = m_ctx.stationCallsign ? m_ctx.stationCallsign() : QString();
        if (call.isEmpty()) {
            QslUploadResult failed;
            failed.message = tr("Club Log: the station profile has no callsign");
            finishBatch(failed);
            return;
        }
        m_account = m_ctx.credentials ? m_ctx.credentials->account(QStringLiteral("clublog")) : QString();
        readSecret(QStringLiteral("clublog"), [this, call](const QString& secret, const QString& error) {
            if (secret.isEmpty()) {
                QslUploadResult failed;
                failed.message = tr("%1: no credentials (%2)").arg(m_busyService, error);
                finishBatch(failed);
                return;
            }
            ClubLogAuth auth;
            auth.email = m_account;
            auth.password = secret;
            auth.callsign = call;
            auth.apiKey = m_clubLogApiKey;
            const QByteArray document = m_ctx.db->exportAdif(m_batch, QCoreApplication::applicationVersion());
            note(tr("Club Log: sending %n QSO\u2026", nullptr, static_cast<int>(m_batch.size())), QStringLiteral("info"));
            m_web.uploadClubLog(auth, document, static_cast<int>(m_batch.size()));
        });
        return;
    }

    // QRZ ed eQSL: prima la credenziale, poi un QSO alla volta.
    const QString credential = m_busyService == QLatin1String("qrz") ? QStringLiteral("qrzlogbook") : QStringLiteral("eqsl");
    m_account = m_ctx.credentials ? m_ctx.credentials->account(credential) : QString();
    readSecret(credential, [this](const QString& secret, const QString& error) {
        if (secret.isEmpty()) {
            QslUploadResult failed;
            failed.message = tr("%1: no credentials (%2)").arg(m_busyService, error);
            finishBatch(failed);
            return;
        }
        m_secret = secret;
        note(tr("%1: sending %n QSO…", nullptr, static_cast<int>(m_batch.size())).arg(m_busyService), QStringLiteral("info"));
        uploadNextWeb();
    });
}

void QslController::uploadNextWeb()
{
    if (m_batch.isEmpty()) {
        QslUploadResult done;
        done.ok = true;
        finishBatch(done);
        return;
    }
    const auto record = m_ctx.db->record(m_batch.first());
    if (!record) {
        m_batch.removeFirst();
        uploadNextWeb();
        return;
    }
    const QString adif = adif::writeRecord(*record);
    if (m_busyService == QLatin1String("qrz"))
        m_web.uploadQrz(m_secret, adif);
    else
        m_web.uploadEqsl(m_account, m_secret, adif);
}

void QslController::markSent(qint64 id, const QString& service, const QslUploadResult& result)
{
    QslState state;
    state.service = service;
    // Anche il duplicato e' "inviato": il servizio ce l'ha.
    state.sent = result.accepted > 0 || result.duplicates > 0 ? QStringLiteral("Y") : QStringLiteral("N");
    if (state.sent == QLatin1String("Y"))
        state.sentDate = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd"));
    state.remoteId = result.remoteId;
    state.lastError = result.ok ? QString() : result.message;
    // La conferma ricevuta non si tocca: la scrive il download delle conferme.
    for (const QslState& existing : m_ctx.db->qslStatus(id)) {
        if (existing.service == service) {
            state.rcvd = existing.rcvd;
            state.rcvdDate = existing.rcvdDate;
        }
    }
    m_ctx.db->setQslState(id, state);
    m_accepted += result.accepted;
    m_duplicates += result.duplicates;
    m_rejected += result.rejected;
}

void QslController::finishBatch(const QslUploadResult& result)
{
    const QString service = m_busyService;
    if (m_batchMode) {
        if (result.ok) {
            for (qint64 id : m_batch)
                markSent(id, service, result.duplicates > 0 && result.accepted == 0
                                          ? [] { QslUploadResult r; r.ok = true; r.duplicates = 1; return r; }()
                                          : [] { QslUploadResult r; r.ok = true; r.accepted = 1; return r; }());
        } else {
            for (qint64 id : m_batch) {
                QslUploadResult failed = result;
                failed.accepted = failed.duplicates = 0;
                markSent(id, service, failed);
            }
        }
        if (!m_adifFile.isEmpty()) {
            QFile::remove(m_adifFile);
            m_adifFile.clear();
        }
    }

    m_secret.clear();
    m_batch.clear();
    m_busyService.clear();
    m_batchMode = false;

    const QString summary = result.ok
        ? tr("%1: %2 sent, %3 already there, %4 rejected").arg(service).arg(m_accepted).arg(m_duplicates).arg(m_rejected)
        : result.message;
    m_lastResult.insert(service, summary);
    note(summary, result.ok ? QStringLiteral("success") : QStringLiteral("error"));
    if (!result.ok && !result.message.isEmpty() && result.message != summary)
        note(QStringLiteral("  ") + result.message, QStringLiteral("warning"));
    if (m_ctx.logChanged)
        m_ctx.logChanged();
    emit changed();
}

void QslController::cancel()
{
    m_tqsl.cancel();
    m_batch.clear();
    m_busyService.clear();
    m_batchMode = false;
    m_secret.clear();
    emit changed();
}

} // namespace decolog::app
