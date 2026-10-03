// Le liste di decodifica di Decodium dentro il log: come si legge un messaggio,
// cosa va in Full Spectrum e cosa in Signal RX.
#include "app/DecodeFeed.h"
#include "core/DecodeText.h"
#include "core/UdpReceiver.h"
#include "core/WsjtxProtocol.h"

#include <QDateTime>
#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUdpSocket>

using namespace decolog;
using namespace decolog::core;
using decolog::app::DecodeFeed;
using decolog::app::DecodeListModel;

namespace {

wsjtx::Decode decode(const QString& message, int snr = -10, quint32 df = 1500, bool isNew = true,
                     const QTime& time = QTime(14, 30, 15))
{
    wsjtx::Decode d;
    d.isNew = isNew;
    d.time = time;
    d.snr = snr;
    d.deltaTime = 0.2;
    d.deltaFrequency = df;
    d.mode = "~";
    d.message = message;
    return d;
}

wsjtx::Status status(const QString& dx = {})
{
    wsjtx::Status st;
    st.dialFrequencyHz = 14074000;
    st.mode = "FT8";
    st.deCall = "IU8LMC";
    st.dxCall = dx;
    st.rxDf = 1500;
    st.txDf = 1500;
    st.trPeriod = 15;
    return st;
}

DecodeFeed::Context context()
{
    DecodeFeed::Context c;
    c.myCalls = [] { return QStringList{"IU8LMC"}; };
    // Il log finto: K1ABC e' gia' lavorato, tutto il resto e' nuovo.
    c.classify = [](const QString& call, const QString&, const QString&) {
        DecodeFeed::Classification k;
        if (call != "K1ABC") {
            k.status = 1;
            k.entity = "Somewhere";
        }
        return k;
    };
    c.bandOf = [](const wsjtx::Status& st) { return st.dialFrequencyHz == 14074000 ? QString("20m") : QString(); };
    c.statusLabel = [](int s) { return s & 1 ? QString("NEW DXCC") : QString(); };
    return c;
}

QString role(const QAbstractItemModel& m, int row, int r)
{
    return m.data(m.index(row, 0), r).toString();
}

} // namespace

class TestDecodeFeed : public QObject {
    Q_OBJECT

private slots:
    void readsMessages()
    {
        using namespace decodetext;
        auto p = parse("CQ DX K1ABC FN42");
        QVERIFY(p.cq);
        QCOMPARE(p.from, QString("K1ABC"));
        QCOMPARE(p.modifier, QString("DX"));
        QCOMPARE(p.grid, QString("FN42"));

        p = parse("CQ POTA 9A3XY JN75");
        QCOMPARE(p.from, QString("9A3XY"));
        p = parse("cq k1abc fn42");
        QCOMPARE(p.from, QString("K1ABC"));

        p = parse("IU8LMC K1ABC -12");
        QVERIFY(!p.cq);
        QCOMPARE(p.to, QString("IU8LMC"));
        QCOMPARE(p.from, QString("K1ABC"));
        QVERIFY(p.grid.isEmpty());

        p = parse("K1ABC IU8LMC RR73");
        QCOMPARE(p.from, QString("IU8LMC"));
        QVERIFY2(p.grid.isEmpty(), "RR73 non e' un locatore");

        p = parse("<PJ4/K1ABC> IU8LMC JN70");
        QCOMPARE(p.to, QString("PJ4/K1ABC"));
        QCOMPARE(p.from, QString("IU8LMC"));
        QCOMPARE(p.grid, QString("JN70"));

        // Testo libero: nessun mittente, meglio niente che sbagliato.
        p = parse("TNX 73 GL");
        QVERIFY(p.from.isEmpty());

        QVERIFY(looksLikeCall("IU8LMC"));
        QVERIFY(looksLikeCall("3Y0J"));
        QVERIFY(looksLikeCall("EA8/IU8LMC/P"));
        QVERIFY(!looksLikeCall("RR73"));
        QVERIFY(!looksLikeCall("JN70"));
        QVERIFY(!looksLikeCall("JN70DC"));
        QVERIFY(!looksLikeCall("R-05"));
        QVERIFY(!looksLikeCall("CQ"));
        QCOMPARE(baseCall("EA8/IU8LMC/P"), QString("IU8LMC"));
        QCOMPARE(baseCall("K1ABC/P"), QString("K1ABC"));

        QVERIFY(mentions("K1ABC/P IU8LMC 73", "K1ABC"));
        QVERIFY(mentions("CQ DX K1ABC FN42", "K1ABC"));
        QVERIFY(mentions("<K1ABC> IU8LMC", "K1ABC"));
        QVERIFY(!mentions("CQ DX K1ABD FN42", "K1ABC"));
    }

    void splitsFullSpectrumAndSignalRx()
    {
        DecodeFeed feed(nullptr, context());
        feed.handleStatus("Decodium", status("K1ABC"));
        QVERIFY(feed.online());
        QCOMPARE(feed.band(), QString("20m"));
        QCOMPARE(feed.dxCall(), QString("K1ABC"));

        feed.handleDecode("Decodium", decode("CQ 9A3XY JN75"));                  // solo banda
        feed.handleDecode("Decodium", decode("IU8LMC DL1AB -07", -7));            // a noi
        feed.handleDecode("Decodium", decode("CQ K1ABC FN42", -3));               // il corrispondente
        feed.handleDecode("Decodium", decode("EA8XX JA1ZZZ -12"));                // altri

        auto* full = qobject_cast<DecodeListModel*>(feed.fullSpectrum());
        auto* sig = qobject_cast<DecodeListModel*>(feed.signalRx());
        QCOMPARE(full->count(), 4);
        QCOMPARE(sig->count(), 2);
        // La piu' recente in cima.
        QCOMPARE(role(*full, 0, DecodeListModel::MessageRole), QString("EA8XX JA1ZZZ -12"));
        QCOMPARE(role(*sig, 0, DecodeListModel::MessageRole), QString("CQ K1ABC FN42"));
        QCOMPARE(role(*sig, 1, DecodeListModel::MessageRole), QString("IU8LMC DL1AB -07"));

        // Cosa vale per il log: K1ABC e' gia' lavorato, 9A3XY no.
        QCOMPARE(role(*full, 3, DecodeListModel::FromRole), QString("9A3XY"));
        QCOMPARE(role(*full, 3, DecodeListModel::StatusLabelRole), QString("NEW DXCC"));
        QCOMPARE(role(*full, 3, DecodeListModel::EntityRole), QString("Somewhere"));
        QCOMPARE(role(*full, 1, DecodeListModel::StatusLabelRole), QString());
        QVERIFY(full->data(full->index(2), DecodeListModel::ForMeRole).toBool());
        QVERIFY(full->data(full->index(1), DecodeListModel::WithDxRole).toBool());
        QVERIFY(!full->data(full->index(0), DecodeListModel::ForMeRole).toBool());
    }

    void ownTransmissionsAndNewPartner()
    {
        DecodeFeed feed(nullptr, context());
        feed.handleStatus("Decodium", status());
        feed.handleDecode("Decodium", decode("CQ K1ABC FN42"));
        feed.handleDecode("Decodium", decode("CQ 9A3XY JN75"));
        auto* sig = qobject_cast<DecodeListModel*>(feed.signalRx());
        QCOMPARE(sig->count(), 0);

        // Comincia una trasmissione: una riga di Signal RX, una sola per trasmissione.
        auto tx = status();
        tx.transmitting = true;
        tx.txMessage = "IU8LMC 9A3XY JN70";
        feed.handleStatus("Decodium", tx);
        feed.handleStatus("Decodium", tx);
        QCOMPARE(sig->count(), 1);
        QVERIFY(sig->data(sig->index(0), DecodeListModel::OwnTxRole).toBool());
        QCOMPARE(role(*sig, 0, DecodeListModel::MessageRole), QString("IU8LMC 9A3XY JN70"));
        tx.transmitting = false;
        feed.handleStatus("Decodium", tx);
        tx.transmitting = true;
        feed.handleStatus("Decodium", tx);
        QCOMPARE(sig->count(), 2);

        // Il corrispondente cambia (doppio clic su un CQ vecchio in Decodium):
        // le sue righe di prima entrano in Signal RX, le nostre restano.
        feed.handleStatus("Decodium", status("9A3XY"));
        QCOMPARE(sig->count(), 3);
        int own = 0, partner = 0;
        for (int i = 0; i < sig->count(); ++i) {
            own += sig->data(sig->index(i), DecodeListModel::OwnTxRole).toBool() ? 1 : 0;
            partner += role(*sig, i, DecodeListModel::MessageRole) == "CQ 9A3XY JN75" ? 1 : 0;
        }
        QCOMPARE(own, 2);
        QCOMPARE(partner, 1);
    }

    void replaysDoNotDuplicate()
    {
        DecodeFeed feed(nullptr, context());
        feed.handleStatus("Decodium", status());
        feed.handleDecode("Decodium", decode("CQ 9A3XY JN75"));
        // Replay: la stessa riga, "non nuova".
        feed.handleDecode("Decodium", decode("CQ 9A3XY JN75", -10, 1500, false));
        auto* full = qobject_cast<DecodeListModel*>(feed.fullSpectrum());
        QCOMPARE(full->count(), 1);
        // Una non ancora vista si aggiunge anche se e' un replay.
        feed.handleDecode("Decodium", decode("CQ DL1AB JO31", -9, 900, false));
        QCOMPARE(full->count(), 2);
    }

    void keepsFewRowsAndRestatuses()
    {
        bool lotsWorked = false;
        auto ctx = context();
        ctx.classify = [&lotsWorked](const QString&, const QString&, const QString&) {
            DecodeFeed::Classification k;
            k.status = lotsWorked ? 0 : 1;
            return k;
        };
        DecodeFeed feed(nullptr, ctx);
        feed.handleStatus("Decodium", status());
        for (int i = 0; i < DecodeFeed::kFullLimit + 30; ++i)
            feed.handleDecode("Decodium", decode(QString("CQ K1AB%1 FN42").arg(QChar('A' + i % 26)), -10, 600 + i));
        auto* full = qobject_cast<DecodeListModel*>(feed.fullSpectrum());
        QCOMPARE(full->count(), DecodeFeed::kFullLimit);
        QCOMPARE(full->data(full->index(0), DecodeListModel::StatusRole).toInt(), 1);
        // Il log e' cambiato: tutti gia' lavorati.
        lotsWorked = true;
        feed.restatus();
        QCOMPARE(full->data(full->index(0), DecodeListModel::StatusRole).toInt(), 0);
        QCOMPARE(full->data(full->index(full->count() - 1), DecodeListModel::StatusRole).toInt(), 0);

        feed.clearFullSpectrum();
        QCOMPARE(full->count(), 0);
    }

    // All'apertura il pannello riparte dalle decodifiche che Decodium ha gia' sul
    // disco, invece di aspettare il prossimo periodo.
    void startsFromDecodiumRecords()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("db.sqlite");
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tst-decodium-history");
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("CREATE TABLE decodes (id INTEGER PRIMARY KEY AUTOINCREMENT, ts_utc INTEGER NOT NULL, "
                           "band TEXT NOT NULL, freq_hz INTEGER NOT NULL, mode TEXT NOT NULL, submode TEXT, "
                           "callsign_dx TEXT, callsign_de TEXT, grid TEXT, snr_db INTEGER, dt_s REAL, df_hz INTEGER, "
                           "message TEXT NOT NULL, confidence INTEGER, session_id INTEGER NOT NULL DEFAULT 1)"));
            auto add = [&](qint64 age, const char* band, qint64 freq, const char* mode, int snr, double dt, const char* msg) {
                q.prepare("INSERT INTO decodes (ts_utc, band, freq_hz, mode, snr_db, dt_s, message) VALUES (?,?,?,?,?,?,?)");
                q.addBindValue(now - age);
                q.addBindValue(band);
                q.addBindValue(freq);
                q.addBindValue(mode);
                q.addBindValue(snr);
                q.addBindValue(dt);
                q.addBindValue(msg);
                QVERIFY(q.exec());
            };
            add(300000, "20M", 14084735, "FT2", -7, 0.7, "R8OAE PD4AVT JO21");
            add(240000, "20M", 14085495, "FT2", -26, 0.5, "CQ PD2WL JO22                       ?");
            add(3 * 3600 * 1000, "20M", 14084900, "FT2", -10, 0.1, "CQ OLD1AA JN70");     // troppo vecchia
            add(200000, "20M", 14074500, "FT8", -10, 0.1, "CQ K1ABC FN42");               // un altro modo
            add(200000, "40M", 7074500, "FT2", -10, 0.1, "CQ EA1ABC IN52");               // un'altra banda
            add(100000, "20M", 14084300, "FT2", -3, 0.2, "IU8LMC 9A3XY JN75");           // per noi
            db.close();
        }
        QSqlDatabase::removeDatabase("tst-decodium-history");

        auto ctx = context();
        ctx.historyPath = [path] { return path; };
        DecodeFeed feed(nullptr, ctx);
        wsjtx::Status st = status();
        st.dialFrequencyHz = 14084000;
        st.mode = "FT2";
        feed.handleStatus("Decodium", st);   // il primo stato porta dentro la storia

        auto* full = qobject_cast<DecodeListModel*>(feed.fullSpectrum());
        QCOMPARE(full->count(), 3);
        // Dalla piu' recente.
        QCOMPARE(role(*full, 0, DecodeListModel::MessageRole), QString("IU8LMC 9A3XY JN75"));
        QCOMPARE(role(*full, 1, DecodeListModel::MessageRole), QString("CQ PD2WL JO22"));
        QVERIFY(full->data(full->index(1), DecodeListModel::LowConfidenceRole).toBool());
        QCOMPARE(role(*full, 1, DecodeListModel::DfRole), QString("1495"));
        QCOMPARE(role(*full, 2, DecodeListModel::DfRole), QString("735"));
        QCOMPARE(role(*full, 2, DecodeListModel::SnrRole), QString("-7"));
        // La riga per noi entra anche in Signal RX.
        auto* sig = qobject_cast<DecodeListModel*>(feed.signalRx());
        QCOMPARE(sig->count(), 1);
        QVERIFY(sig->data(sig->index(0), DecodeListModel::ForMeRole).toBool());
        // Rileggerla non raddoppia niente.
        feed.loadHistory();
        QCOMPARE(full->count(), 3);
        // Un file che non c'e': niente, e niente guai.
        ctx.historyPath = [] { return QString("C:/does/not/exist.sqlite"); };
        DecodeFeed other(nullptr, ctx);
        other.handleStatus("Decodium", st);
        QCOMPARE(qobject_cast<DecodeListModel*>(other.fullSpectrum())->count(), 0);
    }

    // Dal protocollo vero: lo stesso percorso dei datagrammi di Decodium.
    void comesFromTheUdpPort()
    {
        QUdpSocket probe;
        QVERIFY(probe.bind(QHostAddress::LocalHost, 0));
        const quint16 port = probe.localPort();
        probe.close();
        UdpReceiver udp;
        QVERIFY(udp.start(port));
        DecodeFeed feed(&udp, context());

        QUdpSocket decodium;
        QVERIFY(decodium.bind(QHostAddress::LocalHost, 0));
        auto st = status("K1ABC");
        st.txMessage = "K1ABC IU8LMC -10";
        decodium.writeDatagram(wsjtx::buildStatus("Decodium", st), QHostAddress::LocalHost, port);
        auto d = decode("CQ K1ABC FN42", -4, 1620);
        d.lowConfidence = true;
        decodium.writeDatagram(wsjtx::buildDecode("Decodium", d), QHostAddress::LocalHost, port);
        QTRY_COMPARE_WITH_TIMEOUT(qobject_cast<DecodeListModel*>(feed.fullSpectrum())->count(), 1, 3000);
        QVERIFY(feed.online());
        QCOMPARE(feed.program(), QString("Decodium"));
        QCOMPARE(feed.rxDf(), 1500);
        auto* sig = qobject_cast<DecodeListModel*>(feed.signalRx());
        QCOMPARE(sig->count(), 1);
        QVERIFY(sig->data(sig->index(0), DecodeListModel::LowConfidenceRole).toBool());

        // Rispondere: il pacchetto torna a Decodium, con la riga com'era.
        QVERIFY(feed.reply(0, sig->data(sig->index(0), DecodeListModel::SerialRole).toLongLong()));
        QTRY_VERIFY_WITH_TIMEOUT(decodium.hasPendingDatagrams(), 3000);
        const auto r = wsjtx::describe(decodium.receiveDatagram().data());
        QCOMPARE(r.typeName, QString("Reply"));
        QVERIFY2(r.summary.contains("CQ K1ABC FN42"), qPrintable(r.summary));

        // E chiedere le decodifiche di nuovo.
        QVERIFY(feed.replay());
        QTRY_VERIFY_WITH_TIMEOUT(decodium.hasPendingDatagrams(), 3000);
        QCOMPARE(wsjtx::describe(decodium.receiveDatagram().data()).typeName, QString("Replay"));
    }
};

QTEST_GUILESS_MAIN(TestDecodeFeed)
#include "tst_decodefeed.moc"
