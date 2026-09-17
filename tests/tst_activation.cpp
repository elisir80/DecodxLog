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
