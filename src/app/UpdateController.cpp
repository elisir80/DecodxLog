#include "app/UpdateController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDevice>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace decolog::app {

using namespace decolog::core;

namespace {

// Una volta al giorno basta e avanza: le versioni non escono ogni ora.
constexpr int kDayMs = 24 * 60 * 60 * 1000;
// Il primo controllo non all'avvio ma poco dopo: prima si apre il log.
constexpr int kFirstMs = 8000;

QFileDevice::Permissions executableAppImagePermissions()
{
    return QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
           | QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser
           | QFileDevice::ReadGroup | QFileDevice::ExeGroup
           | QFileDevice::ReadOther | QFileDevice::ExeOther;
}

QString downloadsDirectory()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (directory.isEmpty())
        directory = QDir::home().filePath(QStringLiteral("Downloads"));
    return directory;
}

} // namespace

UpdateController::UpdateController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    QSettings s;
    m_automatic = s.value(QStringLiteral("updates/automatic"), true).toBool();
    m_skip = s.value(QStringLiteral("updates/skip")).toString();

    connect(&m_fetcher, &UpdateFetcher::finished, this, [this](const ReleaseInfo& info) {
        m_info = info;
        QSettings().setValue(QStringLiteral("updates/lastCheck"), QDateTime::currentDateTimeUtc());
        if (available()) {
            setStatus(tr("DecoDXLog %1 is out").arg(info.version));
            if (m_ctx.activity) {
                m_ctx.activity(QStringLiteral("UPDATE"),
                               tr("DecoDXLog %1 is out — you have %2").arg(info.version, currentVersion()),
                               QStringLiteral("info"));
            }
            emit updateFound(info.version);
        } else {
            setStatus(info.valid
                          ? tr("this is the latest version")
                          : tr("no newer package is available for this computer"));
        }
        emit changed();
    });

    connect(&m_fetcher, &UpdateFetcher::failed, this, [this](const QString& error) {
        // Non e' una disgrazia: si riprova domani.
        setStatus(tr("cannot ask GitHub: %1").arg(error));
        emit changed();
    });

    connect(&m_fetcher, &UpdateFetcher::progress, this, [this](qint64 done, qint64 total) {
        m_progress = total > 0 ? static_cast<double>(done) / static_cast<double>(total) : -1;
        emit progressChanged();
    });

    connect(&m_fetcher, &UpdateFetcher::downloadFailed, this, [this](const QString& error) {
        m_progress = -1;
        setStatus(tr("the download did not finish: %1").arg(error));
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("UPDATE"), tr("Update not downloaded: %1").arg(error),
                           QStringLiteral("warning"));
        emit progressChanged();
        emit changed();
    });

    connect(&m_fetcher, &UpdateFetcher::downloaded, this, [this](const QString& path) {
        m_progress = 1;
        emit progressChanged();
#if defined(Q_OS_WIN)
        setStatus(tr("starting the installer…"));
        if (!QProcess::startDetached(path, {})) {
            setStatus(tr("cannot start the installer"));
            emit changed();
            return;
        }
        if (m_ctx.activity) {
            m_ctx.activity(QStringLiteral("UPDATE"),
                           tr("Installer ready: %1 — DecoDXLog closes and the installer opens").arg(path),
                           QStringLiteral("info"));
        }
        emit changed();
        if (m_ctx.quit)
            m_ctx.quit();
#elif defined(Q_OS_MACOS)
        setStatus(tr("the disk image is ready in Downloads"));
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            setStatus(tr("the disk image was saved to %1").arg(QDir::toNativeSeparators(path)));
        if (m_ctx.activity) {
            m_ctx.activity(QStringLiteral("UPDATE"),
                           tr("macOS update package ready: %1").arg(path),
                           QStringLiteral("info"));
        }
        emit changed();
#else
        if (m_replaceRunningAppImage) {
            QStringList arguments = QCoreApplication::arguments();
            if (!arguments.isEmpty())
                arguments.removeFirst();
            if (QProcess::startDetached(path, arguments, QFileInfo(path).absolutePath())) {
                setStatus(tr("the AppImage was updated and DecoDXLog is restarting…"));
                if (m_ctx.activity) {
                    m_ctx.activity(QStringLiteral("UPDATE"),
                                   tr("AppImage updated: %1").arg(path),
                                   QStringLiteral("info"));
                }
                emit changed();
                if (m_ctx.quit)
                    m_ctx.quit();
                return;
            }
            setStatus(tr("the AppImage was updated; restart it manually"));
        } else {
            setStatus(tr("the AppImage was saved to Downloads; launch it manually"));
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        if (m_ctx.activity) {
            m_ctx.activity(QStringLiteral("UPDATE"),
                           tr("Linux update package ready: %1").arg(path),
                           QStringLiteral("info"));
        }
        emit changed();
#endif
    });

    m_daily.setInterval(kDayMs);
    connect(&m_daily, &QTimer::timeout, this, [this] { runCheck(false); });
    m_first.setSingleShot(true);
    m_first.setInterval(kFirstMs);
    connect(&m_first, &QTimer::timeout, this, [this] { runCheck(false); });
}

void UpdateController::start()
{
    if (!m_automatic)
        return;
    m_first.start();
    m_daily.start();
}

QString UpdateController::currentVersion() const
{
    return QCoreApplication::applicationVersion();
}

bool UpdateController::available() const
{
    if (!hasPackage())
        return false;
    if (!m_skip.isEmpty() && updates::compareVersions(m_info.version, m_skip) <= 0)
        return false;
    return updates::compareVersions(m_info.version, currentVersion()) > 0;
}

QString UpdateController::lastCheck() const
{
    const QDateTime when = QSettings().value(QStringLiteral("updates/lastCheck")).toDateTime();
    if (!when.isValid())
        return {};
    return QLocale().toString(when.toLocalTime(), QLocale::ShortFormat);
}

QString UpdateController::downloadSize() const
{
    if (m_info.packageBytes <= 0)
        return {};
    return QLocale().formattedDataSize(m_info.packageBytes);
}

void UpdateController::setAutomatic(bool on)
{
    if (m_automatic == on)
        return;
    m_automatic = on;
    QSettings().setValue(QStringLiteral("updates/automatic"), on);
    if (on) {
        m_daily.start();
        runCheck(false);
    } else {
        m_daily.stop();
        m_first.stop();
    }
    emit changed();
}

void UpdateController::checkNow()
{
    runCheck(true);
}

void UpdateController::runCheck(bool announce)
{
    if (m_fetcher.busy())
        return;
    if (announce)
        setStatus(tr("asking GitHub…"));
    m_fetcher.fetch(currentVersion());
    emit changed();
}

void UpdateController::downloadAndInstall()
{
    if (!hasPackage() || m_fetcher.downloading())
        return;

    // Un nome viene dalla release GitHub, ma non gli permettiamo di scegliere
    // directory locali: si salva sempre come semplice basename.
    const QString packageName = QFileInfo(m_info.packageName).fileName();
    if (packageName.isEmpty()) {
        setStatus(tr("the update package has no file name"));
        emit changed();
        return;
    }

    QString directory;
    QString path;
    QFileDevice::Permissions permissions;
    m_replaceRunningAppImage = false;

#if defined(Q_OS_LINUX)
    const QString runningPath = qEnvironmentVariable("APPIMAGE").trimmed();
    const QFileInfo runningInfo(runningPath);
    const QFileInfo runningDirectory(runningInfo.absoluteDir().absolutePath());
    if (runningInfo.isFile() && runningDirectory.isWritable()) {
        path = runningInfo.absoluteFilePath();
        permissions = runningInfo.permissions() | executableAppImagePermissions();
        m_replaceRunningAppImage = true;
    } else {
        directory = downloadsDirectory();
        permissions = executableAppImagePermissions();
    }
#elif defined(Q_OS_MACOS)
    directory = downloadsDirectory();
#else
    directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#endif

    if (!path.isEmpty()) {
        // APPIMAGE e' gia' un path assoluto da sostituire in modo atomico.
    } else {
        if (directory.isEmpty() || !QDir().mkpath(directory)) {
            setStatus(tr("cannot create the download folder"));
            emit changed();
            return;
        }
        path = QDir(directory).filePath(packageName);
    }

    m_progress = 0;
    setStatus(downloadSize().isEmpty()
                  ? tr("downloading the update…")
                  : tr("downloading %1…").arg(downloadSize()));
    emit progressChanged();
    emit changed();
    m_fetcher.download(m_info.package, path, m_info.packageBytes, permissions);
}

void UpdateController::cancelDownload()
{
    m_fetcher.cancelDownload();
    m_progress = -1;
    setStatus(tr("download stopped"));
    emit progressChanged();
    emit changed();
}

void UpdateController::skipThisVersion()
{
    if (!m_info.valid)
        return;
    m_skip = m_info.version;
    QSettings().setValue(QStringLiteral("updates/skip"), m_skip);
    setStatus(tr("version %1 set aside").arg(m_skip));
    emit changed();
}

void UpdateController::openPage()
{
    if (!m_info.page.isEmpty())
        QDesktopServices::openUrl(QUrl(m_info.page));
}

void UpdateController::setStatus(const QString& text)
{
    m_status = text;
}

} // namespace decolog::app
