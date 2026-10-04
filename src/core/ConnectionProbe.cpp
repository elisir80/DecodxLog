#include "core/ConnectionProbe.h"

#include <QHostAddress>
#include <QHostInfo>

namespace decolog::core {

QString ConnectionProbe::Result::text() const
{
    QStringList lines = steps;
    if (!hint.isEmpty())
        lines << hint;
    return lines.join(QLatin1Char('\n'));
}

ConnectionProbe::ConnectionProbe(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<Result>();
    m_timer.setSingleShot(true);
}

void ConnectionProbe::start(const QString& host, quint16 port)
{
    if (m_busy)
        return;
    m_busy = true;
    m_host = host.trimmed();
    m_port = port;
    m_result = {};
    m_connected = false;
    m_gotData = false;

    // Un indirizzo gia' scritto in numeri non ha niente da cercare.
    QHostAddress literal;
    if (literal.setAddress(m_host)) {
        m_result.steps << tr("1. Name: %1 is already an address").arg(m_host);
        m_result.address = literal.toString();
        connectTo(literal);
        return;
    }
    m_lookupId = QHostInfo::lookupHost(m_host, this, [this](const QHostInfo& info) {
        if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
            m_result.steps << tr("1. Name: %1 does not become an address (%2)")
                                  .arg(m_host, info.errorString().isEmpty() ? tr("no answer") : info.errorString());
            finish(Verdict::DnsFailed);
            return;
        }
        // Un indirizzo IPv4 per primo, se c'e': e' quello che usano i nodi.
        QHostAddress chosen = info.addresses().first();
        for (const QHostAddress& a : info.addresses()) {
            if (a.protocol() == QAbstractSocket::IPv4Protocol) {
                chosen = a;
                break;
            }
        }
        m_result.steps << tr("1. Name: %1 is %2").arg(m_host, chosen.toString());
        m_result.address = chosen.toString();
        connectTo(chosen);
    });
}

void ConnectionProbe::connectTo(const QHostAddress& address)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, [this] {
        m_connected = true;
        m_result.steps << tr("2. Port %1: connected").arg(m_port);
        // Il nodo ha un po' di tempo per dire qualcosa.
        m_timer.start(m_bannerMs);
    });
    connect(m_socket, &QTcpSocket::readyRead, this, [this] {
        m_gotData = true;
        const QByteArray data = m_socket->readAll();
        // Un nodo telnet apre con la negoziazione (IAC, comando, opzione): si
        // salta intera, e si tiene la prima riga di testo.
        QByteArray text;
        for (qsizetype i = 0; i < data.size(); ++i) {
            const auto c = static_cast<unsigned char>(data.at(i));
            if (c == 0xFF) {
                i += (i + 1 < data.size() && static_cast<unsigned char>(data.at(i + 1)) >= 251) ? 2 : 1;
            } else if (c >= 0x20 && c < 0x7f) {
                text.append(static_cast<char>(c));
            } else if (c == '\n' && !text.isEmpty()) {
                break;
            }
        }
        m_result.banner = QString::fromLatin1(text).trimmed().left(80);
        m_result.steps << (m_result.banner.isEmpty() ? tr("3. The node answered")
                                                     : tr("3. The node answered: \"%1\"").arg(m_result.banner));
        finish(Verdict::Ok);
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        if (!m_busy || !m_connected || m_gotData)
            return;
        m_result.steps << tr("3. The connection was closed before the node said anything");
        finish(Verdict::ClosedAtOnce);
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        if (!m_busy || m_connected)
            return;
        if (error == QAbstractSocket::ConnectionRefusedError) {
            m_result.steps << tr("2. Port %1: refused").arg(m_port);
            finish(Verdict::Refused);
        } else if (error == QAbstractSocket::SocketTimeoutError) {
            m_result.steps << tr("2. Port %1: no answer from the network").arg(m_port);
            finish(Verdict::TimedOut);
        } else {
            m_result.steps << tr("2. Port %1: %2").arg(m_port).arg(m_socket->errorString());
            finish(Verdict::Error);
        }
    });
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (!m_busy)
            return;
        if (m_connected) {
            m_result.steps << tr("3. Connected, but the node says nothing");
            finish(Verdict::Silent);
        } else {
            m_result.steps << tr("2. Port %1: no answer from the network").arg(m_port);
            finish(Verdict::TimedOut);
        }
    });
    m_timer.start(m_connectMs);
    m_socket->connectToHost(address, m_port);
}

void ConnectionProbe::finish(Verdict verdict)
{
    if (!m_busy)
        return;
    m_busy = false;
    m_timer.stop();
    m_timer.disconnect(this);
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_result.verdict = verdict;
    switch (verdict) {
    case Verdict::Ok:
        break;
    case Verdict::DnsFailed:
        m_result.hint = tr("The name does not resolve: check the network and the DNS. An antivirus or a filtering "
                           "DNS (AVG, Avast…) can also block a name it does not trust: try the address in numbers.");
        break;
    case Verdict::Refused:
    case Verdict::TimedOut:
        m_result.hint = tr("The port does not answer. If other programs reach the network but not this node, a firewall "
                           "or an antivirus (AVG, Avast…) is probably blocking it: allow DecoDXLog in its firewall and "
                           "network shield, or try another node.");
        break;
    case Verdict::ClosedAtOnce:
    case Verdict::Silent:
        m_result.hint = tr("The connection opens but nothing comes back. For a node that answers other people this "
                           "usually means an antivirus or a firewall (AVG, Avast…) is holding it: add DecoDXLog to its "
                           "exceptions and switch off the scan of this connection, or try another node.");
        break;
    case Verdict::Error:
        m_result.hint = tr("The network refused the connection: check the connection, the proxy and the firewall.");
        break;
    }
    emit finished(m_result);
}

} // namespace decolog::core
