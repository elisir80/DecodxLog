// DecoLink: saluto, elenco dei lavorati a blocchi, award, query, ping e messaggi
// in uscita, con un client TCP vero su 127.0.0.1.
#include "core/DecoLinkServer.h"
#include "core/LogDatabase.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>

using namespace decolog::core;

namespace {

class Client {
public:
    QTcpSocket socket;
    QList<QJsonObject> received;

    bool connectTo(quint16 port)
    {
        socket.connectToHost(QHostAddress::LocalHost, port);
        return socket.waitForConnected(3000);
    }
    void send(const QJsonObject& o) { socket.write(QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n'); }

    // Legge finche' non arriva un messaggio del tipo chiesto.
    std::optional<QJsonObject> waitFor(const QString& type, int timeoutMs = 5000)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < timeoutMs) {
            for (qsizetype i = m_consumed; i < received.size(); ++i) {
                if (received.at(i).value("type").toString() == type) {
                    m_consumed = i + 1;
                    return received.at(i);
                }
            }
            if (!socket.waitForReadyRead(100))
                QCoreApplication::processEvents();
            m_buffer += socket.readAll();
            qsizetype nl;
            while ((nl = m_buffer.indexOf('\n')) >= 0) {
                received << QJsonDocument::fromJson(m_buffer.left(nl)).object();
                m_buffer.remove(0, nl + 1);
            }
        }
        return std::nullopt;
    }

private:
    QByteArray m_buffer;
    qsizetype m_consumed{0};
};

} // namespace

class TestDecoLink : public QObject {
    Q_OBJECT

private slots:
    void workedRowsFromDatabase()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        db.insertQso({{"CALL", "9A3XY"}, {"QSO_DATE", "20260916"}, {"TIME_ON", "1452"}, {"BAND", "20m"},
                      {"MODE", "FT2"}, {"GRIDSQUARE", "jn75ws"}, {"LOTW_QSL_RCVD", "Y"}}, "udp_decodium");
        db.insertQso({{"CALL", "EA8XX"}, {"QSO_DATE", "20260917"}, {"TIME_ON", "0900"}, {"BAND", "15m"},
                      {"MODE", "SSB"}, {"SUBMODE", "USB"}, {"EQSL_QSL_RCVD", "Y"}}, "manual");
        const auto rows = db.workedRows(true, true, false);
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0), QJsonArray({"9A3XY", "20m", "FT2", "20260916", "JN75", 1}));
        QCOMPARE(rows.at(1), QJsonArray({"EA8XX", "15m", "SSB", "20260917", "", 0}));   // eQSL non accettata
        QCOMPARE(db.workedRows(false, false, true).at(1).at(5).toInt(), 1);
    }

    void conversation()
    {
        DecoLinkServer server;
        server.setIdentity("0.1.0", "IU8LMC");
        QList<QJsonArray> rows;
        for (int i = 0; i < 4500; ++i)
            rows << QJsonArray{QStringLiteral("K%1AB").arg(i), "20m", "FT8", "20260101", "FN31", 0};
        server.workedRows = [&rows] { return rows; };
        server.awardState = [] {
            return QJsonObject{{"ft2", QJsonObject{{"dxccWorked", 31}, {"dxccConfirmed", 18}}}};
        };
        QJsonObject lastQuery;
        server.resolveQuery = [&lastQuery](const QJsonObject& q) {
            lastQuery = q;
            QJsonArray out;
            for (const auto& c : q.value("calls").toArray())
                out.append(QJsonObject{{"call", c.toString()}, {"workedCall", c.toString() == "K1AB"}});
            return out;
        };
        QVERIFY2(server.start(0), qPrintable(server.lastError()));   // porta libera qualunque

        Client client;
        QVERIFY(client.connectTo(server.port()));

        const auto hello = client.waitFor("hello");
        QVERIFY(hello);
        QCOMPARE(hello->value("app").toString(), QString("DecoDXLog"));
        QCOMPARE(hello->value("protocol").toInt(), 1);
        QCOMPARE(hello->value("station").toString(), QString("IU8LMC"));

        // Prima del saluto del client non arriva niente altro, e i broadcast non lo raggiungono.
        server.broadcast(QJsonObject{{"type", "qso"}});
        client.send({{"type", "hello"}, {"app", "Decodium"}, {"version", "1.0.638"}, {"protocol", 1}});

        int total = 0;
        int chunks = 0;
        bool final = false;
        while (!final) {
            const auto chunk = client.waitFor("worked");
            QVERIFY(chunk);
            ++chunks;
            QCOMPARE(chunk->value("seq").toInt(), chunks);
            total += chunk->value("rows").toArray().size();
            final = chunk->value("final").toBool();
        }
        QCOMPARE(total, 4500);
        QCOMPARE(chunks, 3);                                      // 2000 + 2000 + 500

        const auto award = client.waitFor("award");
        QVERIFY(award);
        QCOMPARE(award->value("ft2").toObject().value("dxccWorked").toInt(), 31);

        QTRY_COMPARE(server.clients().size(), 1);
        QCOMPARE(server.clients().first().app, QString("Decodium"));

        QJsonArray calls;
        for (int i = 0; i < 250; ++i)
            calls.append(i == 0 ? QStringLiteral("K1AB") : QStringLiteral("N%1XX").arg(i));
        client.send({{"type", "query"}, {"id", 7}, {"band", "20m"}, {"calls", calls}});
        const auto status = client.waitFor("status");
        QVERIFY(status);
        QCOMPARE(status->value("id").toInt(), 7);
        QCOMPARE(status->value("results").toArray().size(), 200);  // tetto per richiesta
        QVERIFY(status->value("results").toArray().at(0).toObject().value("workedCall").toBool());
        QCOMPARE(lastQuery.value("band").toString(), QString("20m"));

        client.send({{"type", "ping"}, {"id", 3}});
        const auto pong = client.waitFor("pong");
        QVERIFY(pong);
        QCOMPARE(pong->value("id").toInt(), 3);

        // Tipi sconosciuti e righe non JSON non chiudono la connessione.
        client.socket.write("not json\n");
        client.send({{"type", "future-message"}});
        server.broadcast(QJsonObject{{"type", "qso"}, {"status", "logged"}});
        const auto qso = client.waitFor("qso");
        QVERIFY(qso);
        QCOMPARE(qso->value("status").toString(), QString("logged"));

        client.socket.disconnectFromHost();
        QTRY_COMPARE(server.clientCount(), 0);
    }

    void onlyLoopback()
    {
        DecoLinkServer server;
        QVERIFY(server.start(0));
        QTcpSocket probe;
        // L'indirizzo di ascolto e' 127.0.0.1: nessuna interfaccia di rete.
        QVERIFY(server.isListening());
        probe.connectToHost(QHostAddress::LocalHost, server.port());
        QVERIFY(probe.waitForConnected(3000));
    }
};

QTEST_GUILESS_MAIN(TestDecoLink)
#include "tst_decolink.moc"
