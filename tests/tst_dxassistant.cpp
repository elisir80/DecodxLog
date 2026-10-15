#include "core/DxAssistant.h"

#include <QTest>
#include <QTimeZone>

using namespace decolog::core::dxassistant;

class TestDxAssistant : public QObject {
    Q_OBJECT
private slots:
    void ranksWithExplicitReasonsAndLimits()
    {
        const QDateTime now(QDate(2026, 10, 16), QTime(12, 0), QTimeZone::UTC);
        Candidate strong;
        strong.call = QStringLiteral("3Y0J");
        strong.band = QStringLiteral("20m");
        strong.mode = QStringLiteral("CW");
        strong.spottedAt = now.addSecs(-5 * 60);
        strong.status = (1 << 0) | (1 << 1) | (1 << 11);
        strong.propagationQuality = 3;
        strong.hasPropagation = true;
        strong.hasHistory = true;

        Candidate weak;
        weak.call = QStringLiteral("W1AW");
        weak.spottedAt = now.addSecs(-90 * 60);
        weak.hasPropagation = false;

        const auto ranked = rank({weak, strong}, {}, now);
        QCOMPARE(ranked.size(), 2);
        QCOMPARE(ranked.first().candidate.call, QStringLiteral("3Y0J"));
        QVERIFY(ranked.first().reasons.join(' ').contains(QStringLiteral("new DXCC")));
        QVERIFY(ranked.first().limitations.isEmpty());
        QVERIFY(ranked.last().limitations.join(' ').contains(QStringLiteral("propagation unavailable")));
        QVERIFY(ranked.last().limitations.join(' ').contains(QStringLiteral("old")));
    }

    void neverAddsTransmitOrLogSideEffects()
    {
        Candidate c;
        c.call = QStringLiteral("K1ABC");
        const auto result = rank({c}, {}, QDateTime(QDate(2026, 10, 16), QTime(12, 0), QTimeZone::UTC));
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().candidate.call, QStringLiteral("K1ABC"));
    }
};

QTEST_GUILESS_MAIN(TestDxAssistant)
#include "tst_dxassistant.moc"
