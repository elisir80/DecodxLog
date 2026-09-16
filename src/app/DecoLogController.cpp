#include "app/DecoLogController.h"

#include "core/Bands.h"

#include <QCoreApplication>
#include <QFile>
#include <QHostAddress>
#include <QSettings>

namespace decolog::app {

using namespace decolog::core;

namespace {

// Un client che non si fa sentire da tre battiti (15 s l'uno) e' andato via.
constexpr qint64 kClientTimeoutMs = 45'000;
constexpr int kMaxActivity = 300;
constexpr int kMaxIncoming = 50;

QString nowUtcLabel()
{
    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("HH:mm:ss"));
}

} // namespace

DecoLogController::DecoLogController(QObject* parent)
    : QObject(parent)
{
    QSettings s;
    m_udpPort = s.value(QStringLiteral("udp/port"), 2237).toInt();
    m_multicast = s.value(QStringLiteral("udp/multicastGroup")).toString();

    connect(&m_udp, &UdpReceiver::qsoReceived, this, &DecoLogController::onQsoReceived);
    connect(&m_udp, &UdpReceiver::listeningChanged, this, &DecoLogController::udpChanged);
    connect(&m_udp, &UdpReceiver::clientSeen, this, [this](const UdpClientInfo& c) {
        const bool wasConnected = clientConnected();
        const bool changed = c.id != m_clientName || (!c.version.isEmpty() && c.version != m_clientVersion);
        m_clientName = c.id;
        if (!c.version.isEmpty())
            m_clientVersion = c.version;
        m_clientLastSeen = c.lastSeen;
        if (!wasConnected)
            addActivity(tr("%1 connected from %2").arg(c.id, c.address.toString()));
        if (changed || !wasConnected)
            emit clientChanged();
    });
    connect(&m_udp, &UdpReceiver::clientClosed, this, [this](const QString& id) {
        addActivity(tr("%1 closed").arg(id));
        m_clientLastSeen = {};
        m_status = {};
        emit clientChanged();
    });
    connect(&m_udp, &UdpReceiver::statusReceived, this,
            [this](const QString&, const wsjtx::Status& st) {
                const bool dxChanged = st.dxCall != m_status.dxCall;
                m_status = st;
                emit clientChanged();
                // Il nominativo che Decodium sta lavorando e' quello che interessa
                // adesso: il pannello a destra lo segue da solo.
                if (dxChanged && !st.dxCall.isEmpty())
                    setLookupCall(st.dxCall);
            });

    m_clientWatch.setInterval(5000);
    connect(&m_clientWatch, &QTimer::timeout, this, [this] {
        if (m_clientLastSeen.isValid()
            && m_clientLastSeen.msecsTo(QDateTime::currentDateTimeUtc()) > kClientTimeoutMs) {
            addActivity(tr("%1 not heard for 45 s").arg(m_clientName), QStringLiteral("warning"));
            m_clientLastSeen = {};
            emit clientChanged();
        }
    });
    m_clientWatch.start();
}

DecoLogController::~DecoLogController() = default;

bool DecoLogController::openDatabase(const QString& path)
{
    const bool ok = m_db.open(path);
    if (ok) {
        addActivity(tr("Log opened: %1 (%n QSO)", nullptr, m_db.qsoCount()).arg(path));
    } else {
        addActivity(tr("Cannot open log %1: %2").arg(path, m_db.lastError()), QStringLiteral("error"));
    }
    m_model = new QsoTableModel(&m_db, this);
    return ok;
}

void DecoLogController::startListening()
{
    QHostAddress group;
    if (!m_multicast.trimmed().isEmpty())
        group = QHostAddress(m_multicast.trimmed());
    if (m_udp.start(static_cast<quint16>(m_udpPort), group)) {
        addActivity(group.isNull() ? tr("Listening on UDP %1").arg(m_udpPort)
                                   : tr("Listening on UDP %1, multicast %2").arg(m_udpPort).arg(group.toString()));
    } else if (m_udpPort > 0) {
        addActivity(tr("Cannot listen on UDP %1: %2").arg(m_udpPort).arg(m_udp.lastError()),
                    QStringLiteral("error"));
    }
    emit udpChanged();
}

QString DecoLogController::version() const
{
    return QCoreApplication::applicationVersion();
}

void DecoLogController::setUdpPort(int port)
{
    if (port == m_udpPort || port < 0 || port > 65535)
        return;
    m_udpPort = port;
    QSettings().setValue(QStringLiteral("udp/port"), port);
    startListening();
}

void DecoLogController::setMulticastGroup(const QString& group)
{
    if (group == m_multicast)
        return;
    m_multicast = group;
    QSettings().setValue(QStringLiteral("udp/multicastGroup"), group);
    startListening();
}

bool DecoLogController::clientConnected() const
{
    return m_clientLastSeen.isValid();
}

QString DecoLogController::dialFrequency() const
{
    if (m_status.dialFrequencyHz == 0)
        return {};
    return QString::number(static_cast<double>(m_status.dialFrequencyHz) / 1e6, 'f', 6);
}

QStringList DecoLogController::bands() const
{
    return bands::all();
}

QString DecoLogController::bandForFrequency(const QString& mhz) const
{
    bool ok = false;
    const double f = QString(mhz).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    return ok ? bands::fromMhz(f) : QString();
}

void DecoLogController::onQsoReceived(const AdifRecord& record, const QString& source, const QString& sourceApp)
{
    const InsertResult r = m_db.insertQso(record, source, sourceApp);

    const QString call = record.value(QStringLiteral("CALL")).toUpper();
    const QString submode = record.value(QStringLiteral("SUBMODE"));
    const QString mode = submode.isEmpty() ? record.value(QStringLiteral("MODE")) : submode;
    QVariantMap item{
        {QStringLiteral("time"), nowUtcLabel()},
        {QStringLiteral("call"), call},
        {QStringLiteral("band"), record.value(QStringLiteral("BAND"))},
        {QStringLiteral("mode"), mode},
        {QStringLiteral("rstSent"), record.value(QStringLiteral("RST_SENT"))},
        {QStringLiteral("rstRcvd"), record.value(QStringLiteral("RST_RCVD"))},
        {QStringLiteral("grid"), record.value(QStringLiteral("GRIDSQUARE"))},
        {QStringLiteral("app"), sourceApp},
    };

    switch (r.status) {
    case InsertResult::Status::Inserted:
        item[QStringLiteral("status")] = QStringLiteral("logged");
        m_model->prependQso(r.id);
        addActivity(tr("Logged %1 %2 %3 from %4").arg(call, item.value(QStringLiteral("band")).toString(), mode, sourceApp),
                    QStringLiteral("success"));
        emit logChanged();
        break;
    case InsertResult::Status::Duplicate:
        item[QStringLiteral("status")] = QStringLiteral("duplicate");
        addActivity(tr("Duplicate ignored: %1").arg(r.message), QStringLiteral("warning"));
        break;
    case InsertResult::Status::Invalid:
    case InsertResult::Status::Error:
        item[QStringLiteral("status")] = QStringLiteral("error");
        addActivity(tr("QSO not logged: %1").arg(r.message), QStringLiteral("error"));
        break;
    }

    m_incoming.prepend(item);
    while (m_incoming.size() > kMaxIncoming)
        m_incoming.removeLast();
    emit incomingChanged();

    if (call == m_lookupCall.toUpper())
        refreshWorkedBefore();
}

QString DecoLogController::logManualQso(const QVariantMap& fields)
{
    auto text = [&fields](const char* key) { return fields.value(QLatin1String(key)).toString().trimmed(); };

    AdifRecord r;
    r.set(QStringLiteral("CALL"), text("call").toUpper());
    const QDate date = QDate::fromString(text("date"), QStringLiteral("yyyy-MM-dd"));
    QTime time = QTime::fromString(text("time"), QStringLiteral("HH:mm"));
    if (!time.isValid())
        time = QTime::fromString(text("time"), QStringLiteral("HH:mm:ss"));
    if (date.isValid())
        r.set(QStringLiteral("QSO_DATE"), date.toString(QStringLiteral("yyyyMMdd")));
    if (time.isValid())
        r.set(QStringLiteral("TIME_ON"), time.toString(QStringLiteral("HHmmss")));

    QString freq = text("freq");
    freq.replace(QLatin1Char(','), QLatin1Char('.'));
    r.set(QStringLiteral("FREQ"), freq);
    r.set(QStringLiteral("BAND"), text("band").isEmpty() ? bandForFrequency(freq) : text("band"));
    r.set(QStringLiteral("MODE"), text("mode").toUpper());
    r.set(QStringLiteral("SUBMODE"), text("submode").toUpper());
    r.set(QStringLiteral("RST_SENT"), text("rst_sent"));
    r.set(QStringLiteral("RST_RCVD"), text("rst_rcvd"));
    r.set(QStringLiteral("NAME"), text("name"));
    r.set(QStringLiteral("QTH"), text("qth"));
    r.set(QStringLiteral("GRIDSQUARE"), text("gridsquare").toUpper());
    r.set(QStringLiteral("COMMENT"), text("comment"));

    const InsertResult res = m_db.insertQso(r, QStringLiteral("manual"), QStringLiteral("DecoLog ") + version(), true);
    switch (res.status) {
    case InsertResult::Status::Inserted:
        m_model->prependQso(res.id);
        addActivity(tr("Logged %1 (manual)").arg(r.value(QStringLiteral("CALL"))), QStringLiteral("success"));
        emit logChanged();
        setLookupCall(r.value(QStringLiteral("CALL")));
        refreshWorkedBefore();
        return {};
    case InsertResult::Status::Duplicate:
        return tr("Already in log (within 10 minutes)");
    default:
        return res.message;
    }
}

void DecoLogController::importAdif(const QUrl& url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        addActivity(tr("Cannot read %1: %2").arg(path, file.errorString()), QStringLiteral("error"));
        return;
    }
    const ImportResult r = m_db.importAdif(file.readAll());
    addActivity(tr("Import %1: %2 new, %3 duplicates, %4 rejected")
                    .arg(path).arg(r.inserted).arg(r.duplicates).arg(r.invalid),
                r.invalid ? QStringLiteral("warning") : QStringLiteral("success"));
    for (const QString& e : r.errors)
        addActivity(QStringLiteral("  ") + e, QStringLiteral("warning"));
    m_model->reload();
    emit logChanged();
    refreshWorkedBefore();
}

void DecoLogController::exportAdif(const QUrl& url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        addActivity(tr("Cannot write %1: %2").arg(path, file.errorString()), QStringLiteral("error"));
        return;
    }
    file.write(m_db.exportAdif(version()));
    addActivity(tr("Exported %n QSO to %1", nullptr, m_db.qsoCount()).arg(path), QStringLiteral("success"));
}

void DecoLogController::setLookupCall(const QString& call)
{
    const QString c = call.trimmed().toUpper();
    if (c == m_lookupCall)
        return;
    m_lookupCall = c;
    refreshWorkedBefore();
}

void DecoLogController::refreshWorkedBefore()
{
    const WorkedBefore wb = m_db.workedBefore(m_lookupCall);
    m_workedBefore = {
        {QStringLiteral("count"), wb.count},
        {QStringLiteral("bands"), wb.bands},
        {QStringLiteral("modes"), wb.modes},
        {QStringLiteral("last"), wb.last.isValid() ? wb.last.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QString()},
        {QStringLiteral("lastBand"), wb.lastBand},
        {QStringLiteral("lastMode"), wb.lastMode},
        {QStringLiteral("name"), wb.name},
        {QStringLiteral("gridsquare"), wb.gridsquare},
        {QStringLiteral("country"), wb.country},
    };
    emit lookupChanged();
}

void DecoLogController::addActivity(const QString& text, const QString& level)
{
    m_activity.prepend(QVariantMap{
        {QStringLiteral("time"), nowUtcLabel()},
        {QStringLiteral("text"), text},
        {QStringLiteral("level"), level},
    });
    while (m_activity.size() > kMaxActivity)
        m_activity.removeLast();
    emit activityChanged();
}

} // namespace decolog::app
