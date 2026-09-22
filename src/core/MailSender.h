// DecoDXLog — mandare una email con la QSL attaccata.
//
// Qt non ha un client SMTP, e per una funzione sola non vale la pena tirarsi
// dietro una libreria: il protocollo e' cinque comandi in fila, e qui ci sono.
// Si parla con la casella su TLS — con la cifratura dall'inizio sulla porta
// 465, o chiedendola con STARTTLS sulla 587, che sono i due modi in cui si
// trovano Gmail e quasi tutti gli altri.
//
// Una email per volta, in coda: un invio di cinquanta QSL non apre cinquanta
// connessioni, e se una casella dice di no si sa quale.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>

class QSslSocket;
class QTimer;

namespace decolog::core {

// Come si raggiunge la casella da cui si manda.
struct MailAccount {
    QString host;
    int     port{587};
    QString user;        // di solito l'indirizzo stesso
    QString password;    // per Gmail: la "app password", non quella dell'account
    QString fromName;    // "Martino Merola IU8LMC"
    QString fromAddress; // vuoto: si usa `user`
};

// Una email da mandare: testo semplice piu' un allegato, che qui e' la QSL.
struct MailMessage {
    QString    to;
    QString    subject;
    QString    body;
    QString    attachmentName;   // vuoto: nessun allegato
    QByteArray attachment;
    QString    attachmentType{QStringLiteral("image/png")};
    // Un riferimento di comodo per chi ha messo in coda (l'id del QSO).
    qint64     tag{0};
};

// Il messaggio MIME completo, pronto per il DATA. Sta qui fuori perche' si
// prova da solo: gli accenti nell'oggetto, l'allegato in base64, i punti a
// inizio riga che vanno raddoppiati.
QByteArray buildMime(const MailAccount& account, const MailMessage& message);

class MailSender : public QObject {
    Q_OBJECT

public:
    explicit MailSender(QObject* parent = nullptr);
    ~MailSender() override;

    void setAccount(const MailAccount& account);
    MailAccount account() const { return m_account; }

    // Mette in coda e comincia. Tornano `sent` o `failed` per ognuna.
    void send(const MailMessage& message);
    bool busy() const { return m_state != Idle; }
    int queued() const { return m_queue.size(); }
    void cancel();

signals:
    void sent(qint64 tag, const QString& to);
    void failed(qint64 tag, const QString& to, const QString& error);
    void finished();   // coda vuota

private:
    // I passi del dialogo con la casella, nell'ordine in cui avvengono.
    enum State { Idle, Connecting, Greeting, Ehlo, StartTls, EhloAgain,
                 Auth, AuthUser, AuthPassword, From, Rcpt, Data, Body, Quit };

    void startNext();
    void write(const QByteArray& line);
    void onLine(const QString& line);
    void fail(const QString& error);
    void done();

    MailAccount m_account;
    QQueue<MailMessage> m_queue;
    MailMessage m_current;
    QSslSocket* m_socket{nullptr};
    QTimer* m_timeout{nullptr};
    State m_state{Idle};
    QByteArray m_buffer;
    QStringList m_capabilities;
};

} // namespace decolog::core
