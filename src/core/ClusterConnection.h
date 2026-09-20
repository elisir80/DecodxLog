// DecoLog — una fonte di spot: un nodo DX cluster via telnet, la Reverse Beacon
// Network, HamAlert o le attivazioni POTA.
//
// Telnet: al prompt del login si manda il nominativo (e per HamAlert la password,
// letta dal portachiavi solo in quel momento), poi i comandi scelti
// dall'operatore. Se la connessione cade si riprova da soli, aspettando sempre un
// po' di piu' per non martellare un nodo spento. POTA non ha telnet: si interroga
// l'API pubblica ogni minuto.
#pragma once

#include "core/Spots.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <functional>

class QNetworkAccessManager;
class QTcpSocket;

namespace decolog::core {

struct ClusterSource {
    QString id;
    QString name;
    QString type{QStringLiteral("cluster")};   // cluster | rbn | hamalert | pota
    QString host;
    int     port{0};
    QString login;                              // nominativo; per HamAlert il nome utente
    QString commands;                           // comandi dopo il login, uno per riga
    bool    enabled{true};

    QVariantMap toMap() const;
    static ClusterSource fromMap(const QVariantMap& map);
    // Nodi noti e servizi, per scegliere senza cercare indirizzi.
    static QList<ClusterSource> presets();
};

class ClusterConnection : public QObject {
    Q_OBJECT

public:
    enum class State { Off, Connecting, LoggingIn, Online, Waiting };
    Q_ENUM(State)

    // Legge il segreto del servizio (HamAlert); risponde nella callback.
    using SecretReader = std::function<void(const QString& service,
                                            std::function<void(const QString& secret, const QString& error)>)>;

    explicit ClusterConnection(const ClusterSource& source, QObject* parent = nullptr);
    ~ClusterConnection() override;

    const ClusterSource& source() const { return m_source; }
    void setSource(const ClusterSource& source);
    void setSecretReader(SecretReader reader) { m_secrets = std::move(reader); }
    // Il nominativo per i nodi che non ne hanno uno proprio nella configurazione.
    void setDefaultLogin(const QString& call) { m_defaultLogin = call.trimmed().toUpper(); }
    // Oppure chiesto a ogni tentativo: il profilo stazione puo' arrivare dopo l'avvio.
    void setLoginProvider(std::function<QString()> provider) { m_loginProvider = std::move(provider); }
    // Per i test: l'indirizzo dell'API POTA.
    void setPotaUrl(const QString& url) { m_potaUrl = url; }

    void start();
    void stop();
    // Un comando al nodo (SH/DX, DX 14074 K1ABC ...). false se non collegato.
    bool send(const QString& command);

    State state() const { return m_state; }
    QString stateText() const;
    QString lastError() const { return m_lastError; }
    int spotCount() const { return m_spotCount; }
    QDateTime lastSpotAt() const { return m_lastSpotAt; }

signals:
    void spotReceived(const decolog::core::Spot& spot);
    // Le righe del nodo che non sono spot (annunci, WWV, risposte ai comandi).
    void lineReceived(const QString& line);
    void stateChanged();

private:
    void connectNow();
    void onReadyRead();
    void onDisconnected();
    void handleLine(const QString& line);
    void checkPrompt(const QString& pending);
    void loggedIn();
    void setState(State state, const QString& error = {});
    void scheduleRetry();
    void pollPota();
    QString loginName() const;

    ClusterSource m_source;
    SecretReader  m_secrets;
    QString       m_defaultLogin;
    std::function<QString()> m_loginProvider;
    QTcpSocket*   m_socket{nullptr};
    QNetworkAccessManager* m_net{nullptr};
    QByteArray    m_buffer;
    State         m_state{State::Off};
    QString       m_lastError;
    bool          m_loginSent{false};
    bool          m_passwordSent{false};
    bool          m_commandsSent{false};
    // Se dal nodo non e' mai arrivato niente, non si e' collegati: la porta
    // risponde e basta. Un nodo spento accetta il TCP e poi tace.
    bool          m_heardFromNode{false};
    bool          m_wanted{false};
    int           m_retries{0};
    int           m_spotCount{0};
    QDateTime     m_lastSpotAt;
    QTimer        m_retryTimer;
    QTimer        m_loginTimer;
    QTimer        m_keepAlive;
    QTimer        m_potaTimer;
    QString       m_potaUrl{QStringLiteral("https://api.pota.app/spot/activator")};
    QSet<qint64>  m_potaSeen;
};

} // namespace decolog::core
