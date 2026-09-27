// L'orologio mondiale: il Sole calcolato qui, confrontato con i valori di
// riferimento del 27 settembre 2026 (tolleranza due minuti), i casi polari e
// l'ora legale che cambia da sola.
#include "core/WorldClock.h"

#include <QTest>
#include <QTimeZone>

using namespace decolog::core;

namespace {

QDateTime utc(int y, int mo, int d, int h, int mi)
{
    return QDateTime(QDate(y, mo, d), QTime(h, mi), QTimeZone::UTC);
}

bool near(const QDateTime& got, const QDateTime& want, int minutes = 2)
{
    return got.isValid() && std::abs(got.secsTo(want)) <= minutes * 60;
}

QString show(const QDateTime& t) { return t.toString(QStringLiteral("dd/MM hh:mm")); }

} // namespace

class TestWorldClock : public QObject {
    Q_OBJECT

private slots:
    void referenceDays_data()
    {
        QTest::addColumn<double>("lat");
        QTest::addColumn<double>("lon");
        QTest::addColumn<QDateTime>("rise");
        QTest::addColumn<QDateTime>("set");
        QTest::addColumn<QDateTime>("noon");
        QTest::addColumn<QDateTime>("civilRise");
        QTest::addColumn<QDateTime>("civilSet");
        QTest::newRow("JN71DC") << 41.104 << 14.292 << utc(2026, 9, 27, 4, 56) << utc(2026, 9, 27, 16, 54)
                                << utc(2026, 9, 27, 10, 55) << utc(2026, 9, 27, 4, 28) << utc(2026, 9, 27, 17, 21);
        QTest::newRow("New York") << 40.71 << -74.01 << utc(2026, 9, 27, 10, 49) << utc(2026, 9, 27, 22, 47)
                                  << utc(2026, 9, 27, 16, 48) << utc(2026, 9, 27, 10, 22) << utc(2026, 9, 27, 23, 14);
        QTest::newRow("Sydney") << -33.87 << 151.21 << utc(2026, 9, 26, 19, 39) << utc(2026, 9, 27, 7, 55)
                                << utc(2026, 9, 27, 1, 47) << utc(2026, 9, 26, 19, 14) << utc(2026, 9, 27, 8, 20);
    }

    void referenceDays()
    {
        QFETCH(double, lat);
        QFETCH(double, lon);
        QFETCH(QDateTime, rise);
        QFETCH(QDateTime, set);
        QFETCH(QDateTime, noon);
        QFETCH(QDateTime, civilRise);
        QFETCH(QDateTime, civilSet);
        const worldclock::SunDay day = worldclock::sunDay(QDate(2026, 9, 27), lat, lon);
        QVERIFY(day.sun.valid);
        QVERIFY2(near(day.sun.rise, rise), qPrintable(show(day.sun.rise)));
        QVERIFY2(near(day.sun.set, set), qPrintable(show(day.sun.set)));
        QVERIFY2(near(day.noon, noon), qPrintable(show(day.noon)));
        QVERIFY2(near(day.civil.rise, civilRise), qPrintable(show(day.civil.rise)));
        QVERIFY2(near(day.civil.set, civilSet), qPrintable(show(day.civil.set)));
    }

    void subSolarPoint()
    {
        const auto s = worldclock::subSolar(utc(2026, 9, 27, 12, 0));
        QVERIFY2(std::abs(s.lat - (-1.57)) < 0.05, qPrintable(QString::number(s.lat)));
        QVERIFY2(std::abs(s.lon - (-1.91)) < 0.1, qPrintable(QString::number(s.lon)));
        // A picco il Sole e' a 90°, agli antipodi a -90°.
        QVERIFY(std::abs(worldclock::sunAltitude(s.lat, s.lon, s) - 90.0) < 0.01);
        QVERIFY(std::abs(worldclock::sunAltitude(-s.lat, s.lon + 180.0, s) + 90.0) < 0.01);
    }

    void polarDays()
    {
        // 78°N: a giugno il Sole non tramonta, a dicembre non sorge.
        const auto june = worldclock::sunDay(QDate(2026, 6, 21), 78.0, 15.0);
        QVERIFY(!june.sun.valid);
        QVERIFY(june.sun.alwaysUp);
        const auto december = worldclock::sunDay(QDate(2026, 12, 21), 78.0, 15.0);
        QVERIFY(!december.sun.valid);
        QVERIFY(!december.sun.alwaysUp);
        QVERIFY(december.noon.isValid());
    }

    void terminatorAndGrayline()
    {
        const auto s = worldclock::subSolar(utc(2026, 9, 27, 12, 0));
        const auto t = worldclock::terminator(s, worldclock::kSunset);
        QCOMPARE(t.boundary.size(), 181);
        QCOMPARE(t.boundary.first().x(), -180.0);
        QCOMPARE(t.boundary.last().x(), 180.0);
        // Sul confine il Sole e' proprio alla soglia.
        for (const QPointF& p : t.boundary) {
            if (std::abs(p.y()) < 89.0)
                QVERIFY(std::abs(worldclock::sunAltitude(p.y(), p.x(), s) - worldclock::kSunset) < 1.0);
        }
    }

    void maidenhead()
    {
        QCOMPARE(worldclock::locator4(41.104, 14.292), QString("JN71"));
        QCOMPARE(worldclock::locator4(40.71, -74.01), QString("FN20"));
        QCOMPARE(worldclock::locator4(-33.87, 151.21), QString("QF56"));
    }

    void offsetsAndDaylightSaving()
    {
        QCOMPARE(worldclock::offsetLabel(7200), QString("UTC+2"));
        QCOMPARE(worldclock::offsetLabel(-4 * 3600), QString("UTC−4"));
        QCOMPARE(worldclock::offsetLabel(0), QString("UTC±0"));
        QCOMPARE(worldclock::offsetLabel(19800), QString("UTC+5:30"));
        // New York cambia ora da sola: prima e dopo il 1° novembre 2026.
        const QTimeZone ny("America/New_York");
        QVERIFY(ny.isValid());
        QCOMPARE(ny.offsetFromUtc(utc(2026, 10, 31, 12, 0)), -4 * 3600);
        QCOMPARE(ny.offsetFromUtc(utc(2026, 11, 2, 12, 0)), -5 * 3600);
    }
};

QTEST_GUILESS_MAIN(TestWorldClock)
#include "tst_worldclock.moc"
