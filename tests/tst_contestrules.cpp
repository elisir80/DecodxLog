// DecoDXLog — le regole dei contest: punti, moltiplicatori, scambio.
//
// Ogni numero qui dentro viene dal regolamento, e il commento dice da quale
// punto: un punteggio sbagliato non si vede guardando lo schermo, si vede
// quando arriva la classifica, e allora e' tardi.
#include "core/ContestRules.h"

#include <QTest>

using namespace decolog::core;
using namespace decolog::core::contestrules;

namespace {

// Io sono IU8LMC: Italia, Europa, zona CQ 15, zona ITU 28.
ContestStation me()
{
    return ContestStation{248, QStringLiteral("EU"), 15, 28};
}

ContestQso qso(const QString& call, int dxcc, const QString& cont, const QString& band,
               const QString& mode = QStringLiteral("CW"), const QString& exchange = {},
               int cqZone = 0, int ituZone = 0)
{
    ContestQso q;
    q.call = call;
    q.dxcc = dxcc;
    q.continent = cont;
    q.band = band;
    q.mode = mode;
    q.exchange = exchange;
    q.cqZone = cqZone;
    q.ituZone = ituZone;
    return q;
}

} // namespace

class TestContestRules : public QObject {
    Q_OBJECT

private slots:
    void aContestWithoutRulesSaysSo()
    {
        // Duecentocinquanta contest hanno un identificativo, dieci hanno le
        // regole: gli altri non devono dare un punteggio inventato.
        const ContestRules unknown = forId(QStringLiteral("AL-QSO-PARTY"));
        QVERIFY(!unknown.valid);
        QCOMPARE(points(unknown, qso("W1AW", 291, "NA", "20m"), me()), 0);
        QVERIFY(multipliers(unknown, qso("W1AW", 291, "NA", "20m"), me()).isEmpty());
        QVERIFY(checkExchange(unknown, QStringLiteral("qualunque cosa")).isEmpty());

        QVERIFY(forId(QStringLiteral("CQ-WW-SSB")).valid);
        QCOMPARE(known().size(), 10);
    }

    void cqWorldWideCountsTheZoneAndTheCountry()
    {
        const ContestRules r = forId(QStringLiteral("CQ-WW-CW"));
        QCOMPARE(r.exchange, ContestRules::Exchange::CqZone);

        // Regolamento VII: stesso paese 0 punti, ma conta per il moltiplicatore.
        QCOMPARE(points(r, qso("IK0ABC", 248, "EU", "20m", "CW", "15", 15), me()), 0);
        // Stesso continente, paese diverso: 1 punto.
        QCOMPARE(points(r, qso("DL9ZZT", 230, "EU", "20m", "CW", "14", 14), me()), 1);
        // Continente diverso: 3 punti.
        QCOMPARE(points(r, qso("W1AW", 291, "NA", "20m", "CW", "5", 5), me()), 3);

        // I moltiplicatori sono due, e contano per banda.
        const QStringList mults = multipliers(r, qso("W1AW", 291, "NA", "20m", "CW", "5", 5), me());
        QCOMPARE(mults.size(), 2);
        QVERIFY(mults.at(0).contains(QStringLiteral("zona 5")));
        QVERIFY(mults.at(0).endsWith(QStringLiteral("|20m")));
        QVERIFY(mults.at(1).contains(QStringLiteral("paese 291")));
        // La stessa zona su un'altra banda e' un altro moltiplicatore.
        const QStringList other = multipliers(r, qso("W2XX", 291, "NA", "40m", "CW", "5", 5), me());
        QVERIFY(other.at(0) != mults.at(0));
        // E il proprio paese fa moltiplicatore anche se il QSO vale zero punti.
        QCOMPARE(multipliers(r, qso("IK0ABC", 248, "EU", "20m", "CW", "15", 15), me()).size(), 2);
    }

    void inNorthAmericaTheSameContinentIsWorthTwo()
    {
        // Regolamento VII: fra due stazioni del Nord America un QSO dentro il
        // continente vale 2 punti invece di 1.
        const ContestRules r = forId(QStringLiteral("CQ-WW-SSB"));
        const ContestStation american{291, QStringLiteral("NA"), 5, 8};
        QCOMPARE(points(r, qso("VE3ABC", 1, "NA", "20m", "SSB", "5", 5), american), 2);
        // Ma per un europeo il QSO dentro l'Europa resta 1 punto.
        QCOMPARE(points(r, qso("DL9ZZT", 230, "EU", "20m", "SSB", "14", 14), me()), 1);
    }

    void wpxPaysDoubleOnTheLowBands()
    {
        const ContestRules r = forId(QStringLiteral("CQ-WPX-CW"));
        QCOMPARE(r.exchange, ContestRules::Exchange::Serial);

        // Regolamento VI: continente diverso 3 punti sulle bande alte, 6 sulle
        // basse; stesso continente 1 e 2; il proprio paese 1 su tutte.
        QCOMPARE(points(r, qso("W1AW", 291, "NA", "20m", "CW", "001"), me()), 3);
        QCOMPARE(points(r, qso("W1AW", 291, "NA", "40m", "CW", "001"), me()), 6);
        QCOMPARE(points(r, qso("DL9ZZT", 230, "EU", "20m", "CW", "001"), me()), 1);
        QCOMPARE(points(r, qso("DL9ZZT", 230, "EU", "80m", "CW", "001"), me()), 2);
        QCOMPARE(points(r, qso("IK0ABC", 248, "EU", "20m", "CW", "001"), me()), 1);
        QCOMPARE(points(r, qso("IK0ABC", 248, "EU", "160m", "CW", "001"), me()), 1);

        // Il moltiplicatore e' il prefisso, e si conta una volta sola: la stessa
        // stazione su un'altra banda non ne porta un altro.
        const QStringList first = multipliers(r, qso("DL9ZZT", 230, "EU", "20m"), me());
        const QStringList again = multipliers(r, qso("DL9ZZT", 230, "EU", "40m"), me());
        QCOMPARE(first.size(), 1);
        QCOMPARE(first, again);
        QVERIFY(first.first().contains(QStringLiteral("DL9")));
    }

    void iaruCountsZonesAndHeadquarters()
    {
        const ContestRules r = forId(QStringLiteral("IARU-HF"));
        QCOMPARE(r.exchange, ContestRules::Exchange::ItuZone);

        // Regolamento 5.1: la propria zona 1 punto, una HQ 1 punto, stessa zona
        // altro continente 1, stesso continente altra zona 3, tutto diverso 5.
        QCOMPARE(points(r, qso("IK0ABC", 248, "EU", "20m", "CW", "28", 0, 28), me()), 1);
        QCOMPARE(points(r, qso("DA0HQ", 230, "EU", "20m", "CW", "DARC"), me()), 1);
        QCOMPARE(points(r, qso("DL9ZZT", 230, "EU", "20m", "CW", "28", 0, 28), me()), 1);
        QCOMPARE(points(r, qso("SM3ABC", 284, "EU", "20m", "CW", "18", 0, 18), me()), 3);
        QCOMPARE(points(r, qso("W1AW", 291, "NA", "20m", "CW", "8", 0, 8), me()), 5);

        // 5.2: le zone e le HQ sono moltiplicatori per banda, e una HQ non porta
        // anche la zona.
        const QStringList hq = multipliers(r, qso("DA0HQ", 230, "EU", "20m", "CW", "DARC"), me());
        QCOMPARE(hq.size(), 1);
        QVERIFY(hq.first().startsWith(QStringLiteral("hq DARC")));
        const QStringList zone = multipliers(r, qso("W1AW", 291, "NA", "20m", "CW", "8", 0, 8), me());
        QCOMPARE(zone.size(), 1);
        QVERIFY(zone.first().startsWith(QStringLiteral("zona 8")));
    }

    void ariDxPaysTenForAnItalianStation()
    {
        const ContestRules r = forId(QStringLiteral("ARI-DX"));
        // Una stazione italiana vale 10 punti, anche per un italiano.
        QCOMPARE(points(r, qso("IK0ABC", 248, "EU", "20m", "SSB", "RM"), me()), 10);
        // Anche la Sardegna, che e' un'altra entita' DXCC ma lo stesso paese.
        QCOMPARE(points(r, qso("IS0ABC", 225, "EU", "20m", "SSB", "CA"), me()), 10);
        QCOMPARE(points(r, qso("DL9ZZT", 230, "EU", "20m", "SSB", "001"), me()), 1);
        QCOMPARE(points(r, qso("W1AW", 291, "NA", "20m", "SSB", "001"), me()), 3);

        // I moltiplicatori: le province delle italiane, i paesi delle altre.
        const QStringList province = multipliers(r, qso("IK0ABC", 248, "EU", "20m", "SSB", "RM"), me());
        QCOMPARE(province.size(), 1);
        QVERIFY(province.first().startsWith(QStringLiteral("prov RM")));
        const QStringList country = multipliers(r, qso("W1AW", 291, "NA", "20m", "SSB", "001"), me());
        QCOMPARE(country.size(), 1);
        QVERIFY(country.first().startsWith(QStringLiteral("paese 291")));
        // L'Italia non e' un moltiplicatore-paese: se la provincia non si capisce
        // non si conta niente.
        QVERIFY(multipliers(r, qso("IK0ABC", 248, "EU", "20m", "SSB", "boh"), me()).isEmpty());
    }

    void theAriSectionsContestPaysByBand()
    {
        const ContestRules r = forId(QStringLiteral("ARI-SEZIONI"));
        QCOMPARE(r.exchange, ContestRules::Exchange::AriSection);

        // Punto 7: 1 punto sui 40, 2 su 80 e 20, 3 su 160 e 15, 4 sui 10.
        QCOMPARE(points(r, qso("IK2ABC", 248, "EU", "40m", "SSB", "L01"), me()), 1);
        QCOMPARE(points(r, qso("IK2ABC", 248, "EU", "80m", "SSB", "L01"), me()), 2);
        QCOMPARE(points(r, qso("IK2ABC", 248, "EU", "20m", "SSB", "L01"), me()), 2);
        QCOMPARE(points(r, qso("IK2ABC", 248, "EU", "160m", "SSB", "L01"), me()), 3);
        QCOMPARE(points(r, qso("IK2ABC", 248, "EU", "15m", "SSB", "L01"), me()), 3);
        QCOMPARE(points(r, qso("IK2ABC", 248, "EU", "10m", "SSB", "L01"), me()), 4);
        // Una banda che il contest non usa non porta punti.
        QCOMPARE(points(r, qso("IK2ABC", 248, "EU", "30m", "SSB", "L01"), me()), 0);

        // Punto 8: il codice ASC conta fino a tre volte per banda, una per modo.
        const QStringList ssb = multipliers(r, qso("IK2ABC", 248, "EU", "20m", "SSB", "L01"), me());
        const QStringList cw = multipliers(r, qso("IK2DEF", 248, "EU", "20m", "CW", "L01"), me());
        const QStringList rtty = multipliers(r, qso("IK2GHI", 248, "EU", "20m", "RTTY", "L01"), me());
        QCOMPARE(ssb.size(), 1);
        QVERIFY(ssb != cw && cw != rtty && ssb != rtty);
        // Lo stesso codice nello stesso modo e sulla stessa banda e' lo stesso.
        QCOMPARE(multipliers(r, qso("IK2XYZ", 248, "EU", "20m", "SSB", "L 01"), me()), ssb);
    }

    void the4080PaysByMode()
    {
        const ContestRules r = forId(QStringLiteral("ARI-40-80"));
        // CW 3 punti, RTTY 2, SSB 1.
        QCOMPARE(points(r, qso("IK2ABC", 248, "EU", "40m", "CW", "MI"), me()), 3);
        QCOMPARE(points(r, qso("IK2ABC", 248, "EU", "40m", "RTTY", "MI"), me()), 2);
        QCOMPARE(points(r, qso("IK2ABC", 248, "EU", "80m", "SSB", "MI"), me()), 1);
        // I moltiplicatori sono le province, per banda e per modo.
        const QStringList a = multipliers(r, qso("IK2ABC", 248, "EU", "40m", "CW", "MI"), me());
        const QStringList b = multipliers(r, qso("IK2DEF", 248, "EU", "40m", "SSB", "MI"), me());
        QCOMPARE(a.size(), 1);
        QVERIFY(a != b);
    }

    void theExchangeIsCheckedBeforeItGoesInTheLog()
    {
        // Una zona CQ sta fra 1 e 40: "55" e' un errore di battitura, e dirlo
        // adesso costa meno che ritrovarlo nel log a gara finita.
        const ContestRules ww = forId(QStringLiteral("CQ-WW-SSB"));
        QVERIFY(checkExchange(ww, QStringLiteral("15")).isEmpty());
        QVERIFY(!checkExchange(ww, QStringLiteral("55")).isEmpty());
        QVERIFY(!checkExchange(ww, QStringLiteral("EU")).isEmpty());
        QVERIFY(!checkExchange(ww, QString()).isEmpty());

        const ContestRules wpx = forId(QStringLiteral("CQ-WPX-CW"));
        QVERIFY(checkExchange(wpx, QStringLiteral("001")).isEmpty());
        QVERIFY(!checkExchange(wpx, QStringLiteral("RM")).isEmpty());

        // Nello IARU una sigla di societa' va bene quanto una zona.
        const ContestRules iaru = forId(QStringLiteral("IARU-HF"));
        QVERIFY(checkExchange(iaru, QStringLiteral("28")).isEmpty());
        QVERIFY(checkExchange(iaru, QStringLiteral("ARRL")).isEmpty());
        QVERIFY(!checkExchange(iaru, QStringLiteral("99")).isEmpty());

        // Nell'ARI DX la provincia o il progressivo.
        const ContestRules ari = forId(QStringLiteral("ARI-DX"));
        QVERIFY(checkExchange(ari, QStringLiteral("NA")).isEmpty());
        QVERIFY(checkExchange(ari, QStringLiteral("027")).isEmpty());
        QVERIFY(!checkExchange(ari, QStringLiteral("ZZ")).isEmpty());

        // Il codice ASC e' una lettera e due cifre.
        const ContestRules sez = forId(QStringLiteral("ARI-SEZIONI"));
        QVERIFY(checkExchange(sez, QStringLiteral("L01")).isEmpty());
        QVERIFY(checkExchange(sez, QStringLiteral("r 01")).isEmpty());
        QVERIFY(!checkExchange(sez, QStringLiteral("L1")).isEmpty());
        QVERIFY(!checkExchange(sez, QStringLiteral("123")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestContestRules)
#include "tst_contestrules.moc"
