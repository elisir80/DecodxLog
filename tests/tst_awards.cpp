// Award: prefissi WPX e conteggi lavorati/confermati per banda, modo e conferma.
#include "core/Awards.h"
#include "core/LogDatabase.h"

#include <QTest>

using namespace decolog::core;

class TestAwards : public QObject {
    Q_OBJECT

    static AwardResult find(const QList<AwardResult>& all, const QString& id)
    {
        for (const auto& r : all) {
            if (r.id == id)
                return r;
        }
        return {};
    }

private slots:
    void wpx_data()
    {
        QTest::addColumn<QString>("call");
        QTest::addColumn<QString>("prefix");
        QTest::newRow("simple") << "N8BJQ" << "N8";
        QTest::newRow("two letters") << "WB2ABC" << "WB2";
        QTest::newRow("digit first") << "4X1AB" << "4X1";
        QTest::newRow("uk") << "2E0ABC" << "2E0";
        QTest::newRow("two digits") << "S57AB" << "S57";
        QTest::newRow("italy portable") << "IU8LMC/P" << "IU8";
        QTest::newRow("prefix first") << "EA8/OH2XX" << "EA8";
        QTest::newRow("prefix last") << "N8BJQ/KH6" << "KH6";
        QTest::newRow("no digit") << "LX/DL1ABC" << "LX0";
        QTest::newRow("area change") << "W1AW/4" << "W4";
        QTest::newRow("lowercase") << "ja1zzz" << "JA1";
    }

    void wpx()
    {
        QFETCH(QString, call);
        QFETCH(QString, prefix);
        QCOMPARE(awards::wpxPrefix(call), prefix);
    }

    void countsWorkedAndConfirmed()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        auto qso = [&db](const char* call, const char* date, const char* band, const char* mode, const char* submode,
                         int dxcc, int cqz, std::initializer_list<AdifField> extra) {
            AdifRecord r{{"CALL", call}, {"QSO_DATE", date}, {"TIME_ON", "1200"}, {"BAND", band}, {"MODE", mode},
                         {"SUBMODE", submode}, {"DXCC", QString::number(dxcc)}, {"CQZ", QString::number(cqz)}};
            for (const auto& f : extra)
                r.set(f.name, f.value);
            QCOMPARE(db.insertQso(r, "import").status, InsertResult::Status::Inserted);
        };
        qso("W1AW", "20260101", "20m", "MFSK", "FT2", 291, 5, {{"STATE", "CT"}, {"GRIDSQUARE", "FN31pr"}, {"LOTW_QSL_RCVD", "Y"}});
        qso("K6XX", "20260102", "40m", "FT8", "", 291, 3, {{"STATE", "CA"}, {"GRIDSQUARE", "CM87"}});
        qso("KH6ABC", "20260103", "20m", "SSB", "USB", 110, 31, {{"STATE", "HI"}, {"QSL_RCVD", "Y"}});
        qso("EA8/OH2XX", "20260104", "15m", "CW", "", 29, 33, {{"IOTA", "AF-004"}, {"POTA_REF", "EA-0012"}, {"EQSL_QSL_RCVD", "Y"}});
        qso("VE3XX", "20260105", "20m", "MFSK", "FT2", 1, 4, {{"STATE", "ON"}});   // non e' USA: niente WAS

        const AwardCalculator calc([](int dxcc) { return dxcc == 291 ? QStringLiteral("United States") : QString(); });
        AwardFilter all;
        const auto results = calc.compute(db, all);
        QCOMPARE(results.size(), AwardCalculator::awardIds().size());

        const auto dxcc = find(results, "dxcc");
        QCOMPARE(dxcc.worked(), 4);           // 291, 110, 29, 1
        QCOMPARE(dxcc.confirmed(), 2);        // 291 (LoTW), 110 (cartolina); eQSL non conta
        const auto usa = std::find_if(dxcc.items.cbegin(), dxcc.items.cend(), [](const AwardItem& i) { return i.key == "291"; });
        QCOMPARE(usa->name, QString("United States"));
        QCOMPARE(usa->qsoCount, 2);
        QCOMPARE(usa->bandsWorked, QSet<QString>({"20m", "40m"}));
        QCOMPARE(usa->bandsConfirmed, QSet<QString>({"20m"}));
        QCOMPARE(usa->firstCall, QString("W1AW"));

        QCOMPARE(find(results, "ft2").worked(), 2);
        QCOMPARE(find(results, "waz").worked(), 5);
        const auto was = find(results, "was");
        QCOMPARE(was.worked(), 3);            // CT, CA, HI (ON e' in Canada)
        QCOMPARE(was.items.first().name, QString("California"));
        QCOMPARE(find(results, "grids").worked(), 2);
        QCOMPARE(find(results, "iota").worked(), 1);
        QCOMPARE(find(results, "pota").worked(), 1);
        QCOMPARE(find(results, "wpx").worked(), 5);

        // Anche eQSL come conferma.
        AwardFilter withEqsl;
        withEqsl.confirmEqsl = true;
        QCOMPARE(find(calc.compute(db, withEqsl), "dxcc").confirmed(), 3);

        // Solo 20m, solo FT2.
        AwardFilter band20;
        band20.band = "20m";
        QCOMPARE(find(calc.compute(db, band20), "dxcc").worked(), 3);
        AwardFilter ft2;
        ft2.modeGroup = "FT2";
        QCOMPARE(find(calc.compute(db, ft2), "dxcc").worked(), 2);
        AwardFilter phone;
        phone.modeGroup = "PHONE";
        QCOMPARE(find(calc.compute(db, phone), "dxcc").worked(), 1);
        AwardFilter digital;
        digital.modeGroup = "DIGITAL";
        QCOMPARE(find(calc.compute(db, digital), "dxcc").worked(), 2);

        // Band slot: su 20m tre entita' lavorate (291, 110, 1), due confermate.
        const auto totals = dxcc.bandTotals({"20m", "40m", "15m", "10m"});
        QCOMPARE(totals.size(), 4);
        QCOMPARE(totals.at(0).worked, 3);
        QCOMPARE(totals.at(0).confirmed, 2);
        QCOMPARE(totals.at(1).worked, 1);
        QCOMPARE(totals.at(1).confirmed, 0);
        QCOMPARE(totals.at(3).worked, 0);
    }

    void filterByProfileAndTag()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        const qint64 home = db.saveStationProfile(StationProfile{.name = "Casa", .stationCallsign = "IU8LMC"});
        const qint64 park = db.saveStationProfile(StationProfile{.name = "Parco", .stationCallsign = "IU8LMC/P"});
        auto qso = [&db](const char* call, int dxcc, qint64 profile, const char* tags) {
            AdifRecord r{{"CALL", call}, {"QSO_DATE", "20260101"}, {"TIME_ON", "1200"}, {"BAND", "20m"},
                         {"MODE", "FT8"}, {"DXCC", QString::number(dxcc)}, {"APP_DECOLOG_TAGS", tags}};
            QCOMPARE(db.insertQso(r, "import", {}, false, profile).status, InsertResult::Status::Inserted);
        };
        qso("W1AW", 291, home, "");
        qso("JA1XX", 339, park, "pota,Field Day");
        qso("EA8ABC", 29, park, "sota");

        const AwardCalculator calc;
        AwardFilter byProfile;
        byProfile.stationProfileId = park;
        QCOMPARE(find(calc.compute(db, byProfile), "dxcc").worked(), 2);
        AwardFilter byTag;
        byTag.tag = "POTA";
        const auto pota = find(calc.compute(db, byTag), "dxcc");
        QCOMPARE(pota.worked(), 1);
        QCOMPARE(pota.items.first().key, QString("339"));
        byTag.tag = "field day";
        QCOMPARE(find(calc.compute(db, byTag), "dxcc").worked(), 1);
    }
};

QTEST_GUILESS_MAIN(TestAwards)
#include "tst_awards.moc"
