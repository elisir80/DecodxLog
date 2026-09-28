// DecoDXLog — ascolto UDP dei programmi digitali (Decodium, WSJT-X, JTDX).
//
// Quando l'operatore conferma "Log QSO", il client manda due messaggi: prima
// QSOLogged (campi strutturati), poi LoggedADIF (il record ADIF completo). La
// fonte buona e' LoggedADIF, che non perde niente; QSOLogged serve solo se
// l'ADIF non arriva — alcuni client lo mandano da solo. Per questo un QSOLogged
// resta in attesa per un momento e viene scartato se nel frattempo arriva
// l'ADIF dello stesso collegamento.
#pragma once

#include "core/Adif.h"
#include "core/WsjtxProtocol.h"

#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QTimer>

class QUdpSocket;

namespace decolog::core {

struct UdpClientInfo {
    QString   id;
    QString   version;
    QHostAddress address;
    QDateTime lastSeen;
};

class UdpReceiver : public QObject {
    Q_OBJECT

public:
    explicit UdpReceiver(QObject* parent = nullptr);
    ~UdpReceiver() override;

    // Porta 0 = spento. Un indirizzo multicast (224.0.0.0/4) permette a DecoDXLog
    // di condividere il flusso con GridTracker, JTAlert e simili.
    bool start(quint16 port, const QHostAddress& multicastGroup = {});
    void stop();

    bool    isListening() const;
    quint16 port() const { return m_port; }
    QString lastError() const { return m_lastError; }

    // Quanto aspettare l'ADIF dopo un QSOLogged prima di usare i campi
    // strutturati. Configurabile per i test.
    void setPairingWindowMs(int ms) { m_pairingMs = ms; }

    // true (predefinito): LoggedADIF e' la fonte, QSOLogged solo come riserva.
    // false: si usano i campi strutturati di QSOLogged e l'ADIF si ignora, per i
    // client che mandano un ADIF incompleto.
    void setPreferLoggedAdif(bool prefer) { m_preferAdif = prefer; }
    bool prefersLoggedAdif() const { return m_preferAdif; }

    // Ripetitore: ogni pacchetto dei client si inoltra anche a questi programmi
    // (JTAlert, GridTracker...), e le loro risposte (Reply, Halt TX, testo
    // libero) tornano al client che le aspetta. Cosi' Decodium manda a una porta
    // sola e tutti ricevono. Solo indirizzi IP, "localhost" compreso.
    struct Target {
        QHostAddress address;
        quint16 port{0};
    };
    // "127.0.0.1:2238, 192.168.1.20:2237": quelli che non si capiscono restano
    // fuori, e anche la porta su cui ascolta DecoDXLog stesso.
    static QList<Target> parseTargets(const QString& text, QStringList* rejected = nullptr);
    void setForwardTargets(const QList<Target>& targets) { m_forward = targets; }
    QList<Target> forwardTargets() const { return m_forward; }

    // Per i test: tratta un datagramma come se fosse arrivato dalla rete.
    void handleDatagram(const QByteArray& data, const QHostAddress& from = QHostAddress::LocalHost);

    // Converte i campi strutturati di QSOLogged in un record ADIF.
    static AdifRecord recordFromQsoLogged(const wsjtx::QsoLogged& q);

signals:
    // `source` e' il valore della colonna qso.source: udp_decodium o udp_wsjtx.
    void qsoReceived(const decolog::core::AdifRecord& record,
                     const QString& source,
                     const QString& sourceApp);
    void statusReceived(const QString& clientId, const decolog::core::wsjtx::Status& status);
    // Ogni riga decodificata: chi c'e' in aria adesso.
    void decodeReceived(const QString& clientId, const decolog::core::wsjtx::Decode& decode);
    void clientSeen(const decolog::core::UdpClientInfo& client);
    void clientClosed(const QString& clientId);
    void listeningChanged();

private:
    struct Pending {
        wsjtx::QsoLogged qso;
        QString clientId;
        QTimer* timer{nullptr};
    };

    void readPending();
    bool isForwardTarget(const QHostAddress& address, quint16 port) const;
    void relayBack(const QByteArray& data);
    void touchClient(const QString& id, const QHostAddress& from, const QString& version = {});
    void flushPending(quint64 key);
    static QString sourceFor(const QString& clientId, const QString& programId);

    QUdpSocket* m_socket{nullptr};
    quint16     m_port{0};
    QString     m_lastError;
    int         m_pairingMs{1500};
    bool        m_preferAdif{true};
    quint64     m_nextPendingKey{1};
    QHash<quint64, Pending> m_pending;
    QHash<QString, UdpClientInfo> m_clients;
    QList<Target> m_forward;
    // Da dove scrive ogni client: le risposte dei programmi inoltrati vanno li'.
    QHash<QString, Target> m_endpoints;
    Target m_lastEndpoint;
};

} // namespace decolog::core
