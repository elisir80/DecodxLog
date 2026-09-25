#include "core/TciControl.h"

#include <QDebug>

namespace decolog::core {

namespace {
constexpr int kRetryMs = 5000;
// Il programma della radio manda lo stato e poi "ready;". Chi non lo manda
// (qualche server TCI piu' semplice) non deve bloccare i comandi per sempre.
constexpr int kReadyWaitMs = 2000;
constexpr int kDefaultPort = 40001;
} // namespace

TciControl::TciControl(QObject* parent)
    : RigLink(parent)
{
    connect(&m_socket, &QWebSocket::connected, this, [this] {
        m_ready = false;
        setStatus(tr("Radio connected (TCI %1), waiting for it to be ready…").arg(m_url.authority()));
        m_readyWait.start();
        emit changed();
    });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &TciControl::handleText);
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!m_wanted)
            return;
        setStatus(tr("Radio not reachable via TCI: %1").arg(m_socket.errorString()));
        emit failed(m_status);
        m_retry.start();
        emit changed();
    });
    connect(&m_socket, &QWebSocket::disconnected, this, [this] {
        m_ready = false;
        m_readyWait.stop();
        if (m_wanted)
            m_retry.start();
        emit changed();
    });

    m_retry.setInterval(kRetryMs);
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, [this] {
        if (m_wanted && m_socket.state() == QAbstractSocket::UnconnectedState)
            m_socket.open(m_url);
    });
    m_readyWait.setInterval(kReadyWaitMs);
    m_readyWait.setSingleShot(true);
    connect(&m_readyWait, &QTimer::timeout, this, [this] {
        if (!m_ready && connected()) {
            m_ready = true;
            setStatus(tr("Radio connected (TCI %1)").arg(m_url.authority()));
            flushQueue();
            refresh();
            emit changed();
        }
    });
}

QUrl TciControl::urlFor(const QString& address)
{
    QString text = address.trimmed();
    if (text.isEmpty())
        text = QStringLiteral("127.0.0.1");
    if (!text.contains(QLatin1String("://")))
        text.prepend(QStringLiteral("ws://"));
    QUrl url(text);
    if (url.scheme() == QLatin1String("http"))
        url.setScheme(QStringLiteral("ws"));
    else if (url.scheme() == QLatin1String("https"))
        url.setScheme(QStringLiteral("wss"));
    if (url.port() <= 0)
        url.setPort(kDefaultPort);
    return url;
}

QString TciControl::tciModulation(const QString& hamlibMode)
{
    const QString m = hamlibMode.trimmed().toUpper();
    if (m == QLatin1String("CW") || m == QLatin1String("CWR"))
        return QStringLiteral("cw");
    if (m == QLatin1String("LSB"))
        return QStringLiteral("lsb");
    if (m == QLatin1String("AM"))
        return QStringLiteral("am");
    if (m == QLatin1String("FM") || m == QLatin1String("PKTFM"))
        return QStringLiteral("nfm");
    if (m == QLatin1String("WFM"))
        return QStringLiteral("wfm");
    if (m == QLatin1String("PKTLSB") || m == QLatin1String("RTTY") || m == QLatin1String("RTTYR"))
        return QStringLiteral("digl");   // l'RTTY in AFSK si fa in LSB
    if (m == QLatin1String("PKTUSB"))
        return QStringLiteral("digu");
    return QStringLiteral("usb");
}

QString TciControl::hamlibMode(const QString& tciModulation)
{
    const QString m = tciModulation.trimmed().toLower();
    if (m == QLatin1String("cw"))
        return QStringLiteral("CW");
    if (m == QLatin1String("lsb"))
        return QStringLiteral("LSB");
    if (m == QLatin1String("usb"))
        return QStringLiteral("USB");
    if (m == QLatin1String("am") || m == QLatin1String("sam") || m == QLatin1String("dsb"))
        return QStringLiteral("AM");
    if (m == QLatin1String("nfm") || m == QLatin1String("wfm"))
        return QStringLiteral("FM");
    if (m == QLatin1String("digl"))
        return QStringLiteral("PKTLSB");
    if (m == QLatin1String("digu"))
        return QStringLiteral("PKTUSB");
    return m.toUpper();
}

bool TciControl::connected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void TciControl::connectTo(const QString& address, int trx)
{
    m_url = urlFor(address);
    m_trx = qMax(0, trx);
    m_wanted = true;
    m_ready = false;
    m_queue.clear();
    m_retry.stop();
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();
    setStatus(tr("Looking for the radio via TCI on %1…").arg(m_url.authority()));
    emit changed();
    m_socket.open(m_url);
}

void TciControl::disconnectFromRig()
{
    m_wanted = false;
    m_ready = false;
    m_retry.stop();
    m_readyWait.stop();
    m_queue.clear();
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.close();
    m_frequencyHz = 0;
    m_mode.clear();
    m_transmitting = false;
    setStatus(tr("Radio off"));
    emit changed();
}

void TciControl::setStatus(const QString& text)
{
    m_status = text;
}

void TciControl::send(const QString& command)
{
    if (!connected())
        return;
    if (!m_ready) {
        m_queue << command;
        return;
    }
    if (qEnvironmentVariableIsSet("DECODXLOG_RIG_DEBUG"))
        qDebug() << "tci >" << command;
    m_socket.sendTextMessage(command);
}

void TciControl::flushQueue()
{
    const QStringList pending = m_queue;
    m_queue.clear();
    for (const QString& command : pending)
        send(command);
}

void TciControl::refresh()
{
    // TCI manda da se' quello che cambia; a chiedere si riceve lo stato di
    // adesso (un comando senza il valore e' una domanda).
    send(QStringLiteral("vfo:%1,0;").arg(m_trx));
    send(QStringLiteral("modulation:%1;").arg(m_trx));
}

void TciControl::setFrequency(qint64 hz)
{
    if (hz > 0)
        send(QStringLiteral("vfo:%1,0,%2;").arg(m_trx).arg(hz));
}

void TciControl::setMode(const QString& mode)
{
    if (!mode.trimmed().isEmpty())
        send(QStringLiteral("modulation:%1,%2;").arg(m_trx).arg(tciModulation(mode)));
}

void TciControl::setPtt(bool on)
{
    send(QStringLiteral("trx:%1,%2;").arg(m_trx).arg(on ? QStringLiteral("true") : QStringLiteral("false")));
}

void TciControl::setSpeedWpm(int wpm)
{
    const int clamped = qBound(5, wpm, 60);
    m_wpm = clamped;
    send(QStringLiteral("cw_macros_speed:%1;").arg(clamped));
    emit changed();
}

void TciControl::sendMorse(const QString& text)
{
    // Il punto e virgola chiude il comando e i due punti lo aprono: nel testo
    // non ci possono stare.
    QString clean = text.trimmed().toUpper();
    clean.remove(QLatin1Char(';'));
    clean.replace(QLatin1Char(':'), QLatin1Char(' '));
    if (clean.isEmpty())
        return;
    if (!connected()) {
        emit failed(tr("The radio is not connected: nothing sent in CW"));
        return;
    }
    send(QStringLiteral("cw_macros:%1,%2;").arg(m_trx).arg(clean));
    emit morseSent(clean);
}

void TciControl::stopMorse()
{
    send(QStringLiteral("cw_macros_stop;"));
}

void TciControl::handleText(const QString& text)
{
    if (qEnvironmentVariableIsSet("DECODXLOG_RIG_DEBUG"))
        qDebug() << "tci <" << text;
    // Un messaggio puo' portare piu' comandi, ognuno chiuso dal punto e virgola.
    for (const QString& part : text.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QString command = part.trimmed();
        if (command.isEmpty())
            continue;
        const int colon = command.indexOf(QLatin1Char(':'));
        const QString name = (colon < 0 ? command : command.left(colon)).trimmed().toLower();
        const QStringList args = colon < 0 ? QStringList()
                                           : command.mid(colon + 1).split(QLatin1Char(','));
        handleCommand(name, args);
    }
}

void TciControl::handleCommand(const QString& name, const QStringList& args)
{
    auto argAt = [&args](int i) { return i < args.size() ? args.at(i).trimmed() : QString(); };
    bool moved = false;

    if (name == QLatin1String("ready")) {
        m_readyWait.stop();
        if (!m_ready) {
            m_ready = true;
            setStatus(m_device.isEmpty() ? tr("Radio connected (TCI %1)").arg(m_url.authority())
                                         : tr("Radio connected: %1 (TCI %2)").arg(m_device, m_url.authority()));
            flushQueue();
            refresh();
            moved = true;
        }
    } else if (name == QLatin1String("device")) {
        m_device = argAt(0);
        if (m_ready)
            setStatus(tr("Radio connected: %1 (TCI %2)").arg(m_device, m_url.authority()));
        moved = true;
    } else if (name == QLatin1String("vfo")) {
        // vfo:ricevitore,canale,hz — il canale 0 e' il VFO A.
        if (argAt(0).toInt() == m_trx && argAt(1).toInt() == 0) {
            const qint64 hz = static_cast<qint64>(argAt(2).toDouble());
            if (hz > 0 && hz != m_frequencyHz) {
                m_frequencyHz = hz;
                moved = true;
            }
        }
    } else if (name == QLatin1String("dds") && m_frequencyHz <= 0) {
        // Prima del vfo il centro della banda e' meglio di niente.
        if (argAt(0).toInt() == m_trx) {
            const qint64 hz = static_cast<qint64>(argAt(1).toDouble());
            if (hz > 0) {
                m_frequencyHz = hz;
                moved = true;
            }
        }
    } else if (name == QLatin1String("modulation")) {
        if (argAt(0).toInt() == m_trx && !argAt(1).isEmpty()) {
            const QString mode = hamlibMode(argAt(1));
            if (mode != m_mode) {
                m_mode = mode;
                moved = true;
            }
        }
    } else if (name == QLatin1String("cw_macros_speed")) {
        const int wpm = argAt(0).toInt();
        if (wpm > 0 && wpm != m_wpm) {
            m_wpm = wpm;
            moved = true;
        }
    } else if (name == QLatin1String("trx")) {
        if (argAt(0).toInt() == m_trx) {
            const bool on = argAt(1).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
            if (on != m_transmitting) {
                m_transmitting = on;
                moved = true;
            }
        }
    }

    if (moved)
        emit changed();
}

} // namespace decolog::core
