#include "core/MailSender.h"

#include <QDateTime>
#include <QSslSocket>
#include <QTimer>
#include <QUuid>

namespace decolog::core {

namespace {

// Quanto si aspetta un server che non risponde. Una casella lenta capita; una
// muta per mezzo minuto no.
constexpr int kTimeoutMs = 30'000;

// L'oggetto e il nome di chi manda possono avere accenti: in una intestazione
// non ci stanno, e si scrivono come dice la RFC 2047.
QByteArray encodedWord(const QString& text)
{
    for (const QChar c : text) {
        if (c.unicode() > 127)
            return "=?UTF-8?B?" + text.toUtf8().toBase64() + "?=";
    }
    return text.toUtf8();
}

// Un indirizzo con il nome davanti: Martino Merola <iu8lmc@gmail.com>.
QByteArray addressOf(const QString& name, const QString& address)
{
    if (name.trimmed().isEmpty())
        return address.toUtf8();
    return encodedWord(name) + " <" + address.toUtf8() + ">";
}

} // namespace

QByteArray buildMime(const MailAccount& account, const MailMessage& message)
{
    const QString from = account.fromAddress.isEmpty() ? account.user : account.fromAddress;
    const QByteArray boundary = "decodxlog-" + QUuid::createUuid().toByteArray(QUuid::Id128);

    QByteArray mime;
    mime += "From: " + addressOf(account.fromName, from) + "\r\n";
    mime += "To: " + message.to.toUtf8() + "\r\n";
    mime += "Subject: " + encodedWord(message.subject) + "\r\n";
    mime += "Date: " + QDateTime::currentDateTime().toString(Qt::RFC2822Date).toUtf8() + "\r\n";
    mime += "Message-ID: <" + QUuid::createUuid().toByteArray(QUuid::Id128) + "@decodxlog>\r\n";
    mime += "MIME-Version: 1.0\r\n";

    QByteArray body = message.body.toUtf8();
    // Nel corpo di un messaggio SMTP una riga che comincia con un punto
    // chiuderebbe il messaggio: si raddoppia, come dice la RFC.
    body.replace("\r\n", "\n");
    body.replace("\n", "\r\n");
    body.replace("\r\n.", "\r\n..");
    if (body.startsWith('.'))
        body.prepend('.');

    if (message.attachment.isEmpty()) {
        mime += "Content-Type: text/plain; charset=UTF-8\r\n";
        mime += "Content-Transfer-Encoding: 8bit\r\n\r\n";
        mime += body;
        return mime;
    }

    mime += "Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n\r\n";
    mime += "--" + boundary + "\r\n";
    mime += "Content-Type: text/plain; charset=UTF-8\r\n";
    mime += "Content-Transfer-Encoding: 8bit\r\n\r\n";
    mime += body + "\r\n";
    mime += "--" + boundary + "\r\n";
    mime += "Content-Type: " + message.attachmentType.toUtf8()
            + "; name=\"" + message.attachmentName.toUtf8() + "\"\r\n";
    mime += "Content-Transfer-Encoding: base64\r\n";
    mime += "Content-Disposition: attachment; filename=\"" + message.attachmentName.toUtf8() + "\"\r\n\r\n";
    // Righe da 76 caratteri: e' il limite che tutti i server accettano.
    const QByteArray encoded = message.attachment.toBase64();
    for (qsizetype i = 0; i < encoded.size(); i += 76)
        mime += encoded.mid(i, 76) + "\r\n";
    mime += "--" + boundary + "--\r\n";
    return mime;
}

MailSender::MailSender(QObject* parent)
    : QObject(parent)
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(kTimeoutMs);
    connect(m_timeout, &QTimer::timeout, this, [this] { fail(tr("the mail server did not answer")); });
}

MailSender::~MailSender() = default;

void MailSender::setAccount(const MailAccount& account)
{
    m_account = account;
}

void MailSender::send(const MailMessage& message)
{
    m_queue.enqueue(message);
    if (m_state == Idle)
        startNext();
}

void MailSender::cancel()
{
    m_queue.clear();
    if (m_socket)
        m_socket->abort();
    m_state = Idle;
    m_timeout->stop();
    emit finished();
}

void MailSender::startNext()
{
    if (m_queue.isEmpty()) {
        m_state = Idle;
        emit finished();
        return;
    }
    m_current = m_queue.dequeue();
    m_buffer.clear();
    m_capabilities.clear();
    // Lo stato si alza prima dei controlli: fail() non parla quando siamo
    // fermi, e senza questo un invio senza casella non diceva niente a nessuno.
    m_state = Connecting;

    if (m_account.host.trimmed().isEmpty() || m_account.user.trimmed().isEmpty()) {
        fail(tr("no outgoing mailbox is set up"));
        return;
    }
    if (m_current.to.trimmed().isEmpty()) {
        fail(tr("no email address for this station"));
        return;
    }

    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
    }
    m_socket = new QSslSocket(this);
    connect(m_socket, &QSslSocket::readyRead, this, [this] {
        m_buffer += m_socket->readAll();
        while (true) {
            const int end = m_buffer.indexOf("\r\n");
            if (end < 0)
                break;
            const QString line = QString::fromUtf8(m_buffer.left(end));
            m_buffer.remove(0, end + 2);
            onLine(line);
        }
    });
    connect(m_socket, &QSslSocket::errorOccurred, this,
            [this] { fail(m_socket ? m_socket->errorString() : tr("connection lost")); });
    connect(m_socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError>& errors) {
        // Il certificato della casella deve essere buono: se non lo e' non si
        // manda, perche' li' dentro ci sono le credenziali.
        QStringList what;
        for (const QSslError& e : errors)
            what << e.errorString();
        fail(tr("the mail server's certificate is not trusted (%1)").arg(what.join(QStringLiteral("; "))));
    });

    m_timeout->start();
    if (m_account.port == 465)
        m_socket->connectToHostEncrypted(m_account.host, static_cast<quint16>(m_account.port));
    else
        m_socket->connectToHost(m_account.host, static_cast<quint16>(m_account.port));
}

void MailSender::write(const QByteArray& line)
{
    if (!m_socket)
        return;
    m_socket->write(line + "\r\n");
    m_socket->flush();
    m_timeout->start();
}

void MailSender::onLine(const QString& line)
{
    // Una risposta puo' essere su piu' righe: "250-QUALCOSA" continua,
    // "250 QUALCOSA" chiude. Si aspetta quella che chiude.
    if (line.size() >= 4 && line.at(3) == QLatin1Char('-')) {
        m_capabilities << line.mid(4);
        return;
    }
    const int code = line.left(3).toInt();
    if (code >= 400) {
        fail(line);
        return;
    }

    const QString hello = QStringLiteral("EHLO decodxlog");
    switch (m_state) {
    case Connecting:
    case Greeting:
        m_state = Ehlo;
        write(hello.toUtf8());
        break;
    case Ehlo:
        m_capabilities << line.mid(4);
        // Sulla 465 si e' gia' cifrati; sulle altre si chiede STARTTLS, e se il
        // server non ce l'ha non si manda in chiaro: ci sono le credenziali.
        if (m_socket->isEncrypted()) {
            m_state = Auth;
            write("AUTH LOGIN");
        } else if (m_capabilities.filter(QStringLiteral("STARTTLS"), Qt::CaseInsensitive).isEmpty()) {
            fail(tr("the mail server does not offer an encrypted connection (STARTTLS)"));
        } else {
            m_state = StartTls;
            write("STARTTLS");
        }
        break;
    case StartTls:
        m_state = EhloAgain;
        m_socket->startClientEncryption();
        write(hello.toUtf8());
        break;
    case EhloAgain:
        m_state = Auth;
        write("AUTH LOGIN");
        break;
    case Auth:
        m_state = AuthUser;
        write(m_account.user.toUtf8().toBase64());
        break;
    case AuthUser:
        m_state = AuthPassword;
        write(m_account.password.toUtf8().toBase64());
        break;
    case AuthPassword:
        m_state = From;
        write("MAIL FROM:<" + (m_account.fromAddress.isEmpty() ? m_account.user : m_account.fromAddress).toUtf8() + ">");
        break;
    case From:
        m_state = Rcpt;
        write("RCPT TO:<" + m_current.to.trimmed().toUtf8() + ">");
        break;
    case Rcpt:
        m_state = Data;
        write("DATA");
        break;
    case Data:
        m_state = Body;
        write(buildMime(m_account, m_current) + "\r\n.");
        break;
    case Body:
        emit sent(m_current.tag, m_current.to);
        m_state = Quit;
        write("QUIT");
        break;
    case Quit:
        done();
        break;
    case Idle:
        break;
    }
}

void MailSender::fail(const QString& error)
{
    if (m_state == Idle)
        return;
    m_timeout->stop();
    const State was = m_state;
    m_state = Idle;
    if (m_socket)
        m_socket->abort();
    // Dopo il punto finale il messaggio e' partito: un errore nel saluto di
    // congedo non lo annulla, e dirlo fallito farebbe mandare la stessa QSL due
    // volte.
    if (was == Quit) {
        done();
        return;
    }
    emit failed(m_current.tag, m_current.to, error.trimmed());
    // La successiva si comincia dal giro dopo: cosi' una coda di cinquanta
    // che falliscono tutte non si annida cinquanta volte dentro se stessa.
    QTimer::singleShot(0, this, [this] { startNext(); });
}

void MailSender::done()
{
    m_timeout->stop();
    if (m_socket)
        m_socket->disconnectFromHost();
    m_state = Idle;
    startNext();
}

} // namespace decolog::core
