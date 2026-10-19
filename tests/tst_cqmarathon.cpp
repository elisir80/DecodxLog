#include "core/CqMarathon.h"

#include <QJsonObject>
#include <QTest>

using namespace decolog::core::cqmarathon;

class TestCqMarathon : public QObject {
    Q_OBJECT
private slots:
    void selectsDeterministicallyByEditionCategoryAndEntity()
    {
        const QDateTime t1(QDate(2026, 1, 2), QTime(10, 0), QTimeZone::UTC);
        const QDateTime t2(QDate(2026, 1, 2), QTime(11, 0), QTimeZone::UTC);
        QList<Entry> entries{
            {2, QStringLiteral("K1AAA"), 291, 5, QStringLiteral("20m"), QStringLiteral("CW"), t2},
            {1, QStringLiteral("K2BBB"), 291, 5, QStringLiteral("20m"), QStringLiteral("CW"), t1},
            {3, QStringLiteral("DL1ABC"), 230, 14, QStringLiteral("20m"), QStringLiteral("SSB"), t1},
            {4, QStringLiteral("JA1XYZ"), 339, 25, QStringLiteral("20m"), QStringLiteral("CW"), QDateTime(QDate(2026, 1, 3), QTime(9, 0), QTimeZone::UTC)},
        };
        Rules rules{2026, QStringLiteral("cw"), 1};
        const Result result = select(entries, rules);
        QCOMPARE(result.selected.size(), 2);
        QCOMPARE(result.selected.at(0).qsoId, qint64(1));
        QCOMPARE(result.selected.at(1).qsoId, qint64(4));
        QVERIFY(result.exclusions.join('\n').contains(QStringLiteral("entity limit")));
        QVERIFY(result.exclusions.join('\n').contains(QStringLiteral("category mismatch")));
        rules.stationProfileId = 7;
        Entry selected = entries.at(0);
        selected.stationProfileId = 7;
        Entry other = selected;
        other.qsoId = 10;
        other.stationProfileId = 8;
        const auto scoped = select({selected, other}, rules);
        QCOMPARE(scoped.selected.size(), 1);
        QVERIFY(scoped.exclusions.join('\n').contains(QStringLiteral("station profile")));
        const QJsonObject exported = result.toJson().object();
        QCOMPARE(exported.value(QStringLiteral("format")).toString(), QStringLiteral("decodxlog-cq-marathon-1"));
        QCOMPARE(exported.value(QStringLiteral("edition")).toInt(), 2026);
    }

    void invalidRulesAreRejectedExplicitly()
    {
        Rules bad{2026, QStringLiteral("unknown"), 1};
        const Result result = select({}, bad);
        QVERIFY(result.selected.isEmpty());
        QVERIFY(result.exclusions.first().contains(QStringLiteral("unsupported")));
    }
};

QTEST_MAIN(TestCqMarathon)
#include "tst_cqmarathon.moc"
