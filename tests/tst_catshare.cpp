// La CAT condivisa: il dialogo di rigctld come in Decodium 4.
#include "core/CatShare.h"
#include "core/RigLink.h"

#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>

using namespace decolog::core;

class FakeRig : public RigLink {
public:
    qint64 hz{14074000};
    QString m{"PKTUSB"};
    bool ptt{false};
    void disconnectFromRig() override {}
    bool connected() const override { return true; }
    qint64 frequencyHz() const override { return hz; }
    QString mode() const override { return m; }
    int speedWpm() const override { return 24; }
    QString status() const override { return {}; }
    void refresh() override {}
    void setFrequency(qint64 h) override { hz = h; }
    void setMode(const QString& mode) override { m = mode; }
    void setPtt(bool on) override { ptt = on; }
    void setSpeedWpm(int) override {}
    void sendMorse(const QString&) override {}
    void stopMorse() override {}
};

class TestCatShare : public QObject {
    Q_OBJECT

private slots:
    void protocol()
    {
        FakeRig rig;
        CatShare share;
        share.setRigProvider([&rig]() -> RigLink* { return &rig; });
        QCOMPARE(share.handleLine("\\get_powerstat"), QString("1\n"));
        QCOMPARE(share.handleLine("\\chk_vfo"), QString("0\n"));       // non "CHKVFO 0"
        QVERIFY(share.handleLine("\\dump_state").endsWith("done\n"));
        QVERIFY(share.handleLine("\\dump_state").startsWith("1\n1\n2\n"));
        QCOMPARE(share.handleLine("f"), QString("14074000\n"));
        QCOMPARE(share.handleLine("+\\get_freq"), QString("14074000\n"));
        QCOMPARE(share.handleLine("m"), QString("PKTUSB\n3000\n"));
        QCOMPARE(share.handleLine("v"), QString("VFOA\n"));
        QCOMPARE(share.handleLine("V VFOA"), QString("RPRT 0\n"));
        // Di serie si legge soltanto.
        QCOMPARE(share.handleLine("F 7074000"), QString("RPRT -1\n"));
        QCOMPARE(rig.hz, qint64(14074000));
        QVERIFY(share.handleLine("q").isNull());
        QCOMPARE(share.handleLine("\\foo"), QString("RPRT -11\n"));
    }

    void controlAndPtt()
    {
        FakeRig rig;
        CatShare share;
        share.setRigProvider([&rig]() -> RigLink* { return &rig; });
        share.configure(false, 4533, true, false);
        QCOMPARE(share.handleLine("F 7074000"), QString("RPRT 0\n"));
        QCOMPARE(rig.hz, qint64(7074000));
        QCOMPARE(share.handleLine("M USB 2400"), QString("RPRT 0\n"));
        QCOMPARE(rig.m, QString("USB"));
        // La trasmissione ha un interruttore suo.
        QCOMPARE(share.handleLine("T 1"), QString("RPRT -1\n"));
        QVERIFY(!rig.ptt);
        share.configure(false, 4533, true, true);
        QCOMPARE(share.handleLine("T 1"), QString("RPRT 0\n"));
        QVERIFY(rig.ptt);
        QCOMPARE(share.handleLine("t"), QString("1\n"));
        // Senza controllo, il PTT non passa neanche se chiesto.
        share.configure(false, 4533, false, true);
        QVERIFY(!share.allowPtt());
    }

    void overTcp()
    {
        FakeRig rig;
        CatShare share;
        share.setRigProvider([&rig]() -> RigLink* { return &rig; });
        QTcpSocket probe;
        // Una porta libera alta.
        int port = 45330;
        QVERIFY(share.configure(true, port, false, false));
        QVERIFY(share.listening());
        QTcpSocket c;
        c.connectToHost("127.0.0.1", static_cast<quint16>(port));
        QVERIFY(c.waitForConnected(3000));
        c.write("\\chk_vfo\nf\n");
        QByteArray got;
        QTRY_VERIFY_WITH_TIMEOUT((got += c.readAll()).count('\n') >= 2, 3000);
        QCOMPARE(got, QByteArray("0\n14074000\n"));
        QTRY_COMPARE(share.clientCount(), 1);
        // Una seconda condivisione sulla stessa porta non parte, e dice perche'.
        CatShare second;
        QVERIFY(!second.configure(true, port, false, false));
        QVERIFY(!second.lastError().isEmpty());
        share.configure(false, port, false, false);
        QVERIFY(!share.listening());
    }
};

QTEST_GUILESS_MAIN(TestCatShare)
#include "tst_catshare.moc"
