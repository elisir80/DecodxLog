// DecoDXLog — la rete della stazione multi-operatore, sulla LAN.
//
// Piu' PC, un log solo: ogni DecoDXLog manda in broadcast UDP (porta 12060 di
// default) quello che succede — chi e' acceso e su che banda, i QSO appena
// fatti, i messaggi fra operatori, gli spot interni — e ascolta quello degli
// altri. Un QSO fatto su un PC arriva negli altri log, cosi' i doppi e i
// moltiplicatori sono di tutti. Il nome della rete tiene separate due stazioni
// sulla stessa LAN. Niente server: se un PC si spegne, gli altri continuano.
#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QUdpSocket;

namespace decolog::core {

class ContestNet : public QObject {
    Q_OBJECT

public:
    explicit ContestNet(QObject* parent = nullptr);
    ~ContestNet() override;

    // `group` e' il nome della rete; `id` un identificativo di questo PC che
    // resta uguale fra un avvio e l'altro.
    bool start(quint16 port, const QString& group, const QString& id);
    void stop();
    bool running() const { return m_socket != nullptr; }
    QString lastError() const { return m_lastError; }
    // Dove mandare invece del broadcast (per i test: 127.0.0.1).
    void setTarget(const QHostAddress& address) { m_target = address; }

    // Manda un messaggio di tipo `type` con i campi di `body`.
    void send(const QString& type, const QJsonObject& body);

    // Il messaggio impacchettato e letto: esposti per i test.
    static QByteArray pack(const QString& group, const QString& from, const QString& type, const QJsonObject& body);
    static bool unpack(const QByteArray& datagram, const QString& group, QString* from, QString* type, QJsonObject* body);

signals:
    // Da un altro PC della stessa rete (i nostri messaggi non tornano qui).
    void received(const QString& from, const QString& type, const QJsonObject& body, const QHostAddress& sender);

private:
    void onReadyRead();

    QUdpSocket* m_socket{nullptr};
    quint16 m_port{12060};
    QString m_group;
    QString m_id;
    QString m_lastError;
    QHostAddress m_target{QHostAddress::Broadcast};
};

} // namespace decolog::core
