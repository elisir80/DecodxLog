// SO2R: i comandi OTRSP e la radio 2 su un rigctld finto.
#include "app/So2rController.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace decolog;

class TestSo2r : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("DecoDXLogTest");
        QCoreApplication::setApplicationName("tst_so2r");
        QSettings().clear();
    }

    void otrspCommands()
    {
        QCOMPARE(core::otrsp::commands(1, false), QStringList({"TX1", "RX1"}));
        QCOMPARE(core::otrsp::commands(2, true), QStringList({"TX2", "RX2S"}));
    }

    void focusAndRadio2()
    {
        // Un rigctld finto: risponde alla frequenza e al modo.
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, &server, [&server] {
            QTcpSocket* s = server.nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, s, [s] {
                while (s->canReadLine()) {
                    const QByteArray line = s->readLine().trimmed();
                    if (line == "+f") s->write("get_freq:\nFrequency: 7012300\nRPRT 0\n");
                    else if (line == "+m") s->write("get_mode:\nMode: CW\nPassband: 500\nRPRT 0\n");
                    else s->write("RPRT 0\n");
                }
            });
        });
        app::So2rController so2r({});
        QCOMPARE(so2r.focus(), 1);
        so2r.setAddress(QStringLiteral("127.0.0.1:%1").arg(server.serverPort()));
        so2r.setEnabled(true);
        QVERIFY(!so2r.radio2HasFocus());
        so2r.setFocus(2);
        QVERIFY(so2r.radio2HasFocus());
        QTRY_VERIFY_WITH_TIMEOUT(so2r.radio2Hz() == 7012300, 5000);
        so2r.toggleFocus();
        QCOMPARE(so2r.focus(), 1);
        so2r.setEnabled(false);
        QCOMPARE(so2r.focus(), 1);
    }
};

QTEST_GUILESS_MAIN(TestSo2r)
#include "tst_so2r.moc"
