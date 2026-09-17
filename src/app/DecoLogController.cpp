#include "app/DecoLogController.h"

#include "core/Bands.h"
#include "core/Maidenhead.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QSettings>
#include <QStandardPaths>
#include <cmath>

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

QString serviceLabel(const QString& service)
{
    if (service == QLatin1String("lotw"))    return QStringLiteral("LoTW");
    if (service == QLatin1String("qrz"))     return QStringLiteral("QRZ");
    if (service == QLatin1String("clublog")) return QStringLiteral("ClubLog");
    if (service == QLatin1String("eqsl"))    return QStringLiteral("eQSL");
    if (service == QLatin1String("card"))    return QStringLiteral("Card");
    return service;
}

const QStringList kServices{QStringLiteral("lotw"), QStringLiteral("qrz"), QStringLiteral("clublog"),
                            QStringLiteral("eqsl"), QStringLiteral("card")};

// Campi che la scheda del QSO mostra nelle sue schede; tutto il resto e' "ADIF extra".
const QStringList kKnownFields{
    QStringLiteral("CALL"), QStringLiteral("QSO_DATE"), QStringLiteral("TIME_ON"), QStringLiteral("QSO_DATE_OFF"),
    QStringLiteral("TIME_OFF"), QStringLiteral("BAND"), QStringLiteral("BAND_RX"), QStringLiteral("FREQ"),
    QStringLiteral("FREQ_RX"), QStringLiteral("MODE"), QStringLiteral("SUBMODE"), QStringLiteral("RST_SENT"),
    QStringLiteral("RST_RCVD"), QStringLiteral("GRIDSQUARE"), QStringLiteral("NAME"), QStringLiteral("QTH"),
    QStringLiteral("COUNTRY"), QStringLiteral("DXCC"), QStringLiteral("CQZ"), QStringLiteral("ITUZ"),
    QStringLiteral("CONT"), QStringLiteral("STATE"), QStringLiteral("CNTY"), QStringLiteral("IOTA"),
    QStringLiteral("SOTA_REF"), QStringLiteral("POTA_REF"), QStringLiteral("WWFF_REF"), QStringLiteral("PROP_MODE"),
    QStringLiteral("SAT_NAME"), QStringLiteral("TX_PWR"), QStringLiteral("COMMENT"), QStringLiteral("NOTES"),
    QStringLiteral("STATION_CALLSIGN"), QStringLiteral("OPERATOR"), QStringLiteral("MY_GRIDSQUARE"),
    QStringLiteral("LOTW_QSL_SENT"), QStringLiteral("LOTW_QSLSDATE"), QStringLiteral("LOTW_QSL_RCVD"),
    QStringLiteral("LOTW_QSLRDATE"), QStringLiteral("QRZCOM_QSO_UPLOAD_STATUS"), QStringLiteral("QRZCOM_QSO_UPLOAD_DATE"),
    QStringLiteral("QRZCOM_QSO_DOWNLOAD_STATUS"), QStringLiteral("QRZCOM_QSO_DOWNLOAD_DATE"),
    QStringLiteral("CLUBLOG_QSO_UPLOAD_STATUS"), QStringLiteral("CLUBLOG_QSO_UPLOAD_DATE"),
    QStringLiteral("EQSL_QSL_SENT"), QStringLiteral("EQSL_QSLSDATE"), QStringLiteral("EQSL_QSL_RCVD"),
    QStringLiteral("EQSL_QSLRDATE"), QStringLiteral("QSL_SENT"), QStringLiteral("QSLSDATE"), QStringLiteral("QSL_RCVD"),
    QStringLiteral("QSLRDATE")};

QVariantMap positionMap(const std::optional<maidenhead::LatLon>& p)
{
    if (!p)
        return {};
    return {{QStringLiteral("lat"), p->lat}, {QStringLiteral("lon"), p->lon}};
}

} // namespace

DecoLogController::DecoLogController(QObject* parent)
    : QObject(parent)
{
    QSettings s;
    m_udpPort = s.value(QStringLiteral("udp/port"), 2237).toInt();
    m_multicast = s.value(QStringLiteral("udp/multicastGroup")).toString();
    m_udp.setPreferLoggedAdif(s.value(QStringLiteral("udp/preferLoggedAdif"), true).toBool());
    m_followDx = s.value(QStringLiteral("udp/followDxCall"), true).toBool();
    m_db.setDedupWindows(s.value(QStringLiteral("log/dedupDigitalMinutes"), 2).toInt() * 60,
                         s.value(QStringLiteral("log/dedupManualMinutes"), 10).toInt() * 60);

    m_backupEnabled = s.value(QStringLiteral("backup/enabled"), true).toBool();
    m_backupDir = s.value(QStringLiteral("backup/dir")).toString();
    m_backupTime = s.value(QStringLiteral("backup/time"), QStringLiteral("02:00")).toString();
    m_backupKeep = s.value(QStringLiteral("backup/keep"), 14).toInt();

    m_cloudServer = s.value(QStringLiteral("cloud/server")).toString();
    m_autoSync = s.value(QStringLiteral("cloud/autoSync"), QStringLiteral("qso+5min")).toString();
    m_conflictPolicy = s.value(QStringLiteral("cloud/conflictPolicy"), QStringLiteral("lastEdit")).toString();

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
            addActivity(QStringLiteral("UDP"), tr("%1 connected from %2").arg(c.id, c.address.toString()));
        if (changed || !wasConnected)
            emit clientChanged();
    });
    connect(&m_udp, &UdpReceiver::clientClosed, this, [this](const QString& id) {
        addActivity(QStringLiteral("UDP"), tr("%1 closed").arg(id));
        m_clientLastSeen = {};
        m_status = {};
        emit clientChanged();
    });
    connect(&m_udp, &UdpReceiver::statusReceived, this,
            [this](const QString&, const wsjtx::Status& st) {
                const bool dxChanged = st.dxCall != m_status.dxCall;
                const bool gridChanged = st.deGrid != m_status.deGrid;
                m_status = st;
                emit clientChanged();
                if (gridChanged)
                    emit stationChanged();
                maybeCreateProfileFromDecodium();
                // Il nominativo che Decodium sta lavorando e' quello che interessa
                // adesso: il pannello a destra lo segue da solo.
                if (m_followDx && dxChanged && !st.dxCall.isEmpty())
                    setLookupCall(st.dxCall);
            });

    m_clientWatch.setInterval(5000);
    connect(&m_clientWatch, &QTimer::timeout, this, [this] {
        if (m_clientLastSeen.isValid()
            && m_clientLastSeen.msecsTo(QDateTime::currentDateTimeUtc()) > kClientTimeoutMs) {
            addActivity(QStringLiteral("UDP"), tr("%1 not heard for 45 s").arg(m_clientName), QStringLiteral("warning"));
            m_clientLastSeen = {};
            emit clientChanged();
        }
    });
    m_clientWatch.start();

    m_backupTimer.setInterval(60'000);
    connect(&m_backupTimer, &QTimer::timeout, this, &DecoLogController::checkBackupSchedule);
}

DecoLogController::~DecoLogController() = default;

bool DecoLogController::openDatabase(const QString& path)
{
    const bool ok = m_db.open(path);
    if (ok) {
        addActivity(QStringLiteral("LOG"), tr("Log opened: %1 (%n QSO)", nullptr, m_db.qsoCount()).arg(path));
    } else {
        addActivity(QStringLiteral("LOG"), tr("Cannot open log %1: %2").arg(path, m_db.lastError()),
                    QStringLiteral("error"));
    }
    if (m_backupDir.isEmpty())
        m_backupDir = QDir(QFileInfo(path).absolutePath()).filePath(QStringLiteral("backup"));

    m_model = new QsoTableModel(&m_db, this);
    m_profiles = new StationProfileModel(&m_db, this);
    connect(m_profiles, &StationProfileModel::activeChanged, this, [this] {
        emit stationChanged();
        refreshCallInfo();
    });
    connect(m_profiles, &StationProfileModel::profilesChanged, this, &DecoLogController::stationChanged);
    m_backupTimer.start();
    return ok;
}

void DecoLogController::startListening()
{
    QHostAddress group;
    if (!m_multicast.trimmed().isEmpty())
        group = QHostAddress(m_multicast.trimmed());
    if (m_udp.start(static_cast<quint16>(m_udpPort), group)) {
        addActivity(QStringLiteral("UDP"),
                    group.isNull() ? tr("Listening on UDP %1").arg(m_udpPort)
                                   : tr("Listening on UDP %1, multicast %2").arg(m_udpPort).arg(group.toString()));
    } else if (m_udpPort > 0) {
        addActivity(QStringLiteral("UDP"), tr("Cannot listen on UDP %1: %2").arg(m_udpPort).arg(m_udp.lastError()),
                    QStringLiteral("error"));
    }
    emit udpChanged();
}

QString DecoLogController::version() const
{
    return QCoreApplication::applicationVersion();
}

// ── Impostazioni del collegamento ─────────────────────────────────────────────

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

void DecoLogController::setPreferLoggedAdif(bool prefer)
{
    if (prefer == m_udp.prefersLoggedAdif())
        return;
    m_udp.setPreferLoggedAdif(prefer);
    QSettings().setValue(QStringLiteral("udp/preferLoggedAdif"), prefer);
    emit udpChanged();
}

void DecoLogController::setDedupDigitalMinutes(int minutes)
{
    if (minutes == dedupDigitalMinutes())
        return;
    m_db.setDedupWindows(minutes * 60, m_db.dedupWindowSeconds(true));
    QSettings().setValue(QStringLiteral("log/dedupDigitalMinutes"), minutes);
    emit udpChanged();
}

void DecoLogController::setDedupManualMinutes(int minutes)
{
    if (minutes == dedupManualMinutes())
        return;
    m_db.setDedupWindows(m_db.dedupWindowSeconds(false), minutes * 60);
    QSettings().setValue(QStringLiteral("log/dedupManualMinutes"), minutes);
    emit udpChanged();
}

void DecoLogController::setFollowDxCall(bool follow)
{
    if (follow == m_followDx)
        return;
    m_followDx = follow;
    QSettings().setValue(QStringLiteral("udp/followDxCall"), follow);
    emit udpChanged();
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

QString DecoLogController::dialBand() const
{
    if (m_status.dialFrequencyHz == 0)
        return {};
    return bands::fromMhz(static_cast<double>(m_status.dialFrequencyHz) / 1e6);
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

// ── Stazione ──────────────────────────────────────────────────────────────────

QString DecoLogController::myGrid() const
{
    const QString grid = m_profiles ? m_profiles->activeProfile().value(QStringLiteral("myGridsquare")).toString()
                                    : QString();
    return grid.isEmpty() ? m_status.deGrid : grid;
}

QVariantMap DecoLogController::myPosition() const
{
    return positionMap(maidenhead::toLatLon(myGrid()));
}

// Al primo avvio non ci sono profili: se Decodium dice chi e' e dove sta, se ne
// crea uno. L'operatore lo ritrova in "Station profiles" e lo puo' correggere.
void DecoLogController::maybeCreateProfileFromDecodium()
{
    if (!m_profiles || m_profiles->count() > 0 || m_status.deCall.isEmpty())
        return;
    QVariantMap p{
        {QStringLiteral("name"), m_status.deGrid.isEmpty()
                                     ? m_status.deCall
                                     : QStringLiteral("%1 %2").arg(m_status.deCall, m_status.deGrid.left(6))},
        {QStringLiteral("stationCallsign"), m_status.deCall},
        {QStringLiteral("myGridsquare"), m_status.deGrid},
        {QStringLiteral("isDefault"), true},
    };
    if (m_profiles->save(p) > 0)
        addActivity(QStringLiteral("LOG"), tr("Station profile created from Decodium: %1").arg(p.value("name").toString()),
                    QStringLiteral("success"));
}

void DecoLogController::applyProfile(AdifRecord& record, qint64 profileId) const
{
    if (!m_profiles || profileId <= 0)
        return;
    const QVariantMap p = m_profiles->byId(profileId);
    auto fill = [&record](const char* field, const QString& value) {
        if (record.value(QLatin1String(field)).isEmpty() && !value.isEmpty())
            record.set(QLatin1String(field), value);
    };
    fill("STATION_CALLSIGN", p.value(QStringLiteral("stationCallsign")).toString());
    fill("OPERATOR", p.value(QStringLiteral("operatorCall")).toString());
    fill("MY_GRIDSQUARE", p.value(QStringLiteral("myGridsquare")).toString());
    fill("MY_RIG", p.value(QStringLiteral("myRig")).toString());
    fill("MY_ANTENNA", p.value(QStringLiteral("myAntenna")).toString());
    const double pwr = p.value(QStringLiteral("defaultTxPwr")).toDouble();
    if (pwr > 0)
        fill("TX_PWR", QString::number(pwr));
}

// ── QSO in arrivo ─────────────────────────────────────────────────────────────

void DecoLogController::onQsoReceived(const AdifRecord& input, const QString& source, const QString& sourceApp)
{
    // Il profilo lo indica il nominativo di stazione del QSO; se non corrisponde a
    // nessuno, vale quello attivo. I campi del profilo non si aggiungono: il QSO
    // resta come l'ha mandato Decodium.
    qint64 profileId = m_db.profileForCallsign(input.value(QStringLiteral("STATION_CALLSIGN")));
    if (profileId == 0 && m_profiles)
        profileId = m_profiles->activeProfileId();

    const InsertResult r = m_db.insertQso(input, source, sourceApp, false, profileId);

    const QString call = input.value(QStringLiteral("CALL")).toUpper();
    AdifRecord normalized = input;
    adif::normalizeMode(normalized);
    const QString submode = normalized.value(QStringLiteral("SUBMODE"));
    const QString mode = submode.isEmpty() ? normalized.value(QStringLiteral("MODE")) : submode;
    const QString freq = input.value(QStringLiteral("FREQ"));
    QVariantMap item{
        {QStringLiteral("time"), nowUtcLabel()},
        {QStringLiteral("call"), call},
        {QStringLiteral("band"), input.value(QStringLiteral("BAND"))},
        {QStringLiteral("freq"), freq.isEmpty() ? QString() : QString::number(freq.toDouble(), 'f', 3)},
        {QStringLiteral("mode"), mode},
        {QStringLiteral("rstSent"), input.value(QStringLiteral("RST_SENT"))},
        {QStringLiteral("rstRcvd"), input.value(QStringLiteral("RST_RCVD"))},
        {QStringLiteral("grid"), input.value(QStringLiteral("GRIDSQUARE"))},
        {QStringLiteral("app"), sourceApp},
        {QStringLiteral("message"), m_udp.prefersLoggedAdif() && source.startsWith(QLatin1String("udp"))
                                        ? QStringLiteral("LoggedADIF") : QStringLiteral("QSOLogged")},
        {QStringLiteral("id"), r.id},
    };

    switch (r.status) {
    case InsertResult::Status::Inserted: {
        item[QStringLiteral("status")] = QStringLiteral("logged");
        m_model->insertQso(r.id);
        const auto meta = m_db.meta(r.id);
        QString text = tr("%1 from %2 → %3 %4 %5 saved (uuid %6)")
                           .arg(item.value(QStringLiteral("message")).toString(), sourceApp, call,
                                item.value(QStringLiteral("band")).toString(), mode,
                                meta ? meta->uuid.left(4) + QStringLiteral("…") + meta->uuid.right(2) : QString());
        const bool newDxcc = mode == QLatin1String("FT2") && m_db.isFirstFt2Dxcc(r.id);
        if (newDxcc)
            text += tr(" · new DXCC on FT2: %1").arg(input.value(QStringLiteral("COUNTRY")).isEmpty()
                                                         ? input.value(QStringLiteral("DXCC"))
                                                         : input.value(QStringLiteral("COUNTRY")));
        item[QStringLiteral("newDxcc")] = newDxcc;
        addActivity(QStringLiteral("UDP"), text, newDxcc ? QStringLiteral("highlight") : QStringLiteral("success"));
        emit logChanged();
        break;
    }
    case InsertResult::Status::Duplicate:
        item[QStringLiteral("status")] = QStringLiteral("duplicate");
        addActivity(QStringLiteral("UDP"), tr("Duplicate ignored: %1").arg(r.message), QStringLiteral("warning"));
        break;
    case InsertResult::Status::Invalid:
    case InsertResult::Status::Error:
        item[QStringLiteral("status")] = QStringLiteral("error");
        addActivity(QStringLiteral("UDP"), tr("QSO not logged: %1").arg(r.message), QStringLiteral("error"));
        break;
    }

    m_incoming.prepend(item);
    while (m_incoming.size() > kMaxIncoming)
        m_incoming.removeLast();
    emit incomingChanged();

    if (call == m_lookupCall.toUpper())
        refreshCallInfo();
}

// ── QSO a mano ────────────────────────────────────────────────────────────────

QVariantMap DecoLogController::utcNow() const
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    return {{QStringLiteral("date"), now.toString(QStringLiteral("yyyy-MM-dd"))},
            {QStringLiteral("time"), now.toString(QStringLiteral("HH:mm"))}};
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
    if (!time.isValid())
        time = QTime::fromString(text("time"), QStringLiteral("HHmm"));
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
    r.set(QStringLiteral("TX_PWR"), text("tx_pwr"));
    r.set(QStringLiteral("POTA_REF"), text("pota_ref").toUpper());
    r.set(QStringLiteral("SOTA_REF"), text("sota_ref").toUpper());
    r.set(QStringLiteral("IOTA"), text("iota").toUpper());
    r.set(QStringLiteral("WWFF_REF"), text("wwff_ref").toUpper());
    r.set(QStringLiteral("COMMENT"), text("comment"));

    const qint64 profileId = m_profiles ? m_profiles->activeProfileId() : 0;
    applyProfile(r, profileId);

    const InsertResult res = m_db.insertQso(r, QStringLiteral("manual"), QStringLiteral("DecoLog ") + version(),
                                            true, profileId);
    switch (res.status) {
    case InsertResult::Status::Inserted:
        m_model->insertQso(res.id);
        addActivity(QStringLiteral("LOG"), tr("Logged %1 %2 %3 (manual)")
                                               .arg(r.value(QStringLiteral("CALL")), r.value(QStringLiteral("BAND")),
                                                    r.value(QStringLiteral("MODE"))),
                    QStringLiteral("success"));
        emit logChanged();
        setLookupCall(r.value(QStringLiteral("CALL")));
        refreshCallInfo();
        return {};
    case InsertResult::Status::Duplicate:
        return tr("Already in log (within %n minute(s))", nullptr, dedupManualMinutes());
    default:
        return res.message;
    }
}

// ── Scheda QSO ────────────────────────────────────────────────────────────────

QVariantMap DecoLogController::qsoDetail(qint64 id) const
{
    const auto record = m_db.record(id);
    const auto meta = m_db.meta(id);
    if (!record || !meta)
        return {};

    QVariantMap fields;
    QVariantList extra;
    for (const auto& f : record->fields()) {
        fields.insert(f.name, f.value);
        if (!kKnownFields.contains(f.name))
            extra << QVariantMap{{QStringLiteral("name"), f.name}, {QStringLiteral("value"), f.value}};
    }

    QVariantList qsl;
    const QList<QslState> states = m_db.qslStatus(id);
    for (const QString& service : kServices) {
        QslState st;
        st.service = service;
        for (const auto& s : states) {
            if (s.service == service)
                st = s;
        }
        qsl << QVariantMap{
            {QStringLiteral("service"), service},
            {QStringLiteral("label"), serviceLabel(service)},
            {QStringLiteral("sent"), st.sent},
            {QStringLiteral("sentDate"), st.sentDate},
            {QStringLiteral("rcvd"), st.rcvd},
            {QStringLiteral("rcvdDate"), st.rcvdDate},
            {QStringLiteral("lastError"), st.lastError},
            {QStringLiteral("hasRcvd"), service != QLatin1String("clublog")},
        };
    }

    QVariantList history;
    for (const HistoryEntry& h : m_db.history(id)) {
        history << QVariantMap{
            {QStringLiteral("id"), h.id},
            {QStringLiteral("revision"), h.revision},
            {QStringLiteral("reason"), h.reason},
            {QStringLiteral("recordedAt"), h.recordedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))},
            {QStringLiteral("summary"), QStringLiteral("%1 %2 %3 %4")
                                            .arg(h.record.value(QStringLiteral("CALL")),
                                                 h.record.value(QStringLiteral("BAND")),
                                                 h.record.value(QStringLiteral("SUBMODE")).isEmpty()
                                                     ? h.record.value(QStringLiteral("MODE"))
                                                     : h.record.value(QStringLiteral("SUBMODE")),
                                                 h.record.value(QStringLiteral("NAME")))},
        };
    }

    QVariantMap detail{
        {QStringLiteral("id"), id},
        {QStringLiteral("fields"), fields},
        {QStringLiteral("extra"), extra},
        {QStringLiteral("qsl"), qsl},
        {QStringLiteral("history"), history},
        {QStringLiteral("uuid"), meta->uuid},
        {QStringLiteral("revision"), meta->revision},
        {QStringLiteral("source"), meta->source},
        {QStringLiteral("sourceApp"), meta->sourceApp},
        {QStringLiteral("createdAt"), meta->createdAt},
        {QStringLiteral("updatedAt"), meta->updatedAt},
        {QStringLiteral("dirty"), meta->dirty},
        {QStringLiteral("stationProfileId"), meta->stationProfileId},
        {QStringLiteral("firstFt2Dxcc"), m_db.isFirstFt2Dxcc(id)},
    };

    const auto dx = maidenhead::toLatLon(record->value(QStringLiteral("GRIDSQUARE")));
    const QString ownGrid = record->value(QStringLiteral("MY_GRIDSQUARE")).isEmpty()
                                ? myGrid() : record->value(QStringLiteral("MY_GRIDSQUARE"));
    const auto me = maidenhead::toLatLon(ownGrid);
    if (dx && me) {
        detail[QStringLiteral("distanceKm")] = qRound(maidenhead::distanceKm(*me, *dx));
        detail[QStringLiteral("azimuth")] = qRound(maidenhead::azimuthDeg(*me, *dx));
    }
    if (dx || me) {
        const Ft2Award a = m_db.ft2Award();
        detail[QStringLiteral("ft2DxccWorked")] = a.dxccWorked;
    }
    return detail;
}

QString DecoLogController::saveQso(qint64 id, const QVariantMap& fields, qint64 stationProfileId)
{
    AdifRecord r;
    for (auto it = fields.cbegin(); it != fields.cend(); ++it)
        r.set(it.key(), it.value().toString().trimmed());
    const InsertResult res = m_db.updateQso(id, r, stationProfileId);
    if (res.status != InsertResult::Status::Inserted)
        return res.message.isEmpty() ? tr("Cannot save the QSO") : res.message;
    const auto meta = m_db.meta(id);
    addActivity(QStringLiteral("LOG"), tr("Edited %1 · revision %2").arg(r.value(QStringLiteral("CALL"))).arg(meta ? meta->revision : 0),
                QStringLiteral("success"));
    m_model->reload();
    emit logChanged();
    refreshCallInfo();
    return {};
}

bool DecoLogController::deleteQso(qint64 id)
{
    const auto record = m_db.record(id);
    if (!m_db.softDeleteQso(id))
        return false;
    addActivity(QStringLiteral("LOG"), tr("Deleted %1 (kept in history)").arg(record ? record->value(QStringLiteral("CALL")) : QString()),
                QStringLiteral("warning"));
    m_model->reload();
    emit logChanged();
    refreshCallInfo();
    return true;
}

QString DecoLogController::restoreRevision(qint64 id, qint64 historyId)
{
    const InsertResult res = m_db.restoreRevision(id, historyId);
    if (res.status != InsertResult::Status::Inserted)
        return res.message;
    addActivity(QStringLiteral("LOG"), tr("Restored an earlier revision of QSO #%1").arg(id), QStringLiteral("success"));
    m_model->reload();
    emit logChanged();
    refreshCallInfo();
    return {};
}

// ── Import, export, backup ────────────────────────────────────────────────────

void DecoLogController::importAdif(const QUrl& url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        addActivity(QStringLiteral("IMPORT"), tr("Cannot read %1: %2").arg(path, file.errorString()), QStringLiteral("error"));
        return;
    }
    const ImportResult r = m_db.importAdif(file.readAll(), QStringLiteral("import"),
                                           m_profiles ? m_profiles->activeProfileId() : 0);
    addActivity(QStringLiteral("IMPORT"), tr("%1: %2 new, %3 duplicates, %4 rejected")
                                              .arg(QFileInfo(path).fileName()).arg(r.inserted).arg(r.duplicates).arg(r.invalid),
                r.invalid ? QStringLiteral("warning") : QStringLiteral("success"));
    for (const QString& e : r.errors)
        addActivity(QStringLiteral("IMPORT"), QStringLiteral("  ") + e, QStringLiteral("warning"));
    m_model->reload();
    m_profiles->reload();
    emit logChanged();
    refreshCallInfo();
}

void DecoLogController::exportAdif(const QUrl& url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        addActivity(QStringLiteral("EXPORT"), tr("Cannot write %1: %2").arg(path, file.errorString()), QStringLiteral("error"));
        return;
    }
    file.write(m_db.exportAdif(version()));
    addActivity(QStringLiteral("EXPORT"), tr("%n QSO → %1", nullptr, m_db.qsoCount()).arg(path), QStringLiteral("success"));
}

void DecoLogController::exportQsos(const QVariantList& ids, const QUrl& url)
{
    QList<qint64> list;
    for (const auto& v : ids)
        list << v.toLongLong();
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        addActivity(QStringLiteral("EXPORT"), tr("Cannot write %1: %2").arg(path, file.errorString()), QStringLiteral("error"));
        return;
    }
    file.write(m_db.exportAdif(list, version()));
    addActivity(QStringLiteral("EXPORT"), tr("%n QSO → %1", nullptr, static_cast<int>(list.size())).arg(path),
                QStringLiteral("success"));
}

void DecoLogController::setBackupEnabled(bool enabled)
{
    if (enabled == m_backupEnabled)
        return;
    m_backupEnabled = enabled;
    QSettings().setValue(QStringLiteral("backup/enabled"), enabled);
    emit backupChanged();
}

void DecoLogController::setBackupDir(const QString& dir)
{
    if (dir == m_backupDir || dir.trimmed().isEmpty())
        return;
    m_backupDir = dir.trimmed();
    QSettings().setValue(QStringLiteral("backup/dir"), m_backupDir);
    emit backupChanged();
}

void DecoLogController::setBackupTime(const QString& hhmm)
{
    if (hhmm == m_backupTime || !QTime::fromString(hhmm, QStringLiteral("HH:mm")).isValid())
        return;
    m_backupTime = hhmm;
    QSettings().setValue(QStringLiteral("backup/time"), hhmm);
    emit backupChanged();
}

void DecoLogController::setBackupKeep(int keep)
{
    if (keep == m_backupKeep || keep < 1)
        return;
    m_backupKeep = keep;
    QSettings().setValue(QStringLiteral("backup/keep"), keep);
    emit backupChanged();
}

QString DecoLogController::lastBackup() const
{
    const QDateTime at = QDateTime::fromString(m_db.setting(QStringLiteral("backup.last_at")), Qt::ISODate);
    if (!at.isValid())
        return {};
    const QDateTime utc = at.toUTC();
    return utc.date() == QDateTime::currentDateTimeUtc().date()
               ? utc.toString(QStringLiteral("HH:mm")) + QStringLiteral("Z")
               : utc.toString(QStringLiteral("MM-dd HH:mm")) + QStringLiteral("Z");
}

QString DecoLogController::lastBackupInfo() const
{
    const QString file = m_db.setting(QStringLiteral("backup.last_file"));
    if (file.isEmpty())
        return {};
    const QFileInfo info(file);
    return tr("%1 · %2 MB").arg(info.fileName()).arg(QString::number(info.size() / 1048576.0, 'f', 1));
}

void DecoLogController::backupNow()
{
    QDir().mkpath(m_backupDir);
    const QString name = QStringLiteral("decolog-%1.sqlite")
                             .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddTHHmm")));
    const QString path = QDir(m_backupDir).filePath(name);
    if (!m_db.backupTo(path)) {
        addActivity(QStringLiteral("BACKUP"), tr("Backup failed: %1").arg(m_db.lastError()), QStringLiteral("error"));
        return;
    }
    m_db.setSetting(QStringLiteral("backup.last_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    m_db.setSetting(QStringLiteral("backup.last_file"), path);

    // Solo le copie fatte da DecoLog, dalla piu' recente: le altre non si toccano.
    QFileInfoList copies = QDir(m_backupDir).entryInfoList({QStringLiteral("decolog-*.sqlite")}, QDir::Files, QDir::Name | QDir::Reversed);
    for (qsizetype i = m_backupKeep; i < copies.size(); ++i)
        QFile::remove(copies.at(i).absoluteFilePath());

    addActivity(QStringLiteral("BACKUP"), tr("%1 → %2 (%3 MB)")
                                              .arg(QFileInfo(m_db.path()).fileName(), path,
                                                   QString::number(QFileInfo(path).size() / 1048576.0, 'f', 1)));
    emit backupChanged();
}

void DecoLogController::checkBackupSchedule()
{
    if (!m_backupEnabled || !m_db.isOpen())
        return;
    const QTime when = QTime::fromString(m_backupTime, QStringLiteral("HH:mm"));
    const QDateTime now = QDateTime::currentDateTime();
    if (!when.isValid() || now.time() < when)
        return;
    const QDateTime last = QDateTime::fromString(m_db.setting(QStringLiteral("backup.last_at")), Qt::ISODate).toLocalTime();
    // Una copia al giorno, alla prima occasione dopo l'ora scelta: se il PC era
    // spento alle 02:00, la copia si fa appena DecoLog e' aperto.
    if (last.isValid() && QDateTime(now.date(), when) <= last)
        return;
    backupNow();
}

void DecoLogController::openDatabaseFolder() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_db.path()).absolutePath()));
}

// ── Cloud ─────────────────────────────────────────────────────────────────────

void DecoLogController::setCloudServer(const QString& url)
{
    if (url == m_cloudServer)
        return;
    m_cloudServer = url.trimmed();
    QSettings().setValue(QStringLiteral("cloud/server"), m_cloudServer);
    emit cloudChanged();
}

void DecoLogController::setAutoSync(const QString& mode)
{
    if (mode == m_autoSync)
        return;
    m_autoSync = mode;
    QSettings().setValue(QStringLiteral("cloud/autoSync"), mode);
    emit cloudChanged();
}

void DecoLogController::setConflictPolicy(const QString& policy)
{
    if (policy == m_conflictPolicy)
        return;
    m_conflictPolicy = policy;
    QSettings().setValue(QStringLiteral("cloud/conflictPolicy"), policy);
    emit cloudChanged();
}

// ── Statistiche e pannello del nominativo ─────────────────────────────────────

QVariantMap DecoLogController::ft2Award() const
{
    const Ft2Award a = m_db.ft2Award();
    return {
        {QStringLiteral("qsos"), a.qsos},
        {QStringLiteral("dxccWorked"), a.dxccWorked},
        {QStringLiteral("dxccConfirmed"), a.dxccConfirmed},
        {QStringLiteral("gridsWorked"), a.gridsWorked},
        {QStringLiteral("gridsConfirmed"), a.gridsConfirmed},
    };
}

QVariantList DecoLogController::bandStats() const
{
    QVariantList out;
    for (const auto& row : m_db.countByBand())
        out << QVariantMap{{QStringLiteral("key"), row.key}, {QStringLiteral("count"), row.count}};
    return out;
}

QVariantList DecoLogController::modeStats() const
{
    QVariantList out;
    for (const auto& row : m_db.countByMode())
        out << QVariantMap{{QStringLiteral("key"), row.key}, {QStringLiteral("count"), row.count}};
    return out;
}

QVariantList DecoLogController::qslSummary() const
{
    QVariantList out;
    for (QVariantMap row : m_db.qslSummary()) {
        row[QStringLiteral("label")] = serviceLabel(row.value(QStringLiteral("service")).toString());
        out << row;
    }
    return out;
}

QVariantList DecoLogController::gridPoints() const
{
    QVariantList out;
    for (const QString& grid : m_db.workedGrids()) {
        if (const auto p = maidenhead::toLatLon(grid))
            out << positionMap(p);
    }
    return out;
}

void DecoLogController::setLookupCall(const QString& call)
{
    const QString c = call.trimmed().toUpper();
    if (c == m_lookupCall)
        return;
    m_lookupCall = c;
    refreshCallInfo();
}

void DecoLogController::refreshCallInfo()
{
    const WorkedBefore wb = m_db.workedBefore(m_lookupCall);
    QVariantList recent;
    for (const WorkedEntry& e : wb.recent) {
        recent << QVariantMap{
            {QStringLiteral("date"), e.on.toString(QStringLiteral("yyyy-MM-dd"))},
            {QStringLiteral("band"), e.band},
            {QStringLiteral("mode"), e.mode},
            {QStringLiteral("lotw"), e.lotwRcvd == QLatin1String("Y")},
        };
    }

    QVariantMap info{
        {QStringLiteral("call"), m_lookupCall},
        {QStringLiteral("count"), wb.count},
        {QStringLiteral("bands"), wb.bands},
        {QStringLiteral("modes"), wb.modes},
        {QStringLiteral("last"), wb.last.isValid() ? wb.last.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QString()},
        {QStringLiteral("lastBand"), wb.lastBand},
        {QStringLiteral("lastMode"), wb.lastMode},
        {QStringLiteral("lastId"), wb.lastId},
        {QStringLiteral("name"), wb.name},
        {QStringLiteral("qth"), wb.qth},
        {QStringLiteral("gridsquare"), wb.gridsquare},
        {QStringLiteral("country"), wb.country},
        {QStringLiteral("dxcc"), wb.dxcc},
        {QStringLiteral("cqz"), wb.cqz},
        {QStringLiteral("ituz"), wb.ituz},
        {QStringLiteral("recent"), recent},
        {QStringLiteral("workedFt2"), wb.modes.contains(QStringLiteral("FT2"))},
    };

    const auto dx = maidenhead::toLatLon(wb.gridsquare);
    const auto me = maidenhead::toLatLon(myGrid());
    if (dx) {
        info[QStringLiteral("position")] = positionMap(dx);
        // Ora locale approssimata dal fuso "solare": basta per capire se dall'altra
        // parte e' notte fonda.
        info[QStringLiteral("utcOffsetHours")] = static_cast<int>(std::lround(dx->lon / 15.0));
    }
    if (dx && me) {
        info[QStringLiteral("distanceKm")] = qRound(maidenhead::distanceKm(*me, *dx));
        info[QStringLiteral("azimuth")] = qRound(maidenhead::azimuthDeg(*me, *dx));
    }

    if (wb.lastId > 0) {
        QVariantList qsl;
        const QList<QslState> states = m_db.qslStatus(wb.lastId);
        for (const QString& service : kServices) {
            QString sent = QStringLiteral("N"), rcvd = QStringLiteral("N");
            for (const auto& s : states) {
                if (s.service == service) {
                    sent = s.sent;
                    rcvd = s.rcvd;
                }
            }
            qsl << QVariantMap{{QStringLiteral("label"), serviceLabel(service)},
                               {QStringLiteral("sent"), sent},
                               {QStringLiteral("rcvd"), rcvd}};
        }
        info[QStringLiteral("qsl")] = qsl;
    }
    m_callInfo = info;
    emit lookupChanged();
}

void DecoLogController::addActivity(const QString& category, const QString& text, const QString& level)
{
    m_activity.prepend(QVariantMap{
        {QStringLiteral("time"), nowUtcLabel()},
        {QStringLiteral("category"), category},
        {QStringLiteral("text"), text},
        {QStringLiteral("level"), level},
    });
    while (m_activity.size() > kMaxActivity)
        m_activity.removeLast();
    emit activityChanged();
}

void DecoLogController::clearActivity()
{
    m_activity.clear();
    emit activityChanged();
}

} // namespace decolog::app
