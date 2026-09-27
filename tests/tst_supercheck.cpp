// Super Check Partial e N+1.
#include "core/SuperCheck.h"

#include <QTest>

using namespace decolog::core;

class TestSuperCheck : public QObject {
    Q_OBJECT

private slots:
    void partialAndNPlusOne()
    {
        SuperCheck scp;
        QCOMPARE(scp.load("# MASTER.SCP test\nDL1ABC\nDL1ABD\nK1ABC\nIU8LMC\nW1AW\nnotacall\nDL1AB\nOH2ABC\n"), 7);
        scp.addCalls({"IK8XYZ", "dl1abc"});
        QCOMPARE(scp.size(), 8);
        QVERIFY(scp.contains("iu8lmc"));

        QStringList p = scp.partial("ABC");
        QCOMPARE(p.size(), 3);
        QVERIFY(p.contains("DL1ABC") && p.contains("K1ABC") && p.contains("OH2ABC"));
        // Chi comincia cosi' viene prima.
        p = scp.partial("DL1");
        QCOMPARE(p.first(), QString("DL1AB"));
        QVERIFY(scp.partial("A").isEmpty());
        QCOMPARE(scp.partial("DL1AB?"), QStringList({"DL1ABC", "DL1ABD"}));

        const QStringList n = scp.nPlusOne("DL1ABC");
        QVERIFY(n.contains("DL1ABD"));      // uno cambiato
        QVERIFY(n.contains("DL1AB"));       // uno tolto
        QVERIFY(!n.contains("DL1ABC"));     // non se stesso
        QVERIFY(!n.contains("K1ABC"));
        QVERIFY(scp.nPlusOne("K1AB").contains("K1ABC"));   // uno aggiunto
    }

    void edits()
    {
        QVERIFY(SuperCheck::oneEditApart("W1AW", "W1AX"));
        QVERIFY(SuperCheck::oneEditApart("W1AW", "W1AWX"));
        QVERIFY(SuperCheck::oneEditApart("W1AW", "W1A"));
        QVERIFY(SuperCheck::oneEditApart("W1AW", "WA1AW"));
        QVERIFY(!SuperCheck::oneEditApart("W1AW", "W1AW"));
        QVERIFY(!SuperCheck::oneEditApart("W1AW", "W2AX"));
        QVERIFY(!SuperCheck::oneEditApart("W1AW", "W1AWXY"));
    }
};

QTEST_GUILESS_MAIN(TestSuperCheck)
#include "tst_supercheck.moc"
