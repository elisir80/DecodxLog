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
};

QTEST_GUILESS_MAIN(TestQsoModel)
#include "tst_qsomodel.moc"
