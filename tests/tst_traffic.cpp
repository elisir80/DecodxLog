// Il monitor del traffico con Decodium: cosa tiene, cosa mostra, e le
// risposte che partono davvero verso il programma.
#include "app/TrafficMonitor.h"
#include "core/DecoLinkServer.h"
#include "core/UdpReceiver.h"
#include "core/WsjtxProtocol.h"

#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QTest>
#include <QUdpSocket>

using namespace decolog;
using namespace decolog::core;
using decolog::app::TrafficMonitor;

namespace {

quint16 freePort()
{
    QUdpSocket probe;
    probe.bind(QHostAddress::LocalHost, 0);
    return probe.localPort();
}

QString role(const TrafficMonitor& m, int row, TrafficMonitor::Roles r)
{
    return m.data(m.index(row), r).toString();
}

} // namespace

class TestTraffic : public QObject {
    Q_OBJECT

private slots:
    void recordsOnlyWhileActive()
    {
        TrafficMonitor m(nullptr, nullptr);
        m.setDecoPortPort(0);
        m.record(TrafficMonitor::DecoLink, "in", "127.0.0.1:1", R"({"type":"ping","id":1})");
        QCOMPARE(m.total(), 0);
        m.setActive(true);
        m.record(TrafficMonitor::DecoLink, "in", "127.0.0.1:1", R"({"type":"ping","id":1})");
        m.record(TrafficMonitor::DecoLink, "out", "127.0.0.1:1", R"({"type":"pong","id":1})");
        QCOMPARE(m.total(), 2);
        QCOMPARE(m.count(), 2);
        // La piu' recente in cima.
        QCOMPARE(role(m, 0, TrafficMonitor::TypeRole), QString("pong"));
        QCOMPARE(role(m, 0, TrafficMonitor::DirectionRole), QString("out"));
        QCOMPARE(role(m, 1, TrafficMonitor::ChannelRole), QString("DecoLink"));
    }

    void filtersAndPause()
    {
        TrafficMonitor m(nullptr, nullptr);
        m.setDecoPortPort(0);
        m.setActive(true);
        m.record(TrafficMonitor::Udp, "in", "127.0.0.1:2", wsjtx::buildHeartbeat("Decodium", {}));
        wsjtx::Decode d;
        d.message = "CQ 9A3XY JN75";
        m.record(TrafficMonitor::Udp, "in", "127.0.0.1:2", wsjtx::buildDecode("Decodium", d));
        m.record(TrafficMonitor::DecoLink, "in", "127.0.0.1:1", R"({"type":"query","id":7,"calls":["JA1ZZZ"]})");
        QCOMPARE(m.count(), 3);

        m.setHideRoutine(true);   // via il battito
        QCOMPARE(m.count(), 2);
        m.setShowDecoLink(false);
        QCOMPARE(m.count(), 1);
        QCOMPARE(role(m, 0, TrafficMonitor::TypeRole), QString("Decode"));
        QVERIFY(m.data(m.index(0), TrafficMonitor::CanReplyRole).toBool());
        m.setShowDecoLink(true);
        m.setTextFilter("ja1zzz");
        QCOMPARE(m.count(), 1);
        QCOMPARE(role(m, 0, TrafficMonitor::TypeRole), QString("query"));
        m.setTextFilter({});
        m.setHideRoutine(false);
        QCOMPARE(m.count(), 3);

        // In pausa la vista sta ferma, ma si raccoglie lo stesso.
        m.setPaused(true);
        m.record(TrafficMonitor::DecoLink, "out", "127.0.0.1:1", R"({"type":"award","dxcc":{"worked":1}})");
        QCOMPARE(m.count(), 3);
        QCOMPARE(m.total(), 4);
        m.setPaused(false);
        QCOMPARE(m.count(), 4);
        QCOMPARE(role(m, 0, TrafficMonitor::TypeRole), QString("award"));

        // Il testo da copiare e il dettaglio.
        QVERIFY(m.asText().contains("CQ 9A3XY JN75"));
        const qint64 serial = m.data(m.index(0), TrafficMonitor::SerialRole).toLongLong();
        QVERIFY(m.detail(serial).contains("\"worked\": 1"));

        m.clear();
        QCOMPARE(m.total(), 0);
        QCOMPARE(m.count(), 0);
    }

    void keepsTheLastRowsOnly()
    {
        TrafficMonitor m(nullptr, nullptr);
        m.setDecoPortPort(0);
        m.setActive(true);
        for (int i = 0; i < TrafficMonitor::kLimit + 25; ++i)
            m.record(TrafficMonitor::DecoLink, "in", "x", QByteArray(R"({"type":"ping","id":)") + QByteArray::number(i) + "}");
        QCOMPARE(m.total(), TrafficMonitor::kLimit);
        QCOMPARE(m.count(), TrafficMonitor::kLimit);
        QVERIFY(m.detail(m.data(m.index(0), TrafficMonitor::SerialRole).toLongLong())
                    .contains(QString::number(TrafficMonitor::kLimit + 24)));
    }

    // Le due vie: la riga decodificata da Decodium, la risposta che gli torna.
    void repliesReachDecodium()
    {
        const quint16 port = freePort();
        UdpReceiver udp;
        QVERIFY(udp.start(port));
        DecoLinkServer link;
        TrafficMonitor m(&udp, &link);
        m.setDecoPortPort(0);
        m.setActive(true);
        QSignalSpy clients(&m, &TrafficMonitor::clientsChanged);

        QUdpSocket decodium;
        QVERIFY(decodium.bind(QHostAddress::LocalHost, 0));
        wsjtx::Decode d;
        d.time = QTime(14, 10, 15);
        d.snr = -7;
        d.deltaTime = 0.2;
        d.deltaFrequency = 1234;
        d.mode = "~";
        d.message = "CQ DX JA1ZZZ PM95";
        decodium.writeDatagram(wsjtx::buildDecode("Decodium", d), QHostAddress::LocalHost, port);
        QTRY_COMPARE_WITH_TIMEOUT(m.total(), 1, 3000);
        QVERIFY(clients.count() > 0);
        QCOMPARE(m.udpClients(), QStringList{"Decodium"});

        const qint64 serial = m.data(m.index(0), TrafficMonitor::SerialRole).toLongLong();
        QVERIFY(m.reply(serial));
        QVERIFY(m.lastResultOk());
        QTRY_VERIFY_WITH_TIMEOUT(decodium.hasPendingDatagrams(), 3000);
        const auto r = wsjtx::describe(decodium.receiveDatagram().data());
        QCOMPARE(r.typeName, QString("Reply"));
        QCOMPARE(r.clientId, QString("Decodium"));
        QVERIFY2(r.summary.contains("141015  -7  0.2 1234 ~ CQ DX JA1ZZZ PM95"), qPrintable(r.summary));

        // Senza dire a chi: all'ultimo che ha scritto, col suo nome.
        QVERIFY(m.haltTx({}, false));
        QTRY_VERIFY_WITH_TIMEOUT(decodium.hasPendingDatagrams(), 3000);
        const auto h = wsjtx::describe(decodium.receiveDatagram().data());
        QCOMPARE(h.typeName, QString("HaltTx"));
        QCOMPARE(h.clientId, QString("Decodium"));

        // Quello che parte si vede nel monitor, in uscita.
        QCOMPARE(m.total(), 3);
        QCOMPARE(role(m, 0, TrafficMonitor::DirectionRole), QString("out"));
        QCOMPARE(role(m, 0, TrafficMonitor::TypeRole), QString("HaltTx"));
        // Alla nostra risposta non si risponde.
        QVERIFY(!m.data(m.index(1), TrafficMonitor::CanReplyRole).toBool());
        QVERIFY(!m.reply(m.data(m.index(1), TrafficMonitor::SerialRole).toLongLong()));

        // Il locatore e il testo libero si controllano prima di partire.
        QVERIFY(!m.location({}, "JN"));
        QVERIFY(!m.lastResultOk());
        // DecoLink senza nessuno collegato: si dice, non si finge.
        QVERIFY(!m.sendDecoLink(R"({"type":"tune","freqKhz":14074})"));
        QVERIFY(!m.sendDecoLink("not json"));
    }
};

QTEST_GUILESS_MAIN(TestTraffic)
#include "tst_traffic.moc"
