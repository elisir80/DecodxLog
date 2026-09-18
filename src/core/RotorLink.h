// DecoLog — il rotore d'antenna.
//
// Due modi di parlarci, tutti e due di rete, nessun driver dentro DecoLog:
//
//   · DecoRotor, il gateway di famiglia: WebSocket sulla 8765, che spinge lo
//     stato cinque volte al secondo e accetta puntamenti anche per locatore;
//   · rotctld di Hamlib (DecoRotor lo espone sulla 4532, ma vale per qualsiasi
//     programma che lo parli), interrogato con "p" e comandato con "P".
//
// Il control box resta il cervello: finecorsa e rampe sono sue. Qui si legge
// dove guarda l'antenna e si dice dove deve andare.
#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

class QJsonObject;
class QTcpSocket;
class QWebSocket;

namespace decolog::core {

struct RotorState {
    bool    connected{false};
    bool    hasAz{true};
    bool    hasEl{false};
    bool    moving{false};
    double  az{0.0};
    double  el{0.0};
    double  azTarget{-1.0};     // -1: nessun bersaglio
    double  elTarget{-1.0};
    QString model;
    QString modelLabel;
    QString port;               // la seriale del control box, se la dice
    QString error;
    QDateTime updated;

    QVariantMap toMap() const;
};

namespace rotor {

// Lo stato come lo manda DecoRotor.
RotorState parseState(const QJsonObject& object);
// La risposta di rotctld a "p": azimut ed elevazione su due righe.
bool parsePosition(const QString& reply, double* az, double* el);
// Gradi normalizzati in 0..360.
double normalize(double degrees);

} // namespace rotor

class RotorLink : public QObject {
    Q_OBJECT

public:
    enum class Backend { DecoRotor, Rotctld };

    explicit RotorLink(QObject* parent = nullptr);
    ~RotorLink() override;

    // `host` e `port` sono quelli del gateway; `token` solo per DecoRotor.
    void start(Backend backend, const QString& host, int port, const QString& token = {});
    void stop();
    bool running() const { return m_running; }
    Backend backend() const { return m_backend; }
    const RotorState& state() const { return m_state; }

    void goTo(double az, double el = -1.0);
    void goToLocator(const QString& locator, bool longPath = false);
    void halt(bool fast = false);
    void park();

signals:
    void stateChanged();
    void note(const QString& text, const QString& level);

private:
    void openDecoRotor();
    void openRotctld();
    void retryLater();
    void sendJson(const QVariantMap& command);
    void sendRotctld(const QString& line);
    void handleJson(const QString& message);
    void handleRotctld();

    Backend m_backend{Backend::DecoRotor};
    QString m_host{QStringLiteral("127.0.0.1")};
    int     m_port{8765};
    QString m_token;
    bool    m_running{false};
    int     m_attempts{0};

    RotorState m_state;
    QWebSocket* m_ws{nullptr};
    QTcpSocket* m_tcp{nullptr};
    QByteArray  m_buffer;
    double  m_pendingAz{-1.0};  // puntamento chiesto mentre il gateway non c'era ancora
    QTimer m_retry;
    QTimer m_poll;              // solo rotctld: lo stato va chiesto
};

} // namespace decolog::core
