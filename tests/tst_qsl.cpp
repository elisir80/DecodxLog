// Invio QSL: codici d'uscita di TQSL, risposte di QRZ Logbook ed eQSL, coda e
// stato per servizio nel database.
#include "core/LogDatabase.h"
#include "core/QslUpload.h"

#include <QTest>

using namespace decolog::core;

class TestQsl : public QObject {
    Q_OBJECT

private slots:
    void tqslExitCodes()
    {
        auto r = qsl::resultFromTqslExit(0, "Sending to LoTW", 12);
        QVERIFY(r.ok);
        QCOMPARE(r.accepted, 12);

        r = qsl::resultFromTqslExit(7, "All QSOs were duplicates", 5);
        QVERIFY(r.ok);
        QCOMPARE(r.duplicates, 5);
        QCOMPARE(r.accepted, 0);

        r = qsl::resultFromTqslExit(8, "Some QSOs were duplicates", 5);
        QVERIFY(r.ok);
        QCOMPARE(r.accepted, 5);

        r = qsl::resultFromTqslExit(2, "rejected", 3);
        QVERIFY(!r.ok);
        QCOMPARE(r.rejected, 3);
        QVERIFY(!r.retryLater);

        r = qsl::resultFromTqslExit(10, "connection failed", 3);
        QVERIFY(!r.ok);
        QVERIFY(r.retryLater);

        r = qsl::resultFromTqslExit(5, "no station location", 3);
        QVERIFY(!r.ok);
        QVERIFY(r.message.contains("station location"));
    }

    void qrzAnswers()
    {
        auto r = qsl::parseQrzResponse("RESULT=OK&LOGID=123456&COUNT=1");
        QVERIFY(r.ok);
        QCOMPARE(r.accepted, 1);
        QCOMPARE(r.remoteId, QString("123456"));

        r = qsl::parseQrzResponse("RESULT=FAIL&REASON=Unable to add QSO to database: duplicate");
        QVERIFY(r.ok);
        QCOMPARE(r.duplicates, 1);

        r = qsl::parseQrzResponse("RESULT=AUTH&REASON=invalid api key");
        QVERIFY(!r.ok);
        QCOMPARE(r.rejected, 1);
        QVERIFY(r.message.contains("api key"));

        r = qsl::parseQrzResponse("<html>gateway timeout</html>");
        QVERIFY(!r.ok);
        QVERIFY(r.retryLater);
    }

    void eqslAnswers()
    {
        auto r = qsl::parseEqslResponse("<html><body>Result: 1 out of 1 records added</body></html>");
        QVERIFY(r.ok);
        QCOMPARE(r.accepted, 1);

        r = qsl::parseEqslResponse("<html>Result: 0 out of 1 records added<br>Warning: Duplicate</html>");
        QVERIFY(r.ok);
        QCOMPARE(r.duplicates, 1);

        r = qsl::parseEqslResponse("<html>Bad record: no callsign</html>");
        QVERIFY(!r.ok);
        QCOMPARE(r.rejected, 1);

        r = qsl::parseEqslResponse("");
        QVERIFY(!r.ok);
        QVERIFY(r.retryLater);
    }

    void queueAndState()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        const qint64 a = db.insertQso({{"CALL", "K1ABC"}, {"QSO_DATE", "20260101"}, {"TIME_ON", "1200"},
                                       {"BAND", "20m"}, {"MODE", "FT8"}}, "import").id;
        const qint64 b = db.insertQso({{"CALL", "JA1XX"}, {"QSO_DATE", "20260102"}, {"TIME_ON", "1200"},
                                       {"BAND", "40m"}, {"MODE", "FT8"}, {"LOTW_QSL_SENT", "Y"}}, "import").id;
        QVERIFY(a > 0 && b > 0);

        // b e' gia' inviato a LoTW, a no.
        QCOMPARE(db.uploadPendingCount("lotw"), 1);
        QCOMPARE(db.qsosToUpload("lotw"), QList<qint64>({a}));
        QCOMPARE(db.uploadPendingCount("qrz"), 2);

        QslState sent;
        sent.service = "qrz";
        sent.sent = "Y";
        sent.sentDate = "20260917";
        sent.remoteId = "999";
        QVERIFY(db.setQslState(a, sent));
        QCOMPARE(db.uploadPendingCount("qrz"), 1);
        QCOMPARE(db.qsosToUpload("qrz"), QList<qint64>({b}));
        // Lo stato QSL non crea una revisione nuova.
        QCOMPARE(db.meta(a)->revision, 1);
        QVERIFY(db.meta(a)->dirty);
        QCOMPARE(db.record(a)->value("QRZCOM_QSO_UPLOAD_STATUS"), QString("Y"));

        // Un rifiuto resta scritto e il QSO torna in coda.
        QslState failed;
        failed.service = "eqsl";
        failed.sent = "N";
        failed.lastError = "Bad record";
        QVERIFY(db.setQslState(a, failed));
        QCOMPARE(db.uploadPendingCount("eqsl"), 2);
        bool found = false;
        for (const QslState& st : db.qslStatus(a)) {
            if (st.service == "eqsl") {
                QCOMPARE(st.lastError, QString("Bad record"));
                found = true;
            }
        }
        QVERIFY(found);

        // La conferma ricevuta non si perde quando si riscrive l'invio.
        QslState confirmed;
        confirmed.service = "lotw";
        confirmed.sent = "Y";
        confirmed.rcvd = "Y";
        confirmed.rcvdDate = "20260918";
        QVERIFY(db.setQslState(a, confirmed));
        QslState again;
        again.service = "lotw";
        again.sent = "Y";
        again.rcvd = "Y";
        again.rcvdDate = "20260918";
        QVERIFY(db.setQslState(a, again));
        QCOMPARE(db.record(a)->value("LOTW_QSL_RCVD"), QString("Y"));
    }

    void tqslDiscovery()
    {
        // Su questa macchina TQSL c'e' o non c'e': in ogni caso non deve esplodere.
        const QString path = qsl::findTqsl();
        qInfo() << "tqsl:" << path << "certificato:" << qsl::tqslHasCertificate()
                << "station location:" << qsl::tqslStationLocations();
        if (!path.isEmpty())
            QVERIFY(path.contains("tqsl", Qt::CaseInsensitive));
    }
};

QTEST_GUILESS_MAIN(TestQsl)
#include "tst_qsl.moc"
