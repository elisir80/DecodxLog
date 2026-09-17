#include "core/Callbook.h"

#include "core/NetworkError.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QXmlStreamReader>

namespace decolog::core {

QVariantMap CallbookRecord::toMap() const
{
    return {
        {QStringLiteral("call"), call},
        {QStringLiteral("name"), name},
        {QStringLiteral("qth"), qth},
        {QStringLiteral("address"), address},
        {QStringLiteral("state"), state},
        {QStringLiteral("county"), county},
        {QStringLiteral("country"), country},
        {QStringLiteral("grid"), grid},
        {QStringLiteral("iota"), iota},
        {QStringLiteral("email"), email},
        {QStringLiteral("qslVia"), qslVia},
        {QStringLiteral("imageUrl"), imageUrl},
        {QStringLiteral("dxcc"), dxcc},
        {QStringLiteral("cqZone"), cqZone},
        {QStringLiteral("ituZone"), ituZone},
        {QStringLiteral("lotw"), lotw},
        {QStringLiteral("eqsl"), eqsl},
        {QStringLiteral("source"), source},
    };
}

namespace callbook {

namespace {

// I due servizi rispondono con XML piatti: basta raccogliere il testo degli
// elementi dentro il blocco che interessa. I namespace si ignorano.
QHash<QString, QString> collect(const QByteArray& xml, const QString& block)
{
    QHash<QString, QString> out;
    QXmlStreamReader r(xml);
    int depth = -1;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            if (depth < 0 && r.name() == block) {
                depth = 0;
                continue;
            }
            if (depth >= 0) {
                const QString name = r.name().toString().toLower();
                const QString text = r.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
                if (!out.contains(name))
                    out.insert(name, text);
            }
        } else if (r.isEndElement() && depth >= 0 && r.name() == block) {
            break;
        }
    }
    return out;
}

QString joinNonEmpty(std::initializer_list<QString> parts, const QString& sep = QStringLiteral(" "))
{
    QStringList kept;
    for (const QString& p : parts) {
        if (!p.trimmed().isEmpty())
            kept << p.trimmed();
    }
    return kept.join(sep);
}

bool yes(const QString& v)
{
    return v.compare(QLatin1String("Y"), Qt::CaseInsensitive) == 0 || v == QLatin1String("1");
}

} // namespace

SessionResult parseQrzSession(const QByteArray& xml)
{
    const auto s = collect(xml, QStringLiteral("Session"));
    SessionResult r;
    r.key = s.value(QStringLiteral("key"));
    r.error = s.value(QStringLiteral("error"));
    // QRZ: "Session Timeout", "Invalid session key".
    r.expired = r.error.contains(QLatin1String("Session Timeout"), Qt::CaseInsensitive)
             || r.error.contains(QLatin1String("Invalid session key"), Qt::CaseInsensitive);
    if (r.key.isEmpty() && r.error.isEmpty())
        r.error = QCoreApplication::translate("Callbook", "Unexpected answer from QRZ.com");
    return r;
}

std::optional<CallbookRecord> parseQrzCallsign(const QByteArray& xml)
{
    const auto c = collect(xml, QStringLiteral("Callsign"));
    if (c.value(QStringLiteral("call")).isEmpty())
        return std::nullopt;
    CallbookRecord rec;
    rec.source = QStringLiteral("QRZ.com");
    rec.call = c.value(QStringLiteral("call")).toUpper();
    // "nickname" e' il nome con cui si fa chiamare; altrimenti nome e cognome.
    rec.name = c.value(QStringLiteral("nickname")).isEmpty()
                   ? joinNonEmpty({c.value(QStringLiteral("fname")), c.value(QStringLiteral("name"))})
                   : joinNonEmpty({c.value(QStringLiteral("nickname")), c.value(QStringLiteral("name"))});
    rec.qth = c.value(QStringLiteral("addr2"));
    rec.address = joinNonEmpty({c.value(QStringLiteral("addr1")), c.value(QStringLiteral("addr2"))}, QStringLiteral(", "));
    rec.state = c.value(QStringLiteral("state"));
    rec.county = c.value(QStringLiteral("county"));
    rec.country = c.value(QStringLiteral("country"));
    rec.grid = c.value(QStringLiteral("grid"));
    rec.iota = c.value(QStringLiteral("iota"));
    rec.email = c.value(QStringLiteral("email"));
    rec.qslVia = c.value(QStringLiteral("qslmgr"));
    rec.imageUrl = c.value(QStringLiteral("image"));
    rec.dxcc = c.value(QStringLiteral("dxcc")).toInt();
    rec.cqZone = c.value(QStringLiteral("cqzone")).toInt();
    rec.ituZone = c.value(QStringLiteral("ituzone")).toInt();
    bool okLat = false, okLon = false;
    rec.lat = c.value(QStringLiteral("lat")).toDouble(&okLat);
    rec.lon = c.value(QStringLiteral("lon")).toDouble(&okLon);
    rec.hasPosition = okLat && okLon;
    rec.lotw = yes(c.value(QStringLiteral("lotw")));
    rec.eqsl = yes(c.value(QStringLiteral("eqsl")));
    return rec;
}

SessionResult parseHamQthSession(const QByteArray& xml)
{
    const auto s = collect(xml, QStringLiteral("session"));
    SessionResult r;
    r.key = s.value(QStringLiteral("session_id"));
    r.error = s.value(QStringLiteral("error"));
    r.expired = r.error.contains(QLatin1String("Session does not exist"), Qt::CaseInsensitive)
             || r.error.contains(QLatin1String("expired"), Qt::CaseInsensitive);
    if (r.key.isEmpty() && r.error.isEmpty())
        r.error = QCoreApplication::translate("Callbook", "Unexpected answer from HamQTH");
    return r;
}

std::optional<CallbookRecord> parseHamQthSearch(const QByteArray& xml)
{
    const auto c = collect(xml, QStringLiteral("search"));
    if (c.value(QStringLiteral("callsign")).isEmpty())
        return std::nullopt;
    CallbookRecord rec;
    rec.source = QStringLiteral("HamQTH");
    rec.call = c.value(QStringLiteral("callsign")).toUpper();
    rec.name = c.value(QStringLiteral("nick")).isEmpty() ? c.value(QStringLiteral("adr_name"))
                                                         : c.value(QStringLiteral("nick"));
    rec.qth = c.value(QStringLiteral("qth")).isEmpty() ? c.value(QStringLiteral("adr_city"))
                                                       : c.value(QStringLiteral("qth"));
    rec.address = joinNonEmpty({c.value(QStringLiteral("adr_street1")), c.value(QStringLiteral("adr_city"))},
                               QStringLiteral(", "));
    rec.state = c.value(QStringLiteral("us_state"));
    rec.county = c.value(QStringLiteral("us_county"));
    rec.country = c.value(QStringLiteral("country"));
    rec.grid = c.value(QStringLiteral("grid"));
    rec.iota = c.value(QStringLiteral("iota"));
    rec.email = c.value(QStringLiteral("email"));
    rec.qslVia = c.value(QStringLiteral("qsl_via"));
    rec.imageUrl = c.value(QStringLiteral("picture"));
    rec.dxcc = c.value(QStringLiteral("adif")).toInt();
    rec.cqZone = c.value(QStringLiteral("cq")).toInt();
    rec.ituZone = c.value(QStringLiteral("itu")).toInt();
    bool okLat = false, okLon = false;
    rec.lat = c.value(QStringLiteral("latitude")).toDouble(&okLat);
    rec.lon = c.value(QStringLiteral("longitude")).toDouble(&okLon);
    rec.hasPosition = okLat && okLon;
    rec.lotw = yes(c.value(QStringLiteral("lotw")));
    rec.eqsl = yes(c.value(QStringLiteral("eqsl")));
    return rec;
}

} // namespace callbook

CallbookClient::CallbookClient(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

QString CallbookClient::providerId(Provider p)
{
    switch (p) {
    case Provider::Qrz:    return QStringLiteral("qrz");
    case Provider::HamQth: return QStringLiteral("hamqth");
    default:               return QStringLiteral("off");
    }
}

CallbookClient::Provider CallbookClient::providerFromId(const QString& id)
{
    if (id == QLatin1String("qrz"))    return Provider::Qrz;
    if (id == QLatin1String("hamqth")) return Provider::HamQth;
    return Provider::None;
}

void CallbookClient::setProvider(Provider provider)
{
    if (provider == m_provider)
        return;
    m_provider = provider;
    reset();
}

void CallbookClient::setCredentialReaders(AccountReader accounts, SecretReader secrets)
{
    m_accounts = std::move(accounts);
    m_secrets = std::move(secrets);
}

void CallbookClient::setEndpoints(const QUrl& qrz, const QUrl& hamqth)
{
    m_qrzUrl = qrz;
    m_hamqthUrl = hamqth;
    reset();
}

void CallbookClient::reset()
{
    m_sessionKey.clear();
    m_cache.clear();
    m_notFound.clear();
}

QString CallbookClient::serviceId() const
{
    return providerId(m_provider);
}

QString CallbookClient::sourceName() const
{
    return m_provider == Provider::Qrz ? QStringLiteral("QRZ.com") : QStringLiteral("HamQTH");
}

void CallbookClient::lookup(const QString& callsign)
{
    const QString call = callsign.trimmed().toUpper();
    if (m_provider == Provider::None || call.size() < 3)
        return;

    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (const auto it = m_cache.constFind(call); it != m_cache.constEnd() && it->first.secsTo(now) < 86400) {
        emit found(call, it->second);
        return;
    }
    // Un nominativo che non c'era non si richiede di nuovo per un'ora.
    if (const auto it = m_notFound.constFind(call); it != m_notFound.constEnd() && it->secsTo(now) < 3600) {
        emit failed(call, tr("%1 not found on %2").arg(call, sourceName()));
        return;
    }

    if (m_sessionKey.isEmpty()) {
        login([this, call](const QString& error) {
            if (error.isEmpty())
                query(call, false);
            else
                emit failed(call, error);
        });
    } else {
        query(call, false);
    }
}

void CallbookClient::login(std::function<void(const QString& error)> done)
{
    const QString service = serviceId();
    const QString user = m_accounts ? m_accounts(service) : QString();
    if (user.isEmpty() || !m_secrets) {
        done(tr("%1: no credentials. Add them in Setup → Callbook.").arg(sourceName()));
        return;
    }

    m_secrets(service, [this, user, done](const QString& secret, const QString& error) {
        if (!error.isEmpty() || secret.isEmpty()) {
            done(tr("%1: password not available (%2)").arg(sourceName(), error));
            return;
        }
        QUrl url = m_provider == Provider::Qrz ? m_qrzUrl : m_hamqthUrl;
        QUrlQuery q;
        if (m_provider == Provider::Qrz) {
            q.addQueryItem(QStringLiteral("username"), user);
            q.addQueryItem(QStringLiteral("password"), secret);
            q.addQueryItem(QStringLiteral("agent"), QStringLiteral("DecoLog-") + QCoreApplication::applicationVersion());
        } else {
            q.addQueryItem(QStringLiteral("u"), user);
            q.addQueryItem(QStringLiteral("p"), secret);
        }
        // La password va nella query perche' e' cosi' che la chiedono le due API,
        // e solo verso HTTPS. Non finisce in nessun log.
        url.setQuery(q);
        QNetworkRequest request(url);
        request.setTransferTimeout(15000);
        QNetworkReply* reply = m_net->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, done] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                done(tr("%1: %2").arg(sourceName(), network::safeErrorString(reply)));
                return;
            }
            const QByteArray body = reply->readAll();
            const auto session = m_provider == Provider::Qrz ? callbook::parseQrzSession(body)
                                                             : callbook::parseHamQthSession(body);
            if (session.key.isEmpty()) {
                done(tr("%1: %2").arg(sourceName(), session.error));
                return;
            }
            m_sessionKey = session.key;
            done({});
        });
    });
}

void CallbookClient::query(const QString& call, bool retried)
{
    QUrl url = m_provider == Provider::Qrz ? m_qrzUrl : m_hamqthUrl;
    QUrlQuery q;
    if (m_provider == Provider::Qrz) {
        q.addQueryItem(QStringLiteral("s"), m_sessionKey);
        q.addQueryItem(QStringLiteral("callsign"), call);
    } else {
        q.addQueryItem(QStringLiteral("id"), m_sessionKey);
        q.addQueryItem(QStringLiteral("callsign"), call);
        q.addQueryItem(QStringLiteral("prg"), QStringLiteral("DecoLog"));
    }
    url.setQuery(q);
    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, call, retried] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit failed(call, tr("%1: %2").arg(sourceName(), network::safeErrorString(reply)));
            return;
        }
        const QByteArray body = reply->readAll();
        const auto record = m_provider == Provider::Qrz ? callbook::parseQrzCallsign(body)
                                                        : callbook::parseHamQthSearch(body);
        if (record) {
            m_cache.insert(call, {QDateTime::currentDateTimeUtc(), *record});
            emit found(call, *record);
            return;
        }
        const auto session = m_provider == Provider::Qrz ? callbook::parseQrzSession(body)
                                                         : callbook::parseHamQthSession(body);
        // Sessione scaduta: un nuovo login e un solo nuovo tentativo.
        if ((session.expired || (m_provider == Provider::Qrz && session.key.isEmpty() && !session.error.contains(QLatin1String("Not found"))))
            && !retried) {
            m_sessionKey.clear();
            login([this, call](const QString& error) {
                if (error.isEmpty())
                    query(call, true);
                else
                    emit failed(call, error);
            });
            return;
        }
        if (session.error.contains(QLatin1String("not found"), Qt::CaseInsensitive))
            m_notFound.insert(call, QDateTime::currentDateTimeUtc());
        emit failed(call, tr("%1: %2").arg(sourceName(),
                                           session.error.isEmpty() ? tr("%1 not found").arg(call) : session.error));
    });
}

} // namespace decolog::core
