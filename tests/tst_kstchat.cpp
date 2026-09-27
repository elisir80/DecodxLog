// La chat ON4KST: le righe del server e il login con un server finto.
#include "core/KstChat.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimeZone>

using namespace decolog::core;

class TestKstChat : public QObject {
    Q_OBJECT

private slots:
    void parseMessages()
    {
        const QDateTime now(QDate(2026, 9, 27), QTime(12, 40), QTimeZone::UTC);
        auto m = kst::parseLine("1235Z IK1ABC Mario> QRV 144.300 JN45 any sked?", "IU8LMC", now);
        QVERIFY(!m.system);
        QCOMPARE(m.from, QString("IK1ABC"));
        QCOMPARE(m.name, QString("Mario"));
        QCOMPARE(m.text, QString("QRV 144.300 JN45 any sked?"));
        QCOMPARE(m.time.time(), QTime(12, 35));
        QVERIFY(!m.toMe);

        m = kst::parseLine("1236Z DL1XX Hans> (IU8LMC) pse 144.310", "IU8LMC", now);
        QCOMPARE(m.to, QString("IU8LMC"));
        QCOMPARE(m.text, QString("pse 144.310"));
        QVERIFY(m.toMe);

        m = kst::parseLine("1237Z OK1YY Jan> IU8LMC ur 559", "iu8lmc", now);
        QVERIFY(m.toMe);
        m = kst::parseLine("1237Z OK1YY Jan> IU8LMCX is not me", "IU8LMC", now);
        QVERIFY(!m.toMe);

        m = kst::parseLine("1238Z IU8LMC Carmine> hello", "IU8LMC", now);
        QVERIFY(m.mine);

        // Un messaggio delle 2359 letto poco dopo mezzanotte e' di ieri.
        m = kst::parseLine("2359Z G4ABC Bob> late", "", QDateTime(QDate(2026, 9, 28), QTime(0, 2), QTimeZone::UTC));
        QCOMPARE(m.time.date(), QDate(2026, 9, 27));

        m = kst::parseLine("Welcome to ON4KST chat", "IU8LMC", now);
        QVERIFY(m.system);
    }

    void loginAndChat()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        KstChat chat;
        chat.setServer(QStringLiteral("127.0.0.1"), server.serverPort());
        QSignalSpy messages(&chat, &KstChat::messageReceived);
        chat.start(QStringLiteral("iu8lmc"), QStringLiteral("secret"), 2);
        QVERIFY(server.waitForNewConnection(3000));
        QTcpSocket* s = server.nextPendingConnection();
        QTRY_COMPARE(chat.state(), KstChat::State::LoggingIn);

        auto expect = [s](const QByteArray& line) {
            QTRY_VERIFY_WITH_TIMEOUT(s->canReadLine() || (s->waitForReadyRead(50) && s->canReadLine()), 3000);
            QCOMPARE(s->readLine().trimmed(), line);
        };
        s->write("Login: ");
        expect("IU8LMC");
        s->write("Password: ");
        expect("secret");
        s->write("Your choice           : ");
        expect("2");
        QTRY_COMPARE(chat.state(), KstChat::State::Online);

        s->write("1240Z DL1XX Hans> (IU8LMC) hi\r\n");
        QTRY_COMPARE(messages.count(), 1);
        const auto m = messages.first().first().value<KstMessage>();
        QVERIFY(m.toMe);

        QVERIFY(chat.sendTo(QStringLiteral("dl1xx"), QStringLiteral("559 JN70")));
        expect("/cq DL1XX 559 JN70");
        chat.stop();
        QCOMPARE(chat.state(), KstChat::State::Off);
    }
};

QTEST_GUILESS_MAIN(TestKstChat)
#include "tst_kstchat.moc"
