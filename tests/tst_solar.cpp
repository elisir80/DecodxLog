// DecoLog — il XML del Sole: si legge quello vero, non uno finto.
#include "core/Solar.h"

#include <QTest>

using namespace decolog::core;

namespace {

// Una risposta di hamqsl.com, accorciata ma con la forma vera, "electonflux"
// scritto male compreso: la fonte lo scrive cosi'.
const char* kXml =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
    "<solar>\n"
    "    <solardata>\n"
    "        <source url=\"http://www.hamqsl.com/solar.html\">N0NBH</source>\n"
    "        <updated> 18 Sep 2026 0730 GMT</updated>\n"
    "        <solarflux>97</solarflux>\n"
    "        <aindex> 9</aindex>\n"
    "        <kindex> 2</kindex>\n"
    "        <xray>B2.4</xray>\n"
    "        <sunspots>11</sunspots>\n"
    "        <protonflux>16</protonflux>\n"
    "        <electonflux>1830</electonflux>\n"
    "        <aurora> 2</aurora>\n"
    "        <solarwind>433.1</solarwind>\n"
    "        <magneticfield> -0.6</magneticfield>\n"
    "        <calculatedconditions>\n"
    "            <band name=\"80m-40m\" time=\"day\">Fair</band>\n"
    "            <band name=\"30m-20m\" time=\"day\">Good</band>\n"
    "            <band name=\"12m-10m\" time=\"day\">Poor</band>\n"
    "            <band name=\"80m-40m\" time=\"night\">Good</band>\n"
    "        </calculatedconditions>\n"
    "        <calculatedvhfconditions>\n"
    "            <phenomenon name=\"E-Skip\" location=\"europe\">Band Closed</phenomenon>\n"
    "        </calculatedvhfconditions>\n"
    "        <geomagfield>QUIET</geomagfield>\n"
    "        <signalnoise>S1-S2</signalnoise>\n"
    "        <muf>NoRpt</muf>\n"
    "    </solardata>\n"
    "</solar>";

} // namespace

class TestSolar : public QObject {
    Q_OBJECT

private slots:
    void readsTheNumbers()
    {
        const SolarData data = solar::parse(kXml);
        QVERIFY(data.valid);
        QCOMPARE(data.solarFlux, 97);
        QCOMPARE(data.aIndex, 9);
        QCOMPARE(data.kIndex, 2);
        QCOMPARE(data.sunspots, 11);
        QCOMPARE(data.aurora, 2);
        QCOMPARE(data.xray, QStringLiteral("B2.4"));
        QCOMPARE(data.geomagField, QStringLiteral("QUIET"));
        QCOMPARE(data.signalNoise, QStringLiteral("S1-S2"));
        QCOMPARE(data.electronFlux, QStringLiteral("1830"));
        QCOMPARE(data.source, QStringLiteral("N0NBH"));
        QVERIFY(data.updated.contains(QStringLiteral("18 Sep 2026")));
    }

    void readsTheBands()
    {
        const SolarData data = solar::parse(kXml);
        QCOMPARE(data.hf.size(), 4);
        QCOMPARE(data.hf.at(1).band, QStringLiteral("30m-20m"));
        QCOMPARE(data.hf.at(1).when, QStringLiteral("day"));
        QCOMPARE(data.hf.at(1).condition, QStringLiteral("Good"));
        QCOMPARE(data.hf.at(3).when, QStringLiteral("night"));
        QCOMPARE(data.vhf.size(), 1);
        QCOMPARE(data.vhf.at(0).band, QStringLiteral("E-Skip"));
        QCOMPARE(data.vhf.at(0).condition, QStringLiteral("Band Closed"));
    }

    void conditionColours()
    {
        QCOMPARE(solar::conditionClass(QStringLiteral("Good")), QStringLiteral("good"));
        QCOMPARE(solar::conditionClass(QStringLiteral("Fair")), QStringLiteral("fair"));
        QCOMPARE(solar::conditionClass(QStringLiteral("Poor")), QStringLiteral("poor"));
        QCOMPARE(solar::conditionClass(QStringLiteral("Band Closed")), QStringLiteral("closed"));
        QCOMPARE(solar::conditionClass(QStringLiteral("boh")), QStringLiteral("unknown"));
    }

    void rubbishIsNotData()
    {
        QVERIFY(!solar::parse("").valid);
        QVERIFY(!solar::parse("<html><body>down for maintenance</body></html>").valid);
        // Un XML valido ma vuoto non vale come dato.
        QVERIFY(!solar::parse("<solar><solardata></solardata></solar>").valid);
    }

    void mapForQml()
    {
        const QVariantMap map = solar::parse(kXml).toMap();
        QCOMPARE(map.value(QStringLiteral("solarFlux")).toInt(), 97);
        const QVariantList hf = map.value(QStringLiteral("hf")).toList();
        QCOMPARE(hf.size(), 4);
        QCOMPARE(hf.at(2).toMap().value(QStringLiteral("class")).toString(), QStringLiteral("poor"));
    }
};

QTEST_GUILESS_MAIN(TestSolar)
#include "tst_solar.moc"
