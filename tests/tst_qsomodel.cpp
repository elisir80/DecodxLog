// Tabella del log: i filtri (entita', QSL, profilo, etichetta, date) e il loro
// salvataggio come mappa.
#include "app/QsoTableModel.h"
#include "core/LogDatabase.h"

#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

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
    // Le categorie delle righe, come i colori di Decodium 4.
    void rowCategories()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        auto add = [&db](const char* call, const char* date, const char* band, const char* dxcc,
                         const char* grid = "", bool lotw = false) {
            AdifRecord r{{"CALL", call}, {"QSO_DATE", date}, {"TIME_ON", "1200"}, {"BAND", band}, {"MODE", "FT8"},
                         {"DXCC", dxcc}};
            if (*grid)
                r.set("GRIDSQUARE", grid);
            if (lotw)
                r.set("LOTW_QSL_RCVD", "Y");
            return db.insertQso(r, "import").id;
        };
        const qint64 first = add("K1ABC", "20260101", "20m", "291", "FN42");
        const qint64 sameBand = add("W1AW", "20260102", "20m", "291", "FN42");
        const qint64 newBand = add("W1AW", "20260103", "40m", "291", "FN42");
        const qint64 again = add("W1AW", "20260104", "40m", "291", "FN42", true);
        const qint64 repeat = add("W1AW", "20260105", "40m", "291", "FN42");
        QsoTableModel m(&db);
        auto category = [&m](qint64 id) {
            for (int r = 0; r < m.count(); ++r) {
                const QModelIndex i = m.index(r, 0);
                if (m.data(i, QsoTableModel::IdRole).toLongLong() == id)
                    return m.data(i, QsoTableModel::CategoryRole).toString();
            }
            return QString();
        };
        QCOMPARE(category(first), QString("colorNewDxcc"));
        QCOMPARE(category(sameBand), QString("colorNewCall"));
        QCOMPARE(category(newBand), QString("colorNewDxccBand"));
        QCOMPARE(category(again), QString("colorLotwConfirmed"));
        QCOMPARE(category(repeat), QString("colorB4"));

        // Un QSO nuovo in fondo: la sua categoria si conta da sola.
        const qint64 fresh = add("JA1XX", "20260106", "15m", "339", "PM95");
        m.insertQso(fresh);
        QCOMPARE(category(fresh), QString("colorNewDxcc"));
        const qint64 fresh2 = add("JA1YY", "20260107", "15m", "339", "PM96");
        m.insertQso(fresh2);
        QCOMPARE(category(fresh2), QString("colorNewGrid"));

        // Uno scritto dopo con l'ora di prima: si guarda cosa c'era prima di lui.
        const qint64 older = add("VK2AA", "20251231", "20m", "150", "QF56");
        m.insertQso(older);
        QCOMPARE(category(older), QString("colorNewDxcc"));
        const qint64 olderSame = add("W9XYZ", "20260101", "40m", "291", "FN42");
        m.insertQso(olderSame);
        QCOMPARE(category(olderSame), QString("colorNewDxccBand"));
    }

    // Su un log in un file le categorie si contano su un altro filo: la
    // tabella c'e' subito, i colori arrivano poco dopo, anche per un QSO
    // arrivato mentre si contava.
    void categoriesOfAFileLogArriveLater()
    {
        QTemporaryDir dir;
        LogDatabase db;
        QVERIFY(db.open(dir.filePath("log.sqlite")));
        auto add = [&db](const char* call, const char* date, const char* band, const char* dxcc) {
            return db.insertQso({{"CALL", call}, {"QSO_DATE", date}, {"TIME_ON", "1200"}, {"BAND", band},
                                 {"MODE", "FT8"}, {"DXCC", dxcc}, {"GRIDSQUARE", "FN42"}}, "import").id;
        };
        const qint64 first = add("K1ABC", "20260101", "20m", "291");
        const qint64 sameBand = add("W1AW", "20260102", "20m", "291");
        QsoTableModel m(&db);
        QCOMPARE(m.count(), 2);
        // Un QSO nuovo subito, mentre la conta gira.
        const qint64 fresh = add("JA1XX", "20260106", "15m", "339");
        m.insertQso(fresh);
        auto category = [&m](qint64 id) {
            const int r = m.rowForId(id);
            return r < 0 ? QString() : m.data(m.index(r, 0), QsoTableModel::CategoryRole).toString();
        };
        QTRY_COMPARE(category(first), QString("colorNewDxcc"));
        QCOMPARE(category(sameBand), QString("colorNewCall"));
        QTRY_COMPARE(category(fresh), QString("colorNewDxcc"));
        // E dopo, un QSO nuovo si conta da solo, subito.
        const qint64 next = add("JA1YY", "20260107", "15m", "339");
        m.insertQso(next);
        QCOMPARE(category(next), QString("colorNewCall"));
    }

    // Un clic sull'intestazione: si ordina per quella colonna, e di nuovo al contrario.
    void sortsByColumn()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        fill(db);
        QsoTableModel m(&db);
        QCOMPARE(calls(m), QStringList({"W6XYZ", "EA8ABC", "JA1XX", "K1ABC"}));   // di serie: dal piu' recente
        m.sortBy("call");
        QVERIFY(m.sortAscending());
        QCOMPARE(calls(m), QStringList({"EA8ABC", "JA1XX", "K1ABC", "W6XYZ"}));
        m.sortBy("call");
        QCOMPARE(calls(m), QStringList({"W6XYZ", "K1ABC", "JA1XX", "EA8ABC"}));
        // Le bande nell'ordine delle frequenze, non dell'alfabeto.
        m.sortBy("band");
        QCOMPARE(calls(m), QStringList({"JA1XX", "W6XYZ", "K1ABC", "EA8ABC"}));
        m.sortBy("utc");
        QVERIFY(!m.sortAscending());
        QCOMPARE(calls(m), QStringList({"W6XYZ", "EA8ABC", "JA1XX", "K1ABC"}));
        m.sortBy("utc");
        QCOMPARE(calls(m), QStringList({"K1ABC", "JA1XX", "EA8ABC", "W6XYZ"}));
        // L'ordine resta anche ricaricando (un filtro, un QSO nuovo).
        m.setFilterText("A");
        QCOMPARE(calls(m).first(), QString("K1ABC"));
    }

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

    // Un log grande si legge a pagine: in memoria gli id, i valori quando si
    // guardano. Dodicimila QSO sono sessanta pagine, piu' di quelle tenute:
    // scorrendo tutta la tabella le prime escono e si rileggono.
    void aBigLogIsReadInPages()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        QByteArray adif;
        const QDateTime start(QDate(2020, 1, 1), QTime(0, 0), QTimeZone::UTC);
        for (int i = 0; i < 12000; ++i) {
            const QDateTime on = start.addSecs(i * 60LL);
            const QByteArray call = "DL" + QByteArray::number(i).rightJustified(5, '0');
            adif += "<CALL:" + QByteArray::number(call.size()) + ">" + call + "<QSO_DATE:8>"
                    + on.toString("yyyyMMdd").toLatin1() + "<TIME_ON:4>" + on.toString("HHmm").toLatin1()
                    + "<BAND:3>20m<MODE:3>FT8<EOR>";
        }
        QCOMPARE(db.importAdif(adif).inserted, 12000);

        QsoTableModel m(&db);
        QCOMPARE(m.count(), 12000);
        // Dal piu' recente: l'ultimo nominativo in cima, il primo in fondo.
        QCOMPARE(m.callAt(0), QString("DL11999"));
        QCOMPARE(m.callAt(11999), QString("DL00000"));
        // Tutta la tabella, in ordine, pagina dopo pagina.
        for (int r = 0; r < m.count(); r += 37)
            QCOMPARE(m.callAt(r), QStringLiteral("DL%1").arg(11999 - r, 5, 10, QLatin1Char('0')));
        // E di nuovo in cima, dopo che le prime pagine sono uscite.
        QCOMPARE(m.data(m.index(1, 1)).toString(), QString("DL11998"));

        // L'ordine lo fa SQLite, anche fra una pagina e l'altra.
        m.sortBy("call");
        QCOMPARE(m.callAt(0), QString("DL00000"));
        QCOMPARE(m.callAt(5000), QString("DL05000"));
        m.setSort("utc", false);

        // Un QSO nuovo va in cima ed e' l'unico evidenziato; la riga di prima si
        // trova ancora.
        const InsertResult r = db.insertQso({{"CALL", "ZZ9ZZ"}, {"QSO_DATE", "20260101"}, {"TIME_ON", "1200"},
                                             {"BAND", "20m"}, {"MODE", "FT8"}}, "manual");
        QCOMPARE(r.status, InsertResult::Status::Inserted);
        m.insertQso(r.id);
        QCOMPARE(m.count(), 12001);
        QCOMPARE(m.callAt(0), QString("ZZ9ZZ"));
        QVERIFY(m.data(m.index(0, 0), QsoTableModel::IsNewRole).toBool());
        QVERIFY(!m.data(m.index(1, 0), QsoTableModel::IsNewRole).toBool());
        QCOMPARE(m.callAt(1), QString("DL11999"));
        QCOMPARE(m.rowForId(r.id), 0);
        // Uno scritto a mano con l'ora di prima va al suo posto.
        const InsertResult old = db.insertQso({{"CALL", "OLD1"}, {"QSO_DATE", "20200101"}, {"TIME_ON", "0030"},
                                               {"BAND", "20m"}, {"MODE", "CW"}}, "manual");
        m.insertQso(old.id);
        QCOMPARE(m.callAt(m.rowForId(old.id)), QString("OLD1"));
        QCOMPARE(m.callAt(m.rowForId(old.id) + 1), QString("DL00030"));
        QVERIFY(!m.data(m.index(0, 0), QsoTableModel::IsNewRole).toBool());
        // I filtri anche.
        m.setFilterText("DL0001");
        QCOMPARE(m.count(), 10);
        QCOMPARE(m.callAt(0), QString("DL00019"));
    }
};

QTEST_GUILESS_MAIN(TestQsoModel)
#include "tst_qsomodel.moc"
