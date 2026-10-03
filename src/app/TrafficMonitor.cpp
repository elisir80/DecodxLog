#include "app/TrafficMonitor.h"

#include "core/DecoLinkServer.h"
#include "core/DecoPortMonitor.h"
#include "core/UdpReceiver.h"
#include "core/WsjtxProtocol.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTime>
#include <QUdpSocket>

namespace decolog::app {

using namespace decolog::core;

namespace {

// Dei messaggi grossi (l'elenco del log su DecoLink, a blocchi di duemila
// righe) si tiene l'inizio: il monitor serve a vedere cosa passa, non a farne
// una copia.
constexpr int kKeepBytes = 16 * 1024;

QString hexDump(const QByteArray& data)
{
    QStringList lines;
    for (int at = 0; at < data.size(); at += 16) {
        const QByteArray chunk = data.mid(at, 16);
        QString hex;
        QString text;
        for (int i = 0; i < 16; ++i) {
            if (i < chunk.size()) {
                const auto c = static_cast<unsigned char>(chunk.at(i));
                hex += QStringLiteral("%1 ").arg(c, 2, 16, QLatin1Char('0'));
                text += c >= 32 && c < 127 ? QChar(c) : QChar('.');
            } else {
                hex += QStringLiteral("   ");
            }
            if (i == 7)
                hex += QLatin1Char(' ');
        }
        lines << QStringLiteral("%1  %2 %3").arg(at, 4, 16, QLatin1Char('0')).arg(hex, text);
    }
    return lines.join(QLatin1Char('\n'));
}

// Una riga DecoLink in poche parole: il tipo e quello che conta.
void describeDecoLink(const QByteArray& line, QString* type, QString* summary, bool* routine)
{
    const QJsonDocument doc = QJsonDocument::fromJson(line);
    if (!doc.isObject()) {
        *type = QStringLiteral("?");
        *summary = QString::fromUtf8(line.left(300));
        return;
    }
    const QJsonObject o = doc.object();
    *type = o.value(QStringLiteral("type")).toString(QStringLiteral("?"));
    *routine = *type == QLatin1String("ping") || *type == QLatin1String("pong");
    if (*type == QLatin1String("worked")) {
        *summary = QStringLiteral("seq %1 · %2 rows%3")
                       .arg(o.value(QStringLiteral("seq")).toInt())
                       .arg(o.value(QStringLiteral("rows")).toArray().size())
                       .arg(o.value(QStringLiteral("final")).toBool() ? QStringLiteral(" · final") : QString());
        return;
    }
    if (*type == QLatin1String("status")) {
        QStringList calls;
        for (const auto& r : o.value(QStringLiteral("results")).toArray())
            calls << r.toObject().value(QStringLiteral("call")).toString();
        *summary = QStringLiteral("id %1 · %2").arg(o.value(QStringLiteral("id")).toInt()).arg(calls.join(QLatin1Char(' ')));
        return;
    }
    if (*type == QLatin1String("query")) {
        QStringList calls;
        for (const auto& c : o.value(QStringLiteral("calls")).toArray())
            calls << c.toString();
        *summary = QStringLiteral("id %1 · %2 %3 · %4")
                       .arg(o.value(QStringLiteral("id")).toInt())
                       .arg(o.value(QStringLiteral("band")).toString(), o.value(QStringLiteral("mode")).toString(),
                            calls.join(QLatin1Char(' ')));
        return;
    }
    // Il resto senza il tipo, che c'e' gia' nella sua colonna.
    QJsonObject rest = o;
    rest.remove(QStringLiteral("type"));
    *summary = QString::fromUtf8(QJsonDocument(rest).toJson(QJsonDocument::Compact)).left(300);
}

} // namespace

TrafficMonitor::TrafficMonitor(UdpReceiver* udp, DecoLinkServer* decoLink, QObject* parent)
    : QAbstractListModel(parent)
    , m_udp(udp)
    , m_decoLink(decoLink)
    , m_decoPort(new DecoPortListener(this))
    , m_decoPortPort(decoport::kAnnouncePort)
{
    if (m_udp) {
        connect(m_udp, &UdpReceiver::traffic, this, [this](const QString& dir, const QString& peer, const QByteArray& data) {
            record(Udp, dir, peer, data);
        });
    }
    if (m_decoLink) {
        connect(m_decoLink, &DecoLinkServer::traffic, this, [this](const QString& dir, const QString& peer, const QByteArray& line) {
            record(DecoLink, dir, peer, line);
        });
        connect(m_decoLink, &DecoLinkServer::clientsChanged, this, &TrafficMonitor::clientsChanged);
    }
    connect(m_decoPort, &DecoPortListener::traffic, this, [this](const QString& dir, const QString& peer, const QByteArray& data) {
        record(DecoPort, dir, peer, data);
    });
}

TrafficMonitor::~TrafficMonitor() = default;

int TrafficMonitor::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant TrafficMonitor::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row& r = m_rows.at(index.row());
    switch (role) {
    case TimeRole: return r.time;
    case ChannelRole: return channelName(r.channel);
    case DirectionRole: return r.direction;
    case PeerRole: return r.peer;
    case TypeRole: return r.type;
    case ClientRole: return r.client;
    case Qt::DisplayRole:
    case SummaryRole: return r.summary;
    case BytesRole: return static_cast<int>(r.data.size());
    case CanReplyRole: return r.canReply;
    case SerialRole: return static_cast<qint64>(r.serial);
    }
    return {};
}

QHash<int, QByteArray> TrafficMonitor::roleNames() const
{
    return {{TimeRole, "time"},       {ChannelRole, "channel"}, {DirectionRole, "direction"},
            {PeerRole, "peer"},       {TypeRole, "type"},       {ClientRole, "client"},
            {SummaryRole, "summary"}, {BytesRole, "bytes"},     {CanReplyRole, "canReply"},
            {SerialRole, "serial"}};
}

QString TrafficMonitor::channelName(Channel channel)
{
    switch (channel) {
    case Udp: return QStringLiteral("UDP");
    case DecoLink: return QStringLiteral("DecoLink");
    case DecoPort: return QStringLiteral("DecoPort");
    }
    return {};
}

void TrafficMonitor::setActive(bool on)
{
    if (m_active == on)
        return;
    m_active = on;
    // DecoPort si ascolta solo col monitor aperto: la porta degli annunci e'
    // condivisa, ma non c'e' motivo di tenerla quando nessuno guarda.
    if (on && m_decoPortPort != 0)
        m_decoPort->start(m_decoPortPort);
    else
        m_decoPort->stop();
    emit activeChanged();
    emit decoPortChanged();
    emit clientsChanged();
}

void TrafficMonitor::setPaused(bool on)
{
    if (m_paused == on)
        return;
    m_paused = on;
    // In pausa la vista resta ferma e le righe si raccolgono lo stesso: si
    // rimettono in fila alla ripresa.
    if (!on)
        rebuild();
    emit pausedChanged();
}

void TrafficMonitor::setShow(Channel channel, bool on)
{
    if (m_show[channel] == on)
        return;
    m_show[channel] = on;
    rebuild();
    emit filterChanged();
}

void TrafficMonitor::setHideRoutine(bool on)
{
    if (m_hideRoutine == on)
        return;
    m_hideRoutine = on;
    rebuild();
    emit filterChanged();
}

void TrafficMonitor::setTextFilter(const QString& text)
{
    const QString t = text.trimmed();
    if (m_textFilter == t)
        return;
    m_textFilter = t;
    rebuild();
    emit filterChanged();
}

QStringList TrafficMonitor::udpClients() const
{
    return m_udp ? m_udp->clientIds() : QStringList();
}

int TrafficMonitor::decoLinkClients() const
{
    return m_decoLink ? m_decoLink->clientCount() : 0;
}

bool TrafficMonitor::decoPortListening() const
{
    return m_decoPort->listening();
}

QString TrafficMonitor::decoPortError() const
{
    return m_decoPort->lastError();
}

bool TrafficMonitor::matches(const Row& row) const
{
    if (!m_show[row.channel])
        return false;
    if (m_hideRoutine && row.routine)
        return false;
    if (m_textFilter.isEmpty())
        return true;
    for (const QString* field : {&row.type, &row.summary, &row.peer, &row.client, &row.direction}) {
        if (field->contains(m_textFilter, Qt::CaseInsensitive))
            return true;
    }
    return channelName(row.channel).contains(m_textFilter, Qt::CaseInsensitive);
}

void TrafficMonitor::rebuild()
{
    beginResetModel();
    m_rows.clear();
    for (auto it = m_all.crbegin(); it != m_all.crend(); ++it) {
        if (matches(*it))
            m_rows.append(*it);
    }
    endResetModel();
    emit countChanged();
}

void TrafficMonitor::record(Channel channel, const QString& direction, const QString& peer, const QByteArray& data)
{
    if (!m_active)
        return;
    Row row;
    row.serial = ++m_serial;
    row.time = QDateTime::currentDateTimeUtc().toString(QStringLiteral("HH:mm:ss.zzz"));
    row.channel = channel;
    row.direction = direction;
    row.peer = peer;
    row.data = data.size() > kKeepBytes ? data.left(kKeepBytes) : data;

    switch (channel) {
    case Udp: {
        const auto d = wsjtx::describe(data);
        row.type = d.typeName;
        row.client = d.clientId;
        row.summary = d.summary;
        const auto type = static_cast<wsjtx::Type>(d.type);
        row.routine = d.valid && type == wsjtx::Type::Heartbeat;
        // Si risponde a quello che Decodium ha decodificato, non a una nostra
        // risposta tornata indietro.
        row.canReply = d.valid && type == wsjtx::Type::Decode && direction == QLatin1String("in");
        if (direction == QLatin1String("in") && d.valid && !d.clientId.isEmpty()
            && !m_udpClients.contains(d.clientId)) {
            m_udpClients << d.clientId;
            emit clientsChanged();
        }
        break;
    }
    case DecoLink:
        row.client = QStringLiteral("Decodium");
        describeDecoLink(data, &row.type, &row.summary, &row.routine);
        if (data.size() > kKeepBytes)
            row.summary += QStringLiteral(" · %1 bytes").arg(data.size());
        break;
    case DecoPort: {
        const auto d = decoport::describe(data);
        row.type = d.typeName;
        row.client = d.valid ? QStringLiteral("%1").arg(d.streamId, 8, 16, QLatin1Char('0')) : QString();
        row.summary = d.summary;
        row.routine = d.valid && (d.type == 1 || d.type == 4);
        break;
    }
    }

    m_all.append(row);
    const bool show = !m_paused && matches(row);
    if (show) {
        beginInsertRows({}, 0, 0);
        m_rows.prepend(row);
        endInsertRows();
    }
    while (m_all.size() > kLimit) {
        const quint64 gone = m_all.constFirst().serial;
        m_all.removeFirst();
        if (!m_paused && !m_rows.isEmpty() && m_rows.constLast().serial == gone) {
            const int last = static_cast<int>(m_rows.size()) - 1;
            beginRemoveRows({}, last, last);
            m_rows.removeLast();
            endRemoveRows();
        }
    }
    emit countChanged();
}

void TrafficMonitor::demo()
{
    if (m_udp && m_udp->isListening()) {
        if (!m_demoSocket) {
            m_demoSocket = new QUdpSocket(this);
            m_demoSocket->bind(QHostAddress::LocalHost, 0);
        }
        const QString id = QStringLiteral("Decodium");
        wsjtx::Heartbeat hb;
        hb.version = QStringLiteral("4.0.12");
        wsjtx::Status st;
        st.dialFrequencyHz = 14074000;
        st.mode = QStringLiteral("FT8");
        st.txMode = QStringLiteral("FT8");
        st.deCall = QStringLiteral("IU8LMC");
        st.deGrid = QStringLiteral("JN70");
        st.decoding = true;
        const QTime now = QTime::currentTime();
        QList<QByteArray> datagrams{wsjtx::buildHeartbeat(id, hb), wsjtx::buildStatus(id, st)};
        const char* lines[] = {"CQ 9A3XY JN75", "IU8LMC DL1AB -12", "CQ DX JA1ZZZ PM95", "EA8XX K1ABC RR73"};
        for (int i = 0; i < 4; ++i) {
            wsjtx::Decode d;
            d.time = now;
            d.snr = -18 + 5 * i;
            d.deltaTime = 0.1 * i;
            d.deltaFrequency = 600 + 350 * static_cast<quint32>(i);
            d.mode = QStringLiteral("~");
            d.message = QString::fromLatin1(lines[i]);
            datagrams << wsjtx::buildDecode(id, d);
        }
        for (const QByteArray& dg : std::as_const(datagrams))
            m_demoSocket->writeDatagram(dg, QHostAddress::LocalHost, m_udp->port());
    }
    const QString link = QStringLiteral("127.0.0.1:61022");
    record(DecoLink, QStringLiteral("in"), link,
           R"({"type":"hello","app":"Decodium","version":"4.0.12","protocol":1,"station":"IU8LMC"})");
    record(DecoLink, QStringLiteral("out"), link,
           R"({"type":"worked","seq":1,"final":true,"rows":[["DL2ABC","20m","FT8","20260916","JO31",1],["9A3XY","20m","FT2","20260917","JN75",0]]})");
    record(DecoLink, QStringLiteral("in"), link,
           R"({"type":"query","id":7,"band":"20m","mode":"FT8","calls":["9A3XY","JA1ZZZ"]})");
    record(DecoLink, QStringLiteral("out"), link,
           R"({"type":"status","id":7,"results":[{"call":"9A3XY","workedCall":true},{"call":"JA1ZZZ","workedCall":false}]})");
    record(DecoLink, QStringLiteral("in"), link, R"({"type":"ping","id":3})");
    record(DecoLink, QStringLiteral("out"), link, R"({"type":"pong","id":3})");
    decoport::Context ctx;
    ctx.rfFrequencyHz = 14074000;
    ctx.mode = 7;
    ctx.rigLabel = QStringLiteral("Yaesu FT-991");
    ctx.stateFlags = 1 | 2 | 4 | 8;
    ctx.sessionPort = decoport::kSessionPort;
    record(DecoPort, QStringLiteral("in"), QStringLiteral("192.168.1.20:5560"),
           decoport::buildPacket(1, 0x5a17c0deu, 42, decoport::encodeContext(ctx)));
}

void TrafficMonitor::clear()
{
    beginResetModel();
    m_all.clear();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

const TrafficMonitor::Row* TrafficMonitor::find(qint64 serial) const
{
    // Prima quelle che si vedono (in pausa possono essere gia' uscite dalla
    // memoria), poi tutte.
    for (const Row& r : m_rows) {
        if (static_cast<qint64>(r.serial) == serial)
            return &r;
    }
    for (auto it = m_all.crbegin(); it != m_all.crend(); ++it) {
        if (static_cast<qint64>(it->serial) == serial)
            return &*it;
    }
    return nullptr;
}

QString TrafficMonitor::detail(qint64 serial) const
{
    const Row* found = find(serial);
    if (!found)
        return {};
    const Row& r = *found;
    QString head = QStringLiteral("%1  %2  %3  %4  %5  %6 bytes\n%7\n\n")
                       .arg(r.time, channelName(r.channel), r.direction, r.peer, r.type)
                       .arg(r.data.size())
                       .arg(r.summary);
    if (r.channel == DecoLink) {
        const QJsonDocument doc = QJsonDocument::fromJson(r.data);
        return head + (doc.isNull() ? QString::fromUtf8(r.data) : QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    }
    return head + hexDump(r.data);
}

QString TrafficMonitor::asText() const
{
    QStringList out;
    for (const Row& r : m_rows) {
        out << QStringLiteral("%1\t%2\t%3\t%4\t%5\t%6\t%7")
                   .arg(r.time, channelName(r.channel), r.direction, r.peer, r.type, r.client, r.summary);
    }
    return out.join(QLatin1Char('\n'));
}

void TrafficMonitor::setResult(bool ok, const QString& text)
{
    m_lastResultOk = ok;
    m_lastResult = text;
    emit lastResultChanged();
}

QString TrafficMonitor::target(const QString& client) const
{
    // Decodium, come WSJT-X, scarta i messaggi che non portano il suo id.
    return client.isEmpty() && m_udp ? m_udp->lastClientId() : client;
}

bool TrafficMonitor::sendUdp(const QString& client, const QByteArray& datagram, const QString& what)
{
    if (client.isEmpty() || !m_udp || !m_udp->sendToClient(client, datagram)) {
        setResult(false, tr("%1 not sent: no program has written to the UDP port yet.").arg(what));
        return false;
    }
    setResult(true, tr("%1 sent to %2.").arg(what, client));
    return true;
}

bool TrafficMonitor::reply(qint64 serial, int modifiers)
{
    const Row* r = find(serial);
    if (!r || !r->canReply) {
        setResult(false, tr("Pick a decoded line (Decode) to answer."));
        return false;
    }
    const auto msg = wsjtx::parse(r->data);
    const auto* dec = msg ? std::get_if<wsjtx::Decode>(&msg->payload) : nullptr;
    if (!dec) {
        setResult(false, tr("This line cannot be read back as a decode."));
        return false;
    }
    return sendUdp(msg->clientId,
                   wsjtx::buildReply(msg->clientId, *dec, static_cast<quint8>(modifiers), msg->schema),
                   tr("Reply to \"%1\"").arg(dec->message));
}

bool TrafficMonitor::haltTx(const QString& to, bool autoOnly)
{
    const QString client = target(to);
    return sendUdp(client, wsjtx::buildHaltTx(client, autoOnly),
                   autoOnly ? tr("Auto TX off") : tr("Halt TX"));
}

bool TrafficMonitor::freeText(const QString& to, const QString& text, bool send)
{
    const QString client = target(to);
    return sendUdp(client, wsjtx::buildFreeText(client, text, send),
                   send ? tr("Free text \"%1\" (transmit)").arg(text) : tr("Free text \"%1\"").arg(text));
}

bool TrafficMonitor::replay(const QString& to)
{
    const QString client = target(to);
    return sendUdp(client, wsjtx::buildReplay(client), tr("Replay"));
}

bool TrafficMonitor::clearWindows(const QString& to, int window)
{
    const QString client = target(to);
    return sendUdp(client, wsjtx::buildClear(client, static_cast<quint8>(qBound(0, window, 2))), tr("Clear"));
}

bool TrafficMonitor::location(const QString& to, const QString& grid)
{
    const QString client = target(to);
    const QString g = grid.trimmed().toUpper();
    if (g.size() < 4) {
        setResult(false, tr("A locator has at least four characters."));
        return false;
    }
    return sendUdp(client, wsjtx::buildLocation(client, g), tr("Locator %1").arg(g));
}

bool TrafficMonitor::highlight(const QString& to, const QString& call, const QString& background,
                               const QString& foreground, bool last)
{
    const QString client = target(to);
    const QString c = call.trimmed().toUpper();
    if (c.isEmpty()) {
        setResult(false, tr("Which callsign?"));
        return false;
    }
    return sendUdp(client, wsjtx::buildHighlightCallsign(client, c, background, foreground, last),
                   background.isEmpty() && foreground.isEmpty() ? tr("Highlight of %1 removed").arg(c)
                                                                : tr("Highlight of %1").arg(c));
}

bool TrafficMonitor::sendDecoLink(const QString& json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json.trimmed().toUtf8(), &error);
    if (!doc.isObject() || doc.object().value(QStringLiteral("type")).toString().isEmpty()) {
        setResult(false, doc.isNull() ? tr("Not JSON: %1").arg(error.errorString())
                                      : tr("DecoLink wants a JSON object with a \"type\"."));
        return false;
    }
    if (decoLinkClients() == 0) {
        setResult(false, tr("Decodium is not connected to DecoLink."));
        return false;
    }
    m_decoLink->broadcast(doc.object());
    setResult(true, tr("Sent on DecoLink: %1").arg(doc.object().value(QStringLiteral("type")).toString()));
    return true;
}

bool TrafficMonitor::resendDecoLinkList()
{
    if (decoLinkClients() == 0) {
        setResult(false, tr("Decodium is not connected to DecoLink."));
        return false;
    }
    m_decoLink->resendSnapshot();
    setResult(true, tr("The log list is on its way to Decodium."));
    return true;
}

bool TrafficMonitor::resendDecoLinkAward()
{
    if (decoLinkClients() == 0) {
        setResult(false, tr("Decodium is not connected to DecoLink."));
        return false;
    }
    m_decoLink->broadcastAward();
    setResult(true, tr("Award state sent to Decodium."));
    return true;
}

} // namespace decolog::app
