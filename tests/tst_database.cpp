// Database: schema, inserimento, duplicati e import/export senza perdite.
#include "core/LogDatabase.h"
#include "core/Maidenhead.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>

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
        QCOMPARE(db.schemaVersion(), 3);
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

    void qslStatusRoundTrip()
    {
        const QByteArray original =
            "<CALL:4>K1AB<QSO_DATE:8>20260101<TIME_ON:4>1000<BAND:3>20m<MODE:3>FT8"
            "<LOTW_QSL_SENT:1>Y<LOTW_QSLSDATE:8>20260102<LOTW_QSL_RCVD:1>N"
            "<EQSL_QSL_RCVD:1>Y<CLUBLOG_QSO_UPLOAD_STATUS:1>M<QSL_SENT:1>Q<EOR>";
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        QCOMPARE(db.importAdif(original).inserted, 1);

        QSqlQuery q(db.connection());
        QVERIFY(q.exec("SELECT service, sent, sent_date, rcvd FROM qsl_status ORDER BY service"));
        QStringList rows;
        while (q.next())
            rows << q.value(0).toString() + ":" + q.value(1).toString() + q.value(2).toString() + q.value(3).toString();
        QCOMPARE(rows, QStringList({"card:QN", "clublog:MN", "eqsl:NY", "lotw:Y20260102N"}));

        const AdifDocument after = adif::parse(db.exportAdif());
        QCOMPARE(fieldMap(after.records.first()), fieldMap(adif::parse(original).records.first()));
    }

    void editKeepsHistoryAndRestores()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        const auto ins = db.insertQso({{"CALL", "9A3XY"}, {"QSO_DATE", "20260916"}, {"TIME_ON", "145215"},
                                       {"BAND", "20m"}, {"MODE", "FT2"}, {"NAME", "Ivan"}}, "udp_decodium");
        QCOMPARE(ins.status, InsertResult::Status::Inserted);

        AdifRecord edited = *db.record(ins.id);
        edited.set("NAME", "Ivan Horvat");
        edited.set("LOTW_QSL_SENT", "Y");
        QCOMPARE(db.updateQso(ins.id, edited).status, InsertResult::Status::Inserted);

        const auto m = db.meta(ins.id);
        QCOMPARE(m->revision, 2);
        QVERIFY(m->dirty);
        QCOMPARE(db.record(ins.id)->value("NAME"), QString("Ivan Horvat"));
        QCOMPARE(db.qslStatus(ins.id).size(), 1);

        const auto hist = db.history(ins.id);
        QCOMPARE(hist.size(), 1);
        QCOMPARE(hist.first().revision, 1);
        QCOMPARE(hist.first().record.value("NAME"), QString("Ivan"));

        QCOMPARE(db.restoreRevision(ins.id, hist.first().id).status, InsertResult::Status::Inserted);
        QCOMPARE(db.record(ins.id)->value("NAME"), QString("Ivan"));
        QCOMPARE(db.qslStatus(ins.id).size(), 0);
        QCOMPARE(db.meta(ins.id)->revision, 3);
        QCOMPARE(db.history(ins.id).size(), 2);

        // Dentro una transazione aperta da chi chiama (import, correzioni in blocco).
        QVERIFY(db.connection().transaction());
        AdifRecord again = *db.record(ins.id);
        again.set("DXCC", "497");
        QCOMPARE(db.updateQso(ins.id, again).status, InsertResult::Status::Inserted);
        QVERIFY(db.connection().commit());
        QCOMPARE(db.record(ins.id)->value("DXCC"), QString("497"));
        QCOMPARE(db.meta(ins.id)->revision, 4);

        QVERIFY(db.softDeleteQso(ins.id));
        QCOMPARE(db.qsoCount(), 0);
        QVERIFY(db.meta(ins.id)->deleted);
        QCOMPARE(db.history(ins.id).first().reason, QString("delete"));
        QCOMPARE(db.idsWithoutDxcc().size(), 0);
    }

    void stationProfiles()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        StationProfile home;
        home.name = "Home JN71DC";
        home.stationCallsign = "iu8lmc";
        home.myGridsquare = "JN71DC";
        home.isDefault = true;
        const qint64 homeId = db.saveStationProfile(home);
        QVERIFY(homeId > 0);

        StationProfile portable = home;
        portable.name = "Portable /P";
        portable.stationCallsign = "IU8LMC/P";
        const qint64 portableId = db.saveStationProfile(portable);
        QVERIFY(portableId > 0);

        // Il secondo predefinito toglie il titolo al primo.
        auto profiles = db.stationProfiles();
        QCOMPARE(profiles.size(), 2);
        QCOMPARE(db.stationProfile(homeId)->isDefault, false);
        QCOMPARE(db.stationProfile(portableId)->isDefault, true);
        QCOMPARE(db.stationProfile(homeId)->stationCallsign, QString("IU8LMC"));
        QCOMPARE(db.profileForCallsign("iu8lmc/p"), portableId);

        db.insertQso({{"CALL", "K1AB"}, {"QSO_DATE", "20260101"}, {"TIME_ON", "1000"}, {"BAND", "20m"}, {"MODE", "SSB"}},
                     "manual", {}, true, homeId);
        QCOMPARE(db.stationProfile(homeId)->qsoCount, 1);

        QVERIFY(db.deleteStationProfile(homeId));
        QCOMPARE(db.stationProfiles(false).size(), 1);
        QCOMPARE(db.stationProfile(homeId)->qsoCount, 1);
    }

    void awardStatsAndBackup()
    {
        LogDatabase db;
        const QString dir = QDir::temp().filePath("decolog-test-" + QString::number(QCoreApplication::applicationPid()));
        QDir().mkpath(dir);
        QVERIFY(db.open(dir + "/log.sqlite"));
        const auto a = db.insertQso({{"CALL", "9A3XY"}, {"QSO_DATE", "20260916"}, {"TIME_ON", "1452"}, {"BAND", "20m"},
                                     {"MODE", "FT2"}, {"DXCC", "497"}, {"GRIDSQUARE", "JN75WS"}, {"LOTW_QSL_RCVD", "Y"}}, "udp_decodium");
        const auto b = db.insertQso({{"CALL", "9A1AA"}, {"QSO_DATE", "20260916"}, {"TIME_ON", "1500"}, {"BAND", "40m"},
                                     {"MODE", "FT2"}, {"DXCC", "497"}, {"GRIDSQUARE", "JN85"}}, "udp_decodium");
        db.insertQso({{"CALL", "W1AW"}, {"QSO_DATE", "20260916"}, {"TIME_ON", "1510"}, {"BAND", "20m"},
                      {"MODE", "FT8"}, {"DXCC", "291"}, {"GRIDSQUARE", "FN31"}}, "udp_decodium");
        db.insertQso({{"CALL", "EA8XX"}, {"QSO_DATE", "20260916"}, {"TIME_ON", "1520"}, {"BAND", "20m"},
                      {"MODE", "SSB"}, {"SUBMODE", "USB"}}, "manual");

        const Ft2Award award = db.ft2Award();
        QCOMPARE(award.qsos, 2);
        QCOMPARE(award.dxccWorked, 1);
        QCOMPARE(award.dxccConfirmed, 1);
        QCOMPARE(award.gridsWorked, 2);
        QCOMPARE(award.gridsConfirmed, 1);
        QVERIFY(db.isFirstFt2Dxcc(a.id));    // 14:52, prima di b
        QVERIFY(!db.isFirstFt2Dxcc(b.id));   // 15:00, stesso DXCC gia' lavorato

        const auto byBand = db.countByBand();
        QCOMPARE(byBand.size(), 2);
        QCOMPARE(byBand.first().key, QString("40m"));
        QCOMPARE(db.countByMode().first().key, QString("FT2"));
        QStringList modes;
        for (const auto& row : db.countByMode())
            modes << row.key;
        QVERIFY(modes.contains("SSB"));    // SSB/USB si legge SSB
        QVERIFY(!modes.contains("USB"));

        const QString copy = dir + "/backup.sqlite";
        QVERIFY2(db.backupTo(copy), qPrintable(db.lastError()));
        LogDatabase restored;
        QVERIFY(restored.open(copy));
        QCOMPARE(restored.qsoCount(), 4);
        restored.close();
        db.close();
        QDir(dir).removeRecursively();
    }

    void everythingGoesBackInTheCloudQueue()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        const qint64 a = db.insertQso({{"CALL", "K1ABC"}, {"QSO_DATE", "20260101"}, {"TIME_ON", "1200"},
                                       {"BAND", "20m"}, {"MODE", "FT8"}}, "import").id;
        QVERIFY(a > 0);
        // Appena scritto e' da mandare; quando il Cloud l'ha preso, non piu'.
        QVERIFY(db.markSynced(a, 1));
        QCOMPARE(db.dirtyQsos().size(), 0);

        // Svuotato il Cloud, lassu' non c'e' piu' niente: tutto torna in coda.
        QCOMPARE(db.markAllDirty(), 1);
        QCOMPARE(db.dirtyQsos().size(), 1);
    }

    void maidenheadMath()
    {
        const auto jn71 = maidenhead::toLatLon("JN71DC");
        const auto jn75 = maidenhead::toLatLon("jn75ws");
        QVERIFY(jn71 && jn75);
        QVERIFY(qAbs(jn71->lat - 41.1042) < 0.01);
        QVERIFY(qAbs(jn71->lon - 14.2917) < 0.01);
        const double km = maidenhead::distanceKm(*jn71, *jn75);
        QVERIFY2(km > 520 && km < 550, qPrintable(QString::number(km)));
        const double az = maidenhead::azimuthDeg(*jn71, *jn75);
        QVERIFY2(az > 0 && az < 30, qPrintable(QString::number(az)));
        QVERIFY(!maidenhead::toLatLon("ZZ00"));
        QVERIFY(!maidenhead::toLatLon("JN7"));

        // E la strada all'indietro: dalla posizione al locatore, e ritorno.
        QCOMPARE(maidenhead::fromLatLon(41.1042, 14.2917), QStringLiteral("JN71DC"));
        QCOMPARE(maidenhead::fromLatLon(41.1042, 14.2917, 4), QStringLiteral("JN71"));
        QCOMPARE(maidenhead::fromLatLon(51.05, 13.74), QStringLiteral("JO61UB"));
        QCOMPARE(maidenhead::fromLatLon(0.0, 0.0, 4), QStringLiteral("JJ00"));
        const auto back = maidenhead::toLatLon(maidenhead::fromLatLon(-33.87, 151.21));
        QVERIFY(back);
        QVERIFY(qAbs(back->lat + 33.87) < 0.03 && qAbs(back->lon - 151.21) < 0.06);
        // Una posizione che non sta nel mondo non fa un locatore.
        QVERIFY(maidenhead::fromLatLon(95.0, 0.0).isEmpty());
        QVERIFY(maidenhead::fromLatLon(0.0, -200.0).isEmpty());
    }

    void migrationFromV1()
    {
        const QString path = QDir::temp().filePath("decolog-v1-test.sqlite");
        QFile::remove(path);
        {
            QSqlDatabase raw = QSqlDatabase::addDatabase("QSQLITE", "v1");
            raw.setDatabaseName(path);
            QVERIFY(raw.open());
            QSqlQuery q(raw);
            QVERIFY(q.exec("CREATE TABLE schema_version (version INTEGER NOT NULL, applied_at TEXT)"));
            QVERIFY(q.exec("INSERT INTO schema_version (version) VALUES (1)"));
            QVERIFY(q.exec("CREATE TABLE qso (id INTEGER PRIMARY KEY, call TEXT NOT NULL, notes TEXT)"));
            // La v1 aveva gia' gli stati QSL, senza la colonna della via.
            QVERIFY(q.exec("CREATE TABLE qsl_status (qso_id INTEGER NOT NULL, service TEXT NOT NULL, "
                           "sent TEXT NOT NULL DEFAULT 'N', sent_date TEXT, rcvd TEXT NOT NULL DEFAULT 'N', "
                           "rcvd_date TEXT, remote_id TEXT, last_error TEXT, PRIMARY KEY (qso_id, service))"));
            raw.close();
        }
        QSqlDatabase::removeDatabase("v1");
        {
            LogDatabase db;
            QVERIFY2(db.open(path), qPrintable(db.lastError()));
            QCOMPARE(db.schemaVersion(), 3);
            QSqlQuery q(db.connection());
            QVERIFY(q.exec("SELECT COUNT(*) FROM pragma_table_info('qso') WHERE name = 'tags'") && q.next());
            QCOMPARE(q.value(0).toInt(), 1);
            QVERIFY(q.exec("SELECT COUNT(*) FROM pragma_table_info('qsl_status') WHERE name = 'via'") && q.next());
            QCOMPARE(q.value(0).toInt(), 1);
            db.close();
            // Una seconda apertura non rifa la migrazione.
            QVERIFY(db.open(path));
            QCOMPARE(db.schemaVersion(), 3);
        }
        QFile::remove(path);
    }

    void tags()
    {
        QCOMPARE(LogDatabase::splitTags(" pota , Field  Day,POTA,,portable"), QStringList({"pota", "Field Day", "portable"}));

        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        const qint64 a = db.insertQso({{"CALL", "K1AB"}, {"QSO_DATE", "20260101"}, {"TIME_ON", "1000"}, {"BAND", "20m"},
                                       {"MODE", "FT8"}, {"APP_DECOLOG_TAGS", "pota, pota ,portable"}}, "import").id;
        const qint64 b = db.insertQso({{"CALL", "K2AB"}, {"QSO_DATE", "20260102"}, {"TIME_ON", "1000"}, {"BAND", "20m"},
                                       {"MODE", "FT8"}}, "import").id;
        QCOMPARE(db.record(a)->value("APP_DECOLOG_TAGS"), QString("pota,portable"));

        QCOMPARE(db.setTag({a, b}, "Field Day", true), 2);
        QCOMPARE(db.setTag({a, b}, "field day", true), 0);   // gia' presente, maiuscole a parte
        QCOMPARE(db.record(b)->value("APP_DECOLOG_TAGS"), QString("Field Day"));
        QCOMPARE(db.history(b).first().reason, QString("tag"));

        QCOMPARE(db.setTag({a}, "POTA", false), 1);
        QCOMPARE(db.record(a)->value("APP_DECOLOG_TAGS"), QString("portable,Field Day"));

        const auto counts = db.tagCounts();
        QCOMPARE(counts.size(), 2);
        QCOMPARE(counts.at(0).key, QString("Field Day"));
        QCOMPARE(counts.at(0).count, 2);

        // Le etichette tornano nell'export ADIF.
        QVERIFY(db.exportAdif({a}).contains("<app_decolog_tags:18>portable,Field Day"));
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
