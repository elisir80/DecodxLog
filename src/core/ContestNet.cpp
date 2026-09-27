#include "core/ContestNet.h"

#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QUdpSocket>

namespace decolog::core {

namespace {
constexpr int kVersion = 1;
// Un datagramma UDP sta sotto questa misura senza farsi spezzare sulla LAN.
constexpr int kMaxDatagram = 60000;
}

ContestNet::ContestNet(QObject* parent)
    : QObject(parent)
{
}

ContestNet::~ContestNet()
{
    stop();
}

bool ContestNet::start(quint16 port, const QString& group, const QString& id)
{
    stop();
    m_port = port;
    m_group = group.trimmed();
    m_id = id;
    m_socket = new QUdpSocket(this);
    // Piu' programmi sullo stesso PC (le prove, o due istanze) possono stare
    // sulla stessa porta.
    if (!m_socket->bind(QHostAddress::AnyIPv4, m_port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        m_lastError = m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        return false;
    }
    m_lastError.clear();
    connect(m_socket, &QUdpSocket::readyRead, this, &ContestNet::onReadyRead);
    return true;
}

void ContestNet::stop()
{
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

QByteArray ContestNet::pack(const QString& group, const QString& from, const QString& type, const QJsonObject& body)
{
    QJsonObject o = body;
    o.insert(QStringLiteral("_v"), kVersion);
    o.insert(QStringLiteral("_net"), group);
    o.insert(QStringLiteral("_from"), from);
    o.insert(QStringLiteral("_type"), type);
    return QByteArrayLiteral("DXLNET ") + QJsonDocument(o).toJson(QJsonDocument::Compact);
}

bool ContestNet::unpack(const QByteArray& datagram, const QString& group, QString* from, QString* type, QJsonObject* body)
{
    if (!datagram.startsWith("DXLNET "))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(datagram.mid(7));
    if (!doc.isObject())
        return false;
    QJsonObject o = doc.object();
    if (o.value(QStringLiteral("_v")).toInt() != kVersion)
        return false;
    if (o.value(QStringLiteral("_net")).toString().compare(group, Qt::CaseInsensitive) != 0)
        return false;
    if (from)
        *from = o.value(QStringLiteral("_from")).toString();
    if (type)
        *type = o.value(QStringLiteral("_type")).toString();
    for (const char* k : {"_v", "_net", "_from", "_type"})
        o.remove(QLatin1String(k));
    if (body)
        *body = o;
    return true;
}

void ContestNet::send(const QString& type, const QJsonObject& body)
{
    if (!m_socket)
        return;
    const QByteArray data = pack(m_group, m_id, type, body);
    if (data.size() > kMaxDatagram)
        return;
    if (m_target != QHostAddress(QHostAddress::Broadcast)) {
        m_socket->writeDatagram(data, m_target, m_port);
        return;
    }
    // Il broadcast generico non sempre esce da tutte le schede: si manda anche
    // sul broadcast di ogni rete a cui il PC e' attaccato.
    m_socket->writeDatagram(data, QHostAddress::Broadcast, m_port);
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol && !e.broadcast().isNull())
                m_socket->writeDatagram(data, e.broadcast(), m_port);
        }
    }
}

void ContestNet::onReadyRead()
{
    // Lo stesso messaggio puo' arrivare piu' volte (un broadcast per scheda):
    // chi riceve lo scarta per numero di sequenza, se serve.
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram d = m_socket->receiveDatagram();
        QString from, type;
        QJsonObject body;
        if (!unpack(d.data(), m_group, &from, &type, &body))
            continue;
        if (from == m_id)
            continue;
        emit received(from, type, body, d.senderAddress());
    }
}

} // namespace decolog::core
