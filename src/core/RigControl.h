// DecoDXLog — la radio, vista da Hamlib.
//
// Non si parla al cavo: si parla a **rigctld**, il demone di Hamlib, che sta
// gia' sul computer di chi opera e conosce tutte le radio del mondo. Cosi'
// DecoDXLog non deve sapere niente di porte seriali, e chi aggiorna Hamlib si
// ritrova le radio nuove senza aspettare noi.
//
// Il dialogo e' quello del protocollo esteso di rigctld: si manda "+f" e
// torna "get_freq:\nFrequency: 14074000\nRPRT 0". Ogni risposta finisce con
// RPRT, e li' si sa se e' andata bene.
#pragma once

#include "core/RigLink.h"

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

namespace decolog::core {

class RigControl : public RigLink {
    Q_OBJECT

public:
    explicit RigControl(QObject* parent = nullptr);

    void connectTo(const QString& host, quint16 port);
    void disconnectFromRig() override;

    bool connected() const override;
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    qint64 frequencyHz() const override { return m_frequencyHz; }
    QString mode() const override { return m_mode; }
    int speedWpm() const override { return m_wpm; }
    QString status() const override { return m_status; }

    // Chiede alla radio dove sta: frequenza, modo e velocita' del manipolatore.
    void refresh() override;
    void setFrequency(qint64 hz) override;
    void setMode(const QString& mode) override;
    void setPtt(bool on) override;
    // La velocita' del manipolatore della radio, in parole al minuto.
    void setSpeedWpm(int wpm) override;
    // Manda il testo in CW con il manipolatore della radio, e lo ferma.
    void sendMorse(const QString& text) override;
    // Il testo come lo vuole il manipolatore della radio: maiuscole, lettere,
    // cifre e i segni del CW.
    static QString morseText(const QString& text);
    void stopMorse() override;

    int features() const override { return m_features; }
    bool split() const override { return m_split; }
    qint64 txFrequencyHz() const override { return m_txHz; }
    QString vfo() const override { return m_vfo; }
    int ritHz() const override { return m_rit; }
    int xitHz() const override { return m_xit; }
    void setSplit(bool on, qint64 txHz = 0) override;
    void setVfo(const QString& vfo) override;
    void setRit(int hz) override;
    void setXit(int hz) override;

private:
    struct Pending {
        QString kind;    // "freq", "mode", "speed", "morse", "set"
        QString text;    // per il CW: quello che si e' mandato
        int values{0};   // quante righe di valore ci si aspetta, se non arriva RPRT
        quint64 seq{0};  // l'ordine in cui e' partita
    };

    void send(const QString& kind, const QString& command, int values, const QString& text = {});
    void readFromRig();
    void handleReply(const QStringList& lines);
    void setStatus(const QString& text);

    QTcpSocket* m_socket;
    QTimer m_poll;
    QTimer m_retry;
    QQueue<Pending> m_pending;
    QStringList m_lines;             // righe della risposta in arrivo, fino a RPRT
    QByteArray m_buffer;

    QString m_host;
    quint16 m_port{4532};
    bool m_wanted{false};
    qint64 m_frequencyHz{0};
    QString m_mode;
    int m_wpm{0};
    QString m_status;
    // Quello che la radio sa fare: si parte da tutto, e un errore toglie.
    int m_features{Split | VfoSelect | Rit | Xit};
    bool m_split{false};
    qint64 m_txHz{0};
    QString m_vfo;
    int m_rit{0};
    int m_xit{0};
    int m_extraPoll{0};
    // Una domanda partita prima di un comando risponde con lo stato di prima:
    // per split, frequenza TX, VFO, RIT e XIT si ricorda quando e' partito
    // l'ultimo comando, e le risposte piu' vecchie non lo disfano.
    quint64 m_seq{0};
    QHash<QString, quint64> m_lastSet;
    // Dall'altra parte c'e' un ponte che risponde col valore nudo, senza RPRT:
    // quello di Decodium. Accetta "S" ma non lo esegue; lo split lo fa solo
    // con la frequenza TX ("I"), e solo se in Decodium lo split non e' "nessuno".
    bool m_plainBridge{false};
    // Dopo un comando di split si richiede alla radio se l'ha preso: se no,
    // lo si dice, invece di lasciare la pillola che torna indietro da sola.
    bool m_splitWanted{false};
    // La radio non sa fermare il CW da CAT (le Yaesu con Hamlib): lo si dice
    // una volta e non lo si chiede piu'.
    bool m_noStopMorse{false};
    qint64 m_splitTxWanted{0};
    quint64 m_splitCheck{0};
};

} // namespace decolog::core
