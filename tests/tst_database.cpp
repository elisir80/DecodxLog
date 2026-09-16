// Database: schema, inserimento, duplicati e import/export senza perdite.
#include "core/LogDatabase.h"

#include <QSqlQuery>
#include <QTest>

using namespace decolog::core;

namespace {

// Confronto semantico: i numeri come numeri (14.074 == 14.074000), le ore a
// quattro cifre come le stesse ore con i secondi a zero.
QString canonical(const QString& name, const QString& value)
{
    static const QStringList numeric{"FREQ", "FREQ_RX", "TX_PWR", "DXCC", "CQZ", "ITUZ"};
    if (numeric.contains(name)) {
        bool ok = false;
        const double v = value.toDouble(&ok);
        if (ok)
            return QString::number(v, 'f', 6);
    }
    if ((name == "TIME_ON" || name == "TIME_OFF") && value.size() == 4)
        return value + "00";
    if (name == "BAND" || name == "BAND_RX")
        return value.toLower();
    if (name == "CALL" || name == "MODE" || name == "SUBMODE")
        return value.toUpper();
    return value;
}

QMap<QString, QString> fieldMap(const AdifRecord& r)
{
    QMap<QString, QString> m;
    for (const auto& f : r.fields())
        m.insert(f.name, canonical(f.name, f.value));
    return m;
}

} // namespace

class TestDatabase : public QObject {
    Q_OBJECT

private slots:
    void schemaIsApplied()
    {
        LogDatabase db;
        QVERIFY2(db.open(":memory:"), qPrintable(db.lastError()));
        QCOMPARE(db.schemaVersion(), 1);
        QCOMPARE(db.qsoCount(), 0);
    }

    void insertAndReadBack()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        AdifRecord r{{"CALL", "dl1ab"}, {"QSO_DATE", "20260917"}, {"TIME_ON", "101500"},
                     {"FREQ", "14.084"}, {"MODE", "FT2"}, {"RST_SENT", "-10"},
                     {"STATION_CALLSIGN", "IU8LMC"}, {"APP_DECODIUM_DT", "0.2"}};
        const InsertResult res = db.insertQso(r, "udp_decodium", "Decodium FT2 1.0.637");
        QCOMPARE(res.status, InsertResult::Status::Inserted);

        QSqlQuery q(db.connection());
        QVERIFY(q.exec("SELECT call, band, mode, submode, freq, qso_datetime_on, uuid, revision, dirty, adif_extra FROM qso"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QString("DL1AB"));
        QCOMPARE(q.value(1).toString(), QString("20m"));     // ricavata dalla frequenza
        QCOMPARE(q.value(2).toString(), QString("MFSK"));
        QCOMPARE(q.value(3).toString(), QString("FT2"));
        QCOMPARE(q.value(4).toDouble(), 14.084);
        QCOMPARE(q.value(5).toString(), QString("2026-09-17T10:15:00Z"));
        QCOMPARE(q.value(6).toString().size(), 36);
        QCOMPARE(q.value(7).toInt(), 1);
        QCOMPARE(q.value(8).toInt(), 1);
        QVERIFY(q.value(9).toString().contains("APP_DECODIUM_DT"));

        const auto back = db.record(res.id);
        QVERIFY(back);
        QCOMPARE(back->value("STATION_CALLSIGN"), QString("IU8LMC"));
        QCOMPARE(back->value("SUBMODE"), QString("FT2"));
    }

    void rejectsIncompleteRecords()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        const InsertResult res = db.insertQso({{"CALL", "K1AB"}, {"MODE", "CW"}}, "import");
        QCOMPARE(res.status, InsertResult::Status::Invalid);
        QVERIFY(res.message.contains("QSO_DATE"));
        QCOMPARE(db.qsoCount(), 0);
    }

    void duplicateWindow()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        AdifRecord r{{"CALL", "K1AB"}, {"QSO_DATE", "20260917"}, {"TIME_ON", "101500"},
                     {"BAND", "40m"}, {"MODE", "MFSK"}, {"SUBMODE", "FT2"}};
        QCOMPARE(db.insertQso(r, "udp_decodium").status, InsertResult::Status::Inserted);

        r.set("TIME_ON", "101640");  // +100 s: stesso QSO
        QCOMPARE(db.insertQso(r, "udp_decodium").status, InsertResult::Status::Duplicate);

        r.set("TIME_ON", "102000");  // +5 min: un altro QSO da Decodium...
        QCOMPARE(db.insertQso(r, "udp_decodium").status, InsertResult::Status::Inserted);

        r.set("TIME_ON", "102800");  // ...ma a mano la finestra e' ±10 min
        QCOMPARE(db.insertQso(r, "manual", {}, true).status, InsertResult::Status::Duplicate);

        r.set("SUBMODE", "FT4");     // altro sottomodo: non e' un doppione
        QCOMPARE(db.insertQso(r, "udp_decodium").status, InsertResult::Status::Inserted);
        QCOMPARE(db.qsoCount(), 3);
    }

    void importExportIsLossless()
    {
        const QByteArray original =
            "Exported by some logger\n<ADIF_VER:5>3.1.4<PROGRAMID:4>Test<EOH>\n"
            "<CALL:5>JA1XY<QSO_DATE:8>20240301<TIME_ON:4>0712<QSO_DATE_OFF:8>20240301<TIME_OFF:6>071400"
            "<BAND:3>20M<FREQ:6>14.074<MODE:3>FT8<RST_SENT:3>-08<RST_RCVD:3>-15<GRIDSQUARE:6>PM95vq"
            "<NAME:5>Taro <COUNTRY:5>Japan<DXCC:3>339<CQZ:2>25<ITUZ:2>45<CONT:2>AS<TX_PWR:3>100"
            "<LOTW_QSL_SENT:1>Y<QSL_VIA:6>bureau<MY_SOTA_REF:9>I/CM-123 <USERDEF1:3>abc<EOR>\n"
            "<CALL:4>W1AW<QSO_DATE:8>20240302<TIME_ON:6>180000<FREQ:5>7.030<MODE:2>CW"
            "<RST_SENT:3>599<RST_RCVD:3>579<DXCC:3>bad<COMMENT:13>nice ;) <qso><NOTES:18>line one\r\nline two<EOR>\n";

        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        const ImportResult ir = db.importAdif(original);
        QCOMPARE(ir.inserted, 2);
        QCOMPARE(ir.invalid, 0);

        const AdifDocument before = adif::parse(original);
        const AdifDocument after = adif::parse(db.exportAdif("test"));
        QCOMPARE(after.records.size(), before.records.size());

        for (int i = 0; i < before.records.size(); ++i) {
            QMap<QString, QString> expected = fieldMap(before.records.at(i));
            QMap<QString, QString> actual = fieldMap(after.records.at(i));
            // La banda mancante viene ricavata: e' un'aggiunta, non una perdita.
            if (!expected.contains("BAND"))
                actual.remove("BAND");
            QCOMPARE(actual, expected);
        }
    }

    void workedBefore()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        db.insertQso({{"CALL", "K1AB"}, {"QSO_DATE", "20260101"}, {"TIME_ON", "1000"}, {"BAND", "20m"}, {"MODE", "FT8"}, {"NAME", "Bob"}}, "import");
        db.insertQso({{"CALL", "K1AB"}, {"QSO_DATE", "20260201"}, {"TIME_ON", "1000"}, {"BAND", "40m"}, {"MODE", "MFSK"}, {"SUBMODE", "FT2"}}, "import");
        const WorkedBefore wb = db.workedBefore("k1ab");
        QCOMPARE(wb.count, 2);
        QCOMPARE(wb.bands, QStringList({"40m", "20m"}));
        QCOMPARE(wb.lastMode, QString("FT2"));
        QCOMPARE(wb.name, QString("Bob"));
        QCOMPARE(db.workedBefore("N0NE").count, 0);
    }
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
