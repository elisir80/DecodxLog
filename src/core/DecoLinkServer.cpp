#include "core/DecoLinkServer.h"

#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>

namespace decolog::core {

namespace {

// Una riga troppo lunga senza '\n' non e' un client DecoLink: si chiude.
constexpr qsizetype kMaxLine = 1 << 20;

} // namespace

DecoLinkServer::DecoLinkServer(QObject* parent)
    : QObject(parent)
{
}

DecoLinkServer::~DecoLinkServer()
{
    stop();
}

void DecoLinkServer::setIdentity(const QString& version, const QString& station)
{
    m_version = version;
    m_station = station;
}

bool DecoLinkServer::start(quint16 port)
{
    const bool wasRetrying = m_retryTimer && m_retryTimer->isActive();
    stop();
    m_wantedPort = port;
    m_server = new QTcpServer(this);
    // Solo loopback: il log non si espone alla rete.
    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        m_lastError = m_server->errorString();
        delete m_server;
        m_server = nullptr;
        // La porta occupata non e' una condanna: di solito e' un altro
        // DecoDXLog ancora aperto, e quando si chiude questa deve prendere il
        // suo posto da sola. Senza, Decodium si collegava a una copia e
        // all'altra a seconda di chi era partito prima, e sembrava che il
        // collegamento andasse e venisse.
        if (!m_retryTimer) {
            m_retryTimer = new QTimer(this);
            m_retryTimer->setInterval(m_retryMs);
            connect(m_retryTimer, &QTimer::timeout, this, [this] {
                if (isListening()) {
                    m_retryTimer->stop();
                    return;
                }
                if (start(m_wantedPort))
                    emit listeningRecovered();
            });
        }
        m_retryTimer->setInterval(m_retryMs);
        m_retryTimer->start();
        emit listeningChanged();
        return false;
    }
    if (m_retryTimer)
        m_retryTimer->stop();
    Q_UNUSED(wasRetrying);
    m_lastError.clear();
    connect(m_server, &QTcpServer::newConnection, this, &DecoLinkServer::onNewConnection);
    emit listeningChanged();
    return true;
}

void DecoLinkServer::stop()
{
    if (m_retryTimer)
        m_retryTimer->stop();
    const auto sockets = m_clients.keys();
    for (QTcpSocket* s : sockets) {
        s->disconnect(this);
        s->abort();
        s->deleteLater();
    }
    m_clients.clear();
    m_buffers.clear();
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
        emit listeningChanged();
    }
    emit clientsChanged();
}

bool DecoLinkServer::isListening() const
{
    return m_server && m_server->isListening();
}

quint16 DecoLinkServer::port() const
{
    return m_server ? m_server->serverPort() : 0;
}

QList<DecoLinkServer::ClientInfo> DecoLinkServer::clients() const
{
    return m_clients.values();
}

void DecoLinkServer::onNewConnection()
{
    while (QTcpSocket* s = m_server->nextPendingConnection()) {
        m_clients.insert(s, ClientInfo{});
        connect(s, &QTcpSocket::readyRead, this, [this, s] { onReadyRead(s); });
        connect(s, &QTcpSocket::disconnected, this, [this, s] {
            m_clients.remove(s);
            m_buffers.remove(s);
            s->deleteLater();
            emit clientsChanged();
        });
        // Nel saluto "app" resta DecoLog, e non e' una dimenticanza: e' il
        // nome con cui questo programma si presenta nel protocollo dalla prima
        // versione, e i Decodium gia' installati chiudono la connessione se
        // leggono un nome che non conoscono — si collegavano e si staccavano
        // ogni cinque secondi. Il nome vero viaggia accanto, in "product".
        send(s, QJsonObject{
            {QStringLiteral("type"), QStringLiteral("hello")},
            {QStringLiteral("app"), QStringLiteral("DecoLog")},
            {QStringLiteral("product"), QStringLiteral("DecoDXLog")},
            {QStringLiteral("version"), m_version},
            {QStringLiteral("protocol"), kProtocol},
            {QStringLiteral("station"), m_station},
        });
        emit clientsChanged();
    }
}

void DecoLinkServer::onReadyRead(QTcpSocket* socket)
{
    QByteArray& buffer = m_buffers[socket];
    buffer += socket->readAll();
    qsizetype newline;
    while ((newline = buffer.indexOf('\n')) >= 0) {
        const QByteArray line = buffer.left(newline).trimmed();
        buffer.remove(0, newline + 1);
        if (line.isEmpty())
            continue;
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
        if (error.error == QJsonParseError::NoError && doc.isObject())
            handleMessage(socket, doc.object());
        if (!m_clients.contains(socket))
            return;
    }
    if (buffer.size() > kMaxLine)
        socket->abort();
}

void DecoLinkServer::handleMessage(QTcpSocket* socket, const QJsonObject& message)
{
    const QString type = message.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("hello")) {
        ClientInfo& info = m_clients[socket];
        info.app = message.value(QStringLiteral("app")).toString();
        info.version = message.value(QStringLiteral("version")).toString();
        info.station = message.value(QStringLiteral("station")).toString();
        info.greeted = true;
        emit clientsChanged();
        sendSnapshot(socket);
        return;
    }

    if (type == QLatin1String("ping")) {
        send(socket, QJsonObject{{QStringLiteral("type"), QStringLiteral("pong")},
                                 {QStringLiteral("id"), message.value(QStringLiteral("id"))}});
        return;
    }

    if (type == QLatin1String("query")) {
        QJsonObject query = message;
        QJsonArray calls = message.value(QStringLiteral("calls")).toArray();
        while (calls.size() > kMaxQueryCalls)
            calls.removeLast();
        query.insert(QStringLiteral("calls"), calls);
        send(socket, QJsonObject{
            {QStringLiteral("type"), QStringLiteral("status")},
            {QStringLiteral("id"), message.value(QStringLiteral("id"))},
            {QStringLiteral("results"), resolveQuery ? resolveQuery(query) : QJsonArray{}},
        });
        return;
    }
    // Tipi sconosciuti: ignorati, per compatibilita' con le versioni future.
}

void DecoLinkServer::sendSnapshot(QTcpSocket* socket)
{
    QList<QJsonArray> rows = workedRows ? workedRows() : QList<QJsonArray>{};
    int seq = 0;
    qsizetype i = 0;
    do {
        QJsonArray chunk;
        for (int n = 0; n < kChunkRows && i < rows.size(); ++n, ++i)
            chunk.append(rows.at(i));
        send(socket, QJsonObject{
            {QStringLiteral("type"), QStringLiteral("worked")},
            {QStringLiteral("seq"), ++seq},
            {QStringLiteral("final"), i >= rows.size()},
            {QStringLiteral("rows"), chunk},
        });
    } while (i < rows.size());

    if (awardState) {
        QJsonObject award = awardState();
        award.insert(QStringLiteral("type"), QStringLiteral("award"));
        send(socket, award);
    }
}

void DecoLinkServer::send(QTcpSocket* socket, const QJsonObject& message)
{
    socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

void DecoLinkServer::broadcast(const QJsonObject& message)
{
    const QByteArray line = QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it.value().greeted)
            it.key()->write(line);
    }
}

void DecoLinkServer::resendSnapshot()
{
    for (auto it = m_clients.cbegin(); it != m_clients.cend(); ++it) {
        if (it.value().greeted)
            sendSnapshot(it.key());
    }
}

void DecoLinkServer::broadcastAward()
{
    if (!awardState || m_clients.isEmpty())
        return;
    QJsonObject award = awardState();
    award.insert(QStringLiteral("type"), QStringLiteral("award"));
    broadcast(award);
}

} // namespace decolog::core
