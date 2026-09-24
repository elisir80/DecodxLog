// Invio QSL: codici d'uscita di TQSL, risposte di QRZ Logbook ed eQSL, coda e
// stato per servizio nel database.
#include "core/Adif.h"
#include "core/LogDatabase.h"
#include "core/QslUpload.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

using namespace decolog::core;

namespace {

// Un CRX finto: risponde sempre con `answer`, e si tiene la richiesta.
class FakeCrx : public QTcpServer {
public:
    QByteArray request;
    QByteArray body;
    QByteArray answer;
    int status{200};

    FakeCrx()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* socket = nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                request += socket->readAll();
                const int head = request.indexOf("\r\n\r\n");
                if (head < 0)
                    return;
                body = request.mid(head + 4);
                QByteArray length;
                for (const QByteArray& line : request.left(head).split('\n')) {
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(15).trimmed();
                }
                if (body.size() < length.toInt())
                    return;
                socket->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: application/json\r\n"
                              "Content-Length: " + QByteArray::number(answer.size()) + "\r\n\r\n" + answer);
                socket->disconnectFromHost();
            });
        });
    }
    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/api/").arg(serverPort())); }
};

} // namespace

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

    void clubLogAnswers()
    {
        auto r = qsl::parseClubLogResponse(200, "OK", 3);
        QVERIFY(r.ok);
        QCOMPARE(r.accepted, 3);

        r = qsl::parseClubLogResponse(200, "Duplicate QSO ignored", 1);
        QVERIFY(r.ok);
        QCOMPARE(r.duplicates, 1);
        QCOMPARE(r.accepted, 0);

        // Chiave o password sbagliate: e' inutile ritentare, e il motivo si legge.
        r = qsl::parseClubLogResponse(403, "Invalid API Key", 5);
        QVERIFY(!r.ok);
        QVERIFY(!r.retryLater);
        QCOMPARE(r.rejected, 5);
        QVERIFY(r.message.contains(QLatin1String("Invalid API Key")));

        r = qsl::parseClubLogResponse(500, "<html><body>Server error</body></html>", 2);
        QVERIFY(!r.ok);
        QVERIFY(r.retryLater);

        r = qsl::parseClubLogResponse(0, "", 1);
        QVERIFY(!r.ok);
        QVERIFY(r.retryLater);
    }

    void crxQsoFromTheLog()
    {
        // Il QSO come lo vuole CRX: campi logentry_*, frequenza in kHz, il modo
        // vero (FT8, non MFSK) e l'ora in secondi Unix, come in get_myqsos.
        AdifRecord r{{"CALL", "w1aw"}, {"QSO_DATE", "20241020"}, {"TIME_ON", "1248"}, {"BAND", "40M"},
                     {"FREQ", "7.025"}, {"MODE", "MFSK"}, {"SUBMODE", "FT4"}, {"RST_SENT", "-05"},
                     {"RST_RCVD", "-12"}, {"NAME", "John"}, {"COMMENT", "Good signal"}};
        const QJsonObject o = qsl::crxQsoData(r, 123, 0);
        QCOMPARE(o.value("qso_id").toInteger(), 0);
        QCOMPARE(o.value("f_log_id").toInteger(), 123);
        QCOMPARE(o.value("logentry_his_call").toString(), QString("W1AW"));
        QCOMPARE(o.value("logentry_band").toString(), QString("40m"));
        QCOMPARE(o.value("logentry_frequency").toString(), QString("7025"));
        QCOMPARE(o.value("logentry_mode").toString(), QString("FT4"));
        QCOMPARE(o.value("logentry_his_report").toString(), QString("-05"));
        QCOMPARE(o.value("logentry_my_report").toString(), QString("-12"));
        QCOMPARE(o.value("logentry_his_name").toString(), QString("John"));
        QCOMPARE(o.value("logentry_comment").toString(), QString("Good signal"));
        // 2024-10-20 12:48 UTC
        QCOMPARE(o.value("logentry_date").toInteger(), 1729428480);

        // Un QSO gia' mandato si corregge con il suo numero, e l'SSB resta SSB.
        AdifRecord ssb{{"CALL", "IK0ABC"}, {"QSO_DATE", "20260924"}, {"TIME_ON", "101530"}, {"BAND", "20m"},
                       {"FREQ", "14.2745"}, {"MODE", "SSB"}, {"SUBMODE", "USB"}};
        const QJsonObject again = qsl::crxQsoData(ssb, 123, 456);
        QCOMPARE(again.value("qso_id").toInteger(), 456);
        QCOMPARE(again.value("logentry_mode").toString(), QString("SSB"));
        QCOMPARE(again.value("logentry_frequency").toString(), QString("14274.5"));
        QVERIFY(!again.contains("logentry_his_name"));
    }

    void crxAnswers()
    {
        auto r = qsl::parseCrxResponse(200, R"({"success": true, "message": "QSO created successfully", "qso_id": 457})");
        QVERIFY(r.ok);
        QCOMPARE(r.accepted, 1);
        QCOMPARE(r.remoteId, QString("457"));

        // Chiave sbagliata: vale per tutti, ci si ferma.
        r = qsl::parseCrxResponse(401, R"({"error": "Invalid API key"})");
        QVERIFY(!r.ok);
        QVERIFY(r.retryLater);

        // Un QSO rifiutato: il motivo resta scritto, gli altri partono.
        r = qsl::parseCrxResponse(400, R"({"error": "Missing logentry_his_call"})");
        QVERIFY(!r.ok);
        QVERIFY(!r.retryLater);
        QCOMPARE(r.rejected, 1);
        QVERIFY(r.message.contains(QLatin1String("Missing logentry_his_call")));

        r = qsl::parseCrxResponse(500, "<html>oops</html>");
        QVERIFY(r.retryLater);

        QString error;
        const QVariantList logs = qsl::parseCrxLogs(
            R"({"logs": [{"log_id": 123, "log_name": "IOTA Contest 2023", "log_activation_call": "F/K1ABC", "log_desc": "EU-048"}]})",
            &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(logs.size(), 1);
        QCOMPARE(logs.first().toMap().value("id").toLongLong(), 123);
        QCOMPARE(logs.first().toMap().value("name").toString(), QString("IOTA Contest 2023"));
    }

    void crxOverTheWire()
    {
        // La busta vera: POST JSON {"req": {type, query, apikey, qsoData}}.
        FakeCrx crx;
        QVERIFY(crx.listen(QHostAddress::LocalHost));
        crx.answer = R"({"success": true, "qso_id": 457})";
        WebQslUploader web;
        web.setCrxEndpoint(crx.url());
        QSignalSpy done(&web, &WebQslUploader::finished);
        AdifRecord r{{"CALL", "W1AW"}, {"QSO_DATE", "20241020"}, {"TIME_ON", "1248"}, {"BAND", "40m"},
                     {"FREQ", "7.025"}, {"MODE", "CW"}};
        web.uploadCrx("HAM-XX-12345678-12345678", qsl::crxQsoData(r, 123, 0));
        QVERIFY(done.wait(5000));
        const auto result = done.first().first().value<QslUploadResult>();
        QVERIFY(result.ok);
        QCOMPARE(result.remoteId, QString("457"));

        QVERIFY(crx.request.startsWith("POST /api/ "));
        QVERIFY(crx.request.toLower().contains("content-type: application/json"));
        const QJsonObject req = QJsonDocument::fromJson(crx.body).object().value("req").toObject();
        QCOMPARE(req.value("type").toString(), QString("radio"));
        QCOMPARE(req.value("query").toString(), QString("edit_myqso"));
        QCOMPARE(req.value("apikey").toString(), QString("HAM-XX-12345678-12345678"));
        QCOMPARE(req.value("qsoData").toObject().value("logentry_his_call").toString(), QString("W1AW"));

        // L'elenco dei logbook.
        FakeCrx logs;
        QVERIFY(logs.listen(QHostAddress::LocalHost));
        logs.answer = R"({"logs": [{"log_id": 7, "log_name": "Station", "log_activation_call": "", "log_desc": ""}]})";
        web.setCrxEndpoint(logs.url());
        QSignalSpy listed(&web, &WebQslUploader::crxLogsListed);
        web.listCrxLogs("HAM-XX-12345678-12345678");
        QVERIFY(listed.wait(5000));
        QCOMPARE(listed.first().at(0).toList().size(), 1);
        QVERIFY(listed.first().at(1).toString().isEmpty());
        QCOMPARE(QJsonDocument::fromJson(logs.body).object().value("req").toObject().value("query").toString(),
                 QString("get_mylogs"));
    }

    void clubLogNeedsEverything()
    {
        ClubLogAuth auth;
        QVERIFY(!auth.complete());
        auth.email = QStringLiteral("call@example.org");
        auth.password = QStringLiteral("secret");
        auth.callsign = QStringLiteral("IU8LMC");
        QVERIFY(!auth.complete());       // manca la chiave API
        auth.apiKey = QStringLiteral("abc");
        QVERIFY(auth.complete());

        // Senza credenziali complete non parte nessuna richiesta di rete.
        WebQslUploader uploader;
        QslUploadResult got;
        QObject::connect(&uploader, &WebQslUploader::finished, [&got](const QslUploadResult& r) { got = r; });
        uploader.uploadClubLog(ClubLogAuth{}, "<EOR>", 1);
        QVERIFY(!got.ok);
        QVERIFY(!got.retryLater);
        QVERIFY(!uploader.busy());
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

    void theCertificateIsTheOneOfTheCallsign()
    {
        // TQSL su Windows tiene tutto in %APPDATA%\TrustedQSL: si finge quella
        // cartella e si guarda cosa ci trova DecoDXLog dentro.
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QByteArray appData = qgetenv("APPDATA");
        qputenv("APPDATA", QFile::encodeName(home.path()));

        const QString data = QDir(home.path()).filePath("TrustedQSL");
        QDir().mkpath(data + "/certs");

        // Appena installato TQSL ha solo le sue radici: non firma niente.
        QFile root(data + "/certs/root");
        QVERIFY(root.open(QIODevice::WriteOnly));
        root.write("radici");
        root.close();
        QFile authorities(data + "/certs/authorities");
        QVERIFY(authorities.open(QIODevice::WriteOnly));
        authorities.write("autorita");
        authorities.close();

#ifdef Q_OS_WIN
        QCOMPARE(qsl::tqslDataDirectory(), data);
        QVERIFY(!qsl::tqslHasCertificate());

        // Col certificato del nominativo (il .tq6 caricato in TQSL) si firma.
        QFile user(data + "/certs/user");
        QVERIFY(user.open(QIODevice::WriteOnly));
        user.write("-----BEGIN CERTIFICATE-----");
        user.close();
        QVERIFY(qsl::tqslHasCertificate());

        // E le station location si leggono da li'.
        QFile stations(data + "/station_data");
        QVERIFY(stations.open(QIODevice::WriteOnly));
        stations.write("<StationDataFile><StationData name=\"Casa\"><CALL>IU8LMC</CALL></StationData></StationDataFile>");
        stations.close();
        QCOMPARE(qsl::tqslStationLocations(), QStringList{QStringLiteral("Casa")});
#endif

        if (appData.isEmpty())
            qunsetenv("APPDATA");
        else
            qputenv("APPDATA", appData);
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
