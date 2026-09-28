// Il log ADIF di Decodium: dove sta, quando e' stato registrato un QSO, la
// coda del file letta solo quanto serve, e i QSO che il log conosce gia' (anche
// corretti o cancellati dopo) che non devono tornare.
#include "core/DecodiumLog.h"
#include "core/LogDatabase.h"

#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace decolog::core;

namespace {

QByteArray record(const QString& call, const QDateTime& on, int seconds)
{
    const QDateTime off = on.addSecs(seconds);
    auto field = [](const char* name, const QString& value) {
        return QStringLiteral("<%1:%2>%3 ").arg(QLatin1String(name)).arg(value.size()).arg(value);
    };
    return (field("CALL", call) + field("GRIDSQUARE", QStringLiteral("JO21")) + field("MODE", QStringLiteral("MFSK"))
            + field("SUBMODE", QStringLiteral("FT2")) + field("RST_SENT", QStringLiteral("-10"))
            + field("RST_RCVD", QStringLiteral("-05")) + field("QSO_DATE", on.toString(QStringLiteral("yyyyMMdd")))
            + field("TIME_ON", on.toString(QStringLiteral("HHmmss")))
            + field("QSO_DATE_OFF", off.toString(QStringLiteral("yyyyMMdd")))
            + field("TIME_OFF", off.toString(QStringLiteral("HHmmss"))) + field("BAND", QStringLiteral("20M"))
            + field("FREQ", QStringLiteral("14.084000")) + field("STATION_CALLSIGN", QStringLiteral("IU8LMC"))
            + QStringLiteral("<EOR>\n"))
        .toUtf8();
}

const QDateTime kStart(QDate(2026, 9, 1), QTime(0, 0), QTimeZone::UTC);

// Tremila QSO, uno ogni dieci minuti, come li scrive Decodium.
QByteArray bigLog()
{
    QByteArray out = "Decodium3 ADIF Log\n<EOH>\n";
    for (int i = 0; i < 3000; ++i)
        out += record(QStringLiteral("K%1AA").arg(i), kStart.addSecs(i * 600), 30);
    return out;
}

AdifRecord qso(const char* call, const char* date, const char* time)
{
    return AdifRecord{{"CALL", call}, {"BAND", "20m"}, {"MODE", "MFSK"}, {"SUBMODE", "FT2"},
                      {"QSO_DATE", date}, {"TIME_ON", time}};
}

} // namespace

class TestDecodiumLog : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void loggedAtIsTheEnd()
    {
        AdifRecord r{{"QSO_DATE", "20260928"}, {"TIME_ON", "134744"}, {"QSO_DATE_OFF", "20260928"}, {"TIME_OFF", "134814"}};
        QCOMPARE(decodiumlog::loggedAt(r), QDateTime(QDate(2026, 9, 28), QTime(13, 48, 14), QTimeZone::UTC));
        // Senza la data di fine, dopo mezzanotte e' il giorno dopo.
        AdifRecord night{{"QSO_DATE", "20260928"}, {"TIME_ON", "2359"}, {"TIME_OFF", "0001"}};
        QCOMPARE(decodiumlog::loggedAt(night), QDateTime(QDate(2026, 9, 29), QTime(0, 1), QTimeZone::UTC));
        // Senza fine, l'inizio.
        AdifRecord start{{"QSO_DATE", "20260928"}, {"TIME_ON", "1200"}};
        QCOMPARE(decodiumlog::loggedAt(start), QDateTime(QDate(2026, 9, 28), QTime(12, 0), QTimeZone::UTC));
    }

    void onlyTheTailIsRead()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("decodium_log.adi"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(bigLog());
        // Decodium sta scrivendo l'ultimo: a meta', resta per la volta dopo.
        f.write("<CALL:5>ZZ9ZZ <QSO_DATE:8>20260930 <TIME_");
        f.close();

        // Le ultime due ore: dodici QSO, con un pezzo piccolo che deve crescere.
        const QDateTime last = kStart.addSecs(2999 * 600 + 30);
        decodiumlog::Tail tail = decodiumlog::recent(path, last.addSecs(-7200), last, 4096);
        QVERIFY2(tail.ok, qPrintable(tail.error));
        QCOMPARE(tail.records.size(), 12);
        QCOMPARE(tail.records.last().value("CALL"), QString("K2999AA"));
        QCOMPARE(tail.records.first().value("CALL"), QString("K2988AA"));

        // Fino a un'ora fa: gli ultimi sei no.
        tail = decodiumlog::recent(path, last.addSecs(-7200), last.addSecs(-3600), 4096);
        QCOMPARE(tail.records.size(), 6);

        // Tre settimane indietro: il pezzo cresce fino a leggere quasi tutto.
        tail = decodiumlog::recent(path, kStart.addDays(1), last, 4096);
        QCOMPARE(tail.records.size(), 3000 - 144);

        // Senza "da quando", tutto il file (intestazione compresa).
        tail = decodiumlog::recent(path, {}, last, 4096);
        QCOMPARE(tail.records.size(), 3000);
        QCOMPARE(tail.records.first().value("CALL"), QString("K0AA"));
    }

    void missingFile()
    {
        const decodiumlog::Tail tail = decodiumlog::recent(QStringLiteral("Z:/nowhere/decodium_log.adi"), {}, {});
        QVERIFY(!tail.ok);
        QVERIFY(!tail.error.isEmpty());
    }

    void theLogDecodiumUses()
    {
        // Il file attivo scritto nelle impostazioni di Decodium viene prima.
        QTemporaryDir dir;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
        const QString active = dir.filePath(QStringLiteral("contest.adi"));
        QFile f(active);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        {
            QSettings ini(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"),
                          QStringLiteral("Decodium3"));
            ini.setValue(QStringLiteral("Logbooks/ActivePath"), active);
        }
        const QStringList found = decodiumlog::candidates();
        QVERIFY(!found.isEmpty());
        QCOMPARE(QFileInfo(found.first()).canonicalFilePath(), QFileInfo(active).canonicalFilePath());

        // Un file che non esiste non si propone.
        {
            QSettings ini(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"),
                          QStringLiteral("Decodium3"));
            ini.setValue(QStringLiteral("Logbooks/ActivePath"), dir.filePath(QStringLiteral("gone.adi")));
        }
        for (const QString& p : decodiumlog::candidates())
            QVERIFY(!p.endsWith(QStringLiteral("gone.adi")));
    }

    void theLogKnowsItsQsos()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        const qint64 id = db.insertQso(qso("ON4CAD", "20260928", "140322"), "udp_decodium").id;
        QVERIFY(id > 0);

        // Lo stesso QSO, anche con qualche secondo di differenza.
        QVERIFY(db.knowsQso(qso("ON4CAD", "20260928", "140322")));
        QVERIFY(db.knowsQso(qso("ON4CAD", "20260928", "140400")));
        // Un altro nominativo alla stessa ora, o lo stesso un'ora dopo: no.
        QVERIFY(!db.knowsQso(qso("RW4LN", "20260928", "140322")));
        QVERIFY(!db.knowsQso(qso("ON4CAD", "20260928", "150322")));
        // La banda dalla frequenza, se manca.
        AdifRecord byFreq = qso("ON4CAD", "20260928", "140322");
        byFreq.set("BAND", "");
        byFreq.set("FREQ", "14.084");
        QVERIFY(db.knowsQso(byFreq));

        // Corretto qui (ON4CAD era ON4CAB): quello sbagliato non deve tornare.
        AdifRecord fixed = *db.record(id);
        fixed.set("CALL", "ON4CAB");
        QCOMPARE(db.updateQso(id, fixed).status, InsertResult::Status::Inserted);
        QVERIFY(db.knowsQso(qso("ON4CAD", "20260928", "140322")));
        QVERIFY(db.knowsQso(qso("ON4CAB", "20260928", "140322")));

        // Cancellato qui: nemmeno.
        QVERIFY(db.softDeleteQso(id));
        QVERIFY(db.knowsQso(qso("ON4CAB", "20260928", "140322")));
    }
};

QTEST_GUILESS_MAIN(TestDecodiumLog)
#include "tst_decodiumlog.moc"
