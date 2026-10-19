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

    void staleSpotsArePenalizedOrRejectedByPolicy()
    {
        const QDateTime now(QDate(2026, 10, 16), QTime(12, 0), QTimeZone::UTC);
        Candidate fresh;
        fresh.call = QStringLiteral("FRESH");
        fresh.spottedAt = now.addSecs(-5 * 60);
        fresh.status = 1;
        Candidate stale = fresh;
        stale.call = QStringLiteral("STALE");
        stale.spottedAt = now.addSecs(-90 * 60);
        Preferences p;
        p.freshnessMinutes = 30;
        p.stalePenalty = 0.25;
        const auto penalized = rank({stale, fresh}, p, now);
        QCOMPARE(penalized.first().candidate.call, QStringLiteral("FRESH"));
        QVERIFY(penalized.last().reasons.join(' ').contains(QStringLiteral("penalty")));

        p.rejectStale = true;
        const auto rejected = rank({stale, fresh}, p, now);
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(rejected.first().candidate.call, QStringLiteral("FRESH"));
    }

    void goalsAndStationFeasibilityAreExplicit()
    {
        const QDateTime now(QDate(2026, 10, 18), QTime(12, 0), QTimeZone::UTC);
        Candidate worked;
        worked.call = QStringLiteral("K1AAA");
        worked.band = QStringLiteral("20m");
        worked.mode = QStringLiteral("CW");
        worked.spottedAt = now;
        worked.worked = true;
        worked.confirmed = false;
        worked.awardAccount = QStringLiteral("home");
        Candidate infeasible = worked;
        infeasible.call = QStringLiteral("K2BBB");
        infeasible.band = QStringLiteral("40m");

        Preferences p;
        p.awardAccount = QStringLiteral("home");
        p.availableBands = {QStringLiteral("20m")};
        p.availableModes = {QStringLiteral("CW")};
        p.requireFeasible = true;
        const auto ranked = rank({infeasible, worked}, p, now);
        QCOMPARE(ranked.size(), 1);
        QCOMPARE(ranked.first().candidate.call, QStringLiteral("K1AAA"));
        QVERIFY(ranked.first().reasons.contains(QStringLiteral("worked but unconfirmed")));
        QVERIFY(ranked.first().reasons.contains(QStringLiteral("station feasible")));

        worked.observed = false;
        const auto forecast = rank({worked}, p, now);
        QVERIFY(forecast.first().limitations.contains(QStringLiteral("forecast or inferred data")));
        QCOMPARE(forecast.first().evidence, QStringLiteral("forecast/inferred"));

        Preferences named = p;
        named.availableRadios = {QStringLiteral("IC-7300")};
        named.availableAntennas = {QStringLiteral("dipole")};
        worked.observed = true;
        worked.radio = QStringLiteral("FT-991");
        worked.antenna = QStringLiteral("dipole");
        const auto rejectedRadio = rank({worked}, named, now);
        QCOMPARE(rejectedRadio.size(), 0);
        worked.radio = QStringLiteral("IC-7300");
        const auto acceptedHardware = rank({worked}, named, now);
        QCOMPARE(acceptedHardware.size(), 1);
        QCOMPARE(acceptedHardware.first().evidence, QStringLiteral("observed"));
    }
};

QTEST_GUILESS_MAIN(TestDxAssistant)
#include "tst_dxassistant.moc"
