#include "app/ChatController.h"

#include "core/CredentialStore.h"

#include <QSettings>
#include <algorithm>

namespace decolog::app {

using namespace decolog::core;

namespace {
constexpr int kKeep = 500;
const QString kService = QStringLiteral("on4kst");
}

ChatController::ChatController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    m_room = QSettings().value(QStringLiteral("chat/room"), 2).toInt();
    connect(&m_chat, &KstChat::stateChanged, this, [this] {
        if (m_chat.state() == KstChat::State::Online && m_ctx.activity)
            m_ctx.activity(QStringLiteral("CHAT"), tr("ON4KST: in the %1 room").arg(rooms().value(m_room - 1).toMap()
                                                                                         .value(QStringLiteral("name")).toString()),
                           QStringLiteral("info"));
        emit stateChanged();
    });
    connect(&m_chat, &KstChat::messageReceived, this, &ChatController::add);
    if (m_ctx.credentials)
        connect(m_ctx.credentials, &CredentialStore::changed, this, &ChatController::settingsChanged);
}

QString ChatController::state() const
{
    switch (m_chat.state()) {
    case KstChat::State::Off: return QStringLiteral("off");
    case KstChat::State::Connecting: return QStringLiteral("connecting");
    case KstChat::State::LoggingIn: return QStringLiteral("login");
    case KstChat::State::Online: return QStringLiteral("online");
    }
    return {};
}

QString ChatController::status() const
{
    switch (m_chat.state()) {
    case KstChat::State::Off:
        return m_chat.lastError().isEmpty() ? tr("not connected") : tr("not connected: %1").arg(m_chat.lastError());
    case KstChat::State::Connecting: return tr("connecting…");
    case KstChat::State::LoggingIn: return tr("logging in…");
    case KstChat::State::Online: return tr("online as %1").arg(m_chat.callsign());
    }
    return {};
}

void ChatController::setRoom(int room)
{
    room = std::clamp(room, 1, static_cast<int>(kst::rooms().size()));
    if (room == m_room)
        return;
    m_room = room;
    QSettings().setValue(QStringLiteral("chat/room"), m_room);
    emit settingsChanged();
    // Cambiare stanza vuol dire rientrare.
    if (m_chat.state() != KstChat::State::Off)
        connectChat();
}

QVariantList ChatController::rooms() const
{
    QVariantList out;
    for (const auto& r : kst::rooms())
        out << QVariantMap{{QStringLiteral("number"), r.number}, {QStringLiteral("name"), r.name}};
    return out;
}

QVariantList ChatController::messages() const
{
    QVariantList out;
    out.reserve(m_messages.size());
    for (const KstMessage& m : m_messages) {
        out << QVariantMap{{QStringLiteral("time"), m.time.toString(QStringLiteral("HH:mm"))},
                           {QStringLiteral("from"), m.from},
                           {QStringLiteral("name"), m.name},
                           {QStringLiteral("text"), m.text},
                           {QStringLiteral("to"), m.to},
                           {QStringLiteral("mine"), m.mine},
                           {QStringLiteral("toMe"), m.toMe},
                           {QStringLiteral("system"), m.system}};
    }
    return out;
}

QVariantList ChatController::people() const
{
    QList<Person> list = m_people.values();
    std::sort(list.begin(), list.end(), [](const Person& a, const Person& b) { return a.last > b.last; });
    QVariantList out;
    for (const Person& p : list)
        out << QVariantMap{{QStringLiteral("call"), p.call}, {QStringLiteral("name"), p.name},
                           {QStringLiteral("last"), p.last.toString(QStringLiteral("HH:mm"))}};
    return out;
}

bool ChatController::hasCredentials() const
{
    return m_ctx.credentials && m_ctx.credentials->hasSecret(kService) && !account().isEmpty();
}

QString ChatController::account() const
{
    return m_ctx.credentials ? m_ctx.credentials->account(kService) : QString();
}

void ChatController::connectChat()
{
    if (!m_ctx.credentials) {
        return;
    }
    const QString call = account();
    m_ctx.credentials->readSecret(kService, [this, call](const QString& secret, const QString& error) {
        if (secret.isEmpty() || call.isEmpty()) {
            if (m_ctx.activity)
                m_ctx.activity(QStringLiteral("CHAT"), tr("ON4KST: no credentials (%1)").arg(error.isEmpty() ? tr("Settings → Sync & Cloud") : error),
                               QStringLiteral("warning"));
            return;
        }
        m_chat.start(call, secret, m_room);
    });
}

void ChatController::disconnectChat()
{
    m_chat.stop();
}

bool ChatController::send(const QString& text)
{
    // Quello che scriviamo il server non sempre lo ripete: lo si mette subito.
    if (!m_chat.send(text))
        return false;
    KstMessage m;
    m.time = QDateTime::currentDateTimeUtc();
    m.from = m_chat.callsign();
    m.text = text.trimmed();
    m.mine = true;
    add(m);
    return true;
}

bool ChatController::sendTo(const QString& call, const QString& text)
{
    if (!m_chat.sendTo(call, text))
        return false;
    KstMessage m;
    m.time = QDateTime::currentDateTimeUtc();
    m.from = m_chat.callsign();
    m.to = call.trimmed().toUpper();
    m.text = text.trimmed();
    m.mine = true;
    add(m);
    return true;
}

void ChatController::markRead()
{
    if (m_unread == 0)
        return;
    m_unread = 0;
    emit messagesChanged();
}

void ChatController::clear()
{
    m_messages.clear();
    m_unread = 0;
    emit messagesChanged();
}

void ChatController::injectLine(const QString& line)
{
    add(kst::parseLine(line, account().isEmpty() ? QStringLiteral("IU8LMC") : account()));
}

void ChatController::add(const KstMessage& m)
{
    // La nostra eco, se il server la ripete, e' gia' in lista.
    if (m.mine && !m_messages.isEmpty()) {
        const KstMessage& last = m_messages.last();
        if (last.mine && last.text == m.text && last.time.secsTo(m.time) < 120 && m.from == last.from)
            return;
    }
    m_messages << m;
    while (m_messages.size() > kKeep)
        m_messages.removeFirst();
    if (!m.system && !m.from.isEmpty()) {
        Person& p = m_people[m.from];
        p.call = m.from;
        if (!m.name.isEmpty())
            p.name = m.name;
        p.last = m.time;
    }
    if (m.toMe) {
        ++m_unread;
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CHAT"), QStringLiteral("%1: %2").arg(m.from, m.text), QStringLiteral("highlight"));
        emit mentioned(m.from, m.text);
    }
    emit messagesChanged();
}

} // namespace decolog::app
