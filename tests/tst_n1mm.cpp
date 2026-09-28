// N1MM Logger+: i pacchetti XML dei contatti tradotti in ADIF (frequenze in
// decine di hertz, USB che diventa SSB, zona CQ o ITU secondo la gara), e il
// ricevitore che distingue contatto, correzione e cancellazione.
#include "core/LogDatabase.h"
#include "core/N1mm.h"

#include <QSignalSpy>
#include <QTest>

using namespace decolog::core;

namespace {

const QByteArray kContact = R"(<?xml version="1.0" encoding="utf-8"?>
<contactinfo>
  <app>N1MM</app>
  <contestname>CQWWSSB</contestname>
  <contestnr>73</contestnr>
  <timestamp>2026-10-24 12:03:38</timestamp>
  <mycall>IU8LMC</mycall>
  <band>14</band>
  <rxfreq>1420512</rxfreq>
  <txfreq>1420512</txfreq>
  <operator>IU8LMC</operator>
  <mode>USB</mode>
  <call>w1aw</call>
  <countryprefix>K</countryprefix>
  <snt>59</snt>
  <sntnr>0</sntnr>
  <rcv>59</rcv>
  <rcvnr>0</rcvnr>
  <gridsquare></gridsquare>
  <exchange1></exchange1>
  <section></section>
  <comment>nice signal</comment>
  <name>HIRAM</name>
  <zone>5</zone>
  <ck>0</ck>
  <IsRunQSO>1</IsRunQSO>
  <StationName>CONTEST-PC</StationName>
  <ID>f9ffac4fcd3e479ca86e137df1338531</ID>
</contactinfo>)";

} // namespace

class TestN1mm : public QObject {
    Q_OBJECT

private slots:
    void contactBecomesAdif()
    {
        const n1mm::Packet p = n1mm::parse(kContact);
        QCOMPARE(int(p.kind), int(n1mm::Kind::Contact));
        QCOMPARE(p.id, QString("f9ffac4fcd3e479ca86e137df1338531"));
        const AdifRecord& r = p.record;
        QCOMPARE(r.value("CALL"), QString("W1AW"));
        QCOMPARE(r.value("QSO_DATE"), QString("20261024"));
        QCOMPARE(r.value("TIME_ON"), QString("120338"));
        QCOMPARE(r.value("FREQ"), QString("14.205120"));
        QCOMPARE(r.value("BAND"), QString("20m"));
        QVERIFY(r.value("FREQ_RX").isEmpty());
        QCOMPARE(r.value("MODE"), QString("SSB"));
        QCOMPARE(r.value("SUBMODE"), QString("USB"));
        QCOMPARE(r.value("RST_SENT"), QString("59"));
        QVERIFY(r.value("STX").isEmpty());   // 0 = non c'e'
        QCOMPARE(r.value("CQZ"), QString("5"));
        QCOMPARE(r.value("NAME"), QString("HIRAM"));
        QCOMPARE(r.value("COMMENT"), QString("nice signal"));
        QCOMPARE(r.value("STATION_CALLSIGN"), QString("IU8LMC"));
        QCOMPARE(r.value("CONTEST_ID"), QString("CQWWSSB"));
        QCOMPARE(r.value("APP_N1MM_ID"), QString("f9ffac4fcd3e479ca86e137df1338531"));
    }

    void splitDigitalAndIaru()
    {
        QByteArray xml = kContact;
        xml.replace("<rxfreq>1420512</rxfreq>", "<rxfreq>1408000</rxfreq>")
            .replace("<mode>USB</mode>", "<mode>FT4</mode>")
            .replace("<contestname>CQWWSSB</contestname>", "<contestname>IARU-HF</contestname>")
            .replace("<sntnr>0</sntnr>", "<sntnr>123</sntnr>");
        const AdifRecord r = n1mm::parse(xml).record;
        QCOMPARE(r.value("FREQ"), QString("14.205120"));
        QCOMPARE(r.value("FREQ_RX"), QString("14.080000"));
        QCOMPARE(r.value("MODE"), QString("MFSK"));
        QCOMPARE(r.value("SUBMODE"), QString("FT4"));
        QCOMPARE(r.value("ITUZ"), QString("5"));
        QVERIFY(r.value("CQZ").isEmpty());
        QCOMPARE(r.value("STX"), QString("123"));
    }

    void otherPacketsAreIgnored()
    {
        QCOMPARE(int(n1mm::parse("<RadioInfo><Freq>1420512</Freq></RadioInfo>").kind), int(n1mm::Kind::None));
        QCOMPARE(int(n1mm::parse("not xml at all").kind), int(n1mm::Kind::None));
    }

    void receiverSignals()
    {
        N1mmReceiver rx;
        QSignalSpy added(&rx, &N1mmReceiver::contactReceived);
        QSignalSpy replaced(&rx, &N1mmReceiver::contactReplaced);
        QSignalSpy deleted(&rx, &N1mmReceiver::contactDeleted);
        rx.handleDatagram(kContact);
        QByteArray replace = kContact;
        replace.replace("contactinfo>", "contactreplace>").replace("<name>HIRAM</name>", "<name>HIRAM PERCY</name>");
        rx.handleDatagram(replace);
        rx.handleDatagram(R"(<?xml version="1.0"?><contactdelete><app>N1MM</app><timestamp>2026-10-24 12:03:38</timestamp>
                             <call>W1AW</call><ID>f9ffac4fcd3e479ca86e137df1338531</ID></contactdelete>)");
        QCOMPARE(added.count(), 1);
        QCOMPARE(replaced.count(), 1);
        QCOMPARE(replaced.at(0).at(0).value<AdifRecord>().value("NAME"), QString("HIRAM PERCY"));
        QCOMPARE(deleted.count(), 1);
        QCOMPARE(deleted.at(0).at(0).toString(), QString("f9ffac4fcd3e479ca86e137df1338531"));
        QCOMPARE(deleted.at(0).at(1).value<AdifRecord>().value("QSO_DATE"), QString("20261024"));
    }

    void theQsoIsFoundByItsId()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        const qint64 id = db.insertQso(n1mm::parse(kContact).record, "n1mm").id;
        QVERIFY(id > 0);
        // L'ID resta nel QSO (adif_extra) e si ritrova, vicino alla sua ora.
        const QDateTime near(QDate(2026, 10, 24), QTime(12, 0), QTimeZone::UTC);
        QCOMPARE(db.findByExtra("APP_N1MM_ID", "f9ffac4fcd3e479ca86e137df1338531", near), std::optional<qint64>(id));
        QCOMPARE(db.findByExtra("APP_N1MM_ID", "f9ffac4fcd3e479ca86e137df1338531"), std::optional<qint64>(id));
        // Un altro giorno o un altro ID: no.
        QVERIFY(!db.findByExtra("APP_N1MM_ID", "f9ffac4fcd3e479ca86e137df1338531", near.addDays(5)));
        QVERIFY(!db.findByExtra("APP_N1MM_ID", "00000000000000000000000000000000"));
        QVERIFY(db.softDeleteQso(id));
        QVERIFY(!db.findByExtra("APP_N1MM_ID", "f9ffac4fcd3e479ca86e137df1338531"));
    }
};

QTEST_GUILESS_MAIN(TestN1mm)
#include "tst_n1mm.moc"
