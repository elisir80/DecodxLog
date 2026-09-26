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
    int bestScore = 0;
    for (const QJsonValue& v : root.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = v.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
        if (!url.isValid() || url.isEmpty())
            continue;

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

ReleaseInfo selectPreferredUpdate(const ReleaseInfo& primary,
                                  const ReleaseInfo& fallback,
                                  const QString& currentVersion)
{
    if (primary.valid && compareVersions(primary.version, currentVersion) > 0)
        return primary;
    if (fallback.valid && compareVersions(fallback.version, currentVersion) > 0)
        return fallback;
    return {};
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

    // Come Decodium4, il fork con i pacchetti pubblicati e' la sorgente
    // primaria; l'upstream serve quando il fork non ha ancora il binario per
    // questa piattaforma.
    if (!m_overrideUrl.isEmpty()) {
        requestRelease({QStringLiteral("test endpoint"), m_overrideUrl, {}}, false);
        return;
    }
    requestRelease({QStringLiteral("elisir80/DecodxLog"),
                    QUrl(QStringLiteral("https://api.github.com/repos/elisir80/DecodxLog/releases?per_page=100")),
                    QUrl(QStringLiteral("https://github.com/elisir80/DecodxLog/releases/latest"))},
                   true);
}

void UpdateFetcher::requestRelease(const Source& source, bool allowFallback)
{
    QNetworkRequest request(source.api);
    network::useHttp11(request);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "DecoDXLog");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, source, allowFallback] {
        const QNetworkReply::NetworkError error = reply->error();
        const QString errorText = error == QNetworkReply::NoError
                                      ? QString{}
                                      : reply->errorString();
        const QByteArray payload = reply->readAll();
        reply->deleteLater();

        const auto tryFallback = [this, allowFallback](const QString& failure) {
            if (allowFallback) {
                requestRelease({QStringLiteral("iu8lmc/DecoDXLog"),
                                QUrl(QStringLiteral("https://api.github.com/repos/iu8lmc/DecoDXLog/releases?per_page=100")),
                                QUrl(QStringLiteral("https://github.com/iu8lmc/DecoDXLog/releases/latest"))},
                               false);
                return true;
            }
            m_busy = false;
            emit failed(failure.isEmpty()
                        ? tr("the answer from GitHub was not understood")
                        : failure);
            return false;
        };

        if (error != QNetworkReply::NoError) {
            tryFallback(errorText);
            return;
        }

        if (!updates::isReleasePayload(payload)) {
            tryFallback(tr("the answer from GitHub was not understood"));
            return;
        }

        ReleaseInfo candidate = updates::parseReleases(payload, m_target);
        if (candidate.valid) {
            candidate.repository = source.repository;
            if (candidate.page.isEmpty())
                candidate.page = source.page.toString();
        }

        const ReleaseInfo selected = updates::selectPreferredUpdate(
            candidate, {}, m_currentVersion);
        if (selected.valid) {
            m_busy = false;
            emit finished(selected);
            return;
        }

        if (allowFallback) {
            tryFallback(QString{});
            return;
        }
        finishWithoutUpdate();
    });
}

void UpdateFetcher::finishWithoutUpdate()
{
    m_busy = false;
    emit finished({});
}

void UpdateFetcher::download(const QUrl& url, const QString& path, qint64 expectedBytes,
                             QFileDevice::Permissions permissions)
{
    if (m_download)
        return;

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
        const QFileDevice::Permissions permissions = m_downloadPermissions;
        const bool setPermissions = m_setDownloadPermissions;
        const bool writeFailed = m_downloadWriteFailed;
        m_download = nullptr;
        m_downloadFile = nullptr;
        m_downloadPath.clear();
        m_downloadExpectedBytes = 0;
        m_downloadPermissions = {};
        m_setDownloadPermissions = false;
        m_downloadWriteFailed = false;

        QNetworkReply::NetworkError networkError = reply->error();
        QString error = networkError == QNetworkReply::NoError ? QString{} : reply->errorString();
        if (networkError == QNetworkReply::NoError && !writeFailed) {
            const QByteArray data = reply->readAll();
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
