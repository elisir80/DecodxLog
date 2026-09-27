// La previsione di propagazione: numeri plausibili, non VOACAP.
#include "core/Propagation.h"

#include <QTest>
#include <QTimeZone>

using namespace decolog::core::propagation;

class TestPropagation : public QObject {
    Q_OBJECT

private:
    static int bandIndex(const QString& name)
    {
        for (int i = 0; i < bands().size(); ++i)
            if (bands().at(i).name == name)
                return i;
        return -1;
    }

private slots:
    void foF2DayAndNight()
    {
        // Napoli, fine settembre, 100 macchie: a mezzogiorno una decina di MHz,
        // a mezzanotte qualche MHz.
        const double noon = foF2(40.8, 14.3, QDateTime(QDate(2026, 9, 27), QTime(11, 0), QTimeZone::UTC), 100);
        const double night = foF2(40.8, 14.3, QDateTime(QDate(2026, 9, 27), QTime(1, 0), QTimeZone::UTC), 100);
        QVERIFY2(noon > 7.0 && noon < 12.0, qPrintable(QString::number(noon)));
        QVERIFY2(night > 2.5 && night < 6.0, qPrintable(QString::number(night)));
        // Con poche macchie il giorno e' piu' basso.
        QVERIFY(foF2(40.8, 14.3, QDateTime(QDate(2026, 9, 27), QTime(11, 0), QTimeZone::UTC), 10) < noon);
    }

    void europeToJapan()
    {
        Input in;
        in.fromLat = 40.8; in.fromLon = 14.3;     // JN70
        in.toLat = 35.7;   in.toLon = 139.7;      // Tokyo
        in.date = QDate(2026, 9, 27);
        in.solarFlux = 160;
        const Forecast f = forecast(in);
        QVERIFY(f.valid);
        QCOMPARE(f.hours.size(), 24);
        QVERIFY(f.distanceKm > 9000 && f.distanceKm < 10000);
        QCOMPARE(f.hops, 3);
        const int b20 = bandIndex("20m");
        const int b160 = bandIndex("160m");
        const int b10 = bandIndex("10m");
        // Il 20 m si apre in qualche ora del giorno, il 160 m di giorno no.
        int open20 = 0;
        for (const Hour& h : f.hours)
            open20 += h.quality.at(b20) > Closed ? 1 : 0;
        QVERIFY2(open20 >= 4, qPrintable(QString::number(open20)));
        QCOMPARE(f.hours.at(9).quality.at(b160), int(Closed));
        // La MUF di notte e' piu' bassa di quella del giorno europeo-asiatico.
        QVERIFY(f.hours.at(21).mufMhz < f.hours.at(7).mufMhz);
        // A mezzanotte UTC il 10 m e' chiuso.
        QCOMPARE(f.hours.at(0).quality.at(b10), int(Closed));
    }

    void shortPathNight()
    {
        // Italia–Germania di notte: 40 m si', 15 m no.
        Input in;
        in.fromLat = 40.8; in.fromLon = 14.3;
        in.toLat = 50.1;   in.toLon = 8.7;
        in.date = QDate(2026, 12, 15);
        in.solarFlux = 120;
        const Forecast f = forecast(in);
        QCOMPARE(f.hops, 1);
        QVERIFY(f.hours.at(2).quality.at(bandIndex("40m")) > Closed || f.hours.at(2).quality.at(bandIndex("80m")) > Closed);
        QCOMPARE(f.hours.at(2).quality.at(bandIndex("15m")), int(Closed));
    }
};

QTEST_GUILESS_MAIN(TestPropagation)
#include "tst_propagation.moc"
