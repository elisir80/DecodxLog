// Callbook: lettura delle risposte XML di QRZ.com e HamQTH e il giro completo
// login → ricerca → sessione scaduta → nuovo login, contro un server HTTP finto.
#include "core/Callbook.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QUrlQuery>

using namespace decolog::core;

namespace {

const QByteArray kQrzLogin =
    R"(<?xml version="1.0" ?>
<QRZDatabase version="1.34" xmlns="http://xmldata.qrz.com">
  <Session><Key>2331uf894c4bd29f3923f3bacf02c532d7bd9</Key><Count>123</Count>
  <SubExp>Wed Jan 1 12:34:03 2027</SubExp><GMTime>Sun Aug 16 03:51:47 2026</GMTime></Session>
</QRZDatabase>)";

const QByteArray kQrzCallsign =
    R"(<?xml version="1.0" ?>
<QRZDatabase version="1.34" xmlns="http://xmldata.qrz.com">
  <Callsign>
    <call>EA8OH</call><fname>Pekka</fname><name>Virtanen</name><addr1>Calle Mayor 1</addr1>
    <addr2>Las Palmas</addr2><country>Canary Islands</country><lat>28.12</lat><lon>-15.43</lon>
    <grid>IL18QI</grid><dxcc>29</dxcc><cqzone>33</cqzone><ituzone>36</ituzone>
    <email>ea8oh@example.org</email><qslmgr>LoTW</qslmgr><lotw>1</lotw><eqsl>0</eqsl>
    <image>https://cdn-xml.qrz.com/x/ea8oh/photo.jpg</image>
  </Callsign>
  <Session><Key>2331uf894c4bd29f3923f3bacf02c532d7bd9</Key></Session>
</QRZDatabase>)";

const QByteArray kQrzNotFound =
    R"(<QRZDatabase version="1.34"><Session><Error>Not found: XX9XX</Error><Key>abc</Key></Session></QRZDatabase>)";
const QByteArray kQrzTimeout =
    R"(<QRZDatabase version="1.34"><Session><Error>Session Timeout</Error></Session></QRZDatabase>)";
const QByteArray kQrzBadPassword =
    R"(<QRZDatabase version="1.34"><Session><Error>Username/password incorrect</Error></Session></QRZDatabase>)";

const QByteArray kHamQthLogin =
    R"(<?xml version="1.0"?><HamQTH version="2.8" xmlns="https://www.hamqth.com"><session>
<session_id>09b0ae90050be03c452ad235a1f2915ad684393c</session_id></session></HamQTH>)";

const QByteArray kHamQthSearch =
    R"(<?xml version="1.0"?><HamQTH version="2.8" xmlns="https://www.hamqth.com"><search>
<callsign>ok7an</callsign><nick>Petr</nick><qth>Neratovice</qth><country>Czech Republic</country>
<adif>503</adif><itu>28</itu><cq>15</cq><grid>jo70gg</grid><adr_name>Petr Hlozek</adr_name>
<adr_city>Neratovice</adr_city><lotw>Y</lotw><eqsl>N</eqsl><latitude>50.2</latitude><longitude>14.5</longitude>
<picture>https://www.hamqth.com/userfiles/o/ok/ok7an/_profile/ok7an.jpg</picture></search></HamQTH>)";

// Un server HTTP minimo: una risposta decisa da una funzione della query.
class FakeHttp : public QTcpServer {
public:
    std::function<QByteArray(const QUrlQuery&)> respond;
    QList<QUrlQuery> requests;

    FakeHttp()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* s = nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, s, [this, s] {
                    const QByteArray head = s->readAll();
                    const QByteArray line = head.left(head.indexOf("\r\n"));
                    const QUrl url(QString::fromLatin1(line.split(' ').value(1)));
                    const QUrlQuery q(url);
                    requests << q;
                    const QByteArray body = respond(q);
                    s->write("HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\nConnection: close\r\nContent-Length: "
                             + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    s->disconnectFromHost();
                });
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });
        listen(QHostAddress::LocalHost);
    }
    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/xml").arg(serverPort())); }
};

void giveCredentials(CallbookClient& c, const QString& password)
{
    c.setCredentialReaders(
        [](const QString&) { return QStringLiteral("IU8LMC"); },
        [password](const QString&, std::function<void(const QString&, const QString&)> done) { done(password, {}); });
}

} // namespace

class TestCallbook : public QObject {
    Q_OBJECT

private slots:
    void parseQrz()
    {
        const auto session = callbook::parseQrzSession(kQrzLogin);
        QCOMPARE(session.key, QString("2331uf894c4bd29f3923f3bacf02c532d7bd9"));
        QVERIFY(session.error.isEmpty());

        const auto rec = callbook::parseQrzCallsign(kQrzCallsign);
        QVERIFY(rec);
        QCOMPARE(rec->call, QString("EA8OH"));
        QCOMPARE(rec->name, QString("Pekka Virtanen"));
        QCOMPARE(rec->qth, QString("Las Palmas"));
        QCOMPARE(rec->grid, QString("IL18QI"));
        QCOMPARE(rec->dxcc, 29);
        QCOMPARE(rec->cqZone, 33);
        QVERIFY(rec->lotw);
        QVERIFY(!rec->eqsl);
        QVERIFY(rec->hasPosition);
        QCOMPARE(rec->source, QString("QRZ.com"));

        QVERIFY(!callbook::parseQrzCallsign(kQrzNotFound));
        QVERIFY(callbook::parseQrzSession(kQrzTimeout).expired);
        QVERIFY(!callbook::parseQrzSession(kQrzBadPassword).expired);
        QVERIFY(callbook::parseQrzSession(kQrzBadPassword).key.isEmpty());
    }

    void parseHamQth()
    {
        QCOMPARE(callbook::parseHamQthSession(kHamQthLogin).key, QString("09b0ae90050be03c452ad235a1f2915ad684393c"));
        const auto rec = callbook::parseHamQthSearch(kHamQthSearch);
        QVERIFY(rec);
        QCOMPARE(rec->call, QString("OK7AN"));
        QCOMPARE(rec->name, QString("Petr"));
        QCOMPARE(rec->qth, QString("Neratovice"));
        QCOMPARE(rec->dxcc, 503);
        QVERIFY(rec->lotw);
        QVERIFY(rec->imageUrl.startsWith("https://"));
    }

    void qrzLoginLookupCacheAndExpiredSession()
    {
        FakeHttp server;
        int lookups = 0;
        server.respond = [&](const QUrlQuery& q) -> QByteArray {
            if (q.hasQueryItem("username"))
                return q.queryItemValue("password") == "right" ? kQrzLogin : kQrzBadPassword;
            ++lookups;
            if (lookups == 2)
                return kQrzTimeout;     // la seconda ricerca trova la sessione scaduta
            return q.queryItemValue("callsign") == "XX9XX" ? kQrzNotFound : kQrzCallsign;
        };

        CallbookClient client;
        client.setEndpoints(server.url(), server.url());
        client.setProvider(CallbookClient::Provider::Qrz);
        giveCredentials(client, "right");
        QSignalSpy found(&client, &CallbookClient::found);
        QSignalSpy failed(&client, &CallbookClient::failed);

        client.lookup("ea8oh");
        QVERIFY(found.wait(5000));
        QCOMPARE(found.first().at(0).toString(), QString("EA8OH"));
        QCOMPARE(server.requests.size(), 2);                         // login + ricerca
        QCOMPARE(server.requests.at(0).queryItemValue("username"), QString("IU8LMC"));
        QVERIFY(server.requests.at(0).queryItemValue("agent").startsWith("DecoLog"));

        // Dalla cache: nessuna richiesta in piu', risposta immediata.
        client.lookup("EA8OH");
        QCOMPARE(found.size(), 2);
        QCOMPARE(server.requests.size(), 2);

        // Sessione scaduta: nuovo login e secondo tentativo, trasparenti.
        client.lookup("K1ABC");
        QTRY_COMPARE_WITH_TIMEOUT(found.size(), 3, 5000);
        QCOMPARE(server.requests.size(), 5);                         // ricerca, login, ricerca
        QVERIFY(server.requests.at(3).hasQueryItem("username"));

        client.lookup("XX9XX");
        QVERIFY(failed.wait(5000));
        QVERIFY(failed.first().at(1).toString().contains("Not found"));
        // Il "non trovato" non si richiede subito di nuovo.
        const qsizetype before = server.requests.size();
        client.lookup("XX9XX");
        QCOMPARE(failed.size(), 2);
        QCOMPARE(server.requests.size(), before);
    }

    void wrongPasswordAndMissingCredentials()
    {
        FakeHttp server;
        server.respond = [](const QUrlQuery&) { return kQrzBadPassword; };
        CallbookClient client;
        client.setEndpoints(server.url(), server.url());
        client.setProvider(CallbookClient::Provider::Qrz);
        giveCredentials(client, "wrong");
        QSignalSpy failed(&client, &CallbookClient::failed);
        client.lookup("EA8OH");
        QVERIFY(failed.wait(5000));
        QVERIFY(failed.first().at(1).toString().contains("incorrect"));

        CallbookClient noCredentials;
        noCredentials.setProvider(CallbookClient::Provider::HamQth);
        QSignalSpy failed2(&noCredentials, &CallbookClient::failed);
        noCredentials.lookup("OK7AN");
        QCOMPARE(failed2.size(), 1);
        QVERIFY(failed2.first().at(1).toString().contains("no credentials"));
    }

    void hamQthFlow()
    {
        FakeHttp server;
        server.respond = [](const QUrlQuery& q) { return q.hasQueryItem("u") ? kHamQthLogin : kHamQthSearch; };
        CallbookClient client;
        client.setEndpoints(server.url(), server.url());
        client.setProvider(CallbookClient::Provider::HamQth);
        giveCredentials(client, "pw");
        QSignalSpy found(&client, &CallbookClient::found);
        client.lookup("OK7AN");
        QVERIFY(found.wait(5000));
        QCOMPARE(server.requests.at(1).queryItemValue("id"), QString("09b0ae90050be03c452ad235a1f2915ad684393c"));
        QCOMPARE(server.requests.at(1).queryItemValue("prg"), QString("DecoLog"));
    }
};

QTEST_GUILESS_MAIN(TestCallbook)
#include "tst_callbook.moc"
