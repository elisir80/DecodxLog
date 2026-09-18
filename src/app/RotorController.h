// DecoLog — il rotore: dove guarda l'antenna e dove mandarla.
//
// DecoLog non tocca la seriale: parla con DecoRotor (WebSocket) o con un
// rotctld qualsiasi. Quello che aggiunge e' il contesto che ha solo lui — la
// rotta di uno spot del cluster, del nominativo che si sta lavorando, del QSO
// aperto — e la possibilita' di seguirlo da solo.
#pragma once

#include "core/RotorLink.h"

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <functional>

namespace decolog::app {

class RotorController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(QString backend READ backend WRITE setBackend NOTIFY changed)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY changed)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY changed)
    Q_PROPERTY(bool followDx READ followDx WRITE setFollowDx NOTIFY changed)
    Q_PROPERTY(int beamwidth READ beamwidth WRITE setBeamwidth NOTIFY changed)
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString lastTarget READ lastTarget NOTIFY stateChanged)

public:
    struct Context {
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
    };

    explicit RotorController(Context context, QObject* parent = nullptr);

    // Da chiamare quando il resto e' pronto: se e' acceso, si collega.
    void start();
    // Per le prove da riga di comando: accende il rotore solo per questa volta,
    // senza scrivere niente nelle impostazioni dell'operatore.
    void overrideConnection(const QString& backend, const QString& host, int port);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    // "decorotor" | "rotctld"
    QString backend() const { return m_backend; }
    void setBackend(const QString& backend);
    QString host() const { return m_host; }
    void setHost(const QString& host);
    int port() const { return m_port; }
    void setPort(int port);
    bool followDx() const { return m_followDx; }
    void setFollowDx(bool follow);
    int beamwidth() const { return m_beamwidth; }
    void setBeamwidth(int degrees);

    QVariantMap state() const;
    bool connected() const { return m_link.state().connected; }
    QString status() const;
    QString lastTarget() const { return m_lastTarget; }

    // Punta a gradi. `what` e' quello che si sta puntando, per il registro.
    Q_INVOKABLE void pointTo(double azimuth, const QString& what = {});
    // Punta al centro di un locatore: lo conta DecoRotor, che sa il proprio QTH.
    Q_INVOKABLE void pointLocator(const QString& locator, bool longPath = false);
    Q_INVOKABLE void stopNow(bool fast = false);
    Q_INVOKABLE void park();
    // Sposta il bersaglio di qualche grado (i tasti a freccia del pannello).
    Q_INVOKABLE void nudge(double degrees);
    Q_INVOKABLE void reconnect();

    // Il nominativo che Decodium sta lavorando: se "segui" e' acceso e si sa da
    // che parte sta, l'antenna ci va da sola.
    void dxBearing(const QString& call, double azimuth);

signals:
    void changed();
    void stateChanged();

private:
    void apply();
    void note(const QString& text, const QString& level);

    Context m_ctx;
    core::RotorLink m_link;
    bool    m_enabled{false};
    QString m_backend{QStringLiteral("decorotor")};
    QString m_host{QStringLiteral("127.0.0.1")};
    int     m_port{8765};
    bool    m_followDx{false};
    int     m_beamwidth{45};
    QString m_lastTarget;
    QString m_followedCall;
};

} // namespace decolog::app
