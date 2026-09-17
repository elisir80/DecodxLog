// Connessione a un nodo: login, comandi dopo il login, spot ricevuti, HamAlert con
// password e JSON, contro un server telnet finto.
#include "core/ClusterConnection.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace decolog::core;

namespace {

// Un nodo finto: saluta con un prompt e registra quello che riceve.
class FakeNode : public QTcpServer {
public:
    QByteArray greeting{"Welcome to FAKE-1\r\nlogin: "};
    QByteArray received;
    QTcpSocket* client{nullptr};

    FakeNode()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            client = nextPendingConnection();
            connect(client, &QTcpSocket::readyRead, client, [this] { received += client->readAll(); });
            client->write(greeting);
        });
        listen(QHostAddress::LocalHost);
    }
    void say(const QByteArray& data) { client->write(data); }
    bool waitFor(const QByteArray& text, int ms = 5000)
    {
        return QTest::qWaitFor([&] { return received.contains(text); }, ms);
    }
};

ClusterSource sourceFor(const FakeNode& node, const char* type, const char* login, const char* commands)
{
    ClusterSource s;
    s.id = "test";
    s.name = "Fake";
    s.type = QLatin1String(type);
    s.host = "127.0.0.1";
    s.port = node.serverPort();
    s.login = QLatin1String(login);
    s.commands = QLatin1String(commands);
    return s;
}

} // namespace

class TestCluster : public QObject {
    Q_OBJECT

private slots:
    void loginAndSpots()
    {
        FakeNode node;
        ClusterConnection c(sourceFor(node, "cluster", "", "set/ft8\nsh/dx 5"));
        c.setDefaultLogin("iu8lmc");
        QSignalSpy spots(&c, &ClusterConnection::spotReceived);
        QSignalSpy lines(&c, &ClusterConnection::lineReceived);
        c.start();

        // Nominativo del profilo con SSID: non butta fuori Decodium dallo stesso nodo.
        QVERIFY(node.waitFor("IU8LMC-2\r\n"));
        node.say("Hello IU8LMC, this is FAKE-1\r\nIU8LMC-2 de FAKE-1 17-Sep-2026 1240Z >\r\n");
        QVERIFY(node.waitFor("sh/dx 5\r\n"));
        QVERIFY(node.received.contains("set/ft8\r\n"));
        QTRY_COMPARE(c.state(), ClusterConnection::State::Online);

        node.say(" DX de IK8XXX:     14074.0  JA1YYY       FT8 -12 dB              1238Z\r\n"
                 "   21074.0 II7IAME     17-Sep-2026 1231Z FT8 Italian Navy Ship        <IU7EDX>\r\n"
                 "WWV de W0MU <18>:   SFI=150, A=5, K=1, No Storms\r\n");
        QTRY_COMPARE(spots.size(), 2);
        const Spot s = spots.at(0).at(0).value<Spot>();
        QCOMPARE(s.dxCall, QString("JA1YYY"));
        QCOMPARE(s.source, QString("cluster"));
        QCOMPARE(s.sourceName, QString("Fake"));
        QTRY_VERIFY(std::any_of(lines.cbegin(), lines.cend(),
                                [](const QList<QVariant>& a) { return a.at(0).toString().startsWith("WWV"); }));
        QVERIFY(c.send("sh/wwv"));
        QVERIFY(node.waitFor("sh/wwv\r\n"));
        c.stop();
        QCOMPARE(c.state(), ClusterConnection::State::Off);
    }

    void rbnSpotsAreMarked()
    {
        FakeNode node;
        node.greeting = "Please enter your call: ";
        ClusterConnection c(sourceFor(node, "rbn", "IU8LMC", ""));
        QSignalSpy spots(&c, &ClusterConnection::spotReceived);
        c.start();
        QVERIFY(node.waitFor("IU8LMC\r\n"));
        node.say("DX de EA5WU-#:    14025.1  K1ABC          CW    18 dB  24 WPM  CQ      1239Z\r\n");
        QTRY_COMPARE(spots.size(), 1);
        QCOMPARE(spots.at(0).at(0).value<Spot>().source, QString("rbn"));
    }

    void hamAlert()
    {
        FakeNode node;
        ClusterConnection c(sourceFor(node, "hamalert", "myuser", "set/json"));
        QString askedService;
        c.setSecretReader([&askedService](const QString& service, std::function<void(const QString&, const QString&)> done) {
            askedService = service;
            done("pa55word", {});
        });
        QSignalSpy spots(&c, &ClusterConnection::spotReceived);
        c.start();
        QVERIFY(node.waitFor("myuser\r\n"));
        node.say("password: ");
        QVERIFY(node.waitFor("pa55word\r\n"));
        QCOMPARE(askedService, QString("hamalert"));
        node.say("Hello myuser, this is HamAlert\r\n");
        QVERIFY(node.waitFor("set/json\r\n"));
        node.say(R"({"fullCallsign":"3Y0J","frequency":"14.0245","mode":"cw","time":"12:31","spotter":"DL1ABC","source":"cluster"})" "\r\n");
        QTRY_COMPARE(spots.size(), 1);
        QCOMPARE(spots.at(0).at(0).value<Spot>().dxCall, QString("3Y0J"));
    }

    void reconnectsAfterDrop()
    {
        FakeNode node;
        ClusterConnection c(sourceFor(node, "cluster", "IU8LMC", ""));
        c.start();
        QVERIFY(node.waitFor("IU8LMC\r\n"));
        node.client->disconnectFromHost();
        QTRY_COMPARE(c.state(), ClusterConnection::State::Waiting);
        node.received.clear();
        // Primo nuovo tentativo dopo 5 secondi.
        QVERIFY(node.waitFor("IU8LMC\r\n", 8000));
    }
};

QTEST_GUILESS_MAIN(TestCluster)
#include "tst_cluster.moc"
