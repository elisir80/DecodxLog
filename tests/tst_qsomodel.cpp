// Tabella del log: i filtri (entita', QSL, profilo, etichetta, date) e il loro
// salvataggio come mappa.
#include "app/QsoTableModel.h"
#include "core/LogDatabase.h"

#include <QTest>

using namespace decolog::core;
using decolog::app::QsoTableModel;

namespace {

void fill(LogDatabase& db)
{
    const qint64 profile = db.saveStationProfile(StationProfile{.name = "Casa", .stationCallsign = "IU8LMC"});
    db.insertQso({{"CALL", "K1ABC"}, {"QSO_DATE", "20260910"}, {"TIME_ON", "1203"}, {"BAND", "20m"}, {"MODE", "FT2"},
                  {"DXCC", "291"}, {"LOTW_QSL_RCVD", "Y"}, {"APP_DECOLOG_TAGS", "pota,Field Day"}}, "import", {}, false, profile);
    db.insertQso({{"CALL", "JA1XX"}, {"QSO_DATE", "20260911"}, {"TIME_ON", "0715"}, {"BAND", "40m"}, {"MODE", "FT8"},
                  {"DXCC", "339"}, {"APP_DECOLOG_TAGS", "potato"}}, "import");
    db.insertQso({{"CALL", "EA8ABC"}, {"QSO_DATE", "20260912"}, {"TIME_ON", "2359"}, {"BAND", "15m"}, {"MODE", "FT2"},
                  {"DXCC", "29"}, {"QSL_RCVD", "Y"}}, "import", {}, false, profile);
    db.insertQso({{"CALL", "W6XYZ"}, {"QSO_DATE", "20260913"}, {"TIME_ON", "0000"}, {"BAND", "20m"}, {"MODE", "CW"},
                  {"DXCC", "291"}}, "import");
}

QStringList calls(const QsoTableModel& m)
{
    QStringList out;
    for (int r = 0; r < m.count(); ++r)
        out << m.callAt(r);
    return out;
}

} // namespace

class TestQsoModel : public QObject {
    Q_OBJECT

private slots:
    void filters()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        fill(db);
        QsoTableModel m(&db);
        QCOMPARE(m.count(), 4);

        m.setDxccFilter(291);
        QCOMPARE(calls(m), QStringList({"W6XYZ", "K1ABC"}));
        m.setDxccFilter(0);

        m.setQslFilter("lotw");
        QCOMPARE(calls(m), QStringList({"K1ABC"}));
        m.setQslFilter("confirmed");
        QCOMPARE(calls(m), QStringList({"EA8ABC", "K1ABC"}));
        m.setQslFilter("unconfirmed");
        QCOMPARE(calls(m), QStringList({"W6XYZ", "JA1XX"}));
        m.setQslFilter("");

        m.setProfileFilter(1);
        QCOMPARE(calls(m), QStringList({"EA8ABC", "K1ABC"}));
        m.setProfileFilter(0);

        // Etichetta intera, maiuscole a parte: "pota" non prende "potato".
        m.setTagFilter("POTA");
        QCOMPARE(calls(m), QStringList({"K1ABC"}));
        m.setTagFilter("field day");
        QCOMPARE(calls(m), QStringList({"K1ABC"}));
        m.setTagFilter("");

        // Gli estremi sono compresi: il 12 fino alle 23:59.
        m.setDateFrom("2026-09-11");
        m.setDateTo("2026-09-12");
        QCOMPARE(calls(m), QStringList({"EA8ABC", "JA1XX"}));
        QVERIFY(m.filtered());

        const QVariantMap saved = m.filterState();
        m.clearFilters();
        QVERIFY(!m.filtered());
        QCOMPARE(m.count(), 4);
        m.applyFilterState(saved);
        QCOMPARE(calls(m), QStringList({"EA8ABC", "JA1XX"}));
        QCOMPARE(m.shownIds().size(), 2);

        QCOMPARE(m.valueAt(m.rowForId(1), QsoTableModel::Tags), QString());   // riga filtrata via
        m.clearFilters();
        QCOMPARE(m.valueAt(m.rowForId(1), QsoTableModel::Tags), QString("pota, Field Day"));
        QCOMPARE(m.tagsInLog().size(), 3);
    }

    // Le colonne che riempie il callbook: citta', nazione, stato, contea,
    // zone e IOTA. Si chiedono per nome, non per numero.
    void callbookColumns()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        db.insertQso({{"CALL", "W1AW"}, {"QSO_DATE", "20260914"}, {"TIME_ON", "1200"}, {"BAND", "20m"},
                      {"MODE", "CW"}, {"QTH", "Newington"}, {"COUNTRY", "United States"}, {"STATE", "CT"},
                      {"CNTY", "CT,Hartford"}, {"CQZ", "5"}, {"ITUZ", "8"}, {"IOTA", "NA-001"}},
                     "import");
        QsoTableModel m(&db);
        QCOMPARE(m.count(), 1);

        auto column = [&m](const QString& key) {
            for (int c = 0; c < m.columns(); ++c) {
                if (m.columnKey(c) == key)
                    return c;
            }
            return -1;
        };
        QCOMPARE(m.valueAt(0, column("qth")), QString("Newington"));
        QCOMPARE(m.valueAt(0, column("country")), QString("United States"));
        QCOMPARE(m.valueAt(0, column("state")), QString("CT"));
        QCOMPARE(m.valueAt(0, column("county")), QString("CT,Hartford"));
        QCOMPARE(m.valueAt(0, column("cqz")), QString("5"));
        QCOMPARE(m.valueAt(0, column("ituz")), QString("8"));
        QCOMPARE(m.valueAt(0, column("iota")), QString("NA-001"));

        // Le zone a zero sono zone che non ci sono: casella vuota.
        db.insertQso({{"CALL", "IK0ABC"}, {"QSO_DATE", "20260915"}, {"TIME_ON", "1200"}, {"BAND", "40m"},
                      {"MODE", "SSB"}},
                     "import");
        QsoTableModel bare(&db);
        QCOMPARE(bare.valueAt(bare.rowForId(2), column("cqz")), QString());
        QCOMPARE(bare.valueAt(bare.rowForId(2), column("qth")), QString());
    }

    void columnsChosenAndOrdered()
    {
        // Le colonne si scelgono e si mettono nell'ordine che si vuole, anche
        // campi ADIF che il log non ha come colonna (sono nei campi in piu') e
        // campi di altri programmi, per nome.
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        db.insertQso({{"CALL", "M9PAB"}, {"QSO_DATE", "20260923"}, {"TIME_ON", "071917"}, {"TIME_OFF", "072230"},
                      {"BAND", "20m"}, {"MODE", "SSB"}, {"COMMENT", "7267"}, {"QSL_VIA", "BURO"},
                      {"OPERATOR", "IK4IDF"}, {"FREQ_RX", "14.263"}, {"CONT", "EU"}, {"EQSL_QSL_RCVD", "Y"},
                      {"APP_LOGGER32_QSO_NUMBER", "123796"}},
                     "import");
        QsoTableModel m(&db);
        QCOMPARE(m.columnLayout(), QsoTableModel::defaultLayout());

        m.setColumnLayout({"comment", "call", "time_on", "time_off", "qsl_via", "operator", "freq_rx", "cont",
                           "eqsl_qsl_rcvd", "pfx", "x:APP_LOGGER32_QSO_NUMBER", "non_esiste"});
        // Quello che non si conosce non entra; il nominativo resta.
        QCOMPARE(m.columns(), 11);
        QCOMPARE(m.columnKey(0), QString("comment"));
        QCOMPARE(m.valueAt(0, 0), QString("7267"));
        QCOMPARE(m.valueAt(0, 1), QString("M9PAB"));
        QCOMPARE(m.valueAt(0, 2), QString("07:19"));
        QCOMPARE(m.valueAt(0, 3), QString("07:22"));
        QCOMPARE(m.valueAt(0, 4), QString("BURO"));
        QCOMPARE(m.valueAt(0, 5), QString("IK4IDF"));
        QCOMPARE(m.valueAt(0, 6), QString("14.263000"));
        QCOMPARE(m.valueAt(0, 7), QString("EU"));
        QCOMPARE(m.valueAt(0, 8), QString("Y"));
        QCOMPARE(m.valueAt(0, 9), QString("M9"));
        QCOMPARE(m.valueAt(0, 10), QString("123796"));
        QCOMPARE(m.columnTitle(10), QString("APP_LOGGER32_QSO_NUMBER"));
        QCOMPARE(m.data(m.index(0, 1)).toString(), QString("M9PAB"));
        // Per chiave si legge anche quello che non si vede.
        QCOMPARE(m.valueFor(0, "band"), QString("20m"));

        // Senza il nominativo non si resta: torna da solo.
        m.setColumnLayout({"band"});
        QCOMPARE(m.columnLayout(), QStringList({"call", "band"}));
    }
};

QTEST_GUILESS_MAIN(TestQsoModel)
#include "tst_qsomodel.moc"
