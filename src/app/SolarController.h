// DecoDXLog — propagazione: i numeri del Sole, le condizioni banda, lo storico.
//
// Si aggiorna da solo ogni tanto (predefinito: ogni ora), tiene un campione
// all'ora nel database e lo mette accanto ai QSO di quel giorno: e' li' che si
// vede con che condizioni si lavora davvero.
#pragma once

#include "core/Solar.h"
#include "core/Voacap.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

namespace decolog::core { class LogDatabase; }

namespace decolog::app {

class SolarController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap data READ data NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool automatic READ automatic WRITE setAutomatic NOTIFY changed)
    Q_PROPERTY(int intervalMinutes READ intervalMinutes WRITE setIntervalMinutes NOTIFY changed)
    // VOACAP per la previsione sul percorso: c'e' (e' nel pacchetto), si usa,
    // e con che stazione — il modo (per l'SNR che serve), la potenza, il
    // guadagno delle antenne, il rumore del posto (-dBW a 3 MHz).
    Q_PROPERTY(bool voacapAvailable READ voacapAvailable CONSTANT)
    Q_PROPERTY(bool voacapEnabled READ voacapEnabled WRITE setVoacapEnabled NOTIFY changed)
    Q_PROPERTY(QString voacapMode READ voacapMode WRITE setVoacapMode NOTIFY changed)
    Q_PROPERTY(int voacapPower READ voacapPower WRITE setVoacapPower NOTIFY changed)
    Q_PROPERTY(double voacapGain READ voacapGain WRITE setVoacapGain NOTIFY changed)
    Q_PROPERTY(int voacapNoise READ voacapNoise WRITE setVoacapNoise NOTIFY changed)

public:
    struct Context {
        core::LogDatabase* db{nullptr};
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
        // Dove sta la stazione: {lat, lon}, vuota se il locatore non c'e'.
        std::function<QVariantMap()> stationPosition;
    };

    explicit SolarController(Context context, QObject* parent = nullptr);

    // Da chiamare a log aperto: rilegge lo storico e fa il primo scarico.
    void start();

    QVariantMap data() const { return m_data.toMap(); }
    bool busy() const { return m_fetcher.busy(); }
    QString status() const { return m_status; }
    bool automatic() const { return m_automatic; }
    void setAutomatic(bool automatic);
    int intervalMinutes() const { return m_interval; }
    void setIntervalMinutes(int minutes);
    bool voacapAvailable() const { return m_voacap && m_voacap->available(); }
    bool voacapEnabled() const { return m_voacapEnabled; }
    void setVoacapEnabled(bool on);
    QString voacapMode() const { return m_voacapMode; }
    void setVoacapMode(const QString& mode);
    int voacapPower() const { return m_voacapPower; }
    void setVoacapPower(int watts);
    double voacapGain() const { return m_voacapGain; }
    void setVoacapGain(double dbi);
    int voacapNoise() const { return m_voacapNoise; }
    void setVoacapNoise(int noise);

    Q_INVOKABLE void refresh();
    // Per le prove: il XML letto da un file invece che dalla rete.
    void injectXml(const QByteArray& xml);
    // Gli ultimi campioni: {t, sfi, a, k, sunspots}.
    Q_INVOKABLE QVariantList history(int days = 14) const;
    // Giorno per giorno: QSO fatti e SFI medio, per vedere se vanno insieme.
    Q_INVOKABLE QVariantList qsoAgainstFlux(int days = 14) const;
    // La previsione di oggi verso un punto: `target` e' un locatore o {lat, lon}.
    // {valid, distanceKm, azimuth, hops, bands: [nomi], hours: [{hour, muf, luf,
    // quality: [0..3 per banda], rel: [%], snr: [dB]}], currentHour, reason,
    // engine: "voacap" | "model", voacapPending, voacapError}. Con VOACAP
    // acceso la prima risposta e' quella del modello semplice; quella di
    // VOACAP arriva appena pronta (changed()).
    Q_INVOKABLE QVariantMap pathForecast(const QVariant& target) const;

signals:
    void changed();

private:
    void loadHistory();
    void remember(const core::SolarData& data);
    void saveHistory();

    Context m_ctx;
    core::SolarFetcher m_fetcher;
    core::SolarData m_data;
    QVariantList m_history;     // dal piu' vecchio
    QString m_status;
    QTimer m_timer;
    bool m_automatic{true};
    int  m_interval{60};

    core::voacap::Engine* m_voacap{nullptr};
    // Le previsioni fatte, per chiave della richiesta; quelle chieste e non
    // ancora arrivate non si chiedono due volte.
    QHash<QString, core::voacap::Result> m_voacapResults;
    mutable QSet<QString> m_voacapAsked;
    bool m_voacapEnabled{true};
    QString m_voacapMode{QStringLiteral("FT8")};
    int m_voacapPower{100};
    double m_voacapGain{2.0};
    int m_voacapNoise{145};
    void saveVoacap(const char* key, const QVariant& value);
};

} // namespace decolog::app
