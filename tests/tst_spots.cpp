// Spot del cluster: lettura dei formati (DX Spider, RBN, HamAlert, POTA), modo
// dal piano di banda, stato rispetto al log e filtri.
#include "core/LogDatabase.h"
#include "core/Spots.h"
#include "core/VoiceAnnouncer.h"

#include <QTest>
#include <QTimeZone>

using namespace decolog::core;

namespace {

const QDateTime kNow(QDate(2026, 9, 17), QTime(12, 40), QTimeZone::UTC);

EnrichedSpot enriched(const Spot& s, int dxcc = 0, const QString& continent = {}, int status = 0)
{
    EnrichedSpot e;
    e.spot = s;
    e.dxcc = dxcc;
    e.continent = continent;
    e.status = status;
    return e;
}

} // namespace

class TestSpots : public QObject {
    Q_OBJECT

private slots:
    void clusterLine()
    {
        const auto s = spots::parseDxLine(
            "DX de IK8XXX:     14074.0  JA1YYY       FT8 -12 dB 1234 Hz  JN71        1238Z JN70", kNow);
        QVERIFY(s);
        QCOMPARE(s->spotter, QString("IK8XXX"));
        QCOMPARE(s->dxCall, QString("JA1YYY"));
        QCOMPARE(s->freqKhz, 14074.0);
        QCOMPARE(s->band, QString("20m"));
        QCOMPARE(s->mode, QString("FT8"));
        QVERIFY(s->hasSnr);
        QCOMPARE(s->snr, -12);
        QCOMPARE(s->time, QDateTime(QDate(2026, 9, 17), QTime(12, 38), QTimeZone::UTC));
        QCOMPARE(s->dxGrid, QString("JN71"));
        QVERIFY(!s->isSkimmer());
    }

    void rbnLines()
    {
        const auto cw = spots::parseDxLine("DX de EA5WU-#:    14025.1  K1ABC          CW    18 dB  24 WPM  CQ      1239Z", kNow);
        QVERIFY(cw);
        QCOMPARE(cw->spotter, QString("EA5WU-#"));
        QVERIFY(cw->isSkimmer());
        QCOMPARE(cw->mode, QString("CW"));
        QCOMPARE(cw->snr, 18);
        QCOMPARE(cw->wpm, 24);

        // Spot della sera prima, letto dopo mezzanotte: data di ieri.
        const QDateTime justAfterMidnight(QDate(2026, 9, 18), QTime(0, 2), QTimeZone::UTC);
        const auto late = spots::parseDxLine("DX de W3LPL:  7005.0  VP8ABC  up 2  2358Z", justAfterMidnight);
        QVERIFY(late);
        QCOMPARE(late->time.date(), QDate(2026, 9, 17));
        QCOMPARE(late->mode, QString("CW"));   // dal piano di banda

        QVERIFY(!spots::parseDxLine("WWV de W0MU <18>:   SFI=150, A=5, K=1", kNow));
        QVERIFY(!spots::parseDxLine("DX de IK8XXX:  14074.0  HELLO  test  1238Z", kNow));
    }

    void showDxLine()
    {
        const auto s = spots::parseShowDxLine("   21074.0 II7IAME     17-Sep-2026 1231Z FT8 Italian Navy Ship        <IU7EDX>", kNow);
        QVERIFY(s);
        QCOMPARE(s->dxCall, QString("II7IAME"));
        QCOMPARE(s->spotter, QString("IU7EDX"));
        QCOMPARE(s->mode, QString("FT8"));
        QCOMPARE(s->band, QString("15m"));
        QCOMPARE(s->time, QDateTime(QDate(2026, 9, 17), QTime(12, 31), QTimeZone::UTC));
        QVERIFY(!spots::parseShowDxLine(" 17-Sep-2026   12   100   9   2 No Storms -> Minor w/G1               <VE7CC>", kNow));
    }

    void modeFromFrequency()
    {
        QCOMPARE(spots::modeFor(14075.3, ""), QString("FT8"));
        QCOMPARE(spots::modeFor(14081.2, ""), QString("FT4"));
        QCOMPARE(spots::modeFor(14085.0, ""), QString("FT2"));
        QCOMPARE(spots::modeFor(50317.0, ""), QString("FT2"));
        QCOMPARE(spots::modeFor(14020.0, ""), QString("CW"));
        QCOMPARE(spots::modeFor(14200.0, ""), QString("SSB"));
        QCOMPARE(spots::modeFor(7090.0, "FT8 dx"), QString("FT8"));   // il commento vince
        QCOMPARE(spots::modeFor(14083.0, "JTTY test"), QString("JTTY"));
        QCOMPARE(spots::modeFor(21300.0, "pse QSL"), QString("SSB"));
        QCOMPARE(spots::modeKey("USB"), QString("PHONE"));
        QCOMPARE(spots::modeKey("DIGI"), QString());
    }

    void references()
    {
        auto s = spots::parseDxLine("DX de K1ABC:  14062.0  W1XYZ  POTA US-1511 CW  1230Z", kNow);
        QVERIFY(s);
        QCOMPARE(s->potaRef, QString("US-1511"));
        s = spots::parseDxLine("DX de K1ABC:  14285.0  OE5XYZ  SOTA OE/OO-123 IOTA EU-005 IFF-0123  1230Z", kNow);
        QVERIFY(s);
        QCOMPARE(s->sotaRef, QString("OE/OO-123"));
        QCOMPARE(s->iotaRef, QString("EU-005"));
        QCOMPARE(s->wwffRef, QString("IFF-0123"));
    }

    void hamAlertJson()
    {
        const QByteArray line = R"({"fullCallsign":"3Y0J","callsign":"3Y0J","frequency":"14.0245","band":"20m","mode":"cw",)"
                                R"("time":"12:31","spotter":"DL1ABC","comment":"up 1","source":"cluster","entity":"Bouvet","dxcc":24})";
        const auto s = spots::parseHamAlertJson(line, kNow);
        QVERIFY(s);
        QCOMPARE(s->dxCall, QString("3Y0J"));
        QCOMPARE(s->freqKhz, 14024.5);
        QCOMPARE(s->mode, QString("CW"));
        QCOMPARE(s->source, QString("hamalert"));
        QCOMPARE(s->sourceName, QString("HamAlert · cluster"));
        QCOMPARE(s->time.time(), QTime(12, 31));
        QVERIFY(!spots::parseHamAlertJson("not json", kNow));
    }

    void potaJson()
    {
        const QByteArray json = R"([{"spotId":1,"activator":"N8UC","frequency":"7075","mode":"FT8","reference":"US-1511",)"
                                R"("spotTime":"2026-09-17T11:43:45","spotter":"N8UC","comments":"QRT","name":"Kal-Haven Trail State Park",)"
                                R"("grid4":"EN72","grid6":"EN72dh"}])";
        const auto list = spots::parsePotaJson(json);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().dxCall, QString("N8UC"));
        QCOMPARE(list.first().freqKhz, 7075.0);
        QCOMPARE(list.first().band, QString("40m"));
        QCOMPARE(list.first().potaRef, QString("US-1511"));
        QCOMPARE(list.first().dxGrid, QString("EN72dh"));
        QCOMPARE(list.first().source, QString("pota"));
    }

    void statusFromLog()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        auto qso = [&db](const char* call, const char* band, const char* mode, const char* submode, int dxcc, bool lotw) {
            AdifRecord r{{"CALL", call}, {"QSO_DATE", "20260101"}, {"TIME_ON", "1200"}, {"BAND", band},
                         {"MODE", mode}, {"SUBMODE", submode}, {"DXCC", QString::number(dxcc)}};
            if (lotw)
                r.set("LOTW_QSL_RCVD", "Y");
            QCOMPARE(db.insertQso(r, "import").status, InsertResult::Status::Inserted);
        };
        qso("JA1XX", "20m", "FT8", "", 339, true);
        qso("K1ABC", "40m", "CW", "", 291, false);

        LogIndex index;
        index.rebuild(db, true, true, false);
        QCOMPARE(index.qsoCount(), 2);

        Spot s;
        s.dxCall = "3Y0J";
        s.band = "20m";
        s.mode = "CW";
        QCOMPARE(index.status(s, 24), StatusNewDxcc | StatusNewCall);

        s.dxCall = "JA1XX";
        s.mode = "FT8";
        QCOMPARE(index.status(s, 339), int(StatusWorkedBand));

        s.dxCall = "JA2YY";
        s.band = "40m";
        QCOMPARE(index.status(s, 339), StatusNewCall | StatusNewBand | StatusNewSlot);

        s.dxCall = "W1AW";
        s.band = "40m";
        s.mode = "SSB";
        QCOMPARE(index.status(s, 291), StatusNewCall | StatusNewMode | StatusNewSlot | StatusUnconfirmed);

        // Modo sconosciuto: niente "nuovo modo", e "gia' lavorato" guarda solo la banda.
        s.dxCall = "K1ABC";
        s.mode = "DIGI";
        QCOMPARE(index.status(s, 291), StatusWorkedBand | StatusUnconfirmed);
    }

    void filter()
    {
        Spot s;
        s.dxCall = "VP8ABC";
        s.band = "20m";
        s.mode = "USB";
        s.time = kNow.addSecs(-120);
        s.source = "cluster";
        s.comment = "Falklands";
        const EnrichedSpot e = enriched(s, 141, "SA", StatusNewBand);

        SpotFilter f;
        QVERIFY(f.isEmpty());
        QVERIFY(f.matches(e, kNow));
        f.modes = {"SSB"};
        f.bands = {"20m", "15m"};
        f.dxContinents = {"SA"};
        QVERIFY(f.matches(e, kNow));
        f.anyStatus = StatusNewDxcc;
        QVERIFY(!f.matches(e, kNow));
        f.anyStatus = StatusNewDxcc | StatusNewBand;
        QVERIFY(f.matches(e, kNow));
        f.calls = "3Y0J, VP8*";
        QVERIFY(f.matches(e, kNow));
        f.calls = "VP9*";
        QVERIFY(!f.matches(e, kNow));
        f.calls.clear();
        f.text = "falk";
        QVERIFY(f.matches(e, kNow));
        f.maxAgeMinutes = 1;
        QVERIFY(!f.matches(e, kNow));

        const SpotFilter back = SpotFilter::fromMap(f.toMap());
        QCOMPARE(back.modes, f.modes);
        QCOMPARE(back.anyStatus, f.anyStatus);
        QCOMPARE(back.maxAgeMinutes, 1);

        SpotFilter noSkimmer;
        noSkimmer.skimmers = false;
        Spot sk = s;
        sk.spotter = "EA5WU-#";
        QVERIFY(!noSkimmer.matches(enriched(sk), kNow));
    }

    void voice()
    {
        QCOMPARE(VoiceAnnouncer::spell("iu8lmc", false), QString("I U 8 L M C"));
        QCOMPARE(VoiceAnnouncer::spell("3Y0J/P", true), QString("3, Yankee, 0, Juliett, stroke, Papa"));
        // Si costruisce e dice che voci ci sono, senza parlare.
        VoiceAnnouncer v;
        qInfo() << "voice backend" << v.backend() << "available" << v.available() << "voices" << v.voices();
    }

    void tuning()
    {
        auto t = spots::tuningFor(14075.3, "FT8");
        QCOMPARE(t.dialKhz, 14074.0);
        QCOMPARE(t.audioHz, 1300);
        t = spots::tuningFor(14086.1, "FT2");
        QCOMPARE(t.dialKhz, 14084.0);
        QCOMPARE(t.audioHz, 2100);
        t = spots::tuningFor(14025.1, "CW");
        QCOMPARE(t.dialKhz, 14025.1);
        QCOMPARE(t.audioHz, 0);
    }

    void lotwUsers()
    {
        LotwUsers users;
        QCOMPARE(users.load("IU8LMC,2026-09-10,10:00:00\nK1ABC,2019-01-01,00:00:00\nbad line\n"), 2);
        QVERIFY(users.isActive("iu8lmc", 365, QDate(2026, 9, 17)));
        QVERIFY(users.isActive("IU8LMC/P", 365, QDate(2026, 9, 17)));
        QVERIFY(!users.isActive("K1ABC", 365, QDate(2026, 9, 17)));
        QVERIFY(!users.isActive("N0NE", 365, QDate(2026, 9, 17)));
    }
};

QTEST_GUILESS_MAIN(TestSpots)
#include "tst_spots.moc"
