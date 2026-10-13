// I nominativi nel testo CW: cosa e' un nominativo, chi e' la stazione che trasmette, e il testo
// con i nominativi cliccabili.
#include "core/CwCalls.h"

#include <QTest>

using namespace decolog::core::cwcalls;

class TestCwCalls : public QObject {
    Q_OBJECT

private slots:
    void realCallsAreCalls()
    {
        for (const char* call : {"K1ABC", "IU8LMC", "9A1AA", "4X6TT", "EA8/IU8LMC", "IU8LMC/P", "JA1XYZ", "DL1ABC"})
            QVERIFY2(isCall(QString::fromLatin1(call)), call);
    }

    void bandsReportsAndWordsAreNot()
    {
        for (const char* word : {"40M", "10M", "73", "599", "5NN", "CQ", "TEST", "100W", "2ND", "QRP", "DE", "K", "R"})
            QVERIFY2(!isCall(QString::fromLatin1(word)), word);
    }

    void thePersonWhoSendsFollowsDe()
    {
        const Found f = find(QStringLiteral("CQ CQ DE IU8LMC IU8LMC K"), true);
        QCOMPARE(f.call, QStringLiteral("IU8LMC"));
        QVERIFY(f.cq);

        // «IU8LMC DE K1ABC»: K1ABC trasmette, IU8LMC e' chi viene chiamato.
        const Found answer = find(QStringLiteral("IU8LMC DE K1ABC K"), true);
        QCOMPARE(answer.call, QStringLiteral("K1ABC"));
        QVERIFY(!answer.cq);
    }

    void cqTestHasTheCallLater()
    {
        const Found f = find(QStringLiteral("CQ TEST K1ABC K1ABC"), true);
        QCOMPARE(f.call, QStringLiteral("K1ABC"));
        QVERIFY(f.cq);
    }

    void aCallWithoutContextIsNotDeclared()
    {
        // Compare una volta, senza DE e senza CQ: potrebbe essere un pezzo di altro.
        QVERIFY(find(QStringLiteral("UR RST 599 K1ABC BK"), true).call.isEmpty());
        // Detto due volte, invece, e' un nominativo.
        QCOMPARE(find(QStringLiteral("K1ABC K1ABC"), true).call, QStringLiteral("K1ABC"));
    }

    void anUnfinishedWordIsNotLookedAt()
    {
        // L'ultima parola e' ancora in arrivo: «K1AB» potrebbe diventare «K1ABC».
        QVERIFY(find(QStringLiteral("CQ DE K1AB"), false).call.isEmpty());
        QCOMPARE(find(QStringLiteral("CQ DE K1AB "), false).call, QStringLiteral("K1AB"));
        QCOMPARE(find(QStringLiteral("CQ DE K1ABC"), true).call, QStringLiteral("K1ABC"));
    }

    void htmlLinksTheCalls()
    {
        const QString html = toHtml(QStringLiteral("CQ DE IU8LMC <K>\nR 40M"), QStringLiteral("#00ff00"));
        QVERIFY2(html.contains(QStringLiteral("href=\"call:IU8LMC\"")), qPrintable(html));
        QVERIFY2(html.contains(QStringLiteral("#00ff00")), qPrintable(html));
        QVERIFY2(html.contains(QStringLiteral("&lt;K&gt;")), qPrintable(html));
        QVERIFY2(html.contains(QStringLiteral("<br>")), qPrintable(html));
        // Una banda non e' un collegamento.
        QVERIFY2(!html.contains(QStringLiteral("call:40M")), qPrintable(html));
    }
};

QTEST_GUILESS_MAIN(TestCwCalls)
#include "tst_cwcalls.moc"
