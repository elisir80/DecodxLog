// La rete multi-operatore: messaggi impacchettati e ricevuti.
#include "core/ContestNet.h"

#include <QSignalSpy>
#include <QTest>
#include <QUdpSocket>

using namespace decolog::core;

class TestContestNet : public QObject {
    Q_OBJECT

private slots:
    void packUnpack()
    {
        const QByteArray d = ContestNet::pack("FIELDDAY", "pc1", "gab", QJsonObject{{"text", "QRV 40m"}});
        QString from, type;
        QJsonObject body;
        QVERIFY(ContestNet::unpack(d, "fieldday", &from, &type, &body));
        QCOMPARE(from, QString("pc1"));
        QCOMPARE(type, QString("gab"));
        QCOMPARE(body.value("text").toString(), QString("QRV 40m"));
        QVERIFY(!body.contains("_net"));
        // Un'altra rete sulla stessa LAN non si sente.
        QVERIFY(!ContestNet::unpack(d, "OTHER", &from, &type, &body));
        QVERIFY(!ContestNet::unpack("hello world", "FIELDDAY", &from, &type, &body));
    }

    void receiveFromAnotherPc()
    {
        ContestNet net;
        // Una porta libera.
        QUdpSocket probe;
        QVERIFY(probe.bind(QHostAddress::LocalHost, 0));
        const quint16 port = probe.localPort();
        probe.close();
        QVERIFY(net.start(port, "M2", "me"));
        QSignalSpy spy(&net, &ContestNet::received);
        QUdpSocket other;
        other.writeDatagram(ContestNet::pack("M2", "run2", "qso", QJsonObject{{"adif", "<CALL:4>W1AW<EOR>"}}),
                            QHostAddress::LocalHost, port);
        // Il proprio messaggio, tornato indietro, si ignora.
        other.writeDatagram(ContestNet::pack("M2", "me", "gab", QJsonObject{{"text", "echo"}}), QHostAddress::LocalHost, port);
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QString("run2"));
        QCOMPARE(spy.first().at(1).toString(), QString("qso"));
        QTest::qWait(200);
        QCOMPARE(spy.count(), 1);
        net.stop();
        QVERIFY(!net.running());
    }
};

QTEST_GUILESS_MAIN(TestContestNet)
#include "tst_contestnet.moc"
