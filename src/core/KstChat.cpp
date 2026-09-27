#include "core/KstChat.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QTimeZone>

namespace decolog::core {

namespace kst {

const QList<Room>& rooms()
{
    static const QList<Room> list{
        {1, QStringLiteral("50/70 MHz")},
        {2, QStringLiteral("144/432 MHz")},
        {3, QStringLiteral("Microwave")},
        {4, QStringLiteral("EME/JT65")},
        {5, QStringLiteral("Low band (160-80 m)")},
        {6, QStringLiteral("50 MHz IARU Region 3")},
        {7, QStringLiteral("50 MHz IARU Region 2")},
        {8, QStringLiteral("144/432 MHz IARU Region 2")},
        {9, QStringLiteral("144/432 MHz IARU Region 3")},
        {10, QStringLiteral("kHz (2000-630 m)")},
        {11, QStringLiteral("WARC (30, 17, 12 m)")},
        {12, QStringLiteral("28 MHz")},
    };
    return list;
}

KstMessage parseLine(const QString& line, const QString& me, const QDateTime& now)
{
    KstMessage m;
    m.time = now;
    // "1234Z IK1ABC Mario> testo" oppure "1234Z IK1ABC Mario> (IZ2XYZ) testo" per un
    // privato, come lo mostra la chat.
    static const QRegularExpression chat(QStringLiteral("^(\\d{4})Z\\s+([A-Z0-9/]+)\\s+(.*?)>\\s?(.*)$"));
    const auto match = chat.match(line.trimmed());
    if (!match.hasMatch()) {
        m.system = true;
        m.text = line.trimmed();
        return m;
    }
    const QString hhmm = match.captured(1);
    const QTime t(hhmm.left(2).toInt(), hhmm.mid(2, 2).toInt());
    if (t.isValid()) {
        QDateTime at(now.date(), t, QTimeZone::UTC);
        // Poco dopo mezzanotte un messaggio delle 2359 e' di ieri.
        if (at > now.addSecs(600))
            at = at.addDays(-1);
        m.time = at;
    }
    m.from = match.captured(2).toUpper();
    m.name = match.captured(3).trimmed();
    m.text = match.captured(4);
    static const QRegularExpression privateTo(QStringLiteral("^\\(([A-Z0-9/]+)\\)\\s*(.*)$"));
    if (const auto p = privateTo.match(m.text); p.hasMatch()) {
        m.to = p.captured(1).toUpper();
        m.text = p.captured(2);
    }
    const QString call = me.trimmed().toUpper();
    if (!call.isEmpty()) {
        m.mine = m.from == call;
        const QRegularExpression word(QStringLiteral("(^|[^A-Z0-9/])%1($|[^A-Z0-9/])").arg(QRegularExpression::escape(call)));
        m.toMe = !m.mine && (m.to == call || word.match(m.text.toUpper()).hasMatch());
    }
    return m;
}

} // namespace kst

KstChat::KstChat(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, [this] { setState(State::LoggingIn); });
    connect(m_socket, &QTcpSocket::readyRead, this, &KstChat::onData);
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        if (m_state != State::Off && m_lastError.isEmpty())
            m_lastError = QCoreApplication::translate("KstChat", "the server closed the connection");
        setState(State::Off);
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        m_lastError = m_socket->errorString();
        setState(State::Off);
    });
    // Una riga vuota ogni tanto: certi router chiudono le connessioni mute.
    m_keepAlive.setInterval(4 * 60 * 1000);
    connect(&m_keepAlive, &QTimer::timeout, this, [this] {
        if (m_state == State::Online)
            write(QString());
    });
}

KstChat::~KstChat()
{
    m_socket->abort();
}

void KstChat::start(const QString& callsign, const QString& password, int room)
{
    stop();
    m_call = callsign.trimmed().toUpper();
    m_password = password;
    m_room = room;
    m_lastError.clear();
    m_buffer.clear();
    setState(State::Connecting);
    m_socket->connectToHost(m_host, m_port);
}

void KstChat::stop()
{
    m_keepAlive.stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        if (m_state == State::Online)
            write(QStringLiteral("/quit"));
        m_socket->disconnectFromHost();
        m_socket->abort();
    }
    setState(State::Off);
}

bool KstChat::send(const QString& text)
{
    const QString t = text.trimmed();
    if (m_state != State::Online || t.isEmpty())
        return false;
    write(t);
    return true;
}

bool KstChat::sendTo(const QString& call, const QString& text)
{
    const QString c = call.trimmed().toUpper();
    if (c.isEmpty())
        return send(text);
    return send(QStringLiteral("/cq %1 %2").arg(c, text.trimmed()));
}

void KstChat::write(const QString& line)
{
    m_socket->write(line.toLatin1() + "\r\n");
}

void KstChat::setState(State s)
{
    if (s == m_state)
        return;
    m_state = s;
    if (s == State::Online)
        m_keepAlive.start();
    else if (s == State::Off)
        m_keepAlive.stop();
    emit stateChanged();
}

void KstChat::onData()
{
    m_buffer += m_socket->readAll();
    // Le domande del login arrivano senza a capo: si guardano subito.
    if (m_state == State::LoggingIn) {
        const QString pending = QString::fromLatin1(m_buffer);
        if (pending.contains(QLatin1String("Login:"), Qt::CaseInsensitive) && !pending.contains(QLatin1String("\n"))) {
            m_buffer.clear();
            write(m_call);
            return;
        }
    }
    int nl = -1;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromLatin1(m_buffer.left(nl)).remove(QLatin1Char('\r'));
        m_buffer.remove(0, nl + 1);
        handleLine(line);
    }
    // Un resto senza a capo puo' essere una domanda: "Password:", "Your choice :".
    if (!m_buffer.isEmpty() && m_state == State::LoggingIn) {
        const QString rest = QString::fromLatin1(m_buffer);
        if (rest.contains(QLatin1String("Login"), Qt::CaseInsensitive)
            || rest.contains(QLatin1String("Password"), Qt::CaseInsensitive)
            || rest.contains(QLatin1String("choice"), Qt::CaseInsensitive)) {
            m_buffer.clear();
            handleLine(rest);
        }
    }
}

void KstChat::handleLine(const QString& line)
{
    const QString t = line.trimmed();
    if (m_state == State::LoggingIn) {
        if (t.contains(QLatin1String("Login"), Qt::CaseInsensitive) && t.endsWith(QLatin1Char(':'))) {
            write(m_call);
            return;
        }
        if (t.contains(QLatin1String("Password"), Qt::CaseInsensitive) && t.endsWith(QLatin1Char(':'))) {
            write(m_password);
            return;
        }
        if (t.contains(QLatin1String("choice"), Qt::CaseInsensitive)) {
            write(QString::number(m_room));
            setState(State::Online);
            return;
        }
        if (t.contains(QLatin1String("wrong"), Qt::CaseInsensitive) || t.contains(QLatin1String("invalid"), Qt::CaseInsensitive)) {
            m_lastError = t;
            stop();
            return;
        }
    }
    if (t.isEmpty())
        return;
    // Un messaggio di chat vuol dire che siamo dentro, anche se la domanda
    // della stanza non l'abbiamo vista.
    KstMessage m = kst::parseLine(t, m_call);
    if (!m.system && m_state == State::LoggingIn)
        setState(State::Online);
    emit messageReceived(m);
}

} // namespace decolog::core
