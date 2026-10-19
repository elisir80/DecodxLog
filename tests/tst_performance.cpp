#include "core/Performance.h"

#include <QTest>

using namespace decolog::core;

class TestPerformance : public QObject {
    Q_OBJECT

private slots:
    void emptySummaryIsInvalid()
    {
        const auto summary = performance::summarize({});
        QVERIFY(!summary.valid);
        QCOMPARE(performance::toText(summary, QStringLiteral("UI")), QStringLiteral("UI: no samples"));
    }

    void percentilesUseNearestObservedRank()
    {
        const auto summary = performance::summarize({95, 5, 20, 40, 10, 80, 60, 30, 70, 50});
        QVERIFY(summary.valid);
        QCOMPARE(summary.samples, 10);
        QCOMPARE(summary.p50Ms, qint64(40));
        QCOMPARE(summary.p95Ms, qint64(95));
        QCOMPARE(summary.maxMs, qint64(95));
    }

    void negativeSamplesAreClamped()
    {
        const auto summary = performance::summarize({-10, 4, 8});
        QVERIFY(summary.valid);
        QCOMPARE(summary.p50Ms, qint64(4));
        QCOMPARE(summary.maxMs, qint64(8));
    }
};

QTEST_GUILESS_MAIN(TestPerformance)
#include "tst_performance.moc"
