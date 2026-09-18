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
#include <QVariantList>
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
    QString locator;            // il QTH del gateway
    QString callsign;
    double  beamwidth{45.0};
    bool    beamwidthKnown{false};   // lo stato non lo dice sempre: la config si'
    bool    linkUp{false};           // il gateway risponde (non il control box)
    int     clients{0};
    // Dalla configurazione del gateway: finecorsa e posizione di riposo.
    double  azMin{0.0};
    double  azMax{360.0};
    double  parkAz{0.0};
    double  parkEl{-1.0};
    double  tolerance{1.0};
    double  stallTimeout{8.0};
    bool    stopOnClientLoss{true};
    bool    tokenRequired{false};
    // Contatori dell'esercizio, come li manda il gateway.
    int     txFrames{0};
    int     rxFrames{0};
    int     errorCount{0};
    int     reconnects{0};
    double  uptime{0.0};
    bool    hasConfig{false};
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

    // Memorie del gateway: nome, azimut ed eventuale elevazione.
    QVariantList presets() const { return m_presets; }
    void requestPresets();
    void recallPreset(const QString& name);
    void savePreset(const QString& name, double az, double el = -1.0);
    void deletePreset(const QString& name);
    // Rotta verso un locatore, senza muovere niente.
    void requestBearing(const QString& locator);

    // Diagnostica: frame della seriale e andamento della posizione.
    QVariantList traffic() const { return m_traffic; }
    QVariantList history() const { return m_history; }
    void requestTraffic(int limit = 60);
    void requestHistory(int limit = 300);
    // Configurazione a caldo: solo i campi che il gateway accetta.
    void setConfig(const QVariantMap& values);

signals:
    void stateChanged();
    void presetsChanged();
    void trafficChanged();
    void historyChanged();
    // {short_path, long_path, distance_km, lat, lon, locator}
    void bearingReady(const QVariantMap& bearing);
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
    QVariantList m_presets;
    QVariantList m_traffic;
    QVariantList m_history;
    QWebSocket* m_ws{nullptr};
    QTcpSocket* m_tcp{nullptr};
    QByteArray  m_buffer;
    double  m_pendingAz{-1.0};  // puntamento chiesto mentre il gateway non c'era ancora
    QTimer m_retry;
    QTimer m_poll;              // solo rotctld: lo stato va chiesto
};

} // namespace decolog::core
