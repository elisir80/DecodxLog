// I modi come li dice l'operatore e come li vuole la radio: FT8 per chi opera
// e' un modo, per Hamlib e' una banda laterale con i dati dentro.
#include "core/Modes.h"

#include <QTest>

using namespace decolog::core;

class TestModes : public QObject {
    Q_OBJECT

private slots:
    void cwAndPhoneGoStraightThrough()
    {
        QCOMPARE(modes::catFor(QStringLiteral("CW")), QStringLiteral("CW"));
        QCOMPARE(modes::catFor(QStringLiteral("CW-R")), QStringLiteral("CWR"));
        QCOMPARE(modes::catFor(QStringLiteral("USB")), QStringLiteral("USB"));
        QCOMPARE(modes::catFor(QStringLiteral("LSB")), QStringLiteral("LSB"));
        QCOMPARE(modes::catFor(QStringLiteral("AM")), QStringLiteral("AM"));
        QCOMPARE(modes::catFor(QStringLiteral("FM")), QStringLiteral("FM"));
    }

    void everyDigitalModeIsDataOnUpperSideband()
    {
        for (const auto* name : {"FT8", "FT4", "FT2", "JS8", "JT65", "Q65", "MSK144", "PSK31", "OLIVIA"})
            QCOMPARE(modes::catFor(QLatin1String(name)), QStringLiteral("PKTUSB"));
    }

    void rttyKeepsItsOwnName()
    {
        QCOMPARE(modes::catFor(QStringLiteral("RTTY")), QStringLiteral("RTTY"));
        QCOMPARE(modes::catFor(QStringLiteral("RTTY-R")), QStringLiteral("RTTYR"));
    }

    void lowercaseAndSpacesDoNotMatter()
    {
        QCOMPARE(modes::catFor(QStringLiteral(" ft8 ")), QStringLiteral("PKTUSB"));
        QCOMPARE(modes::catFor(QStringLiteral("cw")), QStringLiteral("CW"));
    }

    void aSubmodeCountsAsItsMode()
    {
        // Decodium manda anche i sottomodi: FT8-DX resta un FT8.
        QCOMPARE(modes::catFor(QStringLiteral("FT8-DX")), QStringLiteral("PKTUSB"));
        QCOMPARE(modes::catFor(QStringLiteral("PSK31-A")), QStringLiteral("PKTUSB"));
    }

    void whatTheRadioAlreadySaysIsKept()
    {
        QCOMPARE(modes::catFor(QStringLiteral("PKTUSB")), QStringLiteral("PKTUSB"));
        QCOMPARE(modes::catFor(QStringLiteral("CWR")), QStringLiteral("CWR"));
    }

    void whatIsNotKnownDoesNoHarm()
    {
        // Senza un modo, o con uno che non si conosce, si resta su USB: e' la
        // scelta che non mette la radio in uno stato strano.
        QCOMPARE(modes::catFor(QString()), QStringLiteral("USB"));
        QCOMPARE(modes::catFor(QStringLiteral("SSB")), QStringLiteral("USB"));
        QCOMPARE(modes::catFor(QStringLiteral("PHONE")), QStringLiteral("USB"));
        QCOMPARE(modes::catFor(QStringLiteral("DATA")), QStringLiteral("PKTUSB"));
        QCOMPARE(modes::catFor(QStringLiteral("QUALCOSA")), QStringLiteral("USB"));
    }

    void theMenuHasCwPhoneAndDigital()
    {
        const auto all = modes::all();
        QVERIFY(all.size() > 20);
        QCOMPARE(all.first().name, QStringLiteral("CW"));   // primo: chi va in CW lo trova subito
        int cw = 0, voice = 0, data = 0;
        for (const auto& e : all) {
            if (e.group == QLatin1String("cw")) ++cw;
            else if (e.group == QLatin1String("voice")) ++voice;
            else if (e.group == QLatin1String("data")) ++data;
        }
        QVERIFY(cw >= 2);
        QVERIFY(voice >= 4);
        QVERIFY(data >= 10);
        // Ogni voce ha un nome e un modo per la radio: niente buchi nel menu.
        for (const auto& e : all) {
            QVERIFY(!e.name.isEmpty());
            QVERIFY(!e.cat.isEmpty());
            QCOMPARE(modes::catFor(e.name), e.cat);
        }
    }
};

QTEST_MAIN(TestModes)
#include "tst_modes.moc"
