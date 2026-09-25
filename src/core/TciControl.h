// DecoDXLog — la radio via TCI, come in Decodium.
//
// TCI (Transceiver Control Interface) e' il protocollo delle SDR Expert
// Electronics (SunSDR, ColibriNANO con ExpertSDR) e di chi lo parla: un
// WebSocket, di solito sulla porta 40001, dove passano righe di testo corte,
// "comando:argomenti;". Il programma della radio manda da se' quello che
// cambia (vfo, modulation, trx…) e si ferma con "ready;" quando ha detto
// tutto; noi mandiamo le stesse righe per cambiarle. Niente Hamlib in mezzo, e
// niente polling: frequenza e modo arrivano appena si gira la manopola.
//
// Qui c'e' quello che serve a un log: frequenza, modo, PTT e il manipolatore
// della radio (cw_macros) per le macro CW. L'audio e l'IQ, che servono a
// Decodium per decodificare, qui non servono.
#pragma once

#include "core/RigLink.h"

#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

namespace decolog::core {

class TciControl : public RigLink {
    Q_OBJECT

public:
    explicit TciControl(QObject* parent = nullptr);

    // "127.0.0.1:40001", "ws://host:porta" o solo "host" (porta 40001).
    // `trx` e' il ricevitore della radio: 0 il primo, 1 il secondo (RX2).
    void connectTo(const QString& address, int trx = 0);
    void disconnectFromRig() override;

    bool connected() const override;
    qint64 frequencyHz() const override { return m_frequencyHz; }
    QString mode() const override { return m_mode; }
    int speedWpm() const override { return m_wpm; }
    QString status() const override { return m_status; }
    // Il nome che la radio dice di se' ("SunSDR2PRO", "ColibriNANO").
    QString device() const { return m_device; }
    bool transmitting() const { return m_transmitting; }

    void refresh() override;
    void setFrequency(qint64 hz) override;
    void setMode(const QString& mode) override;
    void setPtt(bool on) override;
    void setSpeedWpm(int wpm) override;
    void sendMorse(const QString& text) override;
    void stopMorse() override;

    // L'indirizzo WebSocket di un indirizzo scritto a mano.
    static QUrl urlFor(const QString& address);
    // Da Hamlib a TCI e ritorno: "PKTUSB" <-> "digu", "CW" <-> "cw".
    static QString tciModulation(const QString& hamlibMode);
    static QString hamlibMode(const QString& tciModulation);

private:
    void send(const QString& command);
    void flushQueue();
    void handleText(const QString& text);
    void handleCommand(const QString& name, const QStringList& args);
    void setStatus(const QString& text);

    QWebSocket m_socket;
    QTimer m_retry;
    QTimer m_readyWait;
    QStringList m_queue;          // quello da mandare quando la radio e' pronta
    QUrl m_url;
    int m_trx{0};
    bool m_wanted{false};
    bool m_ready{false};
    qint64 m_frequencyHz{0};
    QString m_mode;
    int m_wpm{0};
    bool m_transmitting{false};
    QString m_device;
    QString m_status;
};

} // namespace decolog::core
