// Le date davanti all'operatore: nella forma della sua lingua, e lette in
// qualunque forma le scriva.
#include "core/Dates.h"

#include <QTest>

using namespace decolog::core;

class TestDates : public QObject {
    Q_OBJECT

private slots:
    void italianStartsFromTheDay()
    {
        dates::setLanguage(QStringLiteral("it"));
        QCOMPARE(dates::show(QStringLiteral("2026-09-25")), QStringLiteral("25/09/2026"));
        QCOMPARE(dates::show(QStringLiteral("20260925")), QStringLiteral("25/09/2026"));
        QCOMPARE(dates::show(QStringLiteral("2026-09-25 12:18")), QStringLiteral("25/09/2026 12:18"));
        QCOMPARE(dates::show(QStringLiteral("2026-09-25T12:18:00")), QStringLiteral("25/09/2026 12:18:00"));
        QCOMPARE(dates::showShort(QStringLiteral("2026-09-25")), QStringLiteral("25/09/26"));
        // Quello che non e' una data resta com'e'.
        QCOMPARE(dates::show(QStringLiteral("Y")), QStringLiteral("Y"));
        QCOMPARE(dates::show(QString()), QString());
    }

    void otherLanguages()
    {
        dates::setLanguage(QStringLiteral("de"));
        QCOMPARE(dates::show(QStringLiteral("2026-09-25")), QStringLiteral("25.09.2026"));
        dates::setLanguage(QStringLiteral("nl"));
        QCOMPARE(dates::show(QStringLiteral("2026-09-25")), QStringLiteral("25-09-2026"));
        dates::setLanguage(QStringLiteral("en"));
        QCOMPARE(dates::show(QStringLiteral("2026-09-25")), QStringLiteral("2026-09-25"));
        dates::setLanguage(QStringLiteral("zh_TW"));
        QCOMPARE(dates::show(QStringLiteral("20260925")), QStringLiteral("2026-09-25"));
    }

    void readsWhatIsTyped()
    {
        dates::setLanguage(QStringLiteral("it"));
        QCOMPARE(dates::read(QStringLiteral("25/09/2026")), QStringLiteral("2026-09-25"));
        QCOMPARE(dates::read(QStringLiteral("5/9/26")), QStringLiteral("2026-09-05"));
        QCOMPARE(dates::read(QStringLiteral("25.09.2026")), QStringLiteral("2026-09-25"));
        QCOMPARE(dates::read(QStringLiteral("2026-09-25")), QStringLiteral("2026-09-25"));
        QCOMPARE(dates::read(QStringLiteral("20260925")), QStringLiteral("2026-09-25"));
        QCOMPARE(dates::read(QStringLiteral("25092026")), QStringLiteral("2026-09-25"));
        QCOMPARE(dates::read(QStringLiteral("31/02/2026")), QString());
        QCOMPARE(dates::read(QStringLiteral("25/09")), QString());
        QCOMPARE(dates::read(QString()), QString());
    }
};

QTEST_GUILESS_MAIN(TestDates)
#include "tst_dates.moc"
