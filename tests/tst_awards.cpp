// Award: prefissi WPX e conteggi lavorati/confermati per banda, modo e conferma.
#include "core/Awards.h"
#include "core/LogDatabase.h"

#include <QTest>

#include <algorithm>

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

    // ── WAC, WAJA, AJD ───────────────────────────────────────────────────────

    void theSixContinentsAreCountedForWac()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        auto worked = [&db](const QString& call, const QString& cont, const QString& band) {
            AdifRecord r;
            r.set(QStringLiteral("CALL"), call);
            r.set(QStringLiteral("QSO_DATE"), QStringLiteral("20260918"));
            r.set(QStringLiteral("TIME_ON"), QStringLiteral("120000"));
            r.set(QStringLiteral("BAND"), band);
            r.set(QStringLiteral("MODE"), QStringLiteral("CW"));
            r.set(QStringLiteral("CONT"), cont);
            r.set(QStringLiteral("DXCC"), QStringLiteral("100"));
            db.insertQso(r, QStringLiteral("test"));
        };
        worked(QStringLiteral("DL9ZZT"), QStringLiteral("EU"), QStringLiteral("20m"));
        worked(QStringLiteral("W1AW"), QStringLiteral("NA"), QStringLiteral("20m"));
        worked(QStringLiteral("PY2ABC"), QStringLiteral("SA"), QStringLiteral("40m"));
        worked(QStringLiteral("JA1ABC"), QStringLiteral("AS"), QStringLiteral("20m"));
        worked(QStringLiteral("ZS6ABC"), QStringLiteral("AF"), QStringLiteral("15m"));
        worked(QStringLiteral("VK3ABC"), QStringLiteral("OC"), QStringLiteral("20m"));
        // L'Antartide non fa numero per il diploma, ma si vede.
        worked(QStringLiteral("DP1POL"), QStringLiteral("AN"), QStringLiteral("20m"));

        AwardCalculator calc;
        const auto results = calc.compute(db, AwardFilter{});
        const auto wac = std::find_if(results.cbegin(), results.cend(),
                                      [](const AwardResult& r) { return r.id == QLatin1String("wac"); });
        QVERIFY(wac != results.cend());
        QCOMPARE(wac->worked(), 7);      // sei continenti piu' l'Antartide
        QCOMPARE(wac->target, 6);
        // Il WAC si fa banda per banda: il 20 metri ne ha cinque.
        const auto totals = wac->bandTotals({QStringLiteral("20m"), QStringLiteral("40m")});
        QCOMPARE(totals.at(0).worked, 5);
        QCOMPARE(totals.at(1).worked, 1);
    }

    void japanesePrefecturesAndDistricts()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        auto worked = [&db](const QString& call, const QString& state) {
            AdifRecord r;
            r.set(QStringLiteral("CALL"), call);
            r.set(QStringLiteral("QSO_DATE"), QStringLiteral("20260918"));
            r.set(QStringLiteral("TIME_ON"), QStringLiteral("120000"));
            r.set(QStringLiteral("BAND"), QStringLiteral("20m"));
            r.set(QStringLiteral("MODE"), QStringLiteral("CW"));
            r.set(QStringLiteral("DXCC"), QStringLiteral("339"));
            r.set(QStringLiteral("STATE"), state);
            db.insertQso(r, QStringLiteral("test"));
        };
        worked(QStringLiteral("JA1ABC"), QStringLiteral("12"));       // Chiba, distretto 1
        worked(QStringLiteral("JA3XYZ"), QStringLiteral("25"));       // Osaka, distretto 3
        worked(QStringLiteral("JH1QRS"), QStringLiteral("JA12"));     // la stessa Chiba, scritta cosi'
        worked(QStringLiteral("JA0TUV"), QStringLiteral("09"));       // Nagano, distretto 0

        AwardCalculator calc;
        const auto results = calc.compute(db, AwardFilter{});
        const auto waja = std::find_if(results.cbegin(), results.cend(),
                                       [](const AwardResult& r) { return r.id == QLatin1String("waja"); });
        const auto ajd = std::find_if(results.cbegin(), results.cend(),
                                      [](const AwardResult& r) { return r.id == QLatin1String("ajd"); });
        QVERIFY(waja != results.cend() && ajd != results.cend());
        QCOMPARE(waja->worked(), 3);     // Chiba contata una volta sola
        QCOMPARE(waja->target, 47);
        QCOMPARE(ajd->worked(), 3);      // distretti 1, 3, 0
        QCOMPARE(ajd->target, 10);
        // Il nome della prefettura si vede, non solo il numero.
        const auto chiba = std::find_if(waja->items.cbegin(), waja->items.cend(),
                                        [](const AwardItem& i) { return i.key == QLatin1String("12"); });
        QVERIFY(chiba != waja->items.cend());
        QCOMPARE(chiba->name, QStringLiteral("Chiba"));
    }

    void thePrefectureIsReadHoweverItIsWritten()
    {
        QCOMPARE(awards::japanPrefecture(QStringLiteral("12")), QStringLiteral("12"));
        QCOMPARE(awards::japanPrefecture(QStringLiteral("JA12")), QStringLiteral("12"));
        QCOMPARE(awards::japanPrefecture(QStringLiteral("12 Chiba")), QStringLiteral("12"));
        QCOMPARE(awards::japanPrefecture(QStringLiteral("01")), QStringLiteral("01"));
        QCOMPARE(awards::japanPrefecture(QStringLiteral("7")), QStringLiteral("07"));
        // Fuori dai 47 non e' una prefettura.
        QVERIFY(awards::japanPrefecture(QStringLiteral("48")).isEmpty());
        QVERIFY(awards::japanPrefecture(QStringLiteral("SN")).isEmpty());
        QVERIFY(awards::japanPrefecture(QString()).isEmpty());
    }

    void theDistrictIsTheDigitOfTheCallsign()
    {
        QCOMPARE(awards::japanDistrict(QStringLiteral("JA1ABC")), QStringLiteral("1"));
        QCOMPARE(awards::japanDistrict(QStringLiteral("JA0TUV")), QStringLiteral("0"));
        QCOMPARE(awards::japanDistrict(QStringLiteral("7K4XYZ")), QStringLiteral("4"));
        QCOMPARE(awards::japanDistrict(QStringLiteral("JH1QRS/3")), QStringLiteral("1"));
        QVERIFY(awards::japanDistrict(QString()).isEmpty());
    }

    void workedAllAfricaCountsTheAfricanEntities()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        auto worked = [&db](const QString& call, const QString& cont, int dxcc, const QString& band) {
            AdifRecord r;
            r.set(QStringLiteral("CALL"), call);
            r.set(QStringLiteral("QSO_DATE"), QStringLiteral("20260918"));
            r.set(QStringLiteral("TIME_ON"), QStringLiteral("120000"));
            r.set(QStringLiteral("BAND"), band);
            r.set(QStringLiteral("MODE"), QStringLiteral("CW"));
            r.set(QStringLiteral("CONT"), cont);
            r.set(QStringLiteral("DXCC"), QString::number(dxcc));
            db.insertQso(r, QStringLiteral("test"));
        };
        worked(QStringLiteral("ZS6ABC"), QStringLiteral("AF"), 462, QStringLiteral("20m"));   // Sudafrica
        worked(QStringLiteral("7X2ABC"), QStringLiteral("AF"), 400, QStringLiteral("20m"));   // Algeria
        worked(QStringLiteral("ZS6XYZ"), QStringLiteral("AF"), 462, QStringLiteral("40m"));   // ancora Sudafrica
        worked(QStringLiteral("DL9ZZT"), QStringLiteral("EU"), 230, QStringLiteral("20m"));   // non conta
        worked(QStringLiteral("ZD8ABC"), QStringLiteral("AF"), 0, QStringLiteral("20m"));     // senza DXCC, non conta

        AwardCalculator calc;
        const auto results = calc.compute(db, AwardFilter{});
        const auto waac = std::find_if(results.cbegin(), results.cend(),
                                       [](const AwardResult& r) { return r.id == QLatin1String("waac"); });
        QVERIFY(waac != results.cend());
        QCOMPARE(waac->worked(), 2);     // due paesi africani, non tre QSO
        // E si legge banda per banda, come gli altri diplomi.
        const auto totals = waac->bandTotals({QStringLiteral("20m"), QStringLiteral("40m")});
        QCOMPARE(totals.at(0).worked, 2);
        QCOMPARE(totals.at(1).worked, 1);
    }

    void japaneseCitiesAndGuns()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        auto worked = [&db](const QString& call, const QString& county, const QString& band) {
            AdifRecord r;
            r.set(QStringLiteral("CALL"), call);
            r.set(QStringLiteral("QSO_DATE"), QStringLiteral("20260918"));
            r.set(QStringLiteral("TIME_ON"), QStringLiteral("120000"));
            r.set(QStringLiteral("BAND"), band);
            r.set(QStringLiteral("MODE"), QStringLiteral("CW"));
            r.set(QStringLiteral("DXCC"), QStringLiteral("339"));
            r.set(QStringLiteral("CNTY"), county);
            db.insertQso(r, QStringLiteral("test"));
        };
        worked(QStringLiteral("JA1ABC"), QStringLiteral("1001"), QStringLiteral("20m"));    // citta', Tokyo
        worked(QStringLiteral("JA1DEF"), QStringLiteral("100105"), QStringLiteral("20m"));  // quartiere, Tokyo
        worked(QStringLiteral("JA1GHI"), QStringLiteral("JCC 1001"), QStringLiteral("40m")); // la stessa citta'
        worked(QStringLiteral("JA3JKL"), QStringLiteral("25007"), QStringLiteral("20m"));   // gun, Osaka
        worked(QStringLiteral("JA3MNO"), QStringLiteral("99001"), QStringLiteral("20m"));   // prefettura che non esiste

        AwardCalculator calc;
        const auto results = calc.compute(db, AwardFilter{});
        const auto jcc = std::find_if(results.cbegin(), results.cend(),
                                      [](const AwardResult& r) { return r.id == QLatin1String("jcc"); });
        const auto jcg = std::find_if(results.cbegin(), results.cend(),
                                      [](const AwardResult& r) { return r.id == QLatin1String("jcg"); });
        QVERIFY(jcc != results.cend() && jcg != results.cend());
        QCOMPARE(jcc->worked(), 2);      // 1001 una volta sola, piu' il quartiere 100105
        QCOMPARE(jcc->target, 100);
        QCOMPARE(jcg->worked(), 1);      // solo il gun di Osaka: 99001 non e' una prefettura
        QCOMPARE(jcg->target, 100);
        // Il numero porta con se' il nome della prefettura.
        const auto tokyo = std::find_if(jcc->items.cbegin(), jcc->items.cend(),
                                        [](const AwardItem& i) { return i.key == QLatin1String("1001"); });
        QVERIFY(tokyo != jcc->items.cend());
        QCOMPARE(tokyo->name, QStringLiteral("Tokyo"));
        // E si legge banda per banda: la citta' 1001 e' stata fatta su due bande.
        const auto totals = jcc->bandTotals({QStringLiteral("20m"), QStringLiteral("40m")});
        QCOMPARE(totals.at(0).worked, 2);
        QCOMPARE(totals.at(1).worked, 1);
    }

    void italianProvincesAreCountedForWaip()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        auto worked = [&db](const QString& call, int dxcc, const QString& state, const QString& band) {
            AdifRecord r;
            r.set(QStringLiteral("CALL"), call);
            r.set(QStringLiteral("QSO_DATE"), QStringLiteral("20260918"));
            r.set(QStringLiteral("TIME_ON"), QStringLiteral("120000"));
            r.set(QStringLiteral("BAND"), band);
            r.set(QStringLiteral("MODE"), QStringLiteral("CW"));
            r.set(QStringLiteral("DXCC"), QString::number(dxcc));
            r.set(QStringLiteral("STATE"), state);
            db.insertQso(r, QStringLiteral("test"));
        };
        worked(QStringLiteral("IU8LMC"), 248, QStringLiteral("NA"), QStringLiteral("20m"));
        worked(QStringLiteral("IK0ABC"), 248, QStringLiteral("RM"), QStringLiteral("20m"));
        worked(QStringLiteral("IZ8XYZ"), 248, QStringLiteral("na"), QStringLiteral("40m"));  // la stessa, in minuscolo
        // La Sardegna e' un'altra entita' DXCC, ma le sue province contano.
        worked(QStringLiteral("IS0ABC"), 225, QStringLiteral("CA"), QStringLiteral("20m"));
        // Una provincia che non esiste, e una stazione che italiana non e'.
        worked(QStringLiteral("IK1QQQ"), 248, QStringLiteral("ZZ"), QStringLiteral("20m"));
        worked(QStringLiteral("DL9ZZT"), 230, QStringLiteral("NA"), QStringLiteral("20m"));

        AwardCalculator calc;
        const auto results = calc.compute(db, AwardFilter{});
        const auto waip = std::find_if(results.cbegin(), results.cend(),
                                       [](const AwardResult& r) { return r.id == QLatin1String("waip"); });
        QVERIFY(waip != results.cend());
        QCOMPARE(waip->worked(), 3);      // NA, RM, CA — non sei QSO
        QCOMPARE(waip->total, 110);
        QCOMPARE(waip->target, 75);
        // La sigla porta con se' il nome, che e' quello che si legge nell'elenco.
        const auto napoli = std::find_if(waip->items.cbegin(), waip->items.cend(),
                                         [](const AwardItem& i) { return i.key == QLatin1String("NA"); });
        QVERIFY(napoli != waip->items.cend());
        QCOMPARE(napoli->name, QStringLiteral("Napoli"));
        // E si legge banda per banda, come gli altri diplomi.
        const auto totals = waip->bandTotals({QStringLiteral("20m"), QStringLiteral("40m")});
        QCOMPARE(totals.at(0).worked, 3);
        QCOMPARE(totals.at(1).worked, 1);
    }

    void theProvinceIsReadHoweverItIsWritten()
    {
        QCOMPARE(awards::italianProvince(QStringLiteral("NA")), QStringLiteral("NA"));
        QCOMPARE(awards::italianProvince(QStringLiteral("na")), QStringLiteral("NA"));
        QCOMPARE(awards::italianProvince(QStringLiteral("I-NA")), QStringLiteral("NA"));
        QCOMPARE(awards::italianProvince(QStringLiteral("NA Napoli")), QStringLiteral("NA"));
        // Carbonia-Iglesias non c'e' piu': i QSO di allora vanno a Sud Sardegna,
        // invece di restare senza provincia.
        QCOMPARE(awards::italianProvince(QStringLiteral("CI")), QStringLiteral("SU"));
        // Le sarde soppresse che il WAIP conta ancora ci sono tutte e tre.
        QCOMPARE(awards::italianProvince(QStringLiteral("OG")), QStringLiteral("OG"));
        QCOMPARE(awards::italianProvince(QStringLiteral("OT")), QStringLiteral("OT"));
        QCOMPARE(awards::italianProvince(QStringLiteral("VS")), QStringLiteral("VS"));
        // Quello che non e' una provincia resta fuori: contarlo vorrebbe dire
        // dire a chi guarda che ha una provincia che non ha lavorato.
        QVERIFY(awards::italianProvince(QStringLiteral("ZZ")).isEmpty());
        QVERIFY(awards::italianProvince(QStringLiteral("")).isEmpty());
        QVERIFY(awards::italianProvince(QStringLiteral("12")).isEmpty());
        // E sono centodieci, come dice il regolamento.
        QCOMPARE(awards::italianProvinces().size(), 110);
    }

    void castlesAreCountedForDci()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        auto worked = [&db](const QString& call, const QString& sig, const QString& sigInfo,
                            const QString& comment, const QString& band) {
            AdifRecord r;
            r.set(QStringLiteral("CALL"), call);
            r.set(QStringLiteral("QSO_DATE"), QStringLiteral("20260918"));
            r.set(QStringLiteral("TIME_ON"), QStringLiteral("120000"));
            r.set(QStringLiteral("BAND"), band);
            r.set(QStringLiteral("MODE"), QStringLiteral("CW"));
            r.set(QStringLiteral("DXCC"), QStringLiteral("248"));
            if (!sig.isEmpty())
                r.set(QStringLiteral("SIG"), sig);
            if (!sigInfo.isEmpty())
                r.set(QStringLiteral("SIG_INFO"), sigInfo);
            if (!comment.isEmpty())
                r.set(QStringLiteral("COMMENT"), comment);
            db.insertQso(r, QStringLiteral("test"));
        };
        // Il log fatto bene: il programma e il riferimento al posto loro.
        worked(QStringLiteral("IK0ABC"), QStringLiteral("DCI"), QStringLiteral("PR001"),
               QString(), QStringLiteral("20m"));
        // Lo stesso castello su un'altra banda, scritto come capita.
        worked(QStringLiteral("IK0DEF"), QString(), QString(),
               QStringLiteral("DCI PR-001 tnx"), QStringLiteral("40m"));
        // Un altro castello, in minuscolo nel commento.
        worked(QStringLiteral("IZ8GHI"), QString(), QString(),
               QStringLiteral("dci na015"), QStringLiteral("20m"));
        // Un numero qualunque nel commento non e' un castello.
        worked(QStringLiteral("IW1JKL"), QString(), QString(),
               QStringLiteral("TNX 001 73"), QStringLiteral("20m"));
        // E nemmeno una provincia che non esiste.
        worked(QStringLiteral("IK2MNO"), QStringLiteral("DCI"), QStringLiteral("ZZ001"),
               QString(), QStringLiteral("20m"));

        AwardCalculator calc;
        const auto results = calc.compute(db, AwardFilter{});
        const auto dci = std::find_if(results.cbegin(), results.cend(),
                                      [](const AwardResult& r) { return r.id == QLatin1String("dci"); });
        QVERIFY(dci != results.cend());
        QCOMPARE(dci->worked(), 2);       // PR001 una volta sola, piu' NA015
        const auto parma = std::find_if(dci->items.cbegin(), dci->items.cend(),
                                        [](const AwardItem& i) { return i.key == QLatin1String("PR001"); });
        QVERIFY(parma != dci->items.cend());
        QCOMPARE(parma->name, QStringLiteral("Parma"));
        QCOMPARE(parma->qsoCount, 2);
        const auto totals = dci->bandTotals({QStringLiteral("20m"), QStringLiteral("40m")});
        QCOMPARE(totals.at(0).worked, 2);
        QCOMPARE(totals.at(1).worked, 1);
    }

    void theCastleReferenceIsReadHoweverItIsWritten()
    {
        const QString none;
        QCOMPARE(awards::dciReference(QStringLiteral("DCI"), QStringLiteral("PR001"), none, none),
                 QStringLiteral("PR001"));
        QCOMPARE(awards::dciReference(QStringLiteral("dci"), QStringLiteral("pr-001"), none, none),
                 QStringLiteral("PR001"));
        QCOMPARE(awards::dciReference(none, none, QStringLiteral("DCI PR001"), none),
                 QStringLiteral("PR001"));
        QCOMPARE(awards::dciReference(none, none, none, QStringLiteral("attivo dci: NA-015 oggi")),
                 QStringLiteral("NA015"));
        // Senza la parola DCI davanti, nel testo libero non si indovina.
        QVERIFY(awards::dciReference(none, none, QStringLiteral("PR001"), none).isEmpty());
        QVERIFY(awards::dciReference(none, none, QStringLiteral("TNX 001"), none).isEmpty());
        // Una provincia che non esiste non e' un castello.
        QVERIFY(awards::dciReference(QStringLiteral("DCI"), QStringLiteral("ZZ001"), none, none).isEmpty());
        // Un altro programma nello stesso campo non diventa un castello.
        QVERIFY(awards::dciReference(QStringLiteral("GMA"), QStringLiteral("I/PI-001"), none, none).isEmpty());
    }

    void theJarlNumberIsReadHoweverItIsWritten()
    {
        QCOMPARE(awards::japanJarlCode(QStringLiteral("1001")), QStringLiteral("1001"));
        QCOMPARE(awards::japanJarlCode(QStringLiteral("10-01")), QStringLiteral("1001"));
        QCOMPARE(awards::japanJarlCode(QStringLiteral("JCC 1001")), QStringLiteral("1001"));
        QCOMPARE(awards::japanJarlCode(QStringLiteral("100105")), QStringLiteral("100105"));
        QCOMPARE(awards::japanJarlCode(QStringLiteral("01001")), QStringLiteral("01001"));
        // Troppo corto, troppo lungo, o una prefettura che non c'e'.
        QVERIFY(awards::japanJarlCode(QStringLiteral("100")).isEmpty());
        QVERIFY(awards::japanJarlCode(QStringLiteral("1234567")).isEmpty());
        QVERIFY(awards::japanJarlCode(QStringLiteral("99001")).isEmpty());
        QVERIFY(awards::japanJarlCode(QStringLiteral("Marion")).isEmpty());
        // Cinque cifre vuol dire distretto, quattro o sei vuol dire citta'.
        QVERIFY(awards::isJapanGun(QStringLiteral("25007")));
        QVERIFY(!awards::isJapanGun(QStringLiteral("1001")));
        QVERIFY(!awards::isJapanGun(QStringLiteral("100105")));
    }
};

QTEST_GUILESS_MAIN(TestAwards)
#include "tst_awards.moc"
