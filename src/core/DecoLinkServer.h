// DecoDXLog — DecoLink, il canale locale verso Decodium (docs/DECOLINK.md).
//
// Il protocollo UDP porta i QSO da Decodium a DecoDXLog; DecoLink porta il log
// nell'altra direzione, cosi' Decodium sa gia' mentre decodifica chi e' stato
// lavorato e confermato anche fuori dal proprio file ADIF, e riceve la conferma
// che il QSO appena fatto e' davvero nel log.
//
// JSON su TCP, una riga per messaggio, solo su 127.0.0.1. Il server non conosce
// il database: i dati arrivano da funzioni fornite da chi lo usa.
#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <functional>

class QTcpServer;
class QTcpSocket;

namespace decolog::core {

class DecoLinkServer : public QObject {
    Q_OBJECT

public:
    static constexpr int kProtocol = 1;
    static constexpr int kDefaultPort = 52237;
    static constexpr int kChunkRows = 2000;
    static constexpr int kMaxQueryCalls = 200;

    struct ClientInfo {
        QString app;
        QString version;
        QString station;
        bool    greeted{false};
    };

    explicit DecoLinkServer(QObject* parent = nullptr);
    ~DecoLinkServer() override;

    // Chi usa il server fornisce i dati.
    std::function<QList<QJsonArray>()> workedRows;               // tutte le righe [call, band, mode, date, grid, conf]
    std::function<QJsonObject()> awardState;                      // {"ft2":{...},"dxcc":{...}}
    std::function<QJsonArray(const QJsonObject& query)> resolveQuery;

    void setIdentity(const QString& version, const QString& station);

    bool start(quint16 port);
    void stop();
    bool isListening() const;
    quint16 port() const;
    QString lastError() const { return m_lastError; }

    QList<ClientInfo> clients() const;
    int clientCount() const { return static_cast<int>(m_clients.size()); }

    // A tutti i client che hanno gia' salutato.
    void broadcast(const QJsonObject& message);
    void broadcastAward();
    // Il log e' cambiato in blocco (import, correzioni): elenco di nuovo a tutti.
    void resendSnapshot();

signals:
    void clientsChanged();
    void listeningChanged();

private:
    void onNewConnection();
    void onReadyRead(QTcpSocket* socket);
    void handleMessage(QTcpSocket* socket, const QJsonObject& message);
    void send(QTcpSocket* socket, const QJsonObject& message);
    void sendSnapshot(QTcpSocket* socket);

    QTcpServer* m_server{nullptr};
    QHash<QTcpSocket*, ClientInfo> m_clients;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    QString m_version;
    QString m_station;
    QString m_lastError;
};

} // namespace decolog::core
