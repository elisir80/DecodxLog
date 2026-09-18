#include "core/CloudSync.h"

#include "core/NetworkError.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace decolog::core {

namespace {

QUrl endpoint(const QUrl& base, const QString& path)
{
    QUrl url = base;
    QString root = url.path();
    while (root.endsWith(QLatin1Char('/')))
        root.chop(1);
    url.setPath(root + path);
    return url;
}

} // namespace

CloudSync::CloudSync(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

QNetworkReply* CloudSync::send(const QString& path, const QVariantMap& body, bool authenticated)
{
    QNetworkRequest request(endpoint(m_base, path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DecoLog/%1").arg(QCoreApplication::applicationVersion()));
    request.setTransferTimeout(60'000);
    if (authenticated && !m_token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    const QByteArray payload = QJsonDocument(QJsonObject::fromVariantMap(body)).toJson(QJsonDocument::Compact);
    return m_net->post(request, payload);
}

QNetworkReply* CloudSync::get(const QString& path, const QVariantMap& query)
{
    QUrl url = endpoint(m_base, path);
    QUrlQuery q;
    for (auto it = query.cbegin(); it != query.cend(); ++it)
        q.addQueryItem(it.key(), it.value().toString());
    url.setQuery(q);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DecoLog/%1").arg(QCoreApplication::applicationVersion()));
    request.setTransferTimeout(60'000);
    if (!m_token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    return m_net->get(request);
}

void CloudSync::watch(QNetworkReply* reply, const QString& what)
{
    m_busy = true;
    connect(reply, &QNetworkReply::finished, this, [this, reply, what] {
        reply->deleteLater();
        m_busy = false;
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const QJsonObject answer = QJsonDocument::fromJson(body).object();

        if (reply->error() != QNetworkReply::NoError || code >= 400) {
            CloudError error;
            error.ok = false;
            error.unauthorized = code == 401 || code == 403;
            // Rete giu', servizio in manutenzione, troppe richieste: si riprova.
            error.retryLater = code == 0 || code == 429 || code >= 500;
            const QString detail = answer.value(QStringLiteral("detail")).toString();
            error.message = detail.isEmpty() ? network::safeErrorString(reply) : detail;
            emit failed(error);
            return;
        }

        if (what == QLatin1String("auth")) {
            emit loggedIn(answer.value(QStringLiteral("token")).toString(),
                          answer.value(QStringLiteral("callsign")).toString());
            return;
        }
        if (what == QLatin1String("push")) {
            QVariantList results;
            for (const QJsonValue& value : answer.value(QStringLiteral("results")).toArray())
                results << value.toObject().toVariantMap();
            emit pushed(results, static_cast<qint64>(answer.value(QStringLiteral("cursor")).toDouble()));
            return;
        }
        if (what == QLatin1String("pull")) {
            QVariantList qsos;
            for (const QJsonValue& value : answer.value(QStringLiteral("qsos")).toArray())
                qsos << value.toObject().toVariantMap();
            emit pulled(qsos, static_cast<qint64>(answer.value(QStringLiteral("cursor")).toDouble()),
                        answer.value(QStringLiteral("more")).toBool());
            return;
        }
        emit statusReady(answer.toVariantMap());
    });
}

void CloudSync::signup(const QString& callsign, const QString& password)
{
    watch(send(QStringLiteral("/v1/auth/signup"),
               {{QStringLiteral("callsign"), callsign},
                {QStringLiteral("password"), password},
                {QStringLiteral("device"), m_device}},
               false),
          QStringLiteral("auth"));
}

void CloudSync::login(const QString& callsign, const QString& password)
{
    watch(send(QStringLiteral("/v1/auth/token"),
               {{QStringLiteral("callsign"), callsign},
                {QStringLiteral("password"), password},
                {QStringLiteral("device"), m_device}},
               false),
          QStringLiteral("auth"));
}

void CloudSync::push(const QVariantList& qsos)
{
    watch(send(QStringLiteral("/v1/sync/push"),
               {{QStringLiteral("device"), m_device}, {QStringLiteral("qsos"), qsos}}, true),
          QStringLiteral("push"));
}

void CloudSync::pull(qint64 since, int limit)
{
    QVariantMap query{{QStringLiteral("since"), QString::number(since)}};
    if (limit > 0)
        query.insert(QStringLiteral("limit"), QString::number(limit));
    watch(get(QStringLiteral("/v1/sync/pull"), query), QStringLiteral("pull"));
}

void CloudSync::status()
{
    watch(get(QStringLiteral("/v1/sync/status"), {}), QStringLiteral("status"));
}

void CloudSync::cancel()
{
    m_busy = false;
}

} // namespace decolog::core
