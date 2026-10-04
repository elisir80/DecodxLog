// Il controllo di un nodo: il nome, la porta, la prima parola. Contro un server
// finto che parla, tace, chiude subito, o non c'e'.
#include "core/ConnectionProbe.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace decolog::core;

namespace {

class FakeNode : public QTcpServer {
public:
    QByteArray greeting;
    bool closeAtOnce{false};
    QList<QTcpSocket*> clients;

    FakeNode()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* c = nextPendingConnection();
            clients << c;
            if (closeAtOnce) {
                c->disconnectFromHost();
                return;
            }
            if (!greeting.isEmpty())
                c->write(greeting);
        });
        listen(QHostAddress::LocalHost);
    }
};

ConnectionProbe::Result probe(const QString& host, quint16 port, int connectMs = 3000, int bannerMs = 600)
{
    ConnectionProbe p;
    p.setTimeouts(connectMs, bannerMs);
    QSignalSpy done(&p, &ConnectionProbe::finished);
    p.start(host, port);
    if (!done.wait(15000))
        return {};
    return done.at(0).at(0).value<ConnectionProbe::Result>();
}

} // namespace

class TestConnectionProbe : public QObject {
    Q_OBJECT

private slots:
    // Un nodo che saluta: tutto a posto, e si vede cosa ha detto (senza la
    // negoziazione telnet che apre quasi tutti i nodi).
    void aNodeThatGreetsIsOk()
    {
        FakeNode node;
        node.greeting = QByteArray("\xff\xfd\x18\xff\xfb\x50", 6) + "Welcome to FAKE-1\r\nlogin: ";
        const auto r = probe(QStringLiteral("127.0.0.1"), node.serverPort());
        QCOMPARE(r.verdict, ConnectionProbe::Verdict::Ok);
        QCOMPARE(r.banner, QStringLiteral("Welcome to FAKE-1"));
        QCOMPARE(r.address, QStringLiteral("127.0.0.1"));
        QVERIFY(r.hint.isEmpty());
        QCOMPARE(r.steps.size(), 3);
        QVERIFY2(r.text().contains(QStringLiteral("Welcome to FAKE-1")), qPrintable(r.text()));
    }

    // Collegato e chiuso senza una parola: il segno tipico di un antivirus.
    void closedAtOnceNamesTheAntivirus()
    {
        FakeNode node;
        node.closeAtOnce = true;
        const auto r = probe(QStringLiteral("127.0.0.1"), node.serverPort());
        QCOMPARE(r.verdict, ConnectionProbe::Verdict::ClosedAtOnce);
        QVERIFY(r.hint.contains(QStringLiteral("Avast")));
        QVERIFY(r.hint.contains(QStringLiteral("AVG")));
    }

    // Collegato e muto: lo stesso, e non si resta li' ad aspettare.
    void aSilentNodeIsReported()
    {
        FakeNode node;
        QElapsedTimer clock;
        clock.start();
        const auto r = probe(QStringLiteral("127.0.0.1"), node.serverPort(), 3000, 400);
        QCOMPARE(r.verdict, ConnectionProbe::Verdict::Silent);
        QVERIFY(!r.hint.isEmpty());
        QVERIFY2(clock.elapsed() < 3000, "ha aspettato piu' del tempo del nodo");
    }

    // Una porta che nessuno ascolta.
    void aClosedPortIsRefused()
    {
        quint16 port = 0;
        {
            QTcpServer s;
            QVERIFY(s.listen(QHostAddress::LocalHost));
            port = s.serverPort();
        }
        const auto r = probe(QStringLiteral("127.0.0.1"), port);
        QVERIFY2(r.verdict == ConnectionProbe::Verdict::Refused || r.verdict == ConnectionProbe::Verdict::TimedOut,
                 qPrintable(r.text()));
        QVERIFY(!r.hint.isEmpty());
    }

    // Un nome che non esiste si ferma al primo passo, senza toccare la rete.
    void aMissingNameStopsAtTheFirstStep()
    {
        const auto r = probe(QStringLiteral("nonexistent.invalid"), 7300);
        QCOMPARE(r.verdict, ConnectionProbe::Verdict::DnsFailed);
        QCOMPARE(r.steps.size(), 1);
        QVERIFY(!r.hint.isEmpty());
    }

    // Un nome che e' gia' un indirizzo non si cerca.
    void anAddressNeedsNoLookup()
    {
        FakeNode node;
        node.greeting = "login: ";
        const auto r = probe(QStringLiteral(" 127.0.0.1 "), node.serverPort());
        QCOMPARE(r.verdict, ConnectionProbe::Verdict::Ok);
        QVERIFY(r.steps.first().contains(QStringLiteral("127.0.0.1")));
    }

    // Due controlli insieme sullo stesso oggetto: il secondo non parte.
    void oneCheckAtATime()
    {
        FakeNode node;
        node.greeting = "login: ";
        ConnectionProbe p;
        QSignalSpy done(&p, &ConnectionProbe::finished);
        p.start(QStringLiteral("127.0.0.1"), node.serverPort());
        QVERIFY(p.busy());
        p.start(QStringLiteral("127.0.0.1"), node.serverPort());
        QVERIFY(done.wait(5000));
        QTest::qWait(200);
        QCOMPARE(done.size(), 1);
        QVERIFY(!p.busy());
    }
};

QTEST_MAIN(TestConnectionProbe)
#include "tst_connectionprobe.moc"
