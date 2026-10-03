#include "core/Updates.h"
#include "core/NetworkError.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSysInfo>

#include <algorithm>

namespace decolog::core {

namespace updates {

namespace {

// "v1.2.0" → [1, 2, 0]. Quello che non e' un numero si butta: "1.2.0-beta1"
// conta come 1.2.0, e va bene cosi' per decidere se c'e' qualcosa di nuovo.
QList<int> pieces(const QString& version)
{
    QList<int> out;
    QString digits;
    for (const QChar c : version) {
        if (c.isDigit()) {
            digits.append(c);
        } else {
            if (!digits.isEmpty()) {
                out.append(digits.toInt());
                digits.clear();
            }
            if (c == QLatin1Char('-') || c == QLatin1Char('+'))
                break;   // da qui in poi e' un pre-rilascio: non conta
        }
    }
    if (!digits.isEmpty())
        out.append(digits.toInt());
    return out;
}

bool hasX86Architecture(const QString& name)
{
    return name.contains(QLatin1String("x86_64"))
           || name.contains(QLatin1String("x86-64"))
           || name.contains(QLatin1String("amd64"))
           || name.contains(QLatin1String("x64"))
           || name.contains(QLatin1String("win64"));
}

bool hasArm64Architecture(const QString& name)
{
    return name.contains(QLatin1String("aarch64"))
           || name.contains(QLatin1String("arm64"));
}

QString targetManifestName(const UpdateTarget& target)
{
    const QString platform = target.platform.trimmed().toLower();
    // Il chiamante ha gia' normalizzato l'architettura; qui non richiamiamo
    // normalizedArchitecture per mantenere questo helper privato e leggero.
    const QString architecture = target.architecture.trimmed().toLower();
    return QStringLiteral("decodxlog-release-%1-%2.json").arg(platform, architecture);
}

int architectureScore(const QString& name, const QString& architecture)
{
    const bool x86 = hasX86Architecture(name);
    const bool arm64 = hasArm64Architecture(name);
    if (x86 && arm64)
        return 0; // un nome ambiguo non e' un pacchetto sicuro

    if (architecture == QLatin1String("x86_64"))
        return arm64 ? 0 : (x86 ? 200 : 100);
    if (architecture == QLatin1String("aarch64"))
        return x86 ? 0 : (arm64 ? 200 : 100);

    // Per una CPU sconosciuta e' sicuro solo un pacchetto davvero generico.
    return (x86 || arm64) ? 0 : 100;
}

ReleaseInfo parseReleaseObject(const QJsonObject& root, const UpdateTarget& target)
{
    ReleaseInfo info;
    if (root.value(QStringLiteral("draft")).toBool()
        || root.value(QStringLiteral("prerelease")).toBool()) {
        return info;
    }

    QString tag = root.value(QStringLiteral("tag_name")).toString().trimmed();
    if (tag.startsWith(QLatin1Char('v')) || tag.startsWith(QLatin1Char('V')))
        tag = tag.mid(1);
    if (tag.isEmpty())
        return info;

    const QString platform = target.platform.trimmed().toLower();
    const QString architecture = normalizedArchitecture(target.architecture);
    QUrl fixedManifest;
    QUrl fixedSignature;
    QUrl platformManifest;
    QUrl platformSignature;
    const QString manifestName = targetManifestName({platform, architecture});
    const QString signatureName = manifestName + QStringLiteral(".sig");
    int bestScore = 0;
    for (const QJsonValue& v : root.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = v.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
        if (!url.isValid() || url.isEmpty())
            continue;

        // L'elenco firmato e la sua firma non sono pacchetti: si tengono a parte.
        if (name == QLatin1String(releasesig::kManifestName)) {
            fixedManifest = url;
            continue;
        }
        if (name == QLatin1String(releasesig::kSignatureName)) {
            fixedSignature = url;
            continue;
        }
        if (name == manifestName) {
            platformManifest = url;
            continue;
        }
        if (name == signatureName) {
            platformSignature = url;
            continue;
        }
        const int score = assetMatchScore(name, {platform, architecture});
        if (score <= bestScore)
            continue;
        bestScore = score;
        info.package = url;
        info.packageName = name;
        info.packageBytes = static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble());
    }

    // L'avviso ha senso solo quando c'e' qualcosa che questa macchina puo'
    // davvero installare. La pagina GitHub resta comunque raggiungibile dalle
    // impostazioni, ma non interrompe chi usa un altro sistema operativo.
    if (info.package.isEmpty())
        return {};

    // Ogni workflow puo' firmare il proprio pacchetto senza correre con gli
    // altri (DMG/AppImage/EXE). Le vecchie release con un unico manifest
    // restano compatibili.
    info.manifest = !platformManifest.isEmpty() ? platformManifest : fixedManifest;
    info.signature = !platformSignature.isEmpty() ? platformSignature : fixedSignature;

    info.valid = true;
    info.version = tag;
    info.page = root.value(QStringLiteral("html_url")).toString();
    info.notes = root.value(QStringLiteral("body")).toString();
    return info;
}

bool isReleasePayload(const QByteArray& json)
{
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError)
        return false;
    if (doc.isArray())
        return true; // nessuna release e' una risposta valida, non un errore
    return doc.isObject() && doc.object().contains(QStringLiteral("tag_name"));
}

} // namespace

int compareVersions(const QString& a, const QString& b)
{
    const QList<int> left = pieces(a);
    const QList<int> right = pieces(b);
    const int n = qMax(left.size(), right.size());
    for (int i = 0; i < n; ++i) {
        const int x = i < left.size() ? left.at(i) : 0;
        const int y = i < right.size() ? right.at(i) : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

UpdateTarget currentTarget()
{
#if defined(Q_OS_WIN)
    const QString platform = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    const QString platform = QStringLiteral("macos");
#else
    const QString platform = QStringLiteral("linux");
#endif
    return {platform, QSysInfo::currentCpuArchitecture()};
}

QString normalizedArchitecture(QString architecture)
{
    architecture = architecture.trimmed().toLower();
    if (architecture == QLatin1String("amd64")
        || architecture == QLatin1String("x64")
        || architecture == QLatin1String("x86-64")) {
        return QStringLiteral("x86_64");
    }
    if (architecture == QLatin1String("arm64")
        || architecture == QLatin1String("arm64-v8a")) {
        return QStringLiteral("aarch64");
    }
    return architecture;
}

int assetMatchScore(const QString& assetName, const UpdateTarget& target)
{
    const QString name = assetName.trimmed().toLower();
    const QString platform = target.platform.trimmed().toLower();
    const QString architecture = normalizedArchitecture(target.architecture);

    if (platform == QLatin1String("windows")) {
        if (!name.endsWith(QLatin1String(".exe"))
            || !name.contains(QLatin1String("setup"))) {
            return 0;
        }
        return architectureScore(name, architecture);
    }

    if (platform == QLatin1String("macos")) {
        if (!name.endsWith(QLatin1String(".dmg")))
            return 0;
    } else if (platform == QLatin1String("linux")) {
        if (!name.endsWith(QLatin1String(".appimage")))
            return 0;
    } else {
        return 0;
    }

    return architectureScore(name, architecture);
}

ReleaseInfo parseRelease(const QByteArray& json, const UpdateTarget& target)
{
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return parseReleaseObject(doc.object(), target);
}

ReleaseInfo parseReleases(const QByteArray& json, const UpdateTarget& target)
{
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError)
        return {};
    if (doc.isObject())
        return parseReleaseObject(doc.object(), target);
    if (!doc.isArray())
        return {};

    ReleaseInfo best;
    for (const QJsonValue& value : doc.array()) {
        const ReleaseInfo candidate = parseReleaseObject(value.toObject(), target);
        if (candidate.valid
            && (!best.valid || compareVersions(candidate.version, best.version) > 0)) {
            best = candidate;
        }
    }
    return best;
}

QList<ReleaseInfo> newerReleases(const QList<ReleaseInfo>& candidates, const QString& currentVersion)
{
    QList<ReleaseInfo> out;
    for (const ReleaseInfo& info : candidates) {
        if (info.valid && compareVersions(info.version, currentVersion) > 0)
            out.append(info);
    }
    std::stable_sort(out.begin(), out.end(), [](const ReleaseInfo& a, const ReleaseInfo& b) {
        return compareVersions(a.version, b.version) > 0;
    });
    return out;
}

void applyManifest(ReleaseInfo& info, const releasesig::Manifest& manifest)
{
    info.signatureState = manifest.state;
    info.signatureKey = manifest.keyId;
    info.packageSha256.clear();
    if (!manifest.verified())
        return;
    const releasesig::SignedFile file = manifest.file(info.packageName);
    // Un pacchetto fuori dall'elenco, o piu' grande o piu' piccolo di come
    // e' stato firmato, non e' quello che chi pubblica ha firmato.
    if (file.sha256.size() != 32 || (info.packageBytes > 0 && file.size != info.packageBytes)) {
        info.signatureState = releasesig::Manifest::State::Invalid;
        return;
    }
    info.packageSha256 = file.sha256;
    info.packageBytes = file.size;
}

} // namespace updates

UpdateFetcher::UpdateFetcher(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

void UpdateFetcher::fetch(const QString& currentVersion)
{
    if (m_busy)
        return;
    m_busy = true;
    m_currentVersion = currentVersion;
    m_target = updates::currentTarget();
    m_found.clear();
    m_candidates.clear();
    m_lastError.clear();
    m_answered = 0;

    // Si chiede a tutte e due le sorgenti: il fork di elisir80, che pubblica
    // anche i pacchetti macOS e Linux, e il repository di iu8lmc. Vince la
    // versione piu' nuova che risulta firmata (ciascuna con la chiave del suo
    // repository); a pari versione il fork, che e' chiesto per primo.
    if (!m_overrideUrl.isEmpty()) {
        m_sources = {{QStringLiteral("test endpoint"), m_overrideUrl, {}}};
    } else {
        m_sources = {
            {QStringLiteral("elisir80/DecodxLog"),
             QUrl(QStringLiteral("https://api.github.com/repos/elisir80/DecodxLog/releases?per_page=100")),
             QUrl(QStringLiteral("https://github.com/elisir80/DecodxLog/releases/latest"))},
            {QStringLiteral("iu8lmc/DecoDXLog"),
             QUrl(QStringLiteral("https://api.github.com/repos/iu8lmc/DecoDXLog/releases?per_page=100")),
             QUrl(QStringLiteral("https://github.com/iu8lmc/DecoDXLog/releases/latest"))},
        };
    }
    requestSource(0);
}

void UpdateFetcher::requestSource(int index)
{
    if (index >= m_sources.size()) {
        sourcesDone();
        return;
    }
    const Source source = m_sources.at(index);
    QNetworkRequest request(source.api);
    network::useHttp11(request);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "DecoDXLog");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, source, index] {
        const QNetworkReply::NetworkError error = reply->error();
        const QByteArray payload = reply->readAll();
        if (error != QNetworkReply::NoError) {
            m_lastError = reply->errorString();
        } else if (!updates::isReleasePayload(payload)) {
            m_lastError = tr("the answer from GitHub was not understood");
        } else {
            ++m_answered;
            ReleaseInfo candidate = updates::parseReleases(payload, m_target);
            if (candidate.valid) {
                candidate.repository = source.repository;
                if (candidate.page.isEmpty())
                    candidate.page = source.page.toString();
                m_found.append(candidate);
            }
        }
        reply->deleteLater();
        requestSource(index + 1);
    });
}

void UpdateFetcher::sourcesDone()
{
    if (m_answered == 0) {
        m_busy = false;
        emit failed(m_lastError.isEmpty() ? tr("the answer from GitHub was not understood") : m_lastError);
        return;
    }
    // Le due sorgenti sono alla pari: conta la versione (decisione di IU8LMC,
    // 03/10/2026). newerReleases le mette gia' dalla piu' nuova.
    m_candidates = updates::newerReleases(m_found, m_currentVersion);
    if (m_candidates.isEmpty()) {
        finishWithoutUpdate();
        return;
    }
    verifyCandidate(0);
}

QNetworkReply* UpdateFetcher::getSmall(const QUrl& url)
{
    QNetworkRequest request(url);
    network::useHttp11(request);
    request.setRawHeader("User-Agent", "DecoDXLog");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(request);
    // L'elenco firmato e' di qualche centinaio di byte: uno enorme non e' il nostro.
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > 256 * 1024)
            reply->abort();
    });
    return reply;
}

void UpdateFetcher::verifyCandidate(int index)
{
    // Nessuna firmata: si propone la piu' nuova, da scaricare a mano.
    if (index >= m_candidates.size()) {
        m_busy = false;
        emit finished(m_candidates.first());
        return;
    }
    ReleaseInfo& candidate = m_candidates[index];
    if (candidate.manifest.isEmpty() || candidate.signature.isEmpty()) {
        candidate.signatureState = releasesig::Manifest::State::Missing;
        verifyCandidate(index + 1);
        return;
    }
    QNetworkReply* manifestReply = getSmall(candidate.manifest);
    connect(manifestReply, &QNetworkReply::finished, this, [this, manifestReply, index] {
        const bool manifestOk = manifestReply->error() == QNetworkReply::NoError;
        const QByteArray manifest = manifestReply->readAll();
        manifestReply->deleteLater();
        if (!manifestOk) {
            m_candidates[index].signatureState = releasesig::Manifest::State::Missing;
            verifyCandidate(index + 1);
            return;
        }
        QNetworkReply* signatureReply = getSmall(m_candidates.at(index).signature);
        connect(signatureReply, &QNetworkReply::finished, this, [this, signatureReply, index, manifest] {
            const bool signatureOk = signatureReply->error() == QNetworkReply::NoError;
            const QByteArray signature = signatureReply->readAll();
            signatureReply->deleteLater();
            ReleaseInfo& info = m_candidates[index];
            if (!signatureOk) {
                info.signatureState = releasesig::Manifest::State::Missing;
                verifyCandidate(index + 1);
                return;
            }
            updates::applyManifest(info, m_keys.isEmpty()
                                             ? releasesig::verify(manifest, signature, info.repository, info.version)
                                             : releasesig::verify(manifest, signature, info.repository, info.version,
                                                                  m_keys));
            if (info.verified()) {
                m_busy = false;
                emit finished(info);
                return;
            }
            verifyCandidate(index + 1);
        });
    });
}

void UpdateFetcher::finishWithoutUpdate()
{
    m_busy = false;
    emit finished({});
}

void UpdateFetcher::download(const QUrl& url, const QString& path, qint64 expectedBytes,
                             const QByteArray& expectedSha256, QFileDevice::Permissions permissions)
{
    if (m_download)
        return;
    // Senza lo SHA-256 firmato non si scarica niente da installare.
    if (expectedSha256.size() != 32 || expectedBytes <= 0) {
        emit downloadFailed(tr("the package is not in the signed list of the release"));
        return;
    }

    auto* output = new QSaveFile(path);
    output->setDirectWriteFallback(false);
    if (!output->open(QIODevice::WriteOnly)) {
        const QString error = output->errorString();
        delete output;
        emit downloadFailed(error);
        return;
    }

    m_downloadPath = path;
    m_downloadExpectedBytes = expectedBytes;
    m_downloadExpectedSha256 = expectedSha256;
    m_downloadHash.reset();
    m_downloadPermissions = permissions;
    m_setDownloadPermissions = permissions != QFileDevice::Permissions{};
    m_downloadWriteFailed = false;
    m_downloadFile = output;

    QNetworkRequest request(url);
    network::useHttp11(request);
    request.setRawHeader("User-Agent", "DecoDXLog");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_download = m_net->get(request);
    connect(m_download, &QNetworkReply::readyRead, this, [this] {
        if (!m_download || !m_downloadFile)
            return;
        const QByteArray data = m_download->readAll();
        m_downloadHash.addData(data);
        if (!data.isEmpty() && m_downloadFile->write(data) != data.size()) {
            m_downloadWriteFailed = true;
            m_download->abort();
        }
    });
    connect(m_download, &QNetworkReply::downloadProgress, this, &UpdateFetcher::progress);
    connect(m_download, &QNetworkReply::finished, this, [this] {
        QNetworkReply* reply = m_download;
        QSaveFile* output = m_downloadFile;
        const QString path = m_downloadPath;
        const qint64 expectedBytes = m_downloadExpectedBytes;
        const QByteArray expectedSha256 = m_downloadExpectedSha256;
        const QFileDevice::Permissions permissions = m_downloadPermissions;
        const bool setPermissions = m_setDownloadPermissions;
        const bool writeFailed = m_downloadWriteFailed;
        m_download = nullptr;
        m_downloadFile = nullptr;
        m_downloadPath.clear();
        m_downloadExpectedBytes = 0;
        m_downloadExpectedSha256.clear();
        m_downloadPermissions = {};
        m_setDownloadPermissions = false;
        m_downloadWriteFailed = false;

        QNetworkReply::NetworkError networkError = reply->error();
        QString error = networkError == QNetworkReply::NoError ? QString{} : reply->errorString();
        if (networkError == QNetworkReply::NoError && !writeFailed) {
            const QByteArray data = reply->readAll();
            m_downloadHash.addData(data);
            if (!data.isEmpty() && output->write(data) != data.size())
                error = output->errorString();
        }
        reply->deleteLater();

        if (networkError != QNetworkReply::NoError) {
            output->cancelWriting();
            delete output;
            emit downloadFailed(error);
            return;
        }
        if (writeFailed || !error.isEmpty()) {
            output->cancelWriting();
            delete output;
            emit downloadFailed(error.isEmpty() ? tr("cannot write %1").arg(path) : error);
            return;
        }
        if (output->size() <= 0 || (expectedBytes > 0 && output->size() != expectedBytes)) {
            output->cancelWriting();
            delete output;
            emit downloadFailed(tr("the downloaded file is incomplete"));
            return;
        }
        // Il controllo che conta: e' proprio il file firmato da chi pubblica?
        // Se no, non resta niente su disco e non si lancia niente.
        if (m_downloadHash.result() != expectedSha256) {
            output->cancelWriting();
            delete output;
            emit downloadFailed(tr("the downloaded file is not the one signed by the publisher: it was discarded"));
            return;
        }

        if (setPermissions && !output->setPermissions(permissions)) {
            error = output->errorString();
            output->cancelWriting();
            delete output;
            emit downloadFailed(error);
            return;
        }
        if (!output->commit()) {
            error = output->errorString();
            delete output;
            emit downloadFailed(error);
            return;
        }
        delete output;
        emit downloaded(path);
    });
}

void UpdateFetcher::cancelDownload()
{
    if (m_download)
        m_download->abort();
}

} // namespace decolog::core
