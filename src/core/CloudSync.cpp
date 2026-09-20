#include "core/CloudSync.h"

#include "core/NetworkError.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
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

namespace cloudsync {

QString detailOf(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();

    if (value.isArray()) {
        QStringList reasons;
        for (const QJsonValue& item : value.toArray()) {
            const QJsonObject entry = item.toObject();
            const QString message = entry.value(QStringLiteral("msg")).toString();
            if (message.isEmpty())
                continue;
            // L'ultimo pezzo di "loc" e' il campo: ["body", "password"] -> password.
            QString field;
            for (const QJsonValue& part : entry.value(QStringLiteral("loc")).toArray()) {
                const QString name = part.toString();
                if (!name.isEmpty() && name != QLatin1String("body"))
                    field = name;
            }
            reasons << (field.isEmpty() ? message : field + QStringLiteral(": ") + message);
        }
        return reasons.join(QStringLiteral("; "));
    }

    if (value.isObject())
        return value.toObject().value(QStringLiteral("msg")).toString();
    return {};
}

} // namespace cloudsync

CloudSync::CloudSync(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

QNetworkReply* CloudSync::send(const QString& path, const QVariantMap& body, bool authenticated)
{
    QNetworkRequest request(endpoint(m_base, path));
    network::useHttp11(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DecoDXLog/%1").arg(QCoreApplication::applicationVersion()));
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
    network::useHttp11(request);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DecoDXLog/%1").arg(QCoreApplication::applicationVersion()));
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
            const QString detail = cloudsync::detailOf(answer.value(QStringLiteral("detail")));
            error.message = detail.isEmpty() ? network::safeErrorString(reply) : detail;
            // Un 404 su una richiesta che il programma sa fare vuol dire che il
            // server e' piu' vecchio del programma: "Not Found" non lo direbbe a
            // nessuno, e chi legge pensa che sia rotto il suo DecoDXLog.
            if (code == 404) {
                error.message = tr("This Cloud server does not know this request (%1): it is older than "
                                   "your DecoDXLog and has to be updated.").arg(what);
            }
            emit failed(error);
            return;
        }

        if (what == QLatin1String("auth")) {
            emit loggedIn(answer.value(QStringLiteral("token")).toString(),
                          answer.value(QStringLiteral("callsign")).toString());
            return;
        }
        auto list = [&answer](const char* name) {
            QVariantList out;
            for (const QJsonValue& value : answer.value(QLatin1String(name)).toArray())
                out << value.toObject().toVariantMap();
            return out;
        };
        if (what == QLatin1String("push")) {
            emit pushed(list("results"), list("docResults"),
                        static_cast<qint64>(answer.value(QStringLiteral("cursor")).toDouble()));
            return;
        }
        if (what == QLatin1String("pull")) {
            emit pulled(list("qsos"), list("docs"),
                        static_cast<qint64>(answer.value(QStringLiteral("cursor")).toDouble()),
                        answer.value(QStringLiteral("more")).toBool());
            return;
        }
        if (what == QLatin1String("purge")) {
            emit purged(answer.value(QStringLiteral("deleted")).toObject().toVariantMap());
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

void CloudSync::purge(const QString& confirm)
{
    watch(send(QStringLiteral("/v1/account/purge"), {{QStringLiteral("confirm"), confirm}}, true),
          QStringLiteral("purge"));
}

void CloudSync::reportPresence(const QVariantMap& state)
{
    if (m_token.isEmpty())
        return;
    QVariantMap body = state;
    body.insert(QStringLiteral("device"), m_device);
    // Si manda e si lascia andare: la frequenza di adesso, fra dieci secondi,
    // e' gia' un'altra. Niente watch(), cosi' un errore non finisce nel
    // registro e non tocca lo stato del sync.
    QNetworkReply* reply = send(QStringLiteral("/v1/presence"), body, true);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void CloudSync::push(const QVariantList& qsos, const QVariantList& docs)
{
    watch(send(QStringLiteral("/v1/sync/push"),
               {{QStringLiteral("device"), m_device},
                {QStringLiteral("qsos"), qsos},
                {QStringLiteral("docs"), docs}}, true),
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
