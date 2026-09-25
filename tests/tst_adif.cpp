// ADIF: lettura robusta e ritorno senza perdite.
#include "core/Adif.h"

#include <QTest>

using namespace decolog::core;

class TestAdif : public QObject {
    Q_OBJECT

private slots:
    void parsesDecodiumLoggedAdif()
    {
        // Quello che manda Decodium in LoggedADIF (MessageClient::logged_ADIF).
        const QByteArray text =
            "\n<adif_ver:5>3.1.0\n<programid:20>Decodium FT2 1.0.637\n<EOH>\n"
            "<call:5>DL1AB <gridsquare:4>JO62 <mode:4>MFSK <submode:3>FT2 <rst_sent:3>-10 "
            "<rst_rcvd:3>-05 <qso_date:8>20260917 <time_on:6>101500 <qso_date_off:8>20260917 "
            "<time_off:6>101545 <band:3>20m <freq:9>14.084000 <station_callsign:6>IU8LMC "
            "<my_gridsquare:6>JN71DC <EOR>";
        const AdifDocument doc = adif::parse(text);
        QCOMPARE(doc.header.value("PROGRAMID"), QString("Decodium FT2 1.0.637"));
        QCOMPARE(doc.records.size(), 1);
        const AdifRecord& r = doc.records.first();
        QCOMPARE(r.value("CALL"), QString("DL1AB"));
        QCOMPARE(r.value("mode"), QString("MFSK"));
        QCOMPARE(r.value("SUBMODE"), QString("FT2"));
        QCOMPARE(r.value("FREQ"), QString("14.084000"));
        QCOMPARE(r.value("MY_GRIDSQUARE"), QString("JN71DC"));
    }

    void acceptsFileWithoutHeaderAndTypeIndicators()
    {
        const AdifDocument doc = adif::parse("<CALL:4:S>K1AB<QSO_DATE:8:D>20250101<eor><call:4>W1AW<eor>");
        QCOMPARE(doc.records.size(), 2);
        QCOMPARE(doc.records.at(0).value("CALL"), QString("K1AB"));
        QCOMPARE(doc.records.at(1).value("CALL"), QString("W1AW"));
    }

    void lengthInCharactersOrUtf8Bytes()
    {
        // "Müller": 6 caratteri, 7 byte UTF-8. Entrambe le scritture vanno lette.
        const AdifDocument chars = adif::parse(QString("<name:6>Müller <call:4>K1AB <eor>").toUtf8());
        QCOMPARE(chars.records.first().value("NAME"), QString("Müller"));
        QCOMPARE(chars.records.first().value("CALL"), QString("K1AB"));

        const AdifDocument bytes = adif::parse(QString("<name:7>Müller <call:4>K1AB <eor>").toUtf8());
        QCOMPARE(bytes.records.first().value("NAME"), QString("Müller"));
        QCOMPARE(bytes.records.first().value("CALL"), QString("K1AB"));
    }

    void latin1Fallback()
    {
        QByteArray latin1 = QByteArray("<name:6>M") + char(0xFC) + "ller <eor>";
        const AdifDocument doc = adif::parse(latin1);
        QCOMPARE(doc.records.first().value("NAME"), QString("Müller"));
    }

    void recordWithoutEorIsKept()
    {
        const AdifDocument doc = adif::parse("<eoh><call:4>K1AB<band:3>40m");
        QCOMPARE(doc.records.size(), 1);
        QCOMPARE(doc.records.first().value("BAND"), QString("40m"));
    }

    void writeThenParseRoundTrip()
    {
        AdifRecord r{{"CALL", "IU8LMC"}, {"NAME", "Zoë <test>"}, {"APP_DECODIUM_SNR", "-12"}};
        AdifDocument doc;
        doc.header.set("PROGRAMID", "DecoDXLog");
        doc.records << r;
        const AdifDocument back = adif::parse(adif::writeDocument(doc));
        QCOMPARE(back.records.size(), 1);
        QCOMPARE(back.records.first().value("NAME"), QString("Zoë <test>"));
        QCOMPARE(back.records.first().value("APP_DECODIUM_SNR"), QString("-12"));
        QCOMPARE(back.header.value("PROGRAMID"), QString("DecoDXLog"));
    }

    void normalizeFt2()
    {
        AdifRecord legacy{{"CALL", "K1AB"}, {"MODE", "FT2"}};
        adif::normalizeMode(legacy);
        QCOMPARE(legacy.value("MODE"), QString("MFSK"));
        QCOMPARE(legacy.value("SUBMODE"), QString("FT2"));

        // JTTY (WSJT-X 3.2.0) come gli altri modi nuovi di WSJT-X.
        AdifRecord jtty{{"CALL", "K1JT"}, {"MODE", "JTTY"}};
        adif::normalizeMode(jtty);
        QCOMPARE(jtty.value("MODE"), QString("MFSK"));
        QCOMPARE(jtty.value("SUBMODE"), QString("JTTY"));

        AdifRecord ft8{{"MODE", "FT8"}};
        adif::normalizeMode(ft8);
        QCOMPARE(ft8.value("MODE"), QString("FT8"));
        QVERIFY(!ft8.contains("SUBMODE"));
    }

    void valuesDamagedByAnOldImportAreCleanedUp()
    {
        // Cosi' sono rimasti nei log: il valore tagliato e il principio del tag
        // successivo appiccicato in fondo.
        QCOMPARE(adif::repairTruncated(QString::fromUtf8("Вильнюс<GRIDSQ")),
                 QString::fromUtf8("Вильнюс"));
        QCOMPARE(adif::repairTruncated(QStringLiteral("Lagan Kalmykia, 359220<G")),
                 QStringLiteral("Lagan Kalmykia, 359220"));
        QCOMPARE(adif::repairTruncated(QStringLiteral("JORGEN SVENSSON<")), QStringLiteral("JORGEN SVENSSON"));
        // La virgola rimasta a penzoloni se ne va con il resto.
        QCOMPARE(adif::repairTruncated(QStringLiteral("Allerod, <QSO_DA")), QStringLiteral("Allerod"));
        // Un valore sano non si tocca, e nemmeno un '<' che ci sta per davvero.
        QCOMPARE(adif::repairTruncated(QStringLiteral("Ted")), QStringLiteral("Ted"));
        QCOMPARE(adif::repairTruncated(QStringLiteral("SNR < -20 dB")), QStringLiteral("SNR < -20 dB"));
        // Un carattere tagliato a meta' non si indovina: meglio vuoto, cosi' il
        // callbook puo' riscriverlo giusto.
        QVERIFY(adif::repairTruncated(QStringLiteral("1291 ") + QChar(0xFFFD) + QStringLiteral("kofljica")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAdif)
#include "tst_adif.moc"
