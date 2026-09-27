#include "app/SuperCheckController.h"

#include "core/LogDatabase.h"
#include "core/NetworkError.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QSqlQuery>
#include <QStandardPaths>

namespace decolog::app {

SuperCheckController::SuperCheckController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    m_status = tr("only the calls of the log: download MASTER.SCP for the full list");
    m_reload.setSingleShot(true);
    m_reload.setInterval(5000);
    connect(&m_reload, &QTimer::timeout, this, &SuperCheckController::rebuild);
}

QString SuperCheckController::filePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("MASTER.SCP"));
}

void SuperCheckController::start()
{
    QFile f(filePath());
    if (f.open(QIODevice::ReadOnly)) {
        m_fileData = f.readAll();
        m_fileTime = QFileInfo(f).lastModified();
    }
    rebuild();
}

QString SuperCheckController::fileDate() const
{
    return m_fileTime.isValid() ? QLocale().toString(m_fileTime.date(), QLocale::ShortFormat) : QString();
}

void SuperCheckController::loadData(const QByteArray& data)
{
    m_fileData = data;
    m_fileTime = QDateTime::currentDateTime();
    rebuild();
}

void SuperCheckController::rebuild()
{
    m_scp.clear();
    m_fileCount = m_fileData.isEmpty() ? 0 : m_scp.load(m_fileData);
    if (m_ctx.db) {
        QStringList calls;
        QSqlQuery q(m_ctx.db->connection());
        q.setForwardOnly(true);
        if (q.exec(QStringLiteral("SELECT DISTINCT call FROM qso WHERE deleted = 0"))) {
            while (q.next())
                calls << q.value(0).toString();
        }
        m_scp.addCalls(calls);
    }
    if (m_status.isEmpty() || !m_busy)
        m_status = m_fileCount > 0 ? tr("%1 calls from MASTER.SCP (%2) and the log").arg(m_fileCount).arg(fileDate())
                                   : tr("only the calls of the log: download MASTER.SCP for the full list");
    emit changed();
}

QStringList SuperCheckController::partial(const QString& fragment, int limit) const
{
    return m_scp.partial(fragment, limit);
}

QStringList SuperCheckController::nPlusOne(const QString& call, int limit) const
{
    return m_scp.nPlusOne(call, limit);
}

void SuperCheckController::download()
{
    if (m_busy)
        return;
    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    m_busy = true;
    m_status = tr("downloading MASTER.SCP…");
    emit changed();
    QNetworkRequest request{QUrl(m_url)};
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_busy = false;
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || data.size() < 1000) {
            m_status = tr("MASTER.SCP not downloaded: %1").arg(reply->error() != QNetworkReply::NoError
                                                                    ? core::network::safeErrorString(reply)
                                                                    : tr("empty answer"));
            if (m_ctx.activity)
                m_ctx.activity(QStringLiteral("CONTEST"), m_status, QStringLiteral("warning"));
            emit changed();
            return;
        }
        QDir().mkpath(QFileInfo(filePath()).absolutePath());
        QSaveFile out(filePath());
        if (out.open(QIODevice::WriteOnly)) {
            out.write(data);
            out.commit();
        }
        m_status.clear();
        loadData(data);
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CONTEST"), tr("MASTER.SCP: %1 calls").arg(m_fileCount), QStringLiteral("info"));
    });
}

} // namespace decolog::app
