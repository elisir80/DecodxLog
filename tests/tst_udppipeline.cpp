// Dal pacchetto UDP al QSO nel log, con il programma vero.
//
// tst_protocol prova il protocollo pezzo per pezzo. Qui si avvia DecoDXLog
// com'e' — senza finestra (offscreen), su un log e impostazioni di prova — gli
// si mandano i datagrammi che mandano Decodium, WSJT-X e JTDX, e si guarda nel
// database cosa e' finito nel log: una volta sola, con la fonte giusta, con il
// DXCC aggiunto dal cty.csv. Il programma racconta cosa fa sul registro
// attivita', che con DECODXLOG_TRACE_ACTIVITY=1 esce anche su stderr.
#include "core/WsjtxProtocol.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <QUdpSocket>

#include <functional>

using namespace decolog::core;

namespace {

wsjtx::QsoLogged qsoFor(const QString& call, const QString& grid, const QString& mode, const QTime& at)
{
    wsjtx::QsoLogged q;
    q.timeOn = QDateTime(QDate(2026, 9, 17), at, QTimeZone::UTC);
    q.timeOff = q.timeOn.addSecs(45);
    q.dxCall = call;
    q.dxGrid = grid;
    q.txFrequencyHz = 14084000;
    q.mode = mode;
    q.reportSent = QStringLiteral("-10");
    q.reportReceived = QStringLiteral("-05");
    q.myCall = QStringLiteral("IU8LMC");
    q.myGrid = QStringLiteral("JN70");
    return q;
}

QByteArray adifFor(const QString& call, const QString& grid, const QString& time, const char* program)
{
    const auto field = [](const char* name, const QString& value) {
        return QStringLiteral("<%1:%2>%3 ").arg(QLatin1String(name)).arg(value.size()).arg(value);
    };
    QString record = field("call", call) + field("gridsquare", grid) + field("mode", QStringLiteral("MFSK"))
                     + field("submode", QStringLiteral("FT2")) + field("rst_sent", QStringLiteral("-10"))
                     + field("rst_rcvd", QStringLiteral("-05")) + field("qso_date", QStringLiteral("20260917"))
                     + field("time_on", time) + field("band", QStringLiteral("20m"))
                     + field("freq", QStringLiteral("14.084000")) + field("station_callsign", QStringLiteral("IU8LMC"))
                     + QStringLiteral("<EOR>");
    const QString header = QStringLiteral("\n<adif_ver:5>3.1.0\n<programid:%1>%2\n<EOH>\n")
                               .arg(qstrlen(program))
                               .arg(QLatin1String(program));
    return (header + record).toUtf8();
}

} // namespace

class TestUdpPipeline : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QProcess m_app;
    QByteArray m_output;
    quint16 m_port{0};
    QString m_db;

    void send(const QByteArray& datagram)
    {
        QUdpSocket s;
        QCOMPARE(s.writeDatagram(datagram, QHostAddress::LocalHost, m_port), qint64(datagram.size()));
    }

    // Una connessione per ogni domanda, in sola lettura: il log e' del programma.
    QVariant scalar(const QString& sql)
    {
        QVariant out;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("probe"));
            db.setDatabaseName(m_db);
            db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=2000"));
            if (db.open()) {
                QSqlQuery q(db);
                if (q.exec(sql) && q.next())
                    out = q.value(0);
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("probe"));
        return out;
    }

    int countCall(const QString& call)
    {
        return scalar(QStringLiteral("SELECT COUNT(*) FROM qso WHERE deleted = 0 AND call = '%1'").arg(call)).toInt();
    }

    QString field(const QString& call, const char* column)
    {
        return scalar(QStringLiteral("SELECT IFNULL(%1, '') FROM qso WHERE deleted = 0 AND call = '%2'")
                          .arg(QLatin1String(column), call))
            .toString();
    }

    bool waitFor(const std::function<bool()>& condition, int ms)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < ms) {
            if (condition())
                return true;
            QTest::qWait(100);
        }
        return condition();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_db = m_dir.filePath(QStringLiteral("log.sqlite"));

        // Una porta libera, presa e lasciata subito.
        {
            QUdpSocket probe;
            QVERIFY(probe.bind(QHostAddress::LocalHost, 0));
            m_port = probe.localPort();
        }
        // Impostazioni di prova: niente backup nella cartella vera.
        QVERIFY(QDir().mkpath(m_dir.filePath(QStringLiteral("Decodium"))));
        QFile ini(m_dir.filePath(QStringLiteral("Decodium/DecoDXLog.ini")));
        QVERIFY(ini.open(QIODevice::WriteOnly));
        ini.write("[backup]\nenabled=false\ndir=" + m_dir.filePath(QStringLiteral("backup")).toUtf8() + "\n");
        ini.close();

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        env.insert(QStringLiteral("DECODXLOG_TRACE_ACTIVITY"), QStringLiteral("1"));
        // --grab chiude il programma da solo anche se la prova si pianta.
        env.insert(QStringLiteral("DECODXLOG_GRAB_MS"), QStringLiteral("240000"));
        m_app.setProcessEnvironment(env);
        m_app.setProcessChannelMode(QProcess::MergedChannels);
        connect(&m_app, &QProcess::readyRead, this, [this] { m_output += m_app.readAll(); });
        m_app.start(QStringLiteral(DECODXLOG_APP),
                    {QStringLiteral("--settings"), m_dir.path(), QStringLiteral("--db"), m_db,
                     QStringLiteral("--port"), QString::number(m_port), QStringLiteral("--grab"),
                     m_dir.filePath(QStringLiteral("shot.png"))});
        QVERIFY2(m_app.waitForStarted(20000), qPrintable(m_app.errorString()));

        // Pronto quando dice di ascoltare su quella porta.
        const QRegularExpression listening(QStringLiteral("\\[activity\\] UDP\\|info\\|[^\\n]*\\b%1\\b").arg(m_port));
        const bool ready = waitFor([&] { return listening.match(QString::fromUtf8(m_output)).hasMatch(); }, 90000);
        QVERIFY2(ready, m_output.right(2000).constData());
    }

    void cleanupTestCase()
    {
        if (m_app.state() != QProcess::NotRunning) {
            m_app.kill();
            m_app.waitForFinished(10000);
        }
    }

    void decodiumSendsTwoMessagesAndOneQsoIsLogged()
    {
        // Decodium: prima QSOLogged, poi LoggedADIF dello stesso collegamento.
        send(wsjtx::buildQsoLogged(QStringLiteral("Decodium"), qsoFor("DL1AB", "JO62", "FT2", QTime(10, 15))));
        send(wsjtx::buildLoggedAdif(QStringLiteral("Decodium"),
                                    adifFor(QStringLiteral("DL1AB"), QStringLiteral("JO62"), QStringLiteral("101500"),
                                            "Decodium FT2 1.0.637")));
        QVERIFY(waitFor([&] { return countCall(QStringLiteral("DL1AB")) > 0; }, 15000));
        // Passata la finestra dell'abbinamento, e' ancora uno solo.
        QTest::qWait(2500);
        QCOMPARE(countCall(QStringLiteral("DL1AB")), 1);
        QCOMPARE(field(QStringLiteral("DL1AB"), "source"), QStringLiteral("udp_decodium"));
        QCOMPARE(field(QStringLiteral("DL1AB"), "submode"), QStringLiteral("FT2"));
        QCOMPARE(field(QStringLiteral("DL1AB"), "gridsquare"), QStringLiteral("JO62"));
        QCOMPARE(field(QStringLiteral("DL1AB"), "band"), QStringLiteral("20m"));
        // Decodium non manda il DXCC: ce lo mette il cty.csv (Germania, 230).
        QCOMPARE(field(QStringLiteral("DL1AB"), "dxcc"), QStringLiteral("230"));
        QVERIFY(field(QStringLiteral("DL1AB"), "source_app").contains(QStringLiteral("Decodium")));
        // E il programma lo racconta come salvato: e' passato da tutta la catena.
        QVERIFY(waitFor([&] { return m_output.contains("[activity] UDP|") && m_output.contains("DL1AB"); }, 5000));
    }

    void jtdxSendsOnlyQsoLogged()
    {
        // JTDX manda solo QSOLogged: si usa dopo la finestra dell'abbinamento.
        send(wsjtx::buildQsoLogged(QStringLiteral("JTDX"), qsoFor("OH2XX", "KP20", "FT8", QTime(10, 20))));
        QVERIFY(waitFor([&] { return countCall(QStringLiteral("OH2XX")) == 1; }, 15000));
        QCOMPARE(field(QStringLiteral("OH2XX"), "source"), QStringLiteral("udp_wsjtx"));
        QCOMPARE(field(QStringLiteral("OH2XX"), "mode"), QStringLiteral("FT8"));
        QCOMPARE(field(QStringLiteral("OH2XX"), "dxcc"), QStringLiteral("224"));
    }

    void wsjtxSendsTheAdif()
    {
        send(wsjtx::buildLoggedAdif(QStringLiteral("WSJT-X"),
                                    adifFor(QStringLiteral("K1ABC"), QStringLiteral("FN42"), QStringLiteral("102500"),
                                            "WSJT-X")));
        QVERIFY(waitFor([&] { return countCall(QStringLiteral("K1ABC")) == 1; }, 15000));
        QCOMPARE(field(QStringLiteral("K1ABC"), "source"), QStringLiteral("udp_wsjtx"));
        QCOMPARE(field(QStringLiteral("K1ABC"), "dxcc"), QStringLiteral("291"));
    }

    void wsjtxResendingAnFt8QsoIsOneQso()
    {
        // FT8 non ha sottomodo: fino alla 1.16.30 il doppione di un QSO senza
        // sottomodo non si riconosceva, e WSJT-X che rimanda lo scriveva due volte.
        QByteArray ft8 = adifFor(QStringLiteral("W2FT8"), QStringLiteral("FN20"), QStringLiteral("104000"), "WSJT-X");
        ft8.replace("<mode:4>MFSK <submode:3>FT2 ", "<mode:3>FT8 ");
        send(wsjtx::buildLoggedAdif(QStringLiteral("WSJT-X"), ft8));
        QVERIFY(waitFor([&] { return countCall(QStringLiteral("W2FT8")) == 1; }, 15000));
        QCOMPARE(field(QStringLiteral("W2FT8"), "mode"), QStringLiteral("FT8"));
        send(wsjtx::buildLoggedAdif(QStringLiteral("WSJT-X"), ft8));
        QTest::qWait(2500);
        QCOMPARE(countCall(QStringLiteral("W2FT8")), 1);
    }

    void theSameQsoSentTwiceIsOneQso()
    {
        // Un client che rimanda lo stesso ADIF (un clic doppio, una rete che
        // ripete): nel log resta uno.
        const QByteArray again = wsjtx::buildLoggedAdif(
            QStringLiteral("Decodium"),
            adifFor(QStringLiteral("DL1AB"), QStringLiteral("JO62"), QStringLiteral("101500"), "Decodium FT2 1.0.637"));
        send(again);
        send(again);
        QTest::qWait(2500);
        QCOMPARE(countCall(QStringLiteral("DL1AB")), 1);
    }

    void heartbeatAndStatusDoNotLog()
    {
        const int before = scalar(QStringLiteral("SELECT COUNT(*) FROM qso")).toInt();
        wsjtx::Heartbeat hb;
        hb.version = QStringLiteral("4.0");
        send(wsjtx::buildHeartbeat(QStringLiteral("Decodium"), hb));
        wsjtx::Status st;
        st.dialFrequencyHz = 14074000;
        st.mode = QStringLiteral("FT8");
        st.dxCall = QStringLiteral("JA1ZZZ");
        send(wsjtx::buildStatus(QStringLiteral("Decodium"), st));
        QTest::qWait(1500);
        QCOMPARE(scalar(QStringLiteral("SELECT COUNT(*) FROM qso")).toInt(), before);
    }

    void rubbishDoesNotStopTheLog()
    {
        // Byte a caso, un datagramma troncato, uno con la firma sbagliata.
        send(QByteArray("hello, this is not WSJT-X"));
        QByteArray valid = wsjtx::buildLoggedAdif(
            QStringLiteral("Decodium"),
            adifFor(QStringLiteral("VK2ABC"), QStringLiteral("QF56"), QStringLiteral("103000"), "Decodium"));
        send(valid.left(valid.size() / 2));
        QByteArray wrongMagic = valid;
        wrongMagic[0] = char(0x00);
        send(wrongMagic);
        QTest::qWait(500);
        QCOMPARE(m_app.state(), QProcess::Running);
        QCOMPARE(countCall(QStringLiteral("VK2ABC")), 0);
        // E il QSO buono, dopo, entra.
        send(valid);
        QVERIFY(waitFor([&] { return countCall(QStringLiteral("VK2ABC")) == 1; }, 15000));
    }

    void aBurstIsAllLogged()
    {
        // Venti QSO uno dietro l'altro, come a fine gara quando si svuota la coda.
        for (int i = 1; i <= 20; ++i) {
            const QString call = QStringLiteral("EA1%1").arg(QChar(QLatin1Char('A' + i)));
            send(wsjtx::buildLoggedAdif(
                QStringLiteral("Decodium"),
                adifFor(call + QStringLiteral("B"), QStringLiteral("IN53"),
                        QStringLiteral("11%1").arg(i, 2, 10, QLatin1Char('0')) + QStringLiteral("00"), "Decodium")));
        }
        QVERIFY(waitFor([&] {
            return scalar(QStringLiteral("SELECT COUNT(*) FROM qso WHERE deleted = 0 AND call LIKE 'EA1_B'")).toInt()
                   == 20;
        }, 20000));
        QCOMPARE(m_app.state(), QProcess::Running);
    }
};

QTEST_GUILESS_MAIN(TestUdpPipeline)
#include "tst_udppipeline.moc"
