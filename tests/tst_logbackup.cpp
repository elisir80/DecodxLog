// Rimettere a posto un backup: guardare dentro una copia senza toccarla, e
// rimetterla al posto del log senza perdere il log di adesso.
#include "core/LogBackup.h"
#include "core/LogDatabase.h"
#include "core/CloudSettings.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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

    void aFailedBackupLeavesNoPartialArtifactOrOldCopyUntouched()
    {
        const QString failedPath = m_dir.filePath(QStringLiteral("missing/out.sqlite"));
        LogDatabase db;
        QVERIFY(db.open(m_log));
        QVERIFY(!db.backupTo(failedPath));
        db.close();
        QVERIFY(!QFile::exists(failedPath + QStringLiteral(".partial")));
        const logbackup::Snapshot old = logbackup::inspect(m_backup);
        QVERIFY2(old.readable, qPrintable(old.problem));
        QCOMPARE(old.qsos, 3);
    }

    void aStationBundleIsAtomicAndRejectsUnsafeResourcePaths()
    {
        const QString resource = m_dir.filePath(QStringLiteral("MASTER.SCP"));
        QFile source(resource);
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("W1AW\n");
        source.close();

        const QString bundle = m_dir.filePath(QStringLiteral("station.bundle"));
        QSettings settings(m_dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        settings.setValue(QStringLiteral("ui/theme"), QStringLiteral("Stellar Light"));
        settings.setValue(QStringLiteral("rig/port"), 4532);
        const auto portable = cloudsettings::collect(settings);
        QVERIFY(portable.contains(QStringLiteral("ui/theme")));
        QVERIFY(!portable.contains(QStringLiteral("rig/port")));
        const auto created = logbackup::createBundle(
            m_backup, bundle, {{resource, QStringLiteral("contest/MASTER.SCP")}}, portable);
        QVERIFY2(created.ok, qPrintable(created.error));
        QVERIFY(logbackup::isBundle(bundle));
        QVERIFY(QFileInfo::exists(QDir(bundle).filePath(QStringLiteral("log.sqlite"))));
        QVERIFY(QFileInfo::exists(QDir(bundle).filePath(QStringLiteral("contest/MASTER.SCP"))));
        QVERIFY(created.files.contains(QStringLiteral("log.sqlite")));
        QVERIFY(created.files.contains(QStringLiteral("contest/MASTER.SCP")));
        QVERIFY(created.files.contains(QStringLiteral("settings.json")));
        QFile manifest(QDir(bundle).filePath(QStringLiteral("manifest.json")));
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        QVERIFY(QJsonDocument::fromJson(manifest.readAll()).object().value(QStringLiteral("files"))
                    .toArray().contains(QStringLiteral("settings.json")));
        QVERIFY(!QFileInfo::exists(bundle + QStringLiteral(".partial")));

        const QString rejected = m_dir.filePath(QStringLiteral("rejected.bundle"));
        const auto bad = logbackup::createBundle(
            m_backup, rejected, {{resource, QStringLiteral("../outside.txt")}});
        QVERIFY(!bad.ok);
        QVERIFY(!logbackup::isBundle(rejected));
        QVERIFY(!QFileInfo::exists(rejected + QStringLiteral(".partial")));
    }

    void restoringAStationBundleRestoresDatabaseAndResources()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString sourcePath = dir.filePath(QStringLiteral("source.sqlite"));
        const QString targetPath = dir.filePath(QStringLiteral("target.sqlite"));
        const QString backupDir = dir.filePath(QStringLiteral("backups"));
        const QString resourceRoot = dir.filePath(QStringLiteral("appdata"));
        QVERIFY(QDir().mkpath(backupDir));
        QVERIFY(QDir().mkpath(resourceRoot));
        LogDatabase source;
        QVERIFY(source.open(sourcePath));
        StationProfile profile;
        profile.name = QStringLiteral("Portable station");
        profile.stationCallsign = QStringLiteral("IU8LMC");
        profile.myGridsquare = QStringLiteral("JN70");
        const qint64 profileId = source.saveStationProfile(profile);
        QVERIFY(profileId > 0);
        const auto inserted = source.insertQso(
            {{"CALL", "W1AW"}, {"QSO_DATE", "20260101"}, {"TIME_ON", "1200"},
             {"BAND", "20m"}, {"MODE", "CW"}, {"LOTW_QSL_SENT", "Y"},
             {"NAME", "Hiram"}},
            "import", {}, false, profileId);
        QCOMPARE(inserted.status, InsertResult::Status::Inserted);
        const QByteArray receivedCard("received-qsl-image");
        const qint64 attachmentId = source.addQslAttachment(
            receivedCard, QStringLiteral("received.jpg"), QStringLiteral("image/jpeg"), {inserted.id});
        QVERIFY(attachmentId > 0);
        AdifRecord edited = *source.record(inserted.id);
        edited.set(QStringLiteral("NAME"), QStringLiteral("Hiram W1AW"));
        QCOMPARE(source.updateQso(inserted.id, edited).status, InsertResult::Status::Inserted);
        const QString sqlite = QDir(backupDir).filePath(QStringLiteral("copy.sqlite"));
        QVERIFY(source.backupTo(sqlite));
        source.close();
        const QString sourceResource = dir.filePath(QStringLiteral("source.wav"));
        QFile wav(sourceResource);
        QVERIFY(wav.open(QIODevice::WriteOnly));
        wav.write("voice-keyer");
        wav.close();
        const QString bundle = dir.filePath(QStringLiteral("station.bundle"));
        QVERIFY(logbackup::createBundle(sqlite, bundle,
                                        {{sourceResource, QStringLiteral("dvk/F1.wav")}}).ok);
        LogDatabase target;
        QVERIFY(target.open(targetPath));
        addQso(target, "JA1XYZ", "20260102");
        target.close();
        const auto restored = logbackup::restoreBundle(bundle, targetPath, backupDir, resourceRoot);
        QVERIFY2(restored.ok, qPrintable(restored.error));
        QCOMPARE(countQsos(targetPath), 1);
        LogDatabase restoredDb;
        QVERIFY(restoredDb.open(targetPath));
        const auto restoredProfiles = restoredDb.stationProfiles();
        QCOMPARE(restoredProfiles.size(), 1);
        QCOMPARE(restoredProfiles.first().stationCallsign, QStringLiteral("IU8LMC"));
        const auto restoredIds = restoredDb.latestIds(QStringLiteral("W1AW"), 1);
        QCOMPARE(restoredIds.size(), 1);
        QCOMPARE(restoredDb.record(restoredIds.first())->value(QStringLiteral("NAME")), QStringLiteral("Hiram W1AW"));
        QCOMPARE(restoredDb.qslStatus(restoredIds.first()).size(), 1);
        const auto attachments = restoredDb.qslAttachments(restoredIds.first());
        QCOMPARE(attachments.size(), 1);
        QCOMPARE(restoredDb.qslAttachmentData(attachments.first().id), QByteArray("received-qsl-image"));
        QCOMPARE(restoredDb.history(restoredIds.first()).size(), 1);
        QCOMPARE(restoredDb.meta(restoredIds.first())->stationProfileId, profileId);
        restoredDb.close();
        QFile restoredResource(QDir(resourceRoot).filePath(QStringLiteral("dvk/F1.wav")));
        QVERIFY(restoredResource.open(QIODevice::ReadOnly));
        QCOMPARE(restoredResource.readAll(), QByteArray("voice-keyer"));
        QVERIFY(QDir(resourceRoot).entryList({QStringLiteral(".decodxlog-bundle-restore-*")},
                                             QDir::Dirs | QDir::Hidden).isEmpty());
    }

    void anIncompleteStationBundleDoesNotTouchTheTargetLog()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString sourcePath = dir.filePath(QStringLiteral("source.sqlite"));
        const QString targetPath = dir.filePath(QStringLiteral("target.sqlite"));
        const QString backupDir = dir.filePath(QStringLiteral("backups"));
        const QString resourceRoot = dir.filePath(QStringLiteral("appdata"));
        QVERIFY(QDir().mkpath(backupDir));
        QVERIFY(QDir().mkpath(resourceRoot));

        LogDatabase source;
        QVERIFY(source.open(sourcePath));
        addQso(source, "W1AW", "20260101");
        const QString sqlite = QDir(backupDir).filePath(QStringLiteral("copy.sqlite"));
        QVERIFY(source.backupTo(sqlite));
        source.close();
        const QString sourceResource = dir.filePath(QStringLiteral("source.wav"));
        QFile wav(sourceResource);
        QVERIFY(wav.open(QIODevice::WriteOnly));
        wav.write("voice-keyer");
        wav.close();
        const QString bundle = dir.filePath(QStringLiteral("station.bundle"));
        QVERIFY(logbackup::createBundle(sqlite, bundle,
                                        {{sourceResource, QStringLiteral("dvk/F1.wav")}}).ok);
        QVERIFY(QFile::remove(QDir(bundle).filePath(QStringLiteral("dvk/F1.wav"))));

        LogDatabase target;
        QVERIFY(target.open(targetPath));
        addQso(target, "JA1XYZ", "20260102");
        target.close();
        const auto restored = logbackup::restoreBundle(bundle, targetPath, backupDir, resourceRoot);
        QVERIFY(!restored.ok);
        QVERIFY(!restored.error.isEmpty());
        QCOMPARE(countQsos(targetPath), 1);
        QVERIFY(QDir(resourceRoot).entryList({QStringLiteral(".decodxlog-bundle-restore-*")},
                                             QDir::Dirs | QDir::Hidden).isEmpty());
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

    void aRestoreKeepsQsoIdentityRevisionQslAndStationProfile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString sourcePath = dir.filePath(QStringLiteral("source.sqlite"));
        const QString targetPath = dir.filePath(QStringLiteral("target.sqlite"));
        const QString backupDir = dir.filePath(QStringLiteral("backups"));
        QVERIFY(QDir().mkpath(backupDir));
        const QString backupPath = QDir(backupDir).filePath(QStringLiteral("decolog-complete.sqlite"));

        LogDatabase source;
        QVERIFY(source.open(sourcePath));
        StationProfile profile;
        profile.name = QStringLiteral("Casa");
        profile.stationCallsign = QStringLiteral("IU8LMC");
        profile.uuid = QStringLiteral("profile-fixed-uuid");
        const qint64 profileId = source.saveStationProfile(profile);
        QVERIFY(profileId > 0);
        const AdifRecord record{{"CALL", "W1AW"}, {"QSO_DATE", "20260101"}, {"TIME_ON", "1200"},
                                {"BAND", "20m"}, {"MODE", "CW"}, {"COMMENT", "before edit"}};
        const InsertResult inserted = source.insertQso(record, QStringLiteral("test"), {}, false, profileId);
        QCOMPARE(inserted.status, InsertResult::Status::Inserted);
        QVERIFY(source.setQslState(inserted.id, QslState{QStringLiteral("lotw"), "Y", {}, "Y", "20260102", "remote-1"}));
        AdifRecord edited = record;
        edited.set(QStringLiteral("COMMENT"), QStringLiteral("after edit"));
        QCOMPARE(source.updateQso(inserted.id, edited).status, InsertResult::Status::Inserted);
        const auto beforeMeta = source.meta(inserted.id);
        QVERIFY(beforeMeta.has_value());
        QVERIFY(beforeMeta->revision >= 2);
        QVERIFY(!source.history(inserted.id).isEmpty());
        QVERIFY(source.backupTo(backupPath));
        source.close();

        LogDatabase target;
        QVERIFY(target.open(targetPath));
        addQso(target, "DL1ABC", "20260202");
        target.close();

        const logbackup::RestoreResult restored = logbackup::restore(backupPath, targetPath, backupDir);
        QVERIFY2(restored.ok, qPrintable(restored.error));

        LogDatabase recovered;
        QVERIFY(recovered.open(targetPath));
        QCOMPARE(recovered.qsoCount(), 1);
        const auto recoveredMeta = recovered.meta(inserted.id);
        QVERIFY(recoveredMeta.has_value());
        QCOMPARE(recoveredMeta->uuid, beforeMeta->uuid);
        QCOMPARE(recoveredMeta->revision, beforeMeta->revision);
        QCOMPARE(recovered.stationProfiles(false).size(), 1);
        QCOMPARE(recovered.stationProfiles(false).first().uuid, QStringLiteral("profile-fixed-uuid"));
        QCOMPARE(recovered.qslStatus(inserted.id).size(), 1);
        QCOMPARE(recovered.qslStatus(inserted.id).first().remoteId, QStringLiteral("remote-1"));
        QCOMPARE(recovered.history(inserted.id).size(), 1);
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
