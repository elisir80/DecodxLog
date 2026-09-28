#include "core/LogBackup.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#endif

namespace decolog::core::logbackup {

namespace {

struct Tr {
    Q_DECLARE_TR_FUNCTIONS(LogBackup)
};

// Una connessione tutta sua, chiusa e tolta all'uscita: inspect() gira anche in
// un thread a parte, e le connessioni Qt non si passano fra thread.
class Connection {
public:
    Connection(const QString& path, bool readOnly)
        : m_name(QStringLiteral("logbackup-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        db.setDatabaseName(path);
        if (readOnly)
            db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        m_open = db.open();
        if (!m_open)
            m_error = db.lastError().text();
    }
    ~Connection()
    {
        {
            QSqlDatabase db = QSqlDatabase::database(m_name, false);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_name);
    }
    bool open() const { return m_open; }
    QString error() const { return m_error; }
    QSqlDatabase db() const { return QSqlDatabase::database(m_name, false); }

private:
    QString m_name;
    bool m_open{false};
    QString m_error;
};

void removeSidecars(const QString& path)
{
    QFile::remove(path + QStringLiteral("-wal"));
    QFile::remove(path + QStringLiteral("-shm"));
    QFile::remove(path + QStringLiteral("-journal"));
}

} // namespace

Snapshot inspect(const QString& path)
{
    Snapshot s;
    s.path = path;
    const QFileInfo info(path);
    if (!info.isFile()) {
        s.problem = Tr::tr("The file is not there.");
        return s;
    }
    s.bytes = info.size();

    Connection c(path, true);
    if (!c.open()) {
        s.problem = Tr::tr("SQLite cannot open it: %1").arg(c.error());
        return s;
    }
    QSqlQuery q(c.db());
    // quick_check legge tutte le pagine senza il controllo, lento, degli indici.
    if (!q.exec(QStringLiteral("PRAGMA quick_check")) || !q.next()) {
        s.problem = Tr::tr("It is not a SQLite database: %1").arg(q.lastError().text());
        return s;
    }
    const QString check = q.value(0).toString();
    if (check != QLatin1String("ok")) {
        s.problem = Tr::tr("SQLite finds it damaged: %1").arg(check.left(200));
        return s;
    }
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' "
                               "AND name IN ('qso', 'schema_version')"))
        || !q.next() || q.value(0).toInt() != 2) {
        s.problem = Tr::tr("It is not a DecoDXLog log.");
        return s;
    }
    if (q.exec(QStringLiteral("SELECT MAX(version) FROM schema_version")) && q.next())
        s.schema = q.value(0).toInt();
    if (!q.exec(QStringLiteral("SELECT COUNT(*), MIN(qso_datetime_on), MAX(qso_datetime_on) FROM qso "
                               "WHERE deleted = 0"))
        || !q.next()) {
        s.problem = Tr::tr("The QSOs cannot be read: %1").arg(q.lastError().text());
        return s;
    }
    s.qsos = q.value(0).toInt();
    s.firstQso = QDateTime::fromString(q.value(1).toString(), Qt::ISODate);
    s.lastQso = QDateTime::fromString(q.value(2).toString(), Qt::ISODate);
    s.readable = true;
    return s;
}

bool isSafetyCopy(const QString& fileName)
{
    return fileName.startsWith(QLatin1String("decodxlog-before-restore-"));
}

QFileInfoList backupsIn(const QString& dir)
{
    QFileInfoList out = QDir(dir).entryInfoList(
        {QStringLiteral("decolog-*.sqlite"), QStringLiteral("decodxlog-before-restore-*.sqlite")}, QDir::Files);
    std::sort(out.begin(), out.end(), [](const QFileInfo& a, const QFileInfo& b) {
        return a.lastModified() > b.lastModified();
    });
    return out;
}

RestoreResult restore(const QString& backup, const QString& target, const QString& safetyDir)
{
    RestoreResult r;
    if (QFileInfo(backup).canonicalFilePath() == QFileInfo(target).canonicalFilePath()) {
        r.error = Tr::tr("The backup is the log itself.");
        return r;
    }
    const Snapshot source = inspect(backup);
    if (!source.readable) {
        r.error = Tr::tr("The backup cannot be used: %1").arg(source.problem);
        return r;
    }

    // 1. Il log di adesso, com'e', nella cartella dei backup. Con VACUUM INTO
    //    ci finisce anche quello che sta ancora nel -wal; se il log e' rovinato
    //    e SQLite non lo legge, si copia il file cosi' com'e'.
    if (QFileInfo::exists(target)) {
        QDir().mkpath(safetyDir);
        // Un nome che non c'e' ancora: due ripristini nello stesso secondo non
        // devono scrivere uno sopra l'altro — e tanto meno sopra la copia che
        // si sta rimettendo.
        const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddTHHmmss"));
        r.safetyCopy = QDir(safetyDir).filePath(QStringLiteral("decodxlog-before-restore-%1.sqlite").arg(stamp));
        for (int n = 2; QFileInfo::exists(r.safetyCopy); ++n)
            r.safetyCopy = QDir(safetyDir).filePath(
                QStringLiteral("decodxlog-before-restore-%1-%2.sqlite").arg(stamp).arg(n));
        bool copied = false;
        {
            Connection c(target, false);
            if (c.open()) {
                QSqlQuery q(c.db());
                q.prepare(QStringLiteral("VACUUM INTO ?"));
                q.addBindValue(r.safetyCopy);
                copied = q.exec();
            }
        }
        if (!copied) {
            QFile::remove(r.safetyCopy);
            copied = QFile::copy(target, r.safetyCopy);
            if (copied && QFileInfo::exists(target + QStringLiteral("-wal")))
                QFile::copy(target + QStringLiteral("-wal"), r.safetyCopy + QStringLiteral("-wal"));
        }
        if (!copied) {
            r.error = Tr::tr("The log as it is now could not be saved to %1: nothing was changed.")
                          .arg(QDir::toNativeSeparators(r.safetyCopy));
            r.safetyCopy.clear();
            return r;
        }
    }

    // 2. La copia accanto al log, controllata, e solo allora al suo posto.
    const QString staging = target + QStringLiteral(".restoring");
    QFile::remove(staging);
    removeSidecars(staging);
    if (!QFile::copy(backup, staging)) {
        r.error = Tr::tr("The backup could not be copied next to the log: nothing was changed.");
        return r;
    }
    QFile(staging).setPermissions(QFile(staging).permissions() | QFileDevice::WriteOwner | QFileDevice::WriteUser);
    const Snapshot staged = inspect(staging);
    removeSidecars(staging);
    if (!staged.readable || staged.qsos != source.qsos) {
        QFile::remove(staging);
        r.error = Tr::tr("The copied backup does not read back the same: nothing was changed.");
        return r;
    }

    // 3. Via il log e i suoi -wal/-shm, che appartengono al file di prima e
    //    applicati al nuovo lo rovinerebbero; poi la copia prende il suo nome.
    if (QFileInfo::exists(target) && !QFile::remove(target)) {
        QFile::remove(staging);
        r.error = Tr::tr("The log is still in use by another program: nothing was changed.");
        return r;
    }
    removeSidecars(target);
    if (!QFile::rename(staging, target)) {
        // Il log e' gia' stato tolto: si rimette la copia di sicurezza, che e'
        // il log di prima.
        if (!r.safetyCopy.isEmpty())
            QFile::copy(r.safetyCopy, target);
        r.error = Tr::tr("The backup could not take the place of the log; the log as it was is back.");
        return r;
    }
    r.ok = true;
    r.qsos = source.qsos;
    return r;
}

bool waitForProcessExit(qint64 pid, int timeoutMs)
{
    if (pid <= 0)
        return true;
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!h)
        return true;   // non c'e' piu' (o non e' mai esistito)
    const DWORD result = WaitForSingleObject(h, static_cast<DWORD>(qMax(0, timeoutMs)));
    CloseHandle(h);
    return result == WAIT_OBJECT_0;
#else
    QElapsedTimer timer;
    timer.start();
    while (::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM) {
        if (timer.elapsed() >= timeoutMs)
            return false;
        QThread::msleep(100);
    }
    return true;
#endif
}

RestoreResult restoreAfterExit(qint64 pid, const QString& backup, const QString& target, const QString& safetyDir)
{
    if (!waitForProcessExit(pid, 30000)) {
        RestoreResult r;
        r.error = Tr::tr("DecoDXLog did not close in time: the log was not touched.");
        return r;
    }
    return restore(backup, target, safetyDir);
}

} // namespace decolog::core::logbackup
