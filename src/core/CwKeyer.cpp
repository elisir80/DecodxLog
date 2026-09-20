#include "core/CwKeyer.h"

#include <QElapsedTimer>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>

namespace decolog::core {

namespace {

// La tavola del codice Morse. Quella che manca da qui non si manda: meglio un
// carattere saltato che un carattere sbagliato in aria.
struct Letter {
    char c;
    const char* code;
};

constexpr Letter kTable[] = {
    {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},   {'E', "."},
    {'F', "..-."},  {'G', "--."},   {'H', "...."},  {'I', ".."},    {'J', ".---"},
    {'K', "-.-"},   {'L', ".-.."},  {'M', "--"},    {'N', "-."},    {'O', "---"},
    {'P', ".--."},  {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},   {'T', "-"},
    {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},  {'Y', "-.--"},
    {'Z', "--.."},
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"},
    {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
    {'/', "-..-."}, {'?', "..--.."},{',', "--..--"},{'.', ".-.-.-"},{'=', "-...-"},
    {'+', ".-.-."}, {'-', "-....-"},{'@', ".--.-."},{':', "---..."},{'\'', ".----."},
};

// Aspetta il tempo giusto: si dorme a colpi corti — l'attesa lunga su Windows
// sbaglia di dieci millisecondi e piu' — e gli ultimi due si contano fermi qui,
// che e' l'unico modo di avere una spaziatura che non traballa.
void waitFor(qint64 microseconds, const std::atomic_bool& stopped)
{
    QElapsedTimer clock;
    clock.start();
    while (!stopped) {
        const qint64 left = microseconds - clock.nsecsElapsed() / 1000;
        if (left <= 0)
            return;
        if (left > 2000)
            QThread::usleep(static_cast<unsigned long>(qMin<qint64>(left - 2000, 5000)));
        else
            QThread::yieldCurrentThread();
    }
}

} // namespace

// ── Il lavoratore: vive nel suo thread e tiene in mano la porta ─────────────

class CwKeyerWorker : public QObject {
    Q_OBJECT

public:
    ~CwKeyerWorker() override { closePort(); }

    std::atomic_bool stopped{false};

public slots:
    void openPort(const QString& name, const QString& line)
    {
        closePort();
        m_line = line.trimmed().toUpper() == QLatin1String("RTS") ? Line::Rts : Line::Dtr;
        m_port = new QSerialPort(name);
        // Niente handshake: il piedino lo comandiamo noi, e se lo comanda anche
        // il driver si accavallano.
        m_port->setBaudRate(QSerialPort::Baud9600);
        m_port->setFlowControl(QSerialPort::NoFlowControl);
        if (!m_port->open(QIODevice::ReadWrite)) {
            const QString why = m_port->errorString();
            delete m_port;
            m_port = nullptr;
            emit failed(tr("Cannot open %1: %2").arg(name, why));
            emit opened(false);
            return;
        }
        key(false);
        emit opened(true);
    }

    void closePort()
    {
        if (!m_port)
            return;
        key(false);
        m_port->close();
        delete m_port;
        m_port = nullptr;
    }

    void enqueue(const QString& text, int wpm)
    {
        {
            QMutexLocker lock(&m_mutex);
            m_queue.enqueue(qMakePair(text, qBound(5, wpm, 60)));
        }
        QMetaObject::invokeMethod(this, "drain", Qt::QueuedConnection);
    }

    void clearQueue()
    {
        QMutexLocker lock(&m_mutex);
        m_queue.clear();
    }

    Q_INVOKABLE void drain()
    {
        if (m_busy)
            return;
        m_busy = true;
        forever {
            QString text;
            int wpm = 24;
            {
                QMutexLocker lock(&m_mutex);
                if (m_queue.isEmpty())
                    break;
                const auto next = m_queue.dequeue();
                text = next.first;
                wpm = next.second;
            }
            sendNow(text, wpm);
            emit textSent(text);
        }
        m_busy = false;
        emit finished();
    }

signals:
    void opened(bool ok);
    void charSent(const QString& character);
    void textSent(const QString& text);
    void finished();
    void failed(const QString& why);

private:
    enum class Line { Dtr, Rts };

    void key(bool down)
    {
        if (!m_port)
            return;
        if (m_line == Line::Rts)
            m_port->setRequestToSend(down);
        else
            m_port->setDataTerminalReady(down);
    }

    void sendNow(const QString& text, int wpm)
    {
        if (!m_port)
            return;
        // Il punto: 1200 diviso le parole al minuto, come dice PARIS.
        const qint64 dot = 1200'000 / wpm;     // microsecondi
        for (const QChar raw : text.toUpper()) {
            if (stopped)
                break;
            if (raw == QLatin1Char(' ')) {
                // Fra due parole sette punti; tre sono gia' passati con
                // l'ultima lettera.
                waitFor(dot * 4, stopped);
                emit charSent(QStringLiteral(" "));
                continue;
            }
            const QString code = CwKeyer::morseOf(raw);
            if (code.isEmpty())
                continue;
            for (qsizetype i = 0; i < code.size(); ++i) {
                if (stopped)
                    break;
                key(true);
                waitFor(code.at(i) == QLatin1Char('-') ? dot * 3 : dot, stopped);
                key(false);
                // Fra un elemento e l'altro un punto di silenzio.
                if (i + 1 < code.size())
                    waitFor(dot, stopped);
            }
            emit charSent(QString(raw));
            // Fra due lettere tre punti: uno l'ha gia' fatto l'elemento.
            waitFor(dot * 2, stopped);
        }
        key(false);
    }

    QSerialPort* m_port{nullptr};
    Line m_line{Line::Dtr};
    QQueue<QPair<QString, int>> m_queue;
    QMutex m_mutex;
    bool m_busy{false};
};

// ── La faccia pubblica ──────────────────────────────────────────────────────

CwKeyer::CwKeyer(QObject* parent)
    : QObject(parent)
    , m_thread(new QThread(this))
    , m_worker(new CwKeyerWorker)
{
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &CwKeyerWorker::charSent, this, &CwKeyer::charSent);
    connect(m_worker, &CwKeyerWorker::textSent, this, &CwKeyer::textSent);
    connect(m_worker, &CwKeyerWorker::failed, this, &CwKeyer::failed);
    connect(m_worker, &CwKeyerWorker::finished, this, [this] {
        m_sending = false;
        emit finished();
    });
    connect(m_worker, &CwKeyerWorker::opened, this, [this](bool ok) { m_open = ok; });
    // La manipolazione vuole precedenza: un ritardo qui si sente in aria.
    m_thread->start(QThread::TimeCriticalPriority);
}

CwKeyer::~CwKeyer()
{
    stop();
    QMetaObject::invokeMethod(m_worker, "closePort", Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait(2000);
}

bool CwKeyer::open(const QString& port, const QString& line)
{
    if (port.trimmed().isEmpty()) {
        emit failed(tr("No serial port chosen for the CW keyer"));
        return false;
    }
    m_port = port.trimmed();
    m_open = false;
    QMetaObject::invokeMethod(m_worker, "openPort", Qt::BlockingQueuedConnection,
                              Q_ARG(QString, m_port), Q_ARG(QString, line));
    return m_open;
}

void CwKeyer::close()
{
    stop();
    QMetaObject::invokeMethod(m_worker, "closePort", Qt::BlockingQueuedConnection);
    m_open = false;
    m_port.clear();
}

void CwKeyer::send(const QString& text, int wpm)
{
    if (!m_open || text.trimmed().isEmpty())
        return;
    m_worker->stopped = false;
    m_sending = true;
    m_worker->enqueue(text, wpm);
}

void CwKeyer::stop()
{
    m_worker->stopped = true;
    m_worker->clearQueue();
    m_sending = false;
}

QStringList CwKeyer::ports()
{
    QStringList out;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts())
        out << info.portName();
    out.sort();
    return out;
}

int CwKeyer::millisFor(const QString& text, int wpm)
{
    const int dot = 1200 / qBound(5, wpm, 60);
    int total = 0;
    bool first = true;
    for (const QChar c : text.toUpper()) {
        if (c == QLatin1Char(' ')) {
            total += dot * 7;          // sette punti fra due parole
            first = true;
            continue;
        }
        const QString code = morseOf(c);
        if (code.isEmpty())
            continue;
        if (!first)
            total += dot * 3;          // tre punti fra una lettera e l'altra
        first = false;
        for (qsizetype i = 0; i < code.size(); ++i) {
            total += code.at(i) == QLatin1Char('-') ? dot * 3 : dot;
            if (i + 1 < code.size())
                total += dot;          // un punto fra gli elementi
        }
    }
    return total;
}

QString CwKeyer::morseOf(QChar c)
{
    const char upper = c.toUpper().toLatin1();
    for (const Letter& letter : kTable) {
        if (letter.c == upper)
            return QString::fromLatin1(letter.code);
    }
    return {};
}

} // namespace decolog::core

#include "CwKeyer.moc"
