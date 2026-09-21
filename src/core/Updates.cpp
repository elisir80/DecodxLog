#include "core/Updates.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

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

ReleaseInfo parseRelease(const QByteArray& json)
{
    ReleaseInfo info;
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return info;
    const QJsonObject root = doc.object();
    // Una bozza o un pre-rilascio non si propone a chi opera.
    if (root.value(QStringLiteral("draft")).toBool() || root.value(QStringLiteral("prerelease")).toBool())
        return info;

    QString tag = root.value(QStringLiteral("tag_name")).toString().trimmed();
    if (tag.startsWith(QLatin1Char('v')) || tag.startsWith(QLatin1Char('V')))
        tag = tag.mid(1);
    if (tag.isEmpty())
        return info;

    info.version = tag;
    info.page = root.value(QStringLiteral("html_url")).toString();
    info.notes = root.value(QStringLiteral("body")).toString();

    for (const QJsonValue& v : root.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = v.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
        if (url.isEmpty())
            continue;
        if (name.endsWith(QLatin1String("-setup.exe"), Qt::CaseInsensitive)) {
            info.installer = url;
            info.installerBytes = static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble());
        } else if (name.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive)) {
            info.archive = url;
        }
    }
    info.valid = true;
    return info;
}

} // namespace updates

UpdateFetcher::UpdateFetcher(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

void UpdateFetcher::fetch()
{
    if (m_busy)
        return;
    m_busy = true;
    QNetworkRequest request(m_url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "DecoDXLog");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_busy = false;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit failed(reply->errorString());
            return;
        }
        const ReleaseInfo info = updates::parseRelease(reply->readAll());
        if (!info.valid) {
            emit failed(tr("the answer from GitHub was not understood"));
            return;
        }
        emit finished(info);
    });
}

void UpdateFetcher::download(const QUrl& url, const QString& path)
{
    if (m_download)
        return;
    m_downloadPath = path;
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "DecoDXLog");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_download = m_net->get(request);
    connect(m_download, &QNetworkReply::downloadProgress, this, &UpdateFetcher::progress);
    connect(m_download, &QNetworkReply::finished, this, [this] {
        QNetworkReply* reply = m_download;
        m_download = nullptr;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit downloadFailed(reply->errorString());
            return;
        }
        // Prima si scrive tutto, poi si dice che c'e': un installatore a meta'
        // non si deve poter lanciare.
        QFile file(m_downloadPath + QStringLiteral(".part"));
        if (!file.open(QIODevice::WriteOnly)) {
            emit downloadFailed(file.errorString());
            return;
        }
        file.write(reply->readAll());
        file.close();
        QFile::remove(m_downloadPath);
        if (!QFile::rename(file.fileName(), m_downloadPath)) {
            emit downloadFailed(tr("cannot write %1").arg(m_downloadPath));
            return;
        }
        emit downloaded(m_downloadPath);
    });
}

void UpdateFetcher::cancelDownload()
{
    if (m_download)
        m_download->abort();
}

} // namespace decolog::core
