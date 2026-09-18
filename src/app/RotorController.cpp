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

    connect(&m_link, &RotorLink::stateChanged, this, &RotorController::stateChanged);
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
