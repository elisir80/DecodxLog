// DecoDXLog — la chat ON4KST (www.on4kst.info), per chi fa VHF, EME e bande basse.
//
// Si entra per telnet sulla porta 23000: il server chiede il nominativo, la
// password e la stanza (50/70 MHz, 144/432 MHz, EME...). Da li' ogni riga e' un
// messaggio: "1234Z IK1ABC Mario> testo". Si scrive una riga per parlare a
// tutti, "/cq NOMINATIVO testo" per parlare a uno solo.
#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QTimer>

class QTcpSocket;

namespace decolog::core {

struct KstMessage {
    QDateTime time;         // UTC
    QString   from;
    QString   name;
    QString   text;
    QString   to;           // messaggio privato: a chi (vuoto = a tutti)
    bool      mine{false};  // scritto da noi
    bool      toMe{false};  // ci nomina o e' per noi
    bool      system{false};// una riga del server
};

namespace kst {

struct Room {
    int     number;
    QString name;
};
// Le stanze della chat, nell'ordine del server.
const QList<Room>& rooms();
// Una riga del server come messaggio. `me` e' il nostro nominativo (per toMe).
KstMessage parseLine(const QString& line, const QString& me, const QDateTime& now = QDateTime::currentDateTimeUtc());

} // namespace kst

class KstChat : public QObject {
    Q_OBJECT

public:
    enum class State { Off, Connecting, LoggingIn, Online };
    Q_ENUM(State)

    explicit KstChat(QObject* parent = nullptr);
    ~KstChat() override;

    void setServer(const QString& host, quint16 port) { m_host = host; m_port = port; }
    void start(const QString& callsign, const QString& password, int room);
    void stop();
    bool send(const QString& text);
    // Un messaggio a un nominativo solo.
    bool sendTo(const QString& call, const QString& text);

    State state() const { return m_state; }
    QString lastError() const { return m_lastError; }
    QString callsign() const { return m_call; }

signals:
    void messageReceived(const decolog::core::KstMessage& message);
    void stateChanged();

private:
    void onData();
    void handleLine(const QString& line);
    void setState(State s);
    void write(const QString& line);

    QTcpSocket* m_socket{nullptr};
    QString m_host{QStringLiteral("www.on4kst.info")};
    quint16 m_port{23000};
    QString m_call;
    QString m_password;
    int m_room{2};
    State m_state{State::Off};
    QString m_lastError;
    QByteArray m_buffer;
    QTimer m_keepAlive;
    bool m_wanted{false};
};

} // namespace decolog::core

Q_DECLARE_METATYPE(decolog::core::KstMessage)
