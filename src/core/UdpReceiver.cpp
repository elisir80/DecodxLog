#include "core/UdpReceiver.h"

#include "core/Bands.h"

#include <QNetworkDatagram>
#include <QRegularExpression>
#include <QUdpSocket>

namespace decolog::core {

UdpReceiver::UdpReceiver(QObject* parent)
    : QObject(parent)
{
}

UdpReceiver::~UdpReceiver() = default;

bool UdpReceiver::start(quint16 port, const QHostAddress& multicastGroup)
{
    stop();
    if (port == 0)
        return false;

    m_socket = new QUdpSocket(this);
    const bool multicast = !multicastGroup.isNull() && multicastGroup.isMulticast();
    // ShareAddress: se un altro programma ascolta in multicast sulla stessa
    // porta, entrambi ricevono. In unicast il sistema consegna comunque a uno
    // solo dei due, ed e' un limite del protocollo, non di DecoDXLog.
    const auto mode = QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint;
    const QHostAddress bindAddress = multicast ? QHostAddress(QHostAddress::AnyIPv4)
                                               : QHostAddress(QHostAddress::Any);
    if (!m_socket->bind(bindAddress, port, mode)) {
        m_lastError = m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        emit listeningChanged();
        return false;
    }
    if (multicast && !m_socket->joinMulticastGroup(multicastGroup)) {
        m_lastError = m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        emit listeningChanged();
        return false;
    }

    m_port = port;
    m_lastError.clear();
    connect(m_socket, &QUdpSocket::readyRead, this, &UdpReceiver::readPending);
    emit listeningChanged();
    return true;
}

void UdpReceiver::stop()
{
    if (!m_socket)
        return;
    delete m_socket;
    m_socket = nullptr;
    m_port = 0;
    emit listeningChanged();
}

bool UdpReceiver::isListening() const
{
    return m_socket && m_socket->state() == QAbstractSocket::BoundState;
}

namespace {

// Il socket e' in doppio stack: 127.0.0.1 arriva come ::ffff:127.0.0.1.
QHostAddress plain(const QHostAddress& address)
{
    bool isV4 = false;
    const quint32 v4 = address.toIPv4Address(&isV4);
    return isV4 ? QHostAddress(v4) : address;
}

} // namespace

QList<UdpReceiver::Target> UdpReceiver::parseTargets(const QString& text, QStringList* rejected)
{
    QList<Target> out;
    for (QString item : text.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts)) {
        item = item.trimmed();
        const qsizetype colon = item.lastIndexOf(QLatin1Char(':'));
        bool ok = false;
        const int port = colon > 0 ? item.mid(colon + 1).toInt(&ok) : 0;
        QString host = colon > 0 ? item.left(colon) : QString();
        if (host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0)
            host = QStringLiteral("127.0.0.1");
        host.remove(QLatin1Char('[')).remove(QLatin1Char(']'));
        const QHostAddress address(host);
        if (!ok || port <= 0 || port > 65535 || address.isNull()) {
            if (rejected)
                *rejected << item;
            continue;
        }
        out << Target{address, static_cast<quint16>(port)};
    }
    return out;
}

bool UdpReceiver::isForwardTarget(const QHostAddress& address, quint16 port) const
{
    const QHostAddress from = plain(address);
    for (const Target& t : m_forward) {
        if (t.port != port)
            continue;
        const QHostAddress to = plain(t.address);
        // Un programma sulla stessa macchina puo' rispondere da 127.0.0.1 anche
        // se lo si e' scritto con l'indirizzo della rete.
        if (to == from || (to.isLoopback() && from.isLoopback()))
            return true;
    }
    return false;
}

void UdpReceiver::relayBack(const QByteArray& data)
{
    const auto msg = wsjtx::parse(data);
    const Target to = msg && m_endpoints.contains(msg->clientId) ? m_endpoints.value(msg->clientId) : m_lastEndpoint;
    if (m_socket && to.port != 0) {
        m_socket->writeDatagram(data, to.address, to.port);
        emit traffic(QStringLiteral("back"), QStringLiteral("%1:%2").arg(to.address.toString()).arg(to.port), data);
    }
}

bool UdpReceiver::sendToClient(const QString& clientId, const QByteArray& data)
{
    const Target to = !clientId.isEmpty() && m_endpoints.contains(clientId) ? m_endpoints.value(clientId)
                                                                             : m_lastEndpoint;
    if (!m_socket || to.port == 0)
        return false;
    if (m_socket->writeDatagram(data, to.address, to.port) != data.size())
        return false;
    emit traffic(QStringLiteral("out"), QStringLiteral("%1:%2").arg(to.address.toString()).arg(to.port), data);
    return true;
}

void UdpReceiver::readPending()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram();
        const QByteArray data = datagram.data();
        const QHostAddress from = datagram.senderAddress();
        const quint16 fromPort = static_cast<quint16>(datagram.senderPort());
        const QString peer = QStringLiteral("%1:%2").arg(plain(from).toString()).arg(fromPort);
        // Da uno dei programmi a cui si inoltra: e' una risposta per il
        // client (Reply, Halt TX...), non un QSO per il log.
        if (!m_forward.isEmpty() && isForwardTarget(from, fromPort)) {
            emit traffic(QStringLiteral("fwd-in"), peer, data);
            relayBack(data);
            continue;
        }
        // Da dove scrive ogni client: le risposte (dei programmi inoltrati, o
        // di DecoDXLog stesso dal monitor) vanno li'. Prima di dirlo al
        // monitor, che cosi' lo trova gia' fra i client.
        if (const auto msg = wsjtx::parse(data)) {
            m_lastEndpoint = Target{plain(from), fromPort};
            m_endpoints.insert(msg->clientId, m_lastEndpoint);
            m_lastClientId = msg->clientId;
        }
        emit traffic(QStringLiteral("in"), peer, data);
        for (const Target& t : std::as_const(m_forward)) {
            // Mai a se stessi: sarebbe un giro senza fine.
            if (t.port == m_port && (plain(t.address).isLoopback() || t.address.isNull()))
                continue;
            m_socket->writeDatagram(data, t.address, t.port);
            emit traffic(QStringLiteral("fwd"), QStringLiteral("%1:%2").arg(plain(t.address).toString()).arg(t.port), data);
        }
        handleDatagram(data, from);
    }
}

void UdpReceiver::touchClient(const QString& id, const QHostAddress& from, const QString& version)
{
    auto& c = m_clients[id];
    c.id = id;
    // Il socket e' in doppio stack: 127.0.0.1 arriva come ::ffff:127.0.0.1.
    bool isV4 = false;
    const quint32 v4 = from.toIPv4Address(&isV4);
    c.address = isV4 ? QHostAddress(v4) : from;
    c.lastSeen = QDateTime::currentDateTimeUtc();
    if (!version.isEmpty())
        c.version = version;
    emit clientSeen(c);
}

QString UdpReceiver::sourceFor(const QString& clientId, const QString& programId)
{
    const bool decodium = clientId.contains(QLatin1String("Decodium"), Qt::CaseInsensitive)
                       || programId.contains(QLatin1String("Decodium"), Qt::CaseInsensitive);
    return decodium ? QStringLiteral("udp_decodium") : QStringLiteral("udp_wsjtx");
}

void UdpReceiver::handleDatagram(const QByteArray& data, const QHostAddress& from)
{
    const auto msg = wsjtx::parse(data);
    if (!msg)
        return;

    if (const auto* hb = std::get_if<wsjtx::Heartbeat>(&msg->payload)) {
        touchClient(msg->clientId, from, hb->version);
        return;
    }
    touchClient(msg->clientId, from);

    if (const auto* st = std::get_if<wsjtx::Status>(&msg->payload)) {
        emit statusReceived(msg->clientId, *st);
        return;
    }

    if (const auto* d = std::get_if<wsjtx::Decode>(&msg->payload)) {
        emit decodeReceived(msg->clientId, *d);
        return;
    }

    if (std::holds_alternative<wsjtx::Close>(msg->payload)) {
        m_clients.remove(msg->clientId);
        emit clientClosed(msg->clientId);
        return;
    }

    if (const auto* q = std::get_if<wsjtx::QsoLogged>(&msg->payload)) {
        const quint64 key = m_nextPendingKey++;
        Pending p;
        p.qso = *q;
        p.clientId = msg->clientId;
        p.timer = new QTimer(this);
        p.timer->setSingleShot(true);
        connect(p.timer, &QTimer::timeout, this, [this, key] { flushPending(key); });
        p.timer->start(m_preferAdif ? m_pairingMs : 0);
        m_pending.insert(key, p);
        return;
    }

    if (const auto* la = std::get_if<wsjtx::LoggedAdif>(&msg->payload)) {
        if (!m_preferAdif)
            return;
        AdifDocument doc = adif::parse(la->adif);
        const QString programId = doc.header.value(QStringLiteral("PROGRAMID"));
        QString programVersion = doc.header.value(QStringLiteral("PROGRAMVERSION"));
        const QString sourceApp = programVersion.isEmpty() ? programId
                                                           : programId + QLatin1Char(' ') + programVersion;
        for (auto& record : doc.records) {
            adif::normalizeMode(record);
            const QString call = record.value(QStringLiteral("CALL")).toUpper();

            // L'ADIF sostituisce il QSOLogged dello stesso client e nominativo.
            for (auto it = m_pending.begin(); it != m_pending.end();) {
                if (it->clientId == msg->clientId
                    && it->qso.dxCall.compare(call, Qt::CaseInsensitive) == 0) {
                    it->timer->deleteLater();
                    it = m_pending.erase(it);
                } else {
                    ++it;
                }
            }
            emit qsoReceived(record, sourceFor(msg->clientId, programId),
                             sourceApp.isEmpty() ? msg->clientId : sourceApp);
        }
        return;
    }
}

void UdpReceiver::flushPending(quint64 key)
{
    auto it = m_pending.find(key);
    if (it == m_pending.end())
        return;
    const Pending p = *it;
    m_pending.erase(it);
    p.timer->deleteLater();

    const auto client = m_clients.value(p.clientId);
    const QString app = client.version.isEmpty() ? p.clientId
                                                 : p.clientId + QLatin1Char(' ') + client.version;
    emit qsoReceived(recordFromQsoLogged(p.qso), sourceFor(p.clientId, {}), app);
}

AdifRecord UdpReceiver::recordFromQsoLogged(const wsjtx::QsoLogged& q)
{
    AdifRecord r;
    const QDateTime on = (q.timeOn.isValid() ? q.timeOn : q.timeOff).toUTC();
    const QDateTime off = q.timeOff.toUTC();
    r.set(QStringLiteral("CALL"), q.dxCall.toUpper());
    r.set(QStringLiteral("GRIDSQUARE"), q.dxGrid);
    r.set(QStringLiteral("MODE"), q.mode);
    r.set(QStringLiteral("RST_SENT"), q.reportSent);
    r.set(QStringLiteral("RST_RCVD"), q.reportReceived);
    if (on.isValid()) {
        r.set(QStringLiteral("QSO_DATE"), on.toString(QStringLiteral("yyyyMMdd")));
        r.set(QStringLiteral("TIME_ON"), on.toString(QStringLiteral("HHmmss")));
    }
    if (off.isValid()) {
        r.set(QStringLiteral("QSO_DATE_OFF"), off.toString(QStringLiteral("yyyyMMdd")));
        r.set(QStringLiteral("TIME_OFF"), off.toString(QStringLiteral("HHmmss")));
    }
    if (q.txFrequencyHz > 0) {
        const double mhz = static_cast<double>(q.txFrequencyHz) / 1e6;
        r.set(QStringLiteral("BAND"), bands::fromMhz(mhz));
        r.set(QStringLiteral("FREQ"), QString::number(mhz, 'f', 6));
    }
    r.set(QStringLiteral("STATION_CALLSIGN"), q.myCall.toUpper());
    r.set(QStringLiteral("MY_GRIDSQUARE"), q.myGrid);
    r.set(QStringLiteral("OPERATOR"), q.operatorCall.toUpper());
    r.set(QStringLiteral("TX_PWR"), q.txPower);
    r.set(QStringLiteral("COMMENT"), q.comments);
    r.set(QStringLiteral("NAME"), q.name);
    r.set(QStringLiteral("STX_STRING"), q.exchangeSent);
    r.set(QStringLiteral("SRX_STRING"), q.exchangeReceived);
    r.set(QStringLiteral("PROP_MODE"), q.propagationMode);
    adif::normalizeMode(r);
    return r;
}

} // namespace decolog::core
