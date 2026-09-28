// VOACAP: la scheda d'ingresso scritta colonna per colonna come la legge il
// Fortran, la lettura dell'uscita, e — se il motore e' stato compilato — una
// previsione vera da Napoli a Tokyo.
#include "core/Voacap.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace decolog::core;

namespace {

QString lineOf(const QString& text, const QString& card)
{
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        if (line.startsWith(card))
            return line.trimmed();
    }
    return {};
}

} // namespace

class TestVoacap : public QObject
{
    Q_OBJECT

private slots:
    void theDeckIsReadAsVoacapExpectsIt()
    {
        // L'esempio di voacapl: Tangeri → Belgrado, giugno 1994.
        voacap::Request r;
        r.fromLat = 35.80;
        r.fromLon = -5.90;
        r.toLat = 44.90;
        r.toLon = 20.50;
        r.fromLabel = "TANGIER, Morocco";
        r.toLabel = "BELGRADE";
        r.year = 1994;
        r.month = 6;
        r.ssn = 100;
        r.noise = 145;
        r.minAngle = 0.10;
        r.requiredSnr = 73.0;
        r.powerWatts = 500000;
        r.mhz = {6.07, 7.20, 9.70, 11.85, 13.70, 15.35, 17.73, 21.65, 25.89};
        const QString deck = voacap::deck(r);

        QFile f(VOACAPL_SAMPLE_DIR "/voacapx.dat");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString sample = QString::fromLatin1(f.readAll());
        for (const char* card : {"COEFFS", "TIME", "MONTH", "SUNSPOT", "LABEL", "CIRCUIT", "FPROB", "FREQUENCY", "METHOD"})
            QCOMPARE(lineOf(deck, card), lineOf(sample, card));
        // SYSTEM: l'esempio scrive "73.0" e "1.", qui "73.00" e "1.00": gli
        // stessi numeri negli stessi campi da cinque colonne.
        const QString mine = lineOf(deck, "SYSTEM");
        const QString theirs = lineOf(sample, "SYSTEM");
        for (int i = 0; i < 7; ++i)
            QCOMPARE(mine.mid(10 + 5 * i, 5).toDouble(), theirs.mid(10 + 5 * i, 5).toDouble());
        // L'antenna: stesse colonne, il nome fra le parentesi quadre in 21 caratteri.
        const QString tx = lineOf(deck, "ANTENNA       1");
        QCOMPARE(tx.indexOf('['), lineOf(sample, "ANTENNA       1").indexOf('['));
        QCOMPARE(tx.indexOf(']') - tx.indexOf('['), 22);
        QVERIFY(tx.endsWith("500.0000"));
        // Il guadagno dell'isotropa sta nel campo della frequenza di progetto.
        r.txGainDbi = 5.5;
        QVERIFY(lineOf(voacap::deck(r), "ANTENNA       1").contains("     5.500[default/isotrope"));
    }

    void theOutputIsRead()
    {
        QFile f(VOACAPL_SAMPLE_DIR "/voacapx.out");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const voacap::Result r = voacap::parse(QString::fromLatin1(f.readAll()));
        QVERIFY2(r.valid, qPrintable(r.error));
        QCOMPARE(r.hours.size(), 24);
        QCOMPARE(r.mhz.size(), 9);
        // In ordine dalle 00 UTC: la riga "24.0" di VOACAP e' la mezzanotte.
        QCOMPARE(r.hours.first().hourUtc, 0);
        const voacap::HourResult& one = r.hours.at(1);
        QCOMPARE(one.hourUtc, 1);
        QCOMPARE(one.muf, 16.3);
        QCOMPARE(one.cells.size(), 9);
        QCOMPARE(one.cells.at(4).rel, 0.99);      // 13.7 MHz
        QCOMPARE(one.cells.at(4).snr, 105.0);
        QCOMPARE(one.cells.at(4).mode, QString("1F2"));
        QCOMPARE(one.cells.at(8).rel, 0.0);       // 25.9 MHz, sopra la MUF
        QCOMPARE(voacap::qualityOf(one.cells.at(4)), 3);
        QCOMPARE(voacap::qualityOf(one.cells.at(8)), 0);
        // Un'uscita senza ore non e' una previsione.
        QVERIFY(!voacap::parse("nothing here").valid);
    }

    void theModeSetsTheSnr()
    {
        QCOMPARE(voacap::requiredSnrFor("FT8"), 16.0);
        QVERIFY(voacap::requiredSnrFor("SSB") > voacap::requiredSnrFor("CW"));
        QVERIFY(voacap::requiredSnrFor("CW") > voacap::requiredSnrFor("FT8"));
        QCOMPARE(voacap::requiredSnrFor("boh"), voacap::requiredSnrFor("FT8"));
    }

    void aRealForecast()
    {
        qputenv("DECODXLOG_VOACAP_DIR", VOACAP_BUILD_DIR);
        voacap::Engine engine;
        if (!engine.available())
            QSKIP("VOACAP not built here (no Fortran compiler)");
        QTemporaryDir work;
        engine.setWorkDir(work.path());
        voacap::Request r;
        r.fromLat = 40.85;      // Napoli
        r.fromLon = 14.27;
        r.toLat = 35.68;        // Tokyo
        r.toLon = 139.69;
        r.fromLabel = "IU8LMC";
        r.toLabel = "JA1";
        r.year = 2026;
        r.month = 9;
        r.ssn = 110;
        r.requiredSnr = voacap::requiredSnrFor("FT8");
        r.mhz = {3.6, 7.1, 14.1, 21.2, 28.5};
        QSignalSpy spy(&engine, &voacap::Engine::finished);
        engine.run(r);
        QVERIFY(spy.wait(30000));
        const QString key = spy.first().at(0).toString();
        const auto result = spy.first().at(1).value<voacap::Result>();
        QCOMPARE(key, r.key());
        QVERIFY2(result.valid, qPrintable(result.error));
        QCOMPARE(result.hours.size(), 24);
        QCOMPARE(result.mhz.size(), 5);
        // Qualcosa si apre in 24 ore sui 20 metri verso il Giappone, con 100 W e FT8.
        double best = 0;
        for (const auto& h : result.hours)
            best = std::max(best, h.cells.at(2).rel);
        QVERIFY(best > 0.3);
        // Il guadagno delle antenne arriva davvero a VOACAP.
        QFile out(work.path() + "/itshfbc/run/voacapx.out");
        QVERIFY(out.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromLatin1(out.readAll()).contains("+  2.0 dBi"));
        // Una seconda volta, con i dati gia' copiati.
        r.mhz = {14.1};
        engine.run(r);
        QVERIFY(spy.wait(30000));
        QVERIFY(spy.last().at(1).value<voacap::Result>().valid);
    }
};

QTEST_GUILESS_MAIN(TestVoacap)
#include "tst_voacap.moc"
