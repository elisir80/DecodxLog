#include "app/UpdateController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
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
            setStatus(tr("this is the latest version"));
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
        m_installerPath = path;
        emit progressChanged();
        if (m_ctx.activity) {
            m_ctx.activity(QStringLiteral("UPDATE"),
                           tr("Installer ready: %1 — DecoDXLog closes and the installer opens").arg(path),
                           QStringLiteral("info"));
        }
        setStatus(tr("starting the installer…"));
        emit changed();
        // L'installatore non puo' sostituire i file di un programma aperto:
        // prima si lancia, poi si chiude DecoDXLog.
        QProcess::startDetached(path, {});
        if (m_ctx.quit)
            m_ctx.quit();
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
    if (!m_info.valid)
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
    if (m_info.installerBytes <= 0)
        return {};
    return QLocale().formattedDataSize(m_info.installerBytes);
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
    m_fetcher.fetch();
    emit changed();
}

void UpdateController::downloadAndInstall()
{
    if (m_info.installer.isEmpty() || m_fetcher.downloading())
        return;
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString path = QDir(dir).filePath(
        QStringLiteral("DecoDXLog-%1-setup.exe").arg(m_info.version));
    m_progress = 0;
    setStatus(tr("downloading %1…").arg(downloadSize()));
    emit progressChanged();
    emit changed();
    m_fetcher.download(m_info.installer, path);
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
