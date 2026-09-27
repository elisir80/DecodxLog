// DecoDXLog — la chat ON4KST per il QML: stanza, messaggi, chi c'e', non letti.
//
// Nominativo e password stanno nel portachiavi (servizio "on4kst"): qui non si
// scrivono mai. I messaggi che nominano il nostro nominativo si contano come
// non letti e finiscono nel registro attivita'.
#pragma once

#include "core/KstChat.h"

#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

namespace decolog::core { class CredentialStore; }

namespace decolog::app {

class ChatController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool online READ online NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(int room READ room WRITE setRoom NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList rooms READ rooms CONSTANT)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(QVariantList people READ people NOTIFY messagesChanged)
    Q_PROPERTY(int unread READ unread NOTIFY messagesChanged)
    Q_PROPERTY(bool hasCredentials READ hasCredentials NOTIFY settingsChanged)
    Q_PROPERTY(QString account READ account NOTIFY settingsChanged)

public:
    struct Context {
        core::CredentialStore* credentials{nullptr};
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
    };

    explicit ChatController(Context context, QObject* parent = nullptr);

    QString state() const;
    bool online() const { return m_chat.state() == core::KstChat::State::Online; }
    QString status() const;
    int room() const { return m_room; }
    void setRoom(int room);
    QVariantList rooms() const;
    QVariantList messages() const;
    QVariantList people() const;
    int unread() const { return m_unread; }
    bool hasCredentials() const;
    QString account() const;

    Q_INVOKABLE void connectChat();
    Q_INVOKABLE void disconnectChat();
    Q_INVOKABLE bool send(const QString& text);
    Q_INVOKABLE bool sendTo(const QString& call, const QString& text);
    Q_INVOKABLE void markRead();
    Q_INVOKABLE void clear();
    // Per le prove: una riga come se arrivasse dal server.
    Q_INVOKABLE void injectLine(const QString& line);
    // Per i test: un server finto.
    void setServer(const QString& host, quint16 port) { m_chat.setServer(host, port); }

signals:
    void stateChanged();
    void settingsChanged();
    void messagesChanged();
    // Qualcuno ci ha nominato.
    void mentioned(const QString& from, const QString& text);

private:
    void add(const core::KstMessage& m);

    Context m_ctx;
    core::KstChat m_chat;
    int m_room{2};
    QList<core::KstMessage> m_messages;     // dal piu' vecchio
    struct Person {
        QString call;
        QString name;
        QDateTime last;
    };
    QHash<QString, Person> m_people;
    int m_unread{0};
};

} // namespace decolog::app
