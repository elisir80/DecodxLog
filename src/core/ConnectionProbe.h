// DecoDXLog — «perche' questo nodo non si collega?», in tre passi.
//
// Un nodo del cluster che per qualcuno non va (la pagina del sito che dice
// ERR_CONNECTION_CLOSED, il cluster che non porta mai uno spot) puo' essere
// spento, o fuori strada il DNS, o chiusa la porta, o un antivirus (AVG, Avast…)
// o un firewall che tiene la connessione. Da fuori non si distingue, e
// l'operatore scrive «non funziona». Qui si prova un passo alla volta e si dice
// dove si ferma:
//   1. il nome diventa un indirizzo?
//   2. la porta risponde?
//   3. il nodo dice qualcosa, subito (il «login:» di un nodo telnet)?
// «Collegato e subito chiuso» o «collegato e muto» davanti a un nodo che per
// altri risponde sono il segno tipico di un antivirus in mezzo.
#pragma once

#include <QObject>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

namespace decolog::core {

class ConnectionProbe : public QObject {
    Q_OBJECT

public:
    enum class Verdict {
        Ok,            // il nodo ha parlato
        DnsFailed,     // il nome non diventa un indirizzo
        Refused,       // la porta e' chiusa o rifiuta
        TimedOut,      // nessuna risposta dalla rete
        ClosedAtOnce,  // collegato, e chiuso prima che il nodo dicesse niente
        Silent,        // collegato, e il nodo non dice niente
        Error,         // un altro errore di rete
    };
    struct Result {
        Verdict verdict{Verdict::Error};
        QStringList steps;      // una riga per passo, gia' tradotta
        QString address;        // l'indirizzo scelto, se c'e'
        QString banner;         // la prima riga del nodo, se ha parlato
        QString hint;           // cosa fare, gia' tradotto (vuoto se va tutto bene)
        QString text() const;   // tutto insieme, per mostrarlo
    };

    explicit ConnectionProbe(QObject* parent = nullptr);

    // I tempi: per le prove non ci sono otto secondi da buttare.
    void setTimeouts(int connectMs, int bannerMs)
    {
        m_connectMs = connectMs;
        m_bannerMs = bannerMs;
    }
    bool busy() const { return m_busy; }
    void start(const QString& host, quint16 port);

signals:
    void finished(const decolog::core::ConnectionProbe::Result& result);

private:
    void connectTo(const QHostAddress& address);
    void finish(Verdict verdict);

    int m_connectMs{8000};
    int m_bannerMs{6000};
    bool m_busy{false};
    QString m_host;
    quint16 m_port{0};
    Result m_result;
    QTcpSocket* m_socket{nullptr};
    QTimer m_timer;
    bool m_connected{false};
    bool m_gotData{false};
    int m_lookupId{-1};
};

} // namespace decolog::core

Q_DECLARE_METATYPE(decolog::core::ConnectionProbe::Result)
