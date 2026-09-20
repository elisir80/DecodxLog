#include "app/RotorController.h"

#include <QSettings>
#include <cmath>

namespace decolog::app {

using namespace decolog::core;

namespace {

int defaultPortFor(const QString& backend)
{
    // DecoRotor: WebSocket 8765. rotctld: 4532, perche' sulla 4533 e 4534 ci
    // sono gia' il CAT share e lo spot share di Decodium.
    return backend == QLatin1String("rotctld") ? 4532 : 8765;
}

} // namespace

RotorController::RotorController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    QSettings s;
    m_enabled = s.value(QStringLiteral("rotor/enabled"), false).toBool();
    m_backend = s.value(QStringLiteral("rotor/backend"), QStringLiteral("decorotor")).toString();
    m_host = s.value(QStringLiteral("rotor/host"), QStringLiteral("127.0.0.1")).toString();
    m_port = s.value(QStringLiteral("rotor/port"), defaultPortFor(m_backend)).toInt();
    m_followDx = s.value(QStringLiteral("rotor/followDx"), false).toBool();
    m_beamwidth = qBound(5, s.value(QStringLiteral("rotor/beamwidth"), 45).toInt(), 180);

    m_httpPort = qBound(1, s.value(QStringLiteral("rotor/httpPort"), 8080).toInt(), 65535);

    connect(&m_link, &RotorLink::stateChanged, this, [this] {
        // Il verso di rotazione non lo dice il gateway: si legge da come
        // cambia l'azimut, come fa il posto di comando.
        const core::RotorState& s = m_link.state();
        if (!s.moving) {
            m_sense = 0;
        } else if (m_lastAz >= 0.0) {
            double delta = s.az - m_lastAz;
            while (delta > 180.0) delta -= 360.0;
            while (delta < -180.0) delta += 360.0;
            if (qAbs(delta) > 0.05)
                m_sense = delta > 0 ? 1 : -1;
        }
        m_lastAz = s.az;
        emit stateChanged();
    });
    connect(&m_link, &RotorLink::presetsChanged, this, &RotorController::presetsChanged);
    connect(&m_link, &RotorLink::trafficChanged, this, &RotorController::trafficChanged);
    connect(&m_link, &RotorLink::historyChanged, this, &RotorController::historyChanged);
    connect(&m_link, &RotorLink::bearingReady, this, [this](const QVariantMap& bearing) {
        m_bearing = bearing;
        emit bearingChanged();
    });
    connect(&m_link, &RotorLink::note, this, [this](const QString& text, const QString& level) {
        note(text, level);
    });
}

void RotorController::start()
{
    if (m_enabled)
        apply();
}

void RotorController::overrideConnection(const QString& backend, const QString& host, int port)
{
    m_backend = backend == QLatin1String("rotctld") ? QStringLiteral("rotctld") : QStringLiteral("decorotor");
    if (!host.trimmed().isEmpty())
        m_host = host.trimmed();
    if (port > 0)
        m_port = port;
    m_enabled = true;
    apply();
    emit stateChanged();
}

void RotorController::apply()
{
    if (!m_enabled) {
        m_link.stop();
        emit changed();
        emit stateChanged();
        return;
    }
    m_link.start(m_backend == QLatin1String("rotctld") ? RotorLink::Backend::Rotctld
                                                       : RotorLink::Backend::DecoRotor,
                 m_host, m_port, QSettings().value(QStringLiteral("rotor/token")).toString());
    emit changed();
    emit stateChanged();
}

void RotorController::note(const QString& text, const QString& level)
{
    if (m_ctx.activity)
        m_ctx.activity(QStringLiteral("ROTOR"), text, level);
}

// ── Impostazioni ──────────────────────────────────────────────────────────────

void RotorController::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    QSettings().setValue(QStringLiteral("rotor/enabled"), enabled);
    apply();
}

void RotorController::setBackend(const QString& backend)
{
    const QString value = backend == QLatin1String("rotctld") ? QStringLiteral("rotctld")
                                                              : QStringLiteral("decorotor");
    if (value == m_backend)
        return;
    // Cambiando modo cambia anche la porta solita: si sposta, a meno che non sia
    // stata scelta a mano una diversa da tutte e due le predefinite.
    const bool defaultPort = m_port == defaultPortFor(m_backend);
    m_backend = value;
    if (defaultPort)
        m_port = defaultPortFor(value);
    QSettings s;
    s.setValue(QStringLiteral("rotor/backend"), m_backend);
    s.setValue(QStringLiteral("rotor/port"), m_port);
    apply();
}

void RotorController::setHost(const QString& host)
{
    const QString value = host.trimmed();
    if (value == m_host)
        return;
    m_host = value;
    QSettings().setValue(QStringLiteral("rotor/host"), value);
    apply();
}

void RotorController::setPort(int port)
{
    if (port <= 0 || port > 65535 || port == m_port)
        return;
    m_port = port;
    QSettings().setValue(QStringLiteral("rotor/port"), port);
    apply();
}

void RotorController::setHttpPort(int port)
{
    const int value = qBound(1, port, 65535);
    if (value == m_httpPort)
        return;
    m_httpPort = value;
    QSettings().setValue(QStringLiteral("rotor/httpPort"), value);
    emit changed();
}

QString RotorController::tileEndpoint() const
{
    // Con DecoRotor in piedi i riquadri arrivano da lui; con rotctld non c'e'
    // nessun gateway, e la mappa si arrangia con quella stradale.
    if (!m_enabled || m_backend != QLatin1String("decorotor"))
        return {};
    return QStringLiteral("http://%1:%2/tiles/").arg(m_host).arg(m_httpPort);
}

QString RotorController::uptimeText() const
{
    const qint64 seconds = static_cast<qint64>(m_link.state().uptime);
    if (seconds < 60)
        return tr("%1 s").arg(seconds);
    if (seconds < 3600)
        return tr("%1 m").arg(seconds / 60);
    return tr("%1 h %2 m").arg(seconds / 3600).arg((seconds % 3600) / 60);
}

QVariantList RotorController::endpoints() const
{
    // Le tre porte le serve il gateway: se risponde lui, ci sono tutte.
    const bool up = m_link.state().linkUp;
    const bool deco = m_backend == QLatin1String("decorotor");
    return QVariantList{
        QVariantMap{{QStringLiteral("role"), tr("APP (WebSocket)")},
                    {QStringLiteral("address"), QStringLiteral("%1:%2").arg(m_host).arg(deco ? m_port : 8765)},
                    {QStringLiteral("active"), up && deco}},
        QVariantMap{{QStringLiteral("role"), tr("WEB UI")},
                    {QStringLiteral("address"), QStringLiteral("%1:%2").arg(m_host).arg(m_httpPort)},
                    {QStringLiteral("active"), up && deco}},
        QVariantMap{{QStringLiteral("role"), tr("ROTCTLD (Hamlib)")},
                    {QStringLiteral("address"), QStringLiteral("%1:%2").arg(m_host).arg(deco ? 4532 : m_port)},
                    {QStringLiteral("active"), up}},
    };
}

void RotorController::refreshDiagnostics()
{
    if (!m_enabled)
        return;
    m_link.requestTraffic(60);
    m_link.requestHistory(300);
}

void RotorController::setSetting(const QString& key, const QVariant& value)
{
    if (!m_enabled || key.isEmpty())
        return;
    m_link.setConfig(QVariantMap{{key, value}});
    note(tr("Rotor: %1 set on the gateway").arg(key), QStringLiteral("info"));
}

void RotorController::setLimit(const QString& key, double value)
{
    if (!m_enabled || key.isEmpty())
        return;
    // I finecorsa vanno mandati insieme: il gateway vuole l'oggetto intero.
    const core::RotorState& s = m_link.state();
    QVariantMap limits{{QStringLiteral("az_min"), s.azMin}, {QStringLiteral("az_max"), s.azMax}};
    limits.insert(key, value);
    m_link.setConfig(QVariantMap{{QStringLiteral("limits"), limits}});
}

void RotorController::recallPreset(const QString& name)
{
    if (!m_enabled)
        return;
    m_link.recallPreset(name);
    m_lastTarget = name;
    note(tr("Rotor to %1").arg(name), QStringLiteral("info"));
    emit stateChanged();
}

void RotorController::savePresetHere(const QString& name)
{
    if (!m_enabled || name.trimmed().isEmpty())
        return;
    m_link.savePreset(name, m_link.state().az, -1.0);
    note(tr("Rotor: memory \"%1\" at %2°").arg(name.trimmed()).arg(qRound(m_link.state().az)),
         QStringLiteral("info"));
}

void RotorController::deletePreset(const QString& name)
{
    if (m_enabled)
        m_link.deletePreset(name);
}

void RotorController::askBearing(const QString& locator)
{
    if (m_enabled)
        m_link.requestBearing(locator);
}

void RotorController::gotoPosition(double az, double el)
{
    if (!m_enabled)
        return;
    if (az >= 0.0)
        pointTo(az, QString());
    else if (el >= 0.0)
        m_link.goTo(m_link.state().az, el);
}

void RotorController::setFollowDx(bool follow)
{
    if (follow == m_followDx)
        return;
    m_followDx = follow;
    m_followedCall.clear();
    QSettings().setValue(QStringLiteral("rotor/followDx"), follow);
    emit changed();
}

void RotorController::setBeamwidth(int degrees)
{
    const int value = qBound(5, degrees, 180);
    if (value == m_beamwidth)
        return;
    m_beamwidth = value;
    QSettings().setValue(QStringLiteral("rotor/beamwidth"), value);
    emit changed();
}

// ── Stato ─────────────────────────────────────────────────────────────────────

QVariantMap RotorController::state() const
{
    QVariantMap map = m_link.state().toMap();
    map.insert(QStringLiteral("enabled"), m_enabled);
    // Il lobo lo dice il gateway; quello delle impostazioni di DecoDXLog serve
    // solo quando dall'altra parte c'e' un rotctld, che non lo sa.
    if (!m_link.state().beamwidthKnown)
        map.insert(QStringLiteral("beamwidth"), m_beamwidth);
    map.insert(QStringLiteral("backend"), m_backend);
    return map;
}

QString RotorController::status() const
{
    if (!m_enabled)
        return tr("Rotor off");
    const RotorState& s = m_link.state();
    if (!s.connected) {
        return m_backend == QLatin1String("rotctld")
            ? tr("Looking for rotctld on %1:%2…").arg(m_host).arg(m_port)
            : tr("Looking for DecoRotor on %1:%2…").arg(m_host).arg(m_port);
    }
    if (!s.error.isEmpty())
        return s.error;
    if (s.moving && s.azTarget >= 0.0)
        return tr("Turning to %1°").arg(qRound(s.azTarget));
    return s.modelLabel.isEmpty() ? tr("Rotor connected") : s.modelLabel;
}

// ── Comandi ───────────────────────────────────────────────────────────────────

void RotorController::pointTo(double azimuth, const QString& what)
{
    if (!m_enabled) {
        note(tr("The rotor is off: Setup → Rotor"), QStringLiteral("warning"));
        return;
    }
    const double target = rotor::normalize(azimuth);
    m_link.goTo(target);
    m_lastTarget = what.trimmed().isEmpty() ? tr("%1°").arg(qRound(target))
                                            : tr("%1 · %2°").arg(what.trimmed()).arg(qRound(target));
    note(tr("Rotor to %1").arg(m_lastTarget), QStringLiteral("info"));
    emit stateChanged();
}

void RotorController::pointLocator(const QString& locator, bool longPath)
{
    if (!m_enabled || locator.trimmed().size() < 4)
        return;
    if (m_backend == QLatin1String("rotctld")) {
        note(tr("rotctld does not do locators: point in degrees"), QStringLiteral("warning"));
        return;
    }
    m_link.goToLocator(locator, longPath);
    m_lastTarget = locator.trimmed().toUpper();
    note(tr("Rotor to %1").arg(m_lastTarget), QStringLiteral("info"));
    emit stateChanged();
}

void RotorController::stopNow(bool fast)
{
    if (!m_enabled)
        return;
    m_link.halt(fast);
    note(fast ? tr("Rotor: quick stop") : tr("Rotor: stop"), QStringLiteral("warning"));
}

void RotorController::park()
{
    if (!m_enabled)
        return;
    m_link.park();
    note(tr("Rotor: park"), QStringLiteral("info"));
}

void RotorController::nudge(double degrees)
{
    if (!m_enabled || !m_link.state().connected)
        return;
    const RotorState& s = m_link.state();
    const double from = s.azTarget >= 0.0 ? s.azTarget : s.az;
    m_link.goTo(rotor::normalize(from + degrees));
}

void RotorController::reconnect()
{
    if (m_enabled)
        apply();
}

void RotorController::dxBearing(const QString& call, double azimuth)
{
    if (!m_enabled || !m_followDx || azimuth < 0.0)
        return;
    const QString who = call.trimmed().toUpper();
    if (who.isEmpty() || who == m_followedCall)
        return;
    m_followedCall = who;
    pointTo(azimuth, who);
}

} // namespace decolog::app
