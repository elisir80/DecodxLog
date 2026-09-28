// Protocollo UDP: decodifica dei messaggi e scelta fra QSOLogged e LoggedADIF.
#include "core/UdpReceiver.h"
#include "core/WsjtxProtocol.h"

#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>
#include <QUdpSocket>

using namespace decolog::core;

namespace {

wsjtx::QsoLogged sampleQso()
{
    wsjtx::QsoLogged q;
    q.timeOn = QDateTime(QDate(2026, 9, 17), QTime(10, 15, 0), QTimeZone::UTC);
    q.timeOff = QDateTime(QDate(2026, 9, 17), QTime(10, 15, 45), QTimeZone::UTC);
    q.dxCall = "DL1AB";
    q.dxGrid = "JO62";
    q.txFrequencyHz = 14084000;
    q.mode = "FT2";
    q.reportSent = "-10";
    q.reportReceived = "-05";
    q.myCall = "IU8LMC";
    q.myGrid = "JN71DC";
    return q;
}

QByteArray sampleAdif()
{
    return "\n<adif_ver:5>3.1.0\n<programid:20>Decodium FT2 1.0.637\n<EOH>\n"
           "<call:5>DL1AB <gridsquare:4>JO62 <mode:4>MFSK <submode:3>FT2 <rst_sent:3>-10 "
           "<rst_rcvd:3>-05 <qso_date:8>20260917 <time_on:6>101500 <band:3>20m "
           "<freq:9>14.084000 <station_callsign:6>IU8LMC <EOR>";
}

} // namespace

class TestProtocol : public QObject {
    Q_OBJECT

private slots:
    void rejectsForeignDatagrams()
    {
        QVERIFY(!wsjtx::parse("hello").has_value());
        QVERIFY(!wsjtx::parse(QByteArray(64, '\0')).has_value());
    }

    void heartbeatRoundTrip()
    {
        const auto msg = wsjtx::parse(wsjtx::buildHeartbeat("Decodium", {3, "1.0.637", "abc"}));
        QVERIFY(msg);
        QCOMPARE(msg->clientId, QString("Decodium"));
        const auto* hb = std::get_if<wsjtx::Heartbeat>(&msg->payload);
        QVERIFY(hb);
        QCOMPARE(hb->version, QString("1.0.637"));
    }

    void qsoLoggedRoundTrip()
    {
        for (quint32 schema : {2u, 3u}) {
            const auto msg = wsjtx::parse(wsjtx::buildQsoLogged("WSJT-X", sampleQso(), schema));
            QVERIFY(msg);
            const auto* q = std::get_if<wsjtx::QsoLogged>(&msg->payload);
            QVERIFY(q);
            QCOMPARE(q->dxCall, QString("DL1AB"));
            QCOMPARE(q->txFrequencyHz, quint64(14084000));
            QCOMPARE(q->timeOn.toUTC(), sampleQso().timeOn);
            QCOMPARE(q->myGrid, QString("JN71DC"));
        }
    }

    void statusRoundTrip()
    {
        wsjtx::Status st;
        st.dialFrequencyHz = 7047000;
        st.mode = "FT2";
        st.dxCall = "K1AB";
        st.deCall = "IU8LMC";
        st.transmitting = true;
        const auto msg = wsjtx::parse(wsjtx::buildStatus("Decodium", st));
        QVERIFY(msg);
        const auto* back = std::get_if<wsjtx::Status>(&msg->payload);
        QVERIFY(back);
        QCOMPARE(back->dialFrequencyHz, quint64(7047000));
        QCOMPARE(back->dxCall, QString("K1AB"));
        QCOMPARE(back->deCall, QString("IU8LMC"));
        QVERIFY(back->transmitting);
    }

    // Le righe decodificate: servono alla mappa del rotore.
    void decodeRoundTrip()
    {
        wsjtx::Decode d;
        d.time = QTime(12, 34, 15);
        d.snr = -14;
        d.deltaTime = 0.3;
        d.deltaFrequency = 1234;
        d.mode = "~";
        d.message = "CQ EA8ABC IL18";
        const auto msg = wsjtx::parse(wsjtx::buildDecode("Decodium", d));
        QVERIFY(msg);
        const auto* back = std::get_if<wsjtx::Decode>(&msg->payload);
        QVERIFY(back);
        QCOMPARE(back->snr, -14);
        QCOMPARE(back->time, QTime(12, 34, 15));
        QCOMPARE(back->deltaFrequency, quint32(1234));
        QCOMPARE(back->message, QString("CQ EA8ABC IL18"));
    }

    // Il caso normale di WSJT-X e Decodium: arrivano entrambi, vale solo l'ADIF.
    void loggedAdifSupersedesQsoLogged()
    {
        UdpReceiver rx;
        rx.setPairingWindowMs(200);
        QSignalSpy spy(&rx, &UdpReceiver::qsoReceived);

        rx.handleDatagram(wsjtx::buildQsoLogged("Decodium", sampleQso()));
        rx.handleDatagram(wsjtx::buildLoggedAdif("Decodium", sampleAdif()));
        QTest::qWait(400);

        QCOMPARE(spy.count(), 1);
        const auto record = spy.at(0).at(0).value<AdifRecord>();
        QCOMPARE(record.value("CALL"), QString("DL1AB"));
        QCOMPARE(spy.at(0).at(1).toString(), QString("udp_decodium"));
        QCOMPARE(spy.at(0).at(2).toString(), QString("Decodium FT2 1.0.637"));
    }

    // Un client che manda solo QSOLogged non perde il QSO.
    void qsoLoggedAloneIsUsedAfterWindow()
    {
        UdpReceiver rx;
        rx.setPairingWindowMs(100);
        QSignalSpy spy(&rx, &UdpReceiver::qsoReceived);

        rx.handleDatagram(wsjtx::buildQsoLogged("JTDX", sampleQso()));
        QCOMPARE(spy.count(), 0);
        QVERIFY(spy.wait(1000));

        const auto record = spy.at(0).at(0).value<AdifRecord>();
        QCOMPARE(record.value("MODE"), QString("MFSK"));
        QCOMPARE(record.value("SUBMODE"), QString("FT2"));
        QCOMPARE(record.value("BAND"), QString("20m"));
        QCOMPARE(record.value("TIME_ON"), QString("101500"));
        QCOMPARE(spy.at(0).at(1).toString(), QString("udp_wsjtx"));
    }

    void forwardTargetsAreParsed()
    {
        QStringList rejected;
        const auto targets = UdpReceiver::parseTargets("127.0.0.1:2238, localhost:2333;192.168.1.20:2237 junk 1.2.3.4:0",
                                                       &rejected);
        QCOMPARE(targets.size(), 3);
        QCOMPARE(targets.at(1).address, QHostAddress(QHostAddress::LocalHost));
        QCOMPARE(targets.at(1).port, quint16(2333));
        QCOMPARE(targets.at(2).address, QHostAddress("192.168.1.20"));
        QCOMPARE(rejected, QStringList({"junk", "1.2.3.4:0"}));
        QVERIFY(UdpReceiver::parseTargets("").isEmpty());
    }

    // Il ripetitore: Decodium manda a DecoDXLog, GridTracker riceve lo stesso
    // pacchetto, e la sua risposta torna a Decodium.
    void packetsAreRelayedBothWays()
    {
        QUdpSocket decodium;
        QVERIFY(decodium.bind(QHostAddress::LocalHost, 0));
        QUdpSocket gridTracker;
        QVERIFY(gridTracker.bind(QHostAddress::LocalHost, 0));

        UdpReceiver rx;
        rx.setForwardTargets({{QHostAddress(QHostAddress::LocalHost), gridTracker.localPort()}});
        QVERIFY(rx.start(0) == false);   // porta 0 = spento
        // Una porta libera per DecoDXLog.
        QUdpSocket probe;
        QVERIFY(probe.bind(QHostAddress::LocalHost, 0));
        const quint16 port = probe.localPort();
        probe.close();
        QVERIFY(rx.start(port));
        QSignalSpy seen(&rx, &UdpReceiver::clientSeen);

        const QByteArray heartbeat = wsjtx::buildHeartbeat("Decodium", {});
        decodium.writeDatagram(heartbeat, QHostAddress::LocalHost, port);
        QTRY_VERIFY_WITH_TIMEOUT(gridTracker.hasPendingDatagrams(), 3000);
        QCOMPARE(gridTracker.receiveDatagram().data(), heartbeat);
        // DecoDXLog lo ha anche letto per se'.
        QTRY_VERIFY_WITH_TIMEOUT(seen.count() > 0, 3000);

        // GridTracker risponde al mittente (DecoDXLog): la risposta va a Decodium.
        const QByteArray reply = wsjtx::buildHeartbeat("Decodium", {});
        gridTracker.writeDatagram(reply, QHostAddress::LocalHost, port);
        QTRY_VERIFY_WITH_TIMEOUT(decodium.hasPendingDatagrams(), 3000);
        QCOMPARE(decodium.receiveDatagram().data(), reply);
        // E non torna a GridTracker ne' si conta come client.
        QTest::qWait(200);
        QVERIFY(!gridTracker.hasPendingDatagrams());
    }
};

QTEST_GUILESS_MAIN(TestProtocol)
#include "tst_protocol.moc"
