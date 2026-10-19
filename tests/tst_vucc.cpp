#include "core/Vucc.h"

#include <QJsonObject>
#include <QTest>

using namespace decolog::core::vucc;

class TestVucc : public QObject {
    Q_OBJECT
private slots:
    void acceptsConfirmedEvidencePerBandAccountAndSite()
    {
        QList<Entry> entries{
            {1, QStringLiteral("JN52GW"), QStringLiteral("6m"), QStringLiteral("home"), QStringLiteral("JM67"), true, QStringLiteral("lotw")},
            {2, QStringLiteral("JO52GX"), QStringLiteral("6m"), QStringLiteral("home"), QStringLiteral("JM67"), true, QStringLiteral("card")},
            {3, QStringLiteral("JN52GW"), QStringLiteral("6m"), QStringLiteral("home"), QStringLiteral("JM67"), true, QStringLiteral("lotw")},
            {4, QStringLiteral("JP52GY"), QStringLiteral("6m"), QStringLiteral("home"), QStringLiteral("JM68"), true, QStringLiteral("lotw")},
            {5, QStringLiteral("JN52GZ"), QStringLiteral("6m"), QStringLiteral("home"), QStringLiteral("JM67"), false, {}},
        };
        const Result result = select(entries, Rules{QStringLiteral("home"), QStringLiteral("JM67"), QStringLiteral("6m"), 100});
        QCOMPARE(result.eligible.size(), 2);
        QCOMPARE(result.eligible.at(0).grid, QStringLiteral("JN52"));
        QVERIFY(result.exclusions.join('\n').contains(QStringLiteral("already credited")));
        QVERIFY(result.exclusions.join('\n').contains(QStringLiteral("evidence")));
        QVERIFY(result.exclusions.join('\n').contains(QStringLiteral("site mismatch")));
        QCOMPARE(result.toJson().object().value(QStringLiteral("format")).toString(), QStringLiteral("decodxlog-vucc-1"));
    }

    void rejectsInvalidGridAndRules()
    {
        QVERIFY(validGrid4(QStringLiteral("JN52")));
        QVERIFY(!validGrid4(QStringLiteral("ZZ99")));
        const Result result = select({}, Rules{QStringLiteral("home"), {}, {}, 100});
        QVERIFY(result.eligible.isEmpty());
        QVERIFY(result.exclusions.first().contains(QStringLiteral("band is required")));
    }
};

QTEST_MAIN(TestVucc)
#include "tst_vucc.moc"
