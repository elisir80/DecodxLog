// Attivazioni e contest: campi dell'attivatore sul QSO, numero progressivo,
// nome del file per POTA, e il giro completo di una sessione sul log.
#include "app/ActivationController.h"
#include "core/Activation.h"
#include "core/LogDatabase.h"

#include <QTest>

using namespace decolog::core;
using decolog::app::ActivationController;

class TestActivation : public QObject {
    Q_OBJECT

private slots:
    void potaFieldsAndFileName()
    {
        Activation a;
        a.active = true;
        a.kind = Activation::Kind::Pota;
        a.reference = "IT-1234";
        a.myGrid = "JN71DC";
        a.tag = "pota";
        QCOMPARE(a.requiredQsos(), 10);

        AdifRecord r{{"CALL", "K1ABC"}, {"BAND", "20m"}, {"MODE", "FT8"}};
        a.applyTo(r, 0);
        // I campi dell'attivatore, non quelli del cacciatore.
        QCOMPARE(r.value("MY_SIG"), QString("POTA"));
        QCOMPARE(r.value("MY_SIG_INFO"), QString("IT-1234"));
        QCOMPARE(r.value("MY_POTA_REF"), QString("IT-1234"));
        QVERIFY(r.value("POTA_REF").isEmpty());
        QCOMPARE(r.value("MY_GRIDSQUARE"), QString("JN71DC"));
        QCOMPARE(r.value("APP_DECOLOG_TAGS"), QString("pota"));
        QCOMPARE(a.exportFileName("IU8LMC/P", QDate(2026, 9, 17)), QString("IU8LMC_P@IT-1234-20260917.adi"));

        // Un campo che il QSO ha gia' non si tocca.
        AdifRecord mine{{"CALL", "K1ABC"}, {"MY_GRIDSQUARE", "JN70AA"}, {"APP_DECOLOG_TAGS", "pota,test"}};
        a.applyTo(mine, 0);
        QCOMPARE(mine.value("MY_GRIDSQUARE"), QString("JN70AA"));
        QCOMPARE(mine.value("APP_DECOLOG_TAGS"), QString("pota,test"));
    }

    void contestSerial()
    {
        Activation a;
        a.active = true;
        a.kind = Activation::Kind::Contest;
        a.contestId = "CQ-WW-SSB";
        a.serialEnabled = true;
        AdifRecord r{{"CALL", "K1ABC"}};
        a.applyTo(r, 7);
        QCOMPARE(r.value("CONTEST_ID"), QString("CQ-WW-SSB"));
        QCOMPARE(r.value("STX"), QString("7"));
        QCOMPARE(r.value("APP_DECOLOG_TAGS"), QString("cq-ww-ssb"));
        QCOMPARE(a.exportFileName("IU8LMC", QDate(2026, 9, 17)), QString("IU8LMC-CQ-WW-SSB-20260917.adi"));

        const Activation back = Activation::fromMap(a.toMap());
        QCOMPARE(back.contestId, a.contestId);
        QVERIFY(back.serialEnabled);
    }

    void theContestKnowsWhatEachSpotIsWorth()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        ActivationController::Context ctx;
        ctx.db = &db;
        ctx.stationCall = [] { return QStringLiteral("IU8LMC"); };
        ctx.stationGrid = [] { return QStringLiteral("JN70"); };
        ctx.activeProfileId = [] { return qint64(0); };
        // Io sono in Italia: Europa, zona CQ 15.
        ctx.station = [] { return decolog::core::ContestStation{248, QStringLiteral("EU"), 15, 28}; };
        // Un cty.csv in miniatura: quel tanto che basta alla prova.
        ctx.locate = [](const QString& call) {
            if (call.startsWith(QStringLiteral("W")))
                return decolog::core::ContestStation{291, QStringLiteral("NA"), 5, 8};
            if (call.startsWith(QStringLiteral("JA")))
                return decolog::core::ContestStation{339, QStringLiteral("AS"), 25, 45};
            if (call.startsWith(QStringLiteral("DL")))
                return decolog::core::ContestStation{230, QStringLiteral("EU"), 14, 28};
            return decolog::core::ContestStation{};
        };
        ActivationController act(std::move(ctx));
        act.load();
        QVERIFY(act.start({{"kind", "contest"}, {"contestId", "CQ-WW-CW"},
                           {"serialEnabled", true}}).isEmpty());

        // Prima di lavorare qualcuno, ogni spot porta due moltiplicatori nuovi.
        const QVariantMap before = act.spotValue(QStringLiteral("W1AW"), QStringLiteral("20m"),
                                                 QStringLiteral("CW"));
        QCOMPARE(before.value("points").toInt(), 3);
        QCOMPARE(before.value("newMultiplier").toBool(), true);
        QCOMPARE(before.value("duplicate").toBool(), false);

        auto log = [&db, &act](const char* call, const char* band, const char* mode,
                               int dxcc, const char* cont, int cqz) {
            AdifRecord r{{"CALL", call}, {"QSO_DATE", QDateTime::currentDateTimeUtc().toString("yyyyMMdd")},
                         {"TIME_ON", QDateTime::currentDateTimeUtc().toString("hhmmss")},
                         {"BAND", band}, {"MODE", mode}, {"DXCC", QString::number(dxcc)},
                         {"CONT", cont}, {"CQZ", QString::number(cqz)}};
            act.applyTo(r);
            const auto res = db.insertQso(r, "manual", {}, true);
            if (res.status == InsertResult::Status::Inserted)
                act.qsoLogged();
            return res;
        };
        QCOMPARE(log("W1AW", "20m", "CW", 291, "NA", 5).status, InsertResult::Status::Inserted);

        // Adesso il punteggio c'e': 3 punti, zona 5 e paese 291 sui 20 metri.
        const QVariantMap score = act.score();
        QCOMPARE(score.value("valid").toBool(), true);
        QCOMPARE(score.value("points").toInt(), 3);
        QCOMPARE(score.value("multipliers").toInt(), 2);
        QCOMPARE(score.value("score").toInt(), 6);

        // Un altro americano sulla stessa banda non porta piu' niente: stessa
        // zona, stesso paese. E' quello che il cluster deve smettere di segnare.
        const QVariantMap again = act.spotValue(QStringLiteral("W2XX"), QStringLiteral("20m"),
                                                QStringLiteral("CW"));
        QCOMPARE(again.value("points").toInt(), 3);
        QCOMPARE(again.value("newMultiplier").toBool(), false);
        // Ma sui 40 metri sono moltiplicatori nuovi: nel CQ WW si contano per
        // banda, ed e' per questo che si cambia banda.
        const QVariantMap lower = act.spotValue(QStringLiteral("W2XX"), QStringLiteral("40m"),
                                                QStringLiteral("CW"));
        QCOMPARE(lower.value("newMultiplier").toBool(), true);
        // Lo stesso nominativo gia' lavorato su quella banda e' un duplicato.
        const QVariantMap dupe = act.spotValue(QStringLiteral("W1AW"), QStringLiteral("20m"),
                                               QStringLiteral("CW"));
        QCOMPARE(dupe.value("duplicate").toBool(), true);

        // Il giapponese porta zona e paese nuovi, e vale 3 punti.
        const QVariantMap japan = act.spotValue(QStringLiteral("JA1ABC"), QStringLiteral("20m"),
                                                QStringLiteral("CW"));
        QCOMPARE(japan.value("points").toInt(), 3);
        QCOMPARE(japan.value("newMultiplier").toBool(), true);

        // Il tedesco e' nel mio continente: 1 punto.
        const QVariantMap german = act.spotValue(QStringLiteral("DL9ZZT"), QStringLiteral("20m"),
                                                 QStringLiteral("CW"));
        QCOMPARE(german.value("points").toInt(), 1);

        // E il contest e' in CW: la finestra della telegrafia serve.
        QVERIFY(act.isCwContest());
    }

    void aContestWithoutRulesHasNoScoreAndSaysIt()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        ActivationController::Context ctx;
        ctx.db = &db;
        ctx.stationCall = [] { return QStringLiteral("IU8LMC"); };
        ctx.stationGrid = [] { return QStringLiteral("JN70"); };
        ctx.activeProfileId = [] { return qint64(0); };
        ActivationController act(std::move(ctx));
        act.load();
        QVERIFY(act.start({{"kind", "contest"}, {"contestId", "AL-QSO-PARTY"}}).isEmpty());

        const QVariantMap score = act.score();
        QCOMPARE(score.value("valid").toBool(), false);
        QCOMPARE(score.value("score").toInt(), 0);
        // E nessuno spot risulta moltiplicatore: meglio niente che un numero
        // inventato.
        QCOMPARE(act.spotValue(QStringLiteral("W1AW"), QStringLiteral("20m"),
                               QStringLiteral("SSB")).value("newMultiplier").toBool(), false);
        // Un contest in SSB non apre la finestra della telegrafia.
        QVERIFY(!act.isCwContest());
    }

    void sessionOnTheLog()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        QStringList notes;
        ActivationController::Context ctx;
        ctx.db = &db;
        ctx.stationCall = [] { return QStringLiteral("IU8LMC/P"); };
        ctx.stationGrid = [] { return QStringLiteral("JN71DC"); };
        ctx.activeProfileId = [] { return qint64(0); };
        ctx.activity = [&notes](const QString&, const QString& text, const QString&) { notes << text; };
        ActivationController act(std::move(ctx));
        act.load();
        QVERIFY(!act.active());

        // Senza referenza non parte.
        QVERIFY(!act.start({{"kind", "pota"}}).isEmpty());
        QVERIFY(act.start({{"kind", "pota"}, {"reference", "it-1234"}, {"serialEnabled", true}}).isEmpty());
        QVERIFY(act.active());
        QCOMPARE(act.requiredQsos(), 10);
        QCOMPARE(act.nextSerial(), 1);
        QCOMPARE(act.suggestedFileName().left(18), QString("IU8LMC_P@IT-1234-2"));

        auto log = [&db, &act](const char* call, const char* band, const char* mode) {
            AdifRecord r{{"CALL", call}, {"QSO_DATE", QDateTime::currentDateTimeUtc().toString("yyyyMMdd")},
                         {"TIME_ON", QDateTime::currentDateTimeUtc().toString("hhmmss")},
                         {"BAND", band}, {"MODE", mode}};
            act.applyTo(r);
            const auto res = db.insertQso(r, "manual", {}, true);
            if (res.status == InsertResult::Status::Inserted)
                act.qsoLogged();
            return res;
        };

        QCOMPARE(log("K1ABC", "20m", "FT8").status, InsertResult::Status::Inserted);
        QCOMPARE(act.qsoCount(), 1);
        QCOMPARE(act.nextSerial(), 2);
        QCOMPARE(log("JA1XX", "20m", "FT8").status, InsertResult::Status::Inserted);
        QCOMPARE(act.qsoCount(), 2);
        QCOMPARE(act.uniqueCalls(), 2);

        // Dentro l'attivazione K1ABC su 20m FT8 e' un duplicato, su 40m no.
        QVERIFY(act.isDuplicate("k1abc", "20M", "FT8"));
        QVERIFY(!act.isDuplicate("K1ABC", "40m", "FT8"));
        QVERIFY(!act.isDuplicate("K1ABC", "20m", "CW"));

        // Il primo QSO ha la referenza e il numero.
        const auto first = db.record(db.qsosToUpload("lotw").first());
        QVERIFY(first);
        QCOMPARE(first->value("MY_SIG_INFO"), QString("IT-1234"));
        QCOMPARE(first->value("STX"), QString("1"));
        QCOMPARE(first->value("APP_DECOLOG_TAGS"), QString("pota"));

        QCOMPARE(act.qsoIds().size(), 2);
        QCOMPARE(act.perBand().size(), 1);

        // La sessione sopravvive alla chiusura del programma.
        ActivationController again(ActivationController::Context{&db, ctx.stationCall, ctx.stationGrid,
                                                                 ctx.activeProfileId, {}, {}});
        again.load();
        QVERIFY(again.active());
        QCOMPARE(again.session().reference, QString("IT-1234"));
        QCOMPARE(again.qsoCount(), 2);
        QCOMPARE(again.nextSerial(), 3);

        act.stop();
        QVERIFY(!act.active());
        QVERIFY(!act.isDuplicate("K1ABC", "20m", "FT8"));
        QVERIFY(notes.last().contains("2"));
    }
};

QTEST_GUILESS_MAIN(TestActivation)
#include "tst_activation.moc"
