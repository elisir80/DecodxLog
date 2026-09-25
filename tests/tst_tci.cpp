// La radio via TCI: un finto server TCI (un WebSocket che parla come
// ExpertSDR) manda lo stato e "ready;", e raccoglie i comandi che arrivano.
#include "core/TciControl.h"

#include <QSignalSpy>
#include <QTest>
#include <QWebSocket>
#include <QWebSocketServer>

using namespace decolog::core;

namespace {

class FakeTci : public QObject {
public:
    QWebSocketServer server{QStringLiteral("fake tci"), QWebSocketServer::NonSecureMode};
    QWebSocket* client{nullptr};
    QStringList received;
    // Quello che la radio dice appena ci si collega, come ExpertSDR.
    QString greeting{QStringLiteral(
        "protocol:ExpertSDR3,1.9;device:SunSDR2PRO;receive_only:false;trx_count:2;"
        "vfo:0,0,7074000;vfo:0,1,7076000;vfo:1,0,14074000;modulation:0,digu;modulation:1,cw;"
        "cw_macros_speed:22;ready;")};

    FakeTci()
    {
        server.listen(QHostAddress::LocalHost);
        connect(&server, &QWebSocketServer::newConnection, this, [this] {
            client = server.nextPendingConnection();
            connect(client, &QWebSocket::textMessageReceived, this, [this](const QString& text) {
                received << text;
            });
            client->sendTextMessage(greeting);
        });
    }
    QString address() const { return QStringLiteral("127.0.0.1:%1").arg(server.serverPort()); }
    void say(const QString& text) { if (client) client->sendTextMessage(text); }
};

bool waitFor(const std::function<bool()>& ok)
{
    for (int i = 0; i < 100 && !ok(); ++i)
        QTest::qWait(30);
    return ok();
}

} // namespace

class TestTci : public QObject {
    Q_OBJECT

private slots:
    void addressesLikeDecodium()
    {
        QCOMPARE(TciControl::urlFor("127.0.0.1:40001").toString(), QString("ws://127.0.0.1:40001"));
        QCOMPARE(TciControl::urlFor("sdr.local").toString(), QString("ws://sdr.local:40001"));
        QCOMPARE(TciControl::urlFor("http://10.0.0.5:50001").toString(), QString("ws://10.0.0.5:50001"));
        QCOMPARE(TciControl::urlFor("").toString(), QString("ws://127.0.0.1:40001"));
    }

    void modesBothWays()
    {
        QCOMPARE(TciControl::tciModulation("PKTUSB"), QString("digu"));
        QCOMPARE(TciControl::tciModulation("CW"), QString("cw"));
        QCOMPARE(TciControl::tciModulation("LSB"), QString("lsb"));
        QCOMPARE(TciControl::tciModulation("FM"), QString("nfm"));
        QCOMPARE(TciControl::tciModulation("RTTY"), QString("digl"));
        QCOMPARE(TciControl::hamlibMode("digu"), QString("PKTUSB"));
        QCOMPARE(TciControl::hamlibMode("USB"), QString("USB"));
        QCOMPARE(TciControl::hamlibMode("nfm"), QString("FM"));
    }

    void readsTheRadioAndCommandsIt()
    {
        FakeTci tci;
        TciControl rig;
        rig.connectTo(tci.address(), 0);
        QVERIFY(waitFor([&] { return rig.frequencyHz() == 7074000; }));
        QCOMPARE(rig.mode(), QString("PKTUSB"));
        QCOMPARE(rig.speedWpm(), 22);
        QCOMPARE(rig.device(), QString("SunSDR2PRO"));
        QVERIFY(rig.status().contains("SunSDR2PRO"));

        // La manopola: la radio lo dice da sola.
        QSignalSpy changed(&rig, &RigLink::changed);
        tci.say("vfo:0,0,7030500;modulation:0,cw;");
        QVERIFY(waitFor([&] { return rig.frequencyHz() == 7030500; }));
        QCOMPARE(rig.mode(), QString("CW"));
        QVERIFY(changed.count() > 0);
        // Il VFO B e l'altro ricevitore non sono il nostro.
        tci.say("vfo:0,1,7000000;vfo:1,0,21000000;");
        QTest::qWait(150);
        QCOMPARE(rig.frequencyHz(), 7030500);

        rig.setFrequency(14025000);
        rig.setMode("CW");
        rig.setPtt(true);
        rig.setPtt(false);
        rig.setSpeedWpm(28);
        QSignalSpy sent(&rig, &RigLink::morseSent);
        rig.sendMorse("cq test; de iu8lmc");
        rig.stopMorse();
        QVERIFY(waitFor([&] { return tci.received.contains("cw_macros_stop;"); }));
        QVERIFY(tci.received.contains("vfo:0,0,14025000;"));
        QVERIFY(tci.received.contains("modulation:0,cw;"));
        QVERIFY(tci.received.contains("trx:0,true;"));
        QVERIFY(tci.received.contains("trx:0,false;"));
        QVERIFY(tci.received.contains("cw_macros_speed:28;"));
        QVERIFY(tci.received.contains("cw_macros:0,CQ TEST DE IU8LMC;"));
        QCOMPARE(sent.count(), 1);
    }

    void theSecondReceiver()
    {
        FakeTci tci;
        TciControl rig;
        rig.connectTo(tci.address(), 1);
        QVERIFY(waitFor([&] { return rig.frequencyHz() == 14074000; }));
        QCOMPARE(rig.mode(), QString("CW"));
        rig.setFrequency(14030000);
        QVERIFY(waitFor([&] { return tci.received.contains("vfo:1,0,14030000;"); }));
    }

    void aServerWithoutReadyStillWorks()
    {
        // Qualche server TCI non manda "ready;": dopo due secondi si parte lo stesso.
        FakeTci tci;
        tci.greeting = QStringLiteral("vfo:0,0,3573000;modulation:0,digu;");
        TciControl rig;
        rig.connectTo(tci.address(), 0);
        QVERIFY(waitFor([&] { return rig.frequencyHz() == 3573000; }));
        rig.setFrequency(3575000);
        QTRY_VERIFY_WITH_TIMEOUT(tci.received.contains("vfo:0,0,3575000;"), 5000);
    }

    void noRadioNoCw()
    {
        TciControl rig;
        QSignalSpy failed(&rig, &RigLink::failed);
        rig.sendMorse("TEST");
        QCOMPARE(failed.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestTci)
#include "tst_tci.moc"
