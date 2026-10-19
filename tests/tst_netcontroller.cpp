#include "app/NetController.h"

#include <QTest>

class TestNetController : public QObject {
    Q_OBJECT
private slots:
    void sessionMakesSequenceRestartSafe()
    {
        QCOMPARE(decolog::app::NetController::frameKey(QStringLiteral("session-a"), 1),
                 QStringLiteral("session-a:1"));
        QVERIFY(decolog::app::NetController::frameKey(QStringLiteral("session-a"), 1)
                != decolog::app::NetController::frameKey(QStringLiteral("session-b"), 1));
        QVERIFY(decolog::app::NetController::frameKey(QStringLiteral("legacy"), 1)
                != decolog::app::NetController::frameKey(QStringLiteral("session-a"), 1));
    }
};

QTEST_MAIN(TestNetController)
#include "tst_netcontroller.moc"
