// DecoDXLog — propagazione: i numeri del Sole, le condizioni banda, lo storico.
//
// Si aggiorna da solo ogni tanto (predefinito: ogni ora), tiene un campione
// all'ora nel database e lo mette accanto ai QSO di quel giorno: e' li' che si
// vede con che condizioni si lavora davvero.
#pragma once

#include "core/Solar.h"

#include <QObject>
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

public:
    struct Context {
        core::LogDatabase* db{nullptr};
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
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

    Q_INVOKABLE void refresh();
    // Per le prove: il XML letto da un file invece che dalla rete.
    void injectXml(const QByteArray& xml);
    // Gli ultimi campioni: {t, sfi, a, k, sunspots}.
    Q_INVOKABLE QVariantList history(int days = 14) const;
    // Giorno per giorno: QSO fatti e SFI medio, per vedere se vanno insieme.
    Q_INVOKABLE QVariantList qsoAgainstFlux(int days = 14) const;

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
};

} // namespace decolog::app
