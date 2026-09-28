// Rimettere a posto un backup: guardare dentro una copia senza toccarla, e
// rimetterla al posto del log senza perdere il log di adesso.
#include "core/LogBackup.h"
#include "core/LogDatabase.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace decolog::core;

namespace {

void addQso(LogDatabase& db, const char* call, const char* date)
{
    const AdifRecord r{{"CALL", call}, {"QSO_DATE", date}, {"TIME_ON", "1200"}, {"BAND", "20m"}, {"MODE", "CW"}};
    QCOMPARE(db.insertQso(r, "import").status, InsertResult::Status::Inserted);
}

int countQsos(const QString& path)
{
    LogDatabase db;
    if (!db.open(path))
        return -1;
    const int n = db.qsoCount();
    db.close();
    return n;
}

} // namespace

class TestLogBackup : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_log;
    QString m_backups;
    QString m_backup;

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_log = m_dir.filePath(QStringLiteral("log.sqlite"));
        m_backups = m_dir.filePath(QStringLiteral("backup"));
        QVERIFY(QDir().mkpath(m_backups));
        m_backup = QDir(m_backups).filePath(QStringLiteral("decolog-2026-09-27T0200.sqlite"));

        // Tre QSO, la copia notturna, poi altri due dopo la copia.
        LogDatabase db;
        QVERIFY(db.open(m_log));
        addQso(db, "W1AW", "20260101");
        addQso(db, "DL1ABC", "20260301");
        addQso(db, "JA1XYZ", "20260927");
        QVERIFY(db.backupTo(m_backup));
        addQso(db, "VK2AB", "20260928");
        addQso(db, "ZL1AA", "20260928");
        db.close();
        QCOMPARE(countQsos(m_log), 5);
    }

    void aCopyIsReadWithoutTouchingIt()
    {
        const QByteArray before = [this] {
            QFile f(m_backup);
            return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        }();
        const logbackup::Snapshot s = logbackup::inspect(m_backup);
        QVERIFY2(s.readable, qPrintable(s.problem));
        QCOMPARE(s.qsos, 3);
        QCOMPARE(s.firstQso.date(), QDate(2026, 1, 1));
        QCOMPARE(s.lastQso.date(), QDate(2026, 9, 27));
        QVERIFY(s.schema > 0);
        QFile f(m_backup);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), before);
        // E non lascia file accanto alla copia.
        QVERIFY(!QFile::exists(m_backup + QStringLiteral("-wal")));
        QVERIFY(!QFile::exists(m_backup + QStringLiteral("-shm")));
    }

    void whatIsNotALogIsSaid()
    {
        QVERIFY(!logbackup::inspect(m_dir.filePath(QStringLiteral("none.sqlite"))).readable);

        const QString text = m_dir.filePath(QStringLiteral("text.sqlite"));
        QFile t(text);
        QVERIFY(t.open(QIODevice::WriteOnly));
        t.write("this is not a database, just some text that happens to end in .sqlite\n");
        t.close();
        const logbackup::Snapshot notDb = logbackup::inspect(text);
        QVERIFY(!notDb.readable);
        QVERIFY(!notDb.problem.isEmpty());

        // Un SQLite di un altro programma.
        const QString other = m_dir.filePath(QStringLiteral("other.sqlite"));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("other"));
            db.setDatabaseName(other);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE contacts (id INTEGER)")));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("other"));
        QVERIFY(!logbackup::inspect(other).readable);

        // Una copia con una pagina rovinata.
        const QString damaged = m_dir.filePath(QStringLiteral("damaged.sqlite"));
        QVERIFY(QFile::copy(m_backup, damaged));
        QFile d(damaged);
        QVERIFY(d.open(QIODevice::ReadWrite));
        QVERIFY(d.size() > 3 * 4096);
        d.seek(2 * 4096);
        d.write(QByteArray(4096, char(0x5a)));
        d.close();
        const logbackup::Snapshot bad = logbackup::inspect(damaged);
        QVERIFY(!bad.readable);

        // E una copia rovinata non si rimette: il log resta com'e'.
        const logbackup::RestoreResult r = logbackup::restore(damaged, m_log, m_backups);
        QVERIFY(!r.ok);
        QVERIFY(!r.error.isEmpty());
        QCOMPARE(countQsos(m_log), 5);
    }

    void theBackupsAreListedNewestFirst()
    {
        const QFileInfoList list = logbackup::backupsIn(m_backups);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().fileName(), QStringLiteral("decolog-2026-09-27T0200.sqlite"));
        // La copia di sicurezza di un ripristino non ha il nome delle copie
        // notturne: la pulizia di quelle non la tocca.
        QVERIFY(logbackup::isSafetyCopy(QStringLiteral("decodxlog-before-restore-2026-09-28T101500.sqlite")));
        QVERIFY(!QDir::match(QStringLiteral("decolog-*.sqlite"),
                             QStringLiteral("decodxlog-before-restore-2026-09-28T101500.sqlite")));
    }

    void restoringKeepsTheLogOfNowAside()
    {
        // Un -wal rimasto dal log di prima: applicato alla copia la rovinerebbe.
        {
            QFile wal(m_log + QStringLiteral("-wal"));
            QVERIFY(wal.open(QIODevice::WriteOnly));
            wal.write(QByteArray(1024, char(0x11)));
        }
        const logbackup::RestoreResult r = logbackup::restore(m_backup, m_log, m_backups);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.qsos, 3);
        QVERIFY(!QFile::exists(m_log + QStringLiteral("-wal")));
        QVERIFY(!QFile::exists(m_log + QStringLiteral(".restoring")));

        // Il log ha i tre QSO della copia, e la copia e' ancora li'.
        QCOMPARE(countQsos(m_log), 3);
        QVERIFY(QFile::exists(m_backup));
        // Il log di prima, con tutti e cinque i QSO, sta nella cartella dei backup.
        QVERIFY(QFile::exists(r.safetyCopy));
        QVERIFY(logbackup::isSafetyCopy(QFileInfo(r.safetyCopy).fileName()));
        QCOMPARE(logbackup::inspect(r.safetyCopy).qsos, 5);
        QCOMPARE(logbackup::backupsIn(m_backups).size(), 2);

        // E per tornare indietro si rimette quella.
        const logbackup::RestoreResult back = logbackup::restore(r.safetyCopy, m_log, m_backups);
        QVERIFY2(back.ok, qPrintable(back.error));
        QCOMPARE(countQsos(m_log), 5);
        // Due ripristini nello stesso secondo: due copie di sicurezza, nessuna
        // scritta sopra l'altra (ne' sopra quella che si stava rimettendo).
        QVERIFY(back.safetyCopy != r.safetyCopy);
        QCOMPARE(logbackup::inspect(r.safetyCopy).qsos, 5);
        QCOMPARE(logbackup::inspect(back.safetyCopy).qsos, 3);
        QCOMPARE(logbackup::backupsIn(m_backups).size(), 3);
    }

    void theLogItselfIsNotABackup()
    {
        QVERIFY(!logbackup::restore(m_log, m_log, m_backups).ok);
        QCOMPARE(countQsos(m_log), 5);
    }

    void theRestoreWaitsForTheProgramBefore()
    {
        QVERIFY(logbackup::waitForProcessExit(0, 10));
        // Questo processo c'e': aspettarlo non finisce.
        QVERIFY(!logbackup::waitForProcessExit(QCoreApplication::applicationPid(), 200));
        // Un processo gia' uscito si aspetta subito.
        QProcess p;
        p.start(QCoreApplication::applicationFilePath(), {QStringLiteral("-functions")});
        QVERIFY(p.waitForStarted());
        const qint64 pid = p.processId();
        QVERIFY(p.waitForFinished());
        QVERIFY(logbackup::waitForProcessExit(pid, 2000));
    }
};

QTEST_GUILESS_MAIN(TestLogBackup)
#include "tst_logbackup.moc"
