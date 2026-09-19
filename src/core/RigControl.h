// DecoLog — la radio, vista da Hamlib.
//
// Non si parla al cavo: si parla a **rigctld**, il demone di Hamlib, che sta
// gia' sul computer di chi opera e conosce tutte le radio del mondo. Cosi'
// DecoLog non deve sapere niente di porte seriali, e chi aggiorna Hamlib si
// ritrova le radio nuove senza aspettare noi.
//
// Il dialogo e' quello del protocollo esteso di rigctld: si manda "+f" e
// torna "get_freq:\nFrequency: 14074000\nRPRT 0". Ogni risposta finisce con
// RPRT, e li' si sa se e' andata bene.
#pragma once

#include <QObject>
#include <QQueue>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

namespace decolog::core {

class RigControl : public QObject {
    Q_OBJECT

public:
    explicit RigControl(QObject* parent = nullptr);

    void connectTo(const QString& host, quint16 port);
    void disconnectFromRig();

    bool connected() const;
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    qint64 frequencyHz() const { return m_frequencyHz; }
    QString mode() const { return m_mode; }
    int speedWpm() const { return m_wpm; }
    // Cosa sta succedendo, in una riga da mostrare a chi guarda.
    QString status() const { return m_status; }

    // Chiede alla radio dove sta: frequenza, modo e velocita' del manipolatore.
    void refresh();
    void setFrequency(qint64 hz);
    void setMode(const QString& mode);
    void setPtt(bool on);
    // La velocita' del manipolatore della radio, in parole al minuto.
    void setSpeedWpm(int wpm);
    // Manda il testo in CW con il manipolatore della radio, e lo ferma.
    void sendMorse(const QString& text);
    void stopMorse();

signals:
    void changed();
    void failed(const QString& message);
    // Il testo e' stato preso dalla radio: e' partito in aria.
    void morseSent(const QString& text);

private:
    struct Pending {
        QString kind;   // "freq", "mode", "speed", "morse", "set"
        QString text;   // per il CW: quello che si e' mandato
    };

    void send(const QString& kind, const QString& command, const QString& text = {});
    void readFromRig();
    void handleReply(const QStringList& lines);
    void setStatus(const QString& text);

    QTcpSocket* m_socket;
    QTimer m_poll;
    QTimer m_retry;
    QQueue<Pending> m_pending;
    QStringList m_lines;      // righe della risposta in arrivo, fino a RPRT
    QByteArray m_buffer;

    QString m_host;
    quint16 m_port{4532};
    bool m_wanted{false};
    qint64 m_frequencyHz{0};
    QString m_mode;
    int m_wpm{0};
    QString m_status;
};

} // namespace decolog::core
