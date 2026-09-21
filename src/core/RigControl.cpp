#include "core/RigControl.h"

#include <QCoreApplication>
#include <QDebug>

#include <utility>

namespace decolog::core {

namespace {
// Quanto spesso si chiede alla radio dov'e'. Un secondo e mezzo: abbastanza da
// seguire il VFO, poco abbastanza da non intasare la seriale sotto rigctld.
constexpr int kPollMs = 1500;
constexpr int kRetryMs = 5000;
} // namespace

RigControl::RigControl(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, [this] {
        setStatus(tr("Radio connected (rigctld %1:%2)").arg(m_host).arg(m_port));
        m_pending.clear();
        m_lines.clear();
        m_buffer.clear();
        refresh();
        m_poll.start();
        emit changed();
    });
    connect(m_socket, &QTcpSocket::readyRead, this, &RigControl::readFromRig);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!m_wanted)
            return;
        setStatus(tr("Radio not reachable: %1").arg(m_socket->errorString()));
        emit failed(m_status);
        m_poll.stop();
        m_retry.start();
        emit changed();
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        m_poll.stop();
        if (m_wanted)
            m_retry.start();
        emit changed();
    });

    m_poll.setInterval(kPollMs);
    connect(&m_poll, &QTimer::timeout, this, &RigControl::refresh);

    m_retry.setInterval(kRetryMs);
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, [this] {
        if (m_wanted && m_socket->state() == QAbstractSocket::UnconnectedState)
            m_socket->connectToHost(m_host, m_port);
    });
}

bool RigControl::connected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void RigControl::connectTo(const QString& host, quint16 port)
{
    m_host = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    m_port = port > 0 ? port : 4532;
    m_wanted = true;
    m_retry.stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->abort();
    setStatus(tr("Looking for the radio on %1:%2…").arg(m_host).arg(m_port));
    emit changed();
    m_socket->connectToHost(m_host, m_port);
}

void RigControl::disconnectFromRig()
{
    m_wanted = false;
    m_poll.stop();
    m_retry.stop();
    m_socket->abort();
    m_frequencyHz = 0;
    m_mode.clear();
    setStatus(tr("Radio off"));
    emit changed();
}

void RigControl::setStatus(const QString& text)
{
    m_status = text;
}

void RigControl::send(const QString& kind, const QString& command, int values, const QString& text)
{
    if (!connected())
        return;
    m_pending.enqueue({kind, text, values});
    m_socket->write(QStringLiteral("+%1\n").arg(command).toUtf8());
}

void RigControl::refresh()
{
    // Se la radio non risponde piu', la coda cresce a vuoto: meglio fermarsi.
    if (m_pending.size() > 8)
        return;
    send(QStringLiteral("freq"), QStringLiteral("f"), 1);
    send(QStringLiteral("mode"), QStringLiteral("m"), 2);
    send(QStringLiteral("speed"), QStringLiteral("l KEYSPD"), 1);
}

void RigControl::setFrequency(qint64 hz)
{
    if (hz > 0)
        send(QStringLiteral("set"), QStringLiteral("F %1").arg(hz), 0);
}

void RigControl::setMode(const QString& mode)
{
    const QString clean = mode.trimmed().toUpper();
    if (!clean.isEmpty())
        send(QStringLiteral("set"), QStringLiteral("M %1 0").arg(clean), 0);
}

void RigControl::setPtt(bool on)
{
    send(QStringLiteral("set"), QStringLiteral("T %1").arg(on ? 1 : 0), 0);
}

void RigControl::setSpeedWpm(int wpm)
{
    const int clamped = qBound(5, wpm, 60);
    m_wpm = clamped;
    send(QStringLiteral("set"), QStringLiteral("L KEYSPD %1").arg(clamped), 0);
    emit changed();
}

void RigControl::sendMorse(const QString& text)
{
    const QString clean = text.trimmed();
    if (clean.isEmpty())
        return;
    if (!connected()) {
        emit failed(tr("The radio is not connected: nothing sent in CW"));
        return;
    }
    send(QStringLiteral("morse"), QStringLiteral("b %1").arg(clean), 0, clean);
}

void RigControl::stopMorse()
{
    // Il nome lungo dei comandi di rigctld vuole la barra rovescia davanti.
    send(QStringLiteral("set"), QStringLiteral("\\stop_morse"), 0);
}

void RigControl::readFromRig()
{
    m_buffer += m_socket->readAll();
    while (true) {
        const int end = m_buffer.indexOf('\n');
        if (end < 0)
            break;
        const QString line = QString::fromUtf8(m_buffer.left(end)).trimmed();
        m_buffer.remove(0, end + 1);
        m_lines << line;

        if (line.startsWith(QLatin1String("RPRT"))) {
            const QStringList block = m_lines;
            m_lines.clear();
            handleReply(block);
            continue;
        }

        // Rigctld "vero" chiude ogni risposta con RPRT; altri ponti CAT — come
        // quello di Decodium — rispondono col valore nudo e basta. Allora si
        // conta: quando sono arrivate tutte le righe che quella domanda si
        // aspettava, la risposta e' finita lo stesso.
        if (m_pending.isEmpty())
            continue;
        const int wanted = m_pending.head().values;
        if (wanted <= 0)
            continue;
        // Una riga coi due punti ("get_level: KEYSPD", "Mode: CW") dice che
        // questa risposta e' di rigctld vero, e quello il RPRT lo manda sempre:
        // qui non si conta niente e si aspetta.
        //
        // Contare lo stesso era un guaio serio: la risposta si chiudeva al
        // valore nudo, e il RPRT che arrivava subito dopo si prendeva la
        // domanda seguente. Da li' in poi ogni risposta finiva sulla domanda
        // sbagliata — e l'errore "questa radio non manipola" spariva del tutto.
        bool extended = false;
        for (const QString& seen : std::as_const(m_lines)) {
            if (seen.contains(QLatin1Char(':')))
                extended = true;
        }
        if (extended)
            continue;
        if (m_lines.size() >= wanted) {
            QStringList block = m_lines;
            block << QStringLiteral("RPRT 0");
            m_lines.clear();
            handleReply(block);
        }
    }
}

void RigControl::handleReply(const QStringList& lines)
{
    if (qEnvironmentVariableIsSet("DECODXLOG_RIG_DEBUG"))
        qDebug() << "reply" << lines << "pending" << m_pending.size();
    if (m_pending.isEmpty())
        return;
    const Pending what = m_pending.dequeue();
    const int result = lines.last().mid(4).trimmed().toInt();

    if (result != 0) {
        // -1 e' "questa radio non lo sa fare": per il CW vuol dire che il
        // manipolatore della radio non si comanda da qui.
        if (what.kind == QLatin1String("morse")) {
            // -11 e' "non lo so fare": il ponte CAT o la radio non manipolano.
            setStatus(tr("This CAT link does not key CW (rigctld: %1)").arg(result));
            emit morseUnsupported();
            emit failed(tr("The radio did not take the CW text (rigctld: %1). Not every radio — and "
                           "not every CAT bridge — can key CW: for the macros you need rigctld "
                           "talking to the radio itself.").arg(result));
        } else if (what.kind != QLatin1String("speed")) {
            emit failed(tr("The radio answered with an error (rigctld: %1)").arg(result));
        }
        return;
    }

    auto valueOf = [&lines](const QString& key) {
        for (const QString& line : lines) {
            if (line.startsWith(key + QLatin1Char(':')))
                return line.mid(key.size() + 1).trimmed();
        }
        return QString();
    };

    // Senza etichette (risposta nuda) si prendono le righe in ordine.
    QStringList plain;
    for (const QString& line : lines) {
        if (line.startsWith(QLatin1String("RPRT")) || line.endsWith(QLatin1Char(':'))
            || line.contains(QLatin1String(": ")))
            continue;
        plain << line;
    }

    bool moved = false;
    if (what.kind == QLatin1String("freq")) {
        const QString label = valueOf(QStringLiteral("Frequency"));
        const qint64 hz = (label.isEmpty() && !plain.isEmpty() ? plain.first() : label).toLongLong();
        if (hz > 0 && hz != m_frequencyHz) {
            m_frequencyHz = hz;
            moved = true;
        }
    } else if (what.kind == QLatin1String("mode")) {
        const QString label = valueOf(QStringLiteral("Mode"));
        const QString mode = label.isEmpty() && !plain.isEmpty() ? plain.first() : label;
        if (!mode.isEmpty() && mode != m_mode) {
            m_mode = mode;
            moved = true;
        }
    } else if (what.kind == QLatin1String("speed")) {
        // Il livello torna come numero nudo, prima di RPRT.
        for (const QString& line : lines) {
            bool ok = false;
            const int value = line.toInt(&ok);
            if (ok && value > 0 && value != m_wpm) {
                m_wpm = value;
                moved = true;
            }
        }
    } else if (what.kind == QLatin1String("morse")) {
        emit morseSent(what.text);
    }

    if (moved)
        emit changed();
}

} // namespace decolog::core
