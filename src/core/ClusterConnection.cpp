#include "core/ClusterConnection.h"

#include "core/NetworkError.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTcpSocket>
#include <QUuid>

namespace decolog::core {

namespace {

constexpr qsizetype kMaxBuffer = 1 << 20;

// Toglie i comandi di negoziazione telnet (IAC ...): alcuni nodi li mandano
// all'inizio e finirebbero in mezzo al prompt.
QByteArray stripTelnet(const QByteArray& data)
{
    QByteArray out;
    out.reserve(data.size());
    for (qsizetype i = 0; i < data.size(); ++i) {
        const auto c = static_cast<unsigned char>(data.at(i));
        if (c != 0xFF) {
            out.append(data.at(i));
            continue;
        }
        if (i + 1 >= data.size())
            break;
        const auto cmd = static_cast<unsigned char>(data.at(i + 1));
        if (cmd == 0xFF) {             // 0xFF raddoppiato = un byte 0xFF
            out.append(char(0xFF));
            ++i;
        } else if (cmd >= 251 && cmd <= 254) {
            i += 2;                    // WILL/WONT/DO/DONT + opzione
        } else {
            ++i;
        }
    }
    return out;
}

} // namespace

QVariantMap ClusterSource::toMap() const
{
    return {
        {QStringLiteral("id"), id},           {QStringLiteral("name"), name},
        {QStringLiteral("type"), type},       {QStringLiteral("host"), host},
        {QStringLiteral("port"), port},       {QStringLiteral("login"), login},
        {QStringLiteral("commands"), commands}, {QStringLiteral("enabled"), enabled},
    };
}

ClusterSource ClusterSource::fromMap(const QVariantMap& m)
{
    ClusterSource s;
    s.id = m.value(QStringLiteral("id")).toString();
    if (s.id.isEmpty())
        s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.name = m.value(QStringLiteral("name")).toString();
    s.type = m.value(QStringLiteral("type"), QStringLiteral("cluster")).toString();
    s.host = m.value(QStringLiteral("host")).toString().trimmed();
    s.port = m.value(QStringLiteral("port")).toInt();
    s.login = m.value(QStringLiteral("login")).toString().trimmed();
    s.commands = m.value(QStringLiteral("commands")).toString();
    s.enabled = m.value(QStringLiteral("enabled"), true).toBool();
    return s;
}

QList<ClusterSource> ClusterSource::presets()
{
    auto make = [](const char* name, const char* type, const char* host, int port, const char* commands = "") {
        ClusterSource s;
        s.name = QString::fromUtf8(name);
        s.type = QLatin1String(type);
        s.host = QLatin1String(host);
        s.port = port;
        s.commands = QLatin1String(commands);
        return s;
    };
    return {
        make("IZ7AUH (DX Spider)", "cluster", "dx.iz7auh.net", 8000),
        make("VE7CC (CC Cluster, con skimmer)", "cluster", "dxc.ve7cc.net", 23),
        make("W3LPL", "cluster", "w3lpl.net", 7373),
        make("DXFun", "cluster", "dxfun.com", 8000),
        make("NC7J", "cluster", "dxc.nc7j.com", 7373),
        make("RBN · CW e RTTY", "rbn", "telnet.reversebeacon.net", 7000),
        make("RBN · FT8 e FT4", "rbn", "telnet.reversebeacon.net", 7001),
        make("HamAlert", "hamalert", "hamalert.org", 7300, "set/json"),
        make("POTA · attivazioni", "pota", "api.pota.app", 443),
    };
}

ClusterConnection::ClusterConnection(const ClusterSource& source, QObject* parent)
    : QObject(parent)
    , m_source(source)
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &ClusterConnection::connectNow);
    // Se il nodo non chiede il nominativo con un prompt riconoscibile, lo si manda
    // lo stesso dopo qualche secondo: quasi tutti lo aspettano come prima riga.
    m_loginTimer.setSingleShot(true);
    m_loginTimer.setInterval(4000);
    connect(&m_loginTimer, &QTimer::timeout, this, [this] {
        if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
            return;
        // Un nodo che non ha mai aperto bocca non e' un nodo collegato: e' una
        // porta aperta su un servizio spento. Dirsi "online" li' davanti vuol
        // dire mostrare una fonte verde che non portera' mai uno spot.
        if (!m_heardFromNode) {
            setState(State::Waiting, tr("the node answers but says nothing: it may be down — "
                                        "try another source"));
            scheduleRetry();
            return;
        }
        if (!m_loginSent && m_source.type != QLatin1String("hamalert"))
            checkPrompt(QStringLiteral("login:"));
        else if (m_loginSent && !m_commandsSent)
            loggedIn();
    });
    m_keepAlive.setInterval(9 * 60'000);
    connect(&m_keepAlive, &QTimer::timeout, this, [this] {
        if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState)
            m_socket->write("\r\n");
    });
    m_potaTimer.setInterval(60'000);
    connect(&m_potaTimer, &QTimer::timeout, this, &ClusterConnection::pollPota);
}

ClusterConnection::~ClusterConnection()
{
    m_wanted = false;
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
    }
}

void ClusterConnection::setSource(const ClusterSource& source)
{
    const bool reconnect = m_wanted
        && (source.host != m_source.host || source.port != m_source.port || source.login != m_source.login
            || source.type != m_source.type);
    m_source = source;
    if (reconnect) {
        stop();
        start();
    }
}

QString ClusterConnection::loginName() const
{
    // Il nome utente HamAlert va com'e'; un nominativo in maiuscolo.
    if (m_source.type == QLatin1String("hamalert"))
        return m_source.login;
    if (!m_source.login.isEmpty())
        return m_source.login.toUpper();
    QString call = m_loginProvider ? m_loginProvider().trimmed().toUpper() : m_defaultLogin;
    // Sullo stesso nodo con lo stesso nominativo DX Spider chiude la sessione vecchia:
    // quella di Decodium. DecoDXLog entra come CALL-2.
    if (!call.isEmpty() && m_source.type == QLatin1String("cluster") && !call.contains(QLatin1Char('-')))
        call += QStringLiteral("-2");
    return call;
}

QString ClusterConnection::stateText() const
{
    switch (m_state) {
    case State::Off:        return tr("off");
    case State::Connecting: return tr("connecting");
    case State::LoggingIn:  return tr("logging in");
    case State::Online:     return tr("online");
    case State::Waiting:    return m_lastError.isEmpty() ? tr("retrying") : tr("retrying: %1").arg(m_lastError);
    }
    return {};
}

void ClusterConnection::setState(State state, const QString& error)
{
    if (!error.isEmpty() || state == State::Online)
        m_lastError = error;
    if (state == m_state && error.isEmpty())
        return;
    m_state = state;
    emit stateChanged();
}

void ClusterConnection::start()
{
    m_wanted = true;
    m_retries = 0;
    connectNow();
}

void ClusterConnection::stop()
{
    m_wanted = false;
    m_retryTimer.stop();
    m_loginTimer.stop();
    m_keepAlive.stop();
    m_potaTimer.stop();
    if (m_socket) {
        m_socket->disconnect(this);
        if (m_socket->state() == QAbstractSocket::ConnectedState)
            m_socket->write("bye\r\n");
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_lastError.clear();
    setState(State::Off);
}

void ClusterConnection::connectNow()
{
    if (!m_wanted)
        return;

    if (m_source.type == QLatin1String("pota")) {
        if (!m_net)
            m_net = new QNetworkAccessManager(this);
        setState(State::Connecting);
        m_potaTimer.start();
        pollPota();
        return;
    }

    if (loginName().isEmpty()) {
        setState(State::Waiting, tr("no callsign: set it in the station profile or in the source"));
        scheduleRetry();
        return;
    }
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
    }
    m_socket = new QTcpSocket(this);
    m_buffer.clear();
    m_loginSent = m_passwordSent = m_commandsSent = m_heardFromNode = false;
    connect(m_socket, &QTcpSocket::connected, this, [this] {
        setState(State::LoggingIn);
        m_loginTimer.start();
    });
    connect(m_socket, &QTcpSocket::readyRead, this, &ClusterConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClusterConnection::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (m_socket && m_socket->state() != QAbstractSocket::ConnectedState) {
            m_lastError = m_socket->errorString();
            onDisconnected();
        }
    });
    setState(State::Connecting);
    m_socket->connectToHost(m_source.host, static_cast<quint16>(m_source.port));
}

void ClusterConnection::onDisconnected()
{
    m_loginTimer.stop();
    m_keepAlive.stop();
    if (!m_wanted)
        return;
    const QString error = m_lastError.isEmpty() ? tr("connection closed") : m_lastError;
    setState(State::Waiting, error);
    scheduleRetry();
}

void ClusterConnection::scheduleRetry()
{
    if (m_retryTimer.isActive())
        return;
    // 5 s, 10 s, 20 s, 40 s... fino a due minuti.
    const int delay = qMin(120, 5 * (1 << qMin(m_retries, 5)));
    ++m_retries;
    m_retryTimer.start(delay * 1000);
}

bool ClusterConnection::send(const QString& command)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return false;
    m_socket->write(command.toLatin1() + "\r\n");
    return true;
}

void ClusterConnection::onReadyRead()
{
    m_heardFromNode = true;
    m_buffer += stripTelnet(m_socket->readAll());
    qsizetype newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        QByteArray raw = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        raw.replace('\r', "");
        const QString line = QString::fromLatin1(raw);
        handleLine(line);
    }
    if (m_buffer.size() > kMaxBuffer)
        m_buffer.clear();
    // I prompt ("login: ") non finiscono con un a capo.
    if (!m_buffer.isEmpty())
        checkPrompt(QString::fromLatin1(m_buffer));
}

void ClusterConnection::checkPrompt(const QString& pending)
{
    const QString p = pending.trimmed().toLower();
    const bool asksCall = p.endsWith(QLatin1String("login:")) || p.endsWith(QLatin1String("call:"))
        || p.endsWith(QLatin1String("callsign:")) || p.contains(QLatin1String("enter your call"))
        || p.endsWith(QLatin1String("username:"));
    const bool asksPassword = p.endsWith(QLatin1String("password:"));

    if (asksCall && !m_loginSent) {
        m_loginSent = true;
        m_buffer.clear();
        m_socket->write(loginName().toLatin1() + "\r\n");
        m_loginTimer.start();
        return;
    }
    if (asksPassword && m_loginSent && !m_passwordSent) {
        m_passwordSent = true;
        m_buffer.clear();
        const QString service = m_source.type == QLatin1String("hamalert") ? QStringLiteral("hamalert")
                                                                            : QStringLiteral("cluster:") + m_source.id;
        if (!m_secrets) {
            setState(State::Waiting, tr("password needed"));
            return;
        }
        QPointer<QTcpSocket> socket = m_socket;
        m_secrets(service, [this, socket](const QString& secret, const QString& error) {
            if (!socket || socket != m_socket)
                return;
            if (secret.isEmpty()) {
                m_lastError = error.isEmpty() ? tr("no password stored") : error;
                m_wanted = false;   // senza password riprovare non serve
                setState(State::Waiting, m_lastError);
                socket->abort();
                return;
            }
            socket->write(secret.toUtf8() + "\r\n");
            m_loginTimer.start();
        });
    }
}

void ClusterConnection::handleLine(const QString& raw)
{
    // Via i caratteri di comando prima di guardare la riga. DX Spider attacca
    // uno o due BEL (0x07) in coda agli spot, per far suonare il terminale:
    //   "DX de KC7PFR:  14015.0  SJ2W  CQ CONTEST  0214Z\a\a"
    // Restavano appiccicati alla Z dell'orario, la riga non veniva riconosciuta
    // come spot e finiva nella console come testo qualunque — il cluster
    // "girava" ma la tabella restava vuota.
    QString line;
    line.reserve(raw.size());
    for (const QChar c : raw) {
        if (c == QLatin1Char('\t') || c.unicode() >= 0x20)
            line.append(c);
    }
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return;

    // Alcuni nodi mettono uno spazio (o un BEL) prima di "DX de".
    std::optional<Spot> spot;
    if (trimmed.startsWith(QLatin1Char('{')))
        spot = spots::parseHamAlertJson(trimmed.toUtf8());
    else if (trimmed.startsWith(QLatin1String("DX de"), Qt::CaseInsensitive))
        spot = spots::parseDxLine(trimmed);
    else if (!trimmed.isEmpty() && trimmed.at(0).isDigit() && trimmed.endsWith(QLatin1Char('>')))
        spot = spots::parseShowDxLine(trimmed);

    if (spot) {
        if (m_state != State::Online)
            loggedIn();
        if (spot->source == QLatin1String("cluster") && m_source.type == QLatin1String("rbn"))
            spot->source = QStringLiteral("rbn");
        if (spot->sourceName.isEmpty())
            spot->sourceName = m_source.name;
        ++m_spotCount;
        m_lastSpotAt = QDateTime::currentDateTimeUtc();
        emit spotReceived(*spot);
        return;
    }

    const QString lower = line.toLower();
    if (lower.contains(QLatin1String("invalid")) && (lower.contains(QLatin1String("password")) || lower.contains(QLatin1String("login")))) {
        m_lastError = line.trimmed();
        m_wanted = false;
        setState(State::Waiting, m_lastError);
    }
    emit lineReceived(line);
    if (!m_loginSent || !m_commandsSent)
        checkPrompt(line);
    // Il prompt del nodo dopo il login ("IU8LMC de IZ7AUH-6 17-Sep-2026 1240Z >").
    if (m_loginSent && !m_commandsSent && (line.trimmed().endsWith(QLatin1Char('>')) || lower.contains(QLatin1String("hello"))
                                           || lower.contains(QLatin1String("welcome"))))
        loggedIn();
}

void ClusterConnection::loggedIn()
{
    if (m_commandsSent)
        return;
    m_commandsSent = true;
    m_loginTimer.stop();
    m_retries = 0;
    for (const QString& cmd : m_source.commands.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (!cmd.trimmed().isEmpty())
            m_socket->write(cmd.trimmed().toLatin1() + "\r\n");
    }
    m_keepAlive.start();
    setState(State::Online);
}

void ClusterConnection::pollPota()
{
    if (!m_wanted || !m_net)
        return;
    QNetworkRequest request{QUrl(m_potaUrl)};
    network::useHttp11(request);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DecoDXLog/%1").arg(QCoreApplication::applicationVersion()));
    request.setTransferTimeout(20'000);
    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (!m_wanted)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            setState(State::Waiting, network::safeErrorString(reply));
            return;
        }
        const QJsonArray array = QJsonDocument::fromJson(reply->readAll()).array();
        // spotId distingue uno spot nuovo da uno gia' visto al giro prima.
        QSet<qint64> current;
        for (const QJsonValue& v : array) {
            const QJsonObject o = v.toObject();
            const qint64 id = o.value(QLatin1String("spotId")).toVariant().toLongLong();
            current.insert(id);
            if (m_potaSeen.contains(id))
                continue;
            for (const Spot& spot : spots::parsePotaJson(QJsonDocument(QJsonArray{o}).toJson(QJsonDocument::Compact))) {
                ++m_spotCount;
                m_lastSpotAt = QDateTime::currentDateTimeUtc();
                emit spotReceived(spot);
            }
        }
        m_potaSeen = current;
        setState(State::Online);
    });
}

} // namespace decolog::core
