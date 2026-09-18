#include "core/RotorLink.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QWebSocket>
#include <cmath>

namespace decolog::core {

QVariantMap RotorState::toMap() const
{
    return QVariantMap{
        {QStringLiteral("connected"), connected},
        {QStringLiteral("hasAz"), hasAz},
        {QStringLiteral("hasEl"), hasEl},
        {QStringLiteral("moving"), moving},
        {QStringLiteral("az"), az},
        {QStringLiteral("el"), el},
        {QStringLiteral("azTarget"), azTarget},
        {QStringLiteral("elTarget"), elTarget},
        {QStringLiteral("model"), model},
        {QStringLiteral("modelLabel"), modelLabel},
        {QStringLiteral("port"), port},
        {QStringLiteral("error"), error},
        {QStringLiteral("updated"), updated.isValid() ? updated.toString(Qt::ISODate) : QString()},
    };
}

namespace rotor {

double normalize(double degrees)
{
    double value = std::fmod(degrees, 360.0);
    if (value < 0.0)
        value += 360.0;
    return value;
}

RotorState parseState(const QJsonObject& o)
{
    RotorState s;
    s.connected = o.value(QStringLiteral("connected")).toBool();
    s.hasAz = o.value(QStringLiteral("has_az")).toBool(true);
    s.hasEl = o.value(QStringLiteral("has_el")).toBool(false);
    s.moving = o.value(QStringLiteral("moving")).toBool();
    s.az = o.value(QStringLiteral("az")).toDouble();
    s.el = o.value(QStringLiteral("el")).toDouble();
    // I bersagli sono null quando non c'e' niente in corso.
    const auto azTarget = o.value(QStringLiteral("az_target"));
    const auto elTarget = o.value(QStringLiteral("el_target"));
    s.azTarget = azTarget.isDouble() ? azTarget.toDouble() : -1.0;
    s.elTarget = elTarget.isDouble() ? elTarget.toDouble() : -1.0;
    s.model = o.value(QStringLiteral("model")).toString();
    s.modelLabel = o.value(QStringLiteral("model_label")).toString();
    s.port = o.value(QStringLiteral("port")).toString();
    s.error = o.value(QStringLiteral("error")).toString();
    s.updated = QDateTime::currentDateTimeUtc();
    return s;
}

bool parsePosition(const QString& reply, double* az, double* el)
{
    // rotctld risponde con due righe di numeri, o con "RPRT -n" se qualcosa non va.
    if (reply.contains(QLatin1String("RPRT")))
        return false;
    const QStringList lines = reply.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.isEmpty())
        return false;
    bool ok = false;
    // La forma lunga ("Azimuth: 128.40") si accetta lo stesso.
    auto number = [&ok](const QString& line) {
        const QString text = line.contains(QLatin1Char(':')) ? line.section(QLatin1Char(':'), 1).trimmed()
                                                             : line.trimmed();
        return text.toDouble(&ok);
    };
    const double a = number(lines.first());
    if (!ok)
        return false;
    if (az)
        *az = a;
    if (el && lines.size() > 1) {
        const double e = number(lines.at(1));
        if (ok)
            *el = e;
        ok = true;
    }
    return true;
}

} // namespace rotor

// ── Collegamento ──────────────────────────────────────────────────────────────

RotorLink::RotorLink(QObject* parent)
    : QObject(parent)
{
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, [this] {
        if (!m_running)
            return;
        if (m_backend == Backend::DecoRotor)
            openDecoRotor();
        else
            openRotctld();
    });

    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (m_tcp && m_tcp->state() == QAbstractSocket::ConnectedState)
            sendRotctld(QStringLiteral("p"));
    });
}

RotorLink::~RotorLink()
{
    stop();
}

void RotorLink::start(Backend backend, const QString& host, int port, const QString& token)
{
    stop();
    m_backend = backend;
    m_host = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    m_port = port > 0 ? port : (backend == Backend::DecoRotor ? 8765 : 4532);
    m_token = token;
    m_running = true;
    m_attempts = 0;
    if (backend == Backend::DecoRotor)
        openDecoRotor();
    else
        openRotctld();
}

void RotorLink::stop()
{
    m_running = false;
    m_retry.stop();
    m_poll.stop();
    if (m_ws) {
        m_ws->abort();
        m_ws->deleteLater();
        m_ws = nullptr;
    }
    if (m_tcp) {
        m_tcp->abort();
        m_tcp->deleteLater();
        m_tcp = nullptr;
    }
    if (m_state.connected || !m_state.error.isEmpty()) {
        m_state = RotorState{};
        emit stateChanged();
    }
}

void RotorLink::retryLater()
{
    if (!m_running)
        return;
    // Un rotore che non risponde non e' un'emergenza: si riprova piano, fino a
    // mezzo minuto, senza riempire il registro di righe uguali.
    ++m_attempts;
    m_retry.start(qMin(30'000, 2'000 * m_attempts));
}

void RotorLink::openDecoRotor()
{
    if (m_ws) {
        m_ws->abort();
        m_ws->deleteLater();
    }
    m_ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(m_ws, &QWebSocket::connected, this, [this] {
        m_attempts = 0;
        emit note(tr("Rotor: connected to DecoRotor on %1:%2").arg(m_host).arg(m_port), QStringLiteral("success"));
        // Lo stato arriva da solo a ogni giro di polling, ma il primo si chiede.
        sendJson({{QStringLiteral("cmd"), QStringLiteral("state")}});
        if (m_pendingAz >= 0.0) {
            const double target = m_pendingAz;
            m_pendingAz = -1.0;
            goTo(target);
        }
    });
    connect(m_ws, &QWebSocket::textMessageReceived, this, &RotorLink::handleJson);
    connect(m_ws, &QWebSocket::disconnected, this, [this] {
        if (m_state.connected) {
            m_state = RotorState{};
            emit stateChanged();
        }
        retryLater();
    });
    connect(m_ws, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (m_attempts <= 1 && m_ws)
            emit note(tr("Rotor: %1").arg(m_ws->errorString()), QStringLiteral("warning"));
        retryLater();
    });

    QUrl url;
    url.setScheme(QStringLiteral("ws"));
    url.setHost(m_host);
    url.setPort(m_port);
    url.setPath(QStringLiteral("/"));
    if (!m_token.isEmpty())
        url.setQuery(QStringLiteral("token=%1").arg(m_token));
    m_ws->open(url);
}

void RotorLink::openRotctld()
{
    if (m_tcp) {
        m_tcp->abort();
        m_tcp->deleteLater();
    }
    m_buffer.clear();
    m_tcp = new QTcpSocket(this);

    connect(m_tcp, &QTcpSocket::connected, this, [this] {
        m_attempts = 0;
        m_state.connected = true;
        m_state.model = QStringLiteral("rotctld");
        m_state.modelLabel = tr("rotctld (Hamlib)");
        emit note(tr("Rotor: connected to rotctld on %1:%2").arg(m_host).arg(m_port), QStringLiteral("success"));
        emit stateChanged();
        sendRotctld(QStringLiteral("p"));
        m_poll.start();
        if (m_pendingAz >= 0.0) {
            const double target = m_pendingAz;
            m_pendingAz = -1.0;
            goTo(target);
        }
    });
    connect(m_tcp, &QTcpSocket::readyRead, this, &RotorLink::handleRotctld);
    connect(m_tcp, &QTcpSocket::disconnected, this, [this] {
        m_poll.stop();
        if (m_state.connected) {
            m_state = RotorState{};
            emit stateChanged();
        }
        retryLater();
    });
    connect(m_tcp, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (m_attempts <= 1 && m_tcp)
            emit note(tr("Rotor: %1").arg(m_tcp->errorString()), QStringLiteral("warning"));
        m_poll.stop();
        retryLater();
    });
    m_tcp->connectToHost(m_host, static_cast<quint16>(m_port));
}

void RotorLink::sendJson(const QVariantMap& command)
{
    if (!m_ws || m_ws->state() != QAbstractSocket::ConnectedState)
        return;
    m_ws->sendTextMessage(QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(command)).toJson(QJsonDocument::Compact)));
}

void RotorLink::sendRotctld(const QString& line)
{
    if (!m_tcp || m_tcp->state() != QAbstractSocket::ConnectedState)
        return;
    m_tcp->write(line.toLatin1() + '\n');
}

void RotorLink::handleJson(const QString& message)
{
    const QJsonObject o = QJsonDocument::fromJson(message.toUtf8()).object();
    const QString type = o.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("state")) {
        m_state = rotor::parseState(o);
        emit stateChanged();
        return;
    }
    if (type == QLatin1String("error")) {
        m_state.error = o.value(QStringLiteral("message")).toString();
        emit note(tr("Rotor: %1").arg(m_state.error), QStringLiteral("warning"));
        emit stateChanged();
        return;
    }
    if (type == QLatin1String("hello")) {
        // Niente da fare: lo stato arriva subito dopo.
        return;
    }
}

void RotorLink::handleRotctld()
{
    if (!m_tcp)
        return;
    m_buffer += m_tcp->readAll();
    // Le risposte sono corte: si legge quello che c'e' e si riparte pulito.
    const QString reply = QString::fromLatin1(m_buffer);
    if (!reply.endsWith(QLatin1Char('\n')))
        return;
    m_buffer.clear();

    double az = m_state.az;
    double el = m_state.el;
    if (rotor::parsePosition(reply, &az, &el)) {
        const bool moving = std::abs(az - m_state.az) > 0.05;
        m_state.az = az;
        m_state.el = el;
        m_state.hasEl = reply.split(QLatin1Char('\n'), Qt::SkipEmptyParts).size() > 1;
        m_state.moving = moving;
        m_state.updated = QDateTime::currentDateTimeUtc();
        m_state.error.clear();
        emit stateChanged();
    } else if (reply.contains(QLatin1String("RPRT")) && !reply.contains(QLatin1String("RPRT 0"))) {
        m_state.error = reply.trimmed();
        emit stateChanged();
    }
}

// ── Comandi ───────────────────────────────────────────────────────────────────

void RotorLink::goTo(double az, double el)
{
    const double target = rotor::normalize(az);
    const bool ready = m_backend == Backend::DecoRotor
        ? (m_ws && m_ws->state() == QAbstractSocket::ConnectedState)
        : (m_tcp && m_tcp->state() == QAbstractSocket::ConnectedState);
    if (!ready) {
        // Il gateway sta ancora arrivando: il puntamento non si perde, parte
        // appena c'e'.
        m_pendingAz = target;
        m_state.azTarget = target;
        emit stateChanged();
        return;
    }
    if (m_backend == Backend::DecoRotor) {
        QVariantMap command{{QStringLiteral("cmd"), QStringLiteral("goto")},
                            {QStringLiteral("az"), target}};
        if (el >= 0.0)
            command.insert(QStringLiteral("el"), el);
        sendJson(command);
    } else {
        sendRotctld(QStringLiteral("P %1 %2").arg(target, 0, 'f', 1).arg(el >= 0.0 ? el : m_state.el, 0, 'f', 1));
    }
    // Il bersaglio si mostra subito: la conferma arriva col giro dopo.
    m_state.azTarget = target;
    emit stateChanged();
}

void RotorLink::goToLocator(const QString& locator, bool longPath)
{
    if (m_backend != Backend::DecoRotor)
        return;
    sendJson({{QStringLiteral("cmd"), QStringLiteral("goto_locator")},
              {QStringLiteral("locator"), locator.trimmed().toUpper()},
              {QStringLiteral("long_path"), longPath}});
}

void RotorLink::halt(bool fast)
{
    if (m_backend == Backend::DecoRotor) {
        sendJson({{QStringLiteral("cmd"), QStringLiteral("stop")},
                  {QStringLiteral("axis"), QStringLiteral("all")},
                  {QStringLiteral("fast"), fast}});
    } else {
        sendRotctld(QStringLiteral("S"));
    }
    m_state.azTarget = -1.0;
    emit stateChanged();
}

void RotorLink::park()
{
    if (m_backend == Backend::DecoRotor)
        sendJson({{QStringLiteral("cmd"), QStringLiteral("park")}});
    else
        sendRotctld(QStringLiteral("K"));
}

} // namespace decolog::core
