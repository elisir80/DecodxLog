#include "app/RigController.h"

#include "core/QslUpload.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QMediaDevices>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QVarLengthArray>

#include <algorithm>
#include <memory>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace decolog::app {

using core::RigControl;

RigController::RigController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    QSettings s;
    m_enabled = s.value(QStringLiteral("rig/enabled"), false).toBool();
    m_host = s.value(QStringLiteral("rig/host"), QStringLiteral("127.0.0.1")).toString();
    m_port = s.value(QStringLiteral("rig/port"), 4532).toInt();
    m_wpm = s.value(QStringLiteral("cw/wpm"), 24).toInt();
    m_link = s.value(QStringLiteral("rig/link"), QStringLiteral("network")).toString();
    m_serialPort = s.value(QStringLiteral("rig/serialPort")).toString();
    m_rigModel = s.value(QStringLiteral("rig/model"), 0).toInt();
    m_baud = s.value(QStringLiteral("rig/baud"), 38400).toInt();
    m_pttType = s.value(QStringLiteral("rig/pttType"), QStringLiteral("RIG")).toString();
    m_keyerPort = s.value(QStringLiteral("rig/keyerPort")).toString();
    m_keyerLine = s.value(QStringLiteral("rig/keyerLine"), QStringLiteral("DTR")).toString();
    connect(&m_keyer, &core::CwKeyer::failed, this, [this](const QString& why) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CW"), why, QStringLiteral("warning"));
        emit stateChanged();
    });
    if (!m_keyerPort.isEmpty())
        QTimer::singleShot(0, this, [this] { openKeyer(); });
    m_pttPort = s.value(QStringLiteral("rig/pttPort")).toString();
    m_audioInput = s.value(QStringLiteral("cw/audioInput")).toString();
    loadMacros();

    connect(&m_rig, &RigControl::changed, this, &RigController::stateChanged);
    connect(&m_rig, &RigControl::failed, this, [this](const QString& message) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CAT"), message, QStringLiteral("warning"));
        emit stateChanged();
    });
    connect(&m_rig, &RigControl::morseUnsupported, this, [this] {
        m_canKeyCw = false;
        emit stateChanged();
    });
    connect(&m_rig, &RigControl::morseSent, this, [this](const QString& text) {
        m_canKeyCw = true;
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CW"), tr("Sent: %1").arg(text), QStringLiteral("info"));
    });
}

void RigController::start()
{
    if (m_enabled)
        connectNow();
}

QString RigController::frequencyLabel() const
{
    const qint64 hz = m_rig.frequencyHz();
    if (hz <= 0)
        return QStringLiteral("—");
    return QLocale().toString(hz / 1000000.0, 'f', 6) + QStringLiteral(" MHz");
}

void RigController::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    QSettings().setValue(QStringLiteral("rig/enabled"), on);
    if (on)
        connectNow();
    else
        m_rig.disconnectFromRig();
    emit changed();
    emit stateChanged();
}

void RigController::setHost(const QString& host)
{
    const QString clean = host.trimmed();
    if (clean == m_host)
        return;
    m_host = clean;
    QSettings().setValue(QStringLiteral("rig/host"), clean);
    if (m_enabled)
        connectNow();
    emit changed();
}

void RigController::setPort(int port)
{
    if (port == m_port || port <= 0 || port > 65535)
        return;
    m_port = port;
    QSettings().setValue(QStringLiteral("rig/port"), port);
    if (m_enabled)
        connectNow();
    emit changed();
}

QVariantMap RigController::decodiumCat() const
{
    // Decodium tiene le sue nel registro, sotto "radio/manual-cat".
    QSettings decodium(QStringLiteral("HKEY_CURRENT_USER\\Software\\Decodium\\DECODIUM SDR\\radio\\manual-cat"),
                       QSettings::NativeFormat);
    QVariantMap out;
    out.insert(QStringLiteral("port"), decodium.value(QStringLiteral("port")).toString());
    out.insert(QStringLiteral("baud"), decodium.value(QStringLiteral("baud")).toInt());
    out.insert(QStringLiteral("driver"), decodium.value(QStringLiteral("driverId")).toString());
    return out;
}

void RigController::probeRadio()
{
    if (m_probeIndex >= 0)
        return;
    const QString exe = core::qsl::findRigctld();
    if (exe.isEmpty()) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CAT"), tr("Hamlib not found: install it first"),
                           QStringLiteral("warning"));
        return;
    }
    if (m_rigModel <= 0) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CAT"), tr("Pick the radio model first"), QStringLiteral("warning"));
        return;
    }

    // Prima la porta e la velocita' di adesso, poi quella di Decodium, poi
    // tutte le altre: di solito la prima o la seconda basta.
    const QStringList ports = serialPorts();
    QList<int> speeds{m_baud, 38400, 19200, 9600, 115200, 4800};
    m_probe.clear();
    QStringList order;
    if (!m_serialPort.isEmpty())
        order << m_serialPort;
    const QString fromDecodium = decodiumCat().value(QStringLiteral("port")).toString();
    if (!fromDecodium.isEmpty() && !order.contains(fromDecodium))
        order << fromDecodium;
    for (const QString& port : ports) {
        if (!order.contains(port))
            order << port;
    }
    for (const QString& port : std::as_const(order)) {
        QList<int> seen;
        for (const int baud : std::as_const(speeds)) {
            if (baud <= 0 || seen.contains(baud))
                continue;
            seen << baud;
            m_probe << QVariantMap{{QStringLiteral("port"), port}, {QStringLiteral("baud"), baud}};
        }
    }
    if (m_probe.isEmpty()) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CAT"), tr("No serial port on this computer"), QStringLiteral("warning"));
        return;
    }

    m_rig.disconnectFromRig();
    m_probeIndex = 0;
    if (m_ctx.activity) {
        m_ctx.activity(QStringLiteral("CAT"),
                       tr("Looking for the radio on %n port(s)…", nullptr, static_cast<int>(order.size())),
                       QStringLiteral("info"));
    }
    emit stateChanged();
    probeNext();
}

void RigController::probeNext()
{
    m_probeSocket.reset();
    if (m_probeProcess) {
        m_probeProcess->kill();
        m_probeProcess->waitForFinished(1500);
        m_probeProcess.reset();
    }
    if (m_probeIndex < 0 || m_probeIndex >= m_probe.size()) {
        probeFinish(false, QString(), 0);
        return;
    }

    const QVariantMap attempt = m_probe.at(m_probeIndex).toMap();
    const QString port = attempt.value(QStringLiteral("port")).toString();
    const int baud = attempt.value(QStringLiteral("baud")).toInt();

    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 chosen = probe.serverPort();
    probe.close();

    m_probeProcess = std::make_unique<QProcess>();
    m_probeProcess->start(core::qsl::findRigctld(),
                          {QStringLiteral("-m"), QString::number(m_rigModel),
                           QStringLiteral("-r"), port,
                           QStringLiteral("-s"), QString::number(baud),
                           QStringLiteral("-T"), QStringLiteral("127.0.0.1"),
                           QStringLiteral("-t"), QString::number(chosen)});
    if (!m_probeProcess->waitForStarted(3000)) {
        ++m_probeIndex;
        QTimer::singleShot(0, this, &RigController::probeNext);
        return;
    }

    // Un secondo per aprire la seriale, poi si chiede la frequenza.
    QTimer::singleShot(1200, this, [this, port, baud, chosen] {
        if (m_probeIndex < 0)
            return;
        m_probeSocket = std::make_unique<QTcpSocket>();
        m_probeSocket->connectToHost(QStringLiteral("127.0.0.1"), chosen);
        if (!m_probeSocket->waitForConnected(1200)) {
            ++m_probeIndex;
            probeNext();
            return;
        }
        m_probeSocket->write("+f\n");
        m_probeSocket->waitForBytesWritten(500);
        const bool answered = m_probeSocket->waitForReadyRead(1500);
        const QString reply = answered ? QString::fromUtf8(m_probeSocket->readAll()) : QString();
        qint64 hz = 0;
        for (const QString& line : reply.split(QLatin1Char('\n'))) {
            const QString clean = line.section(QLatin1Char(':'), -1).trimmed();
            bool ok = false;
            const qint64 value = clean.toLongLong(&ok);
            if (ok && value > 100000)
                hz = value;
        }
        if (hz > 0) {
            probeFinish(true, port, baud);
            return;
        }
        ++m_probeIndex;
        probeNext();
    });
}

void RigController::probeFinish(bool found, const QString& port, int baud)
{
    m_probeSocket.reset();
    if (m_probeProcess) {
        m_probeProcess->kill();
        m_probeProcess->waitForFinished(1500);
        m_probeProcess.reset();
    }
    m_probeIndex = -1;
    m_probe.clear();

    if (!found) {
        if (m_ctx.activity) {
            m_ctx.activity(QStringLiteral("CAT"),
                           tr("The radio did not answer on any port. Check that it is on, that the "
                              "CAT is enabled, and that no other program is holding the cable."),
                           QStringLiteral("warning"));
        }
        emit stateChanged();
        return;
    }

    setSerialPort(port);
    setBaud(baud);
    setLink(QStringLiteral("serial"));
    if (m_ctx.activity) {
        m_ctx.activity(QStringLiteral("CAT"), tr("Radio found on %1 at %2 baud").arg(port).arg(baud),
                       QStringLiteral("success"));
    }
    m_enabled = true;
    QSettings().setValue(QStringLiteral("rig/enabled"), true);
    connectNow();
    emit changed();
    emit stateChanged();
}

void RigController::setPttType(const QString& type)
{
    const QString clean = type.trimmed().toUpper();
    if (clean == m_pttType)
        return;
    m_pttType = clean;
    QSettings().setValue(QStringLiteral("rig/pttType"), clean);
    if (m_enabled && m_link == QLatin1String("serial"))
        connectNow();
    emit changed();
}

void RigController::setPttPort(const QString& port)
{
    if (port == m_pttPort)
        return;
    m_pttPort = port.trimmed();
    QSettings().setValue(QStringLiteral("rig/pttPort"), m_pttPort);
    if (m_enabled && m_link == QLatin1String("serial"))
        connectNow();
    emit changed();
}

void RigController::testPtt(int milliseconds)
{
    if (!m_rig.connected()) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CAT"), tr("The radio is not connected: no PTT"),
                           QStringLiteral("warning"));
        return;
    }
    m_rig.setPtt(true);
    if (m_ctx.activity)
        m_ctx.activity(QStringLiteral("CAT"), tr("PTT on for a moment: the radio should transmit"),
                       QStringLiteral("info"));
    QTimer::singleShot(qBound(100, milliseconds, 5000), this, [this] { m_rig.setPtt(false); });
}

void RigController::setWpm(int wpm)
{
    const int clamped = qBound(5, wpm, 60);
    m_wpm = clamped;
    QSettings().setValue(QStringLiteral("cw/wpm"), clamped);
    m_rig.setSpeedWpm(clamped);
    emit stateChanged();
}

void RigController::overrideConnection(const QString& host, int port)
{
    m_host = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    m_port = port > 0 ? port : 4532;
    m_enabled = true;
    connectNow();
    emit changed();
}

void RigController::connectNow()
{
    // Radio nuova, speranza nuova: finche' non dice di no, si prova.
    m_canKeyCw = true;
    if (m_link == QLatin1String("serial"))
        startLocalRigctld();
    m_rig.connectTo(m_host, static_cast<quint16>(m_port));
    emit stateChanged();
}

// La radio attaccata col cavo: DecoDXLog non parla la lingua di ogni radio, ma
// Hamlib si'. Quindi si avvia rigctld su quella porta e gli si parla come
// sempre — per chi opera, e' solo "COM5, questa radio".
void RigController::startLocalRigctld()
{
    if (m_rigctld && m_rigctld->state() != QProcess::NotRunning)
        return;
    const QString exe = core::qsl::findRigctld();
    if (exe.isEmpty()) {
        if (m_ctx.activity) {
            m_ctx.activity(QStringLiteral("CAT"),
                           tr("Hamlib not found: install it, or start rigctld yourself and "
                              "use the network link"),
                           QStringLiteral("warning"));
        }
        return;
    }
    if (m_serialPort.isEmpty() || m_rigModel <= 0) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CAT"), tr("Pick the radio model and the serial port first"),
                           QStringLiteral("warning"));
        return;
    }

    // Una porta TCP libera, cosi' due programmi non si pestano i piedi.
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 chosen = probe.serverPort();
    probe.close();

    m_host = QStringLiteral("127.0.0.1");
    m_port = chosen;
    m_rigctld = std::make_unique<QProcess>();
    QStringList arguments{QStringLiteral("-m"), QString::number(m_rigModel),
                          QStringLiteral("-r"), m_serialPort,
                          QStringLiteral("-s"), QString::number(m_baud),
                          QStringLiteral("-T"), QStringLiteral("127.0.0.1"),
                          QStringLiteral("-t"), QString::number(chosen)};
    // Il PTT su un'altra porta: e' il caso di tante stazioni, dove il CAT sta
    // su una COM e il PTT alza RTS o DTR sull'altra.
    if (m_pttType != QLatin1String("RIG") && !m_pttType.isEmpty()) {
        arguments << QStringLiteral("-P") << m_pttType;
        if (!m_pttPort.isEmpty())
            arguments << QStringLiteral("-p") << m_pttPort;
    }
    m_rigctld->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_rigctld.get(), &QProcess::readyReadStandardOutput, this, [this] {
        const QString text = QString::fromUtf8(m_rigctld->readAll());
        for (const QString& line : text.split(QLatin1Char('\n'))) {
            // Le righe che contano sono quelle dove Hamlib dice che non ce la fa.
            if (line.contains(QLatin1String("error"), Qt::CaseInsensitive)
                && m_ctx.activity && !line.trimmed().isEmpty()) {
                m_ctx.activity(QStringLiteral("CAT"), tr("Hamlib: %1").arg(line.trimmed()),
                               QStringLiteral("warning"));
            }
        }
    });
    m_rigctld->start(exe, arguments);
    if (!m_rigctld->waitForStarted(4000)) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CAT"), tr("rigctld did not start"), QStringLiteral("warning"));
        m_rigctld.reset();
        return;
    }
    if (m_ctx.activity) {
        m_ctx.activity(QStringLiteral("CAT"),
                       tr("Hamlib started on %1 (model %2, %3 baud)")
                           .arg(m_serialPort).arg(m_rigModel).arg(m_baud),
                       QStringLiteral("info"));
    }
    // Un attimo: rigctld apre la seriale e poi si mette in ascolto.
    QThread::msleep(600);
}

void RigController::setLink(const QString& link)
{
    const QString clean = link == QLatin1String("serial") ? link : QStringLiteral("network");
    if (clean == m_link)
        return;
    m_link = clean;
    QSettings().setValue(QStringLiteral("rig/link"), clean);
    if (m_enabled)
        connectNow();
    emit changed();
}

void RigController::setSerialPort(const QString& port)
{
    if (port == m_serialPort)
        return;
    m_serialPort = port.trimmed();
    QSettings().setValue(QStringLiteral("rig/serialPort"), m_serialPort);
    emit changed();
}

void RigController::setRigModel(int model)
{
    if (model == m_rigModel)
        return;
    m_rigModel = model;
    QSettings().setValue(QStringLiteral("rig/model"), model);
    emit changed();
}

void RigController::setBaud(int baud)
{
    if (baud == m_baud || baud <= 0)
        return;
    m_baud = baud;
    QSettings().setValue(QStringLiteral("rig/baud"), baud);
    emit changed();
}

QVariantList RigController::rigModels()
{
    if (!m_models.isEmpty())
        return m_models;
    const QString exe = core::qsl::findRigctld();
    if (exe.isEmpty())
        return m_models;
    // "rigctld -l" scrive l'elenco di tutte le radio che Hamlib conosce.
    QProcess list;
    list.start(exe, {QStringLiteral("-l")});
    if (!list.waitForFinished(8000))
        return m_models;
    const QString text = QString::fromUtf8(list.readAllStandardOutput());
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        const QString clean = line.trimmed();
        if (clean.isEmpty() || !clean.at(0).isDigit())
            continue;
        const int id = clean.section(QLatin1Char(' '), 0, 0).toInt();
        const QString rest = clean.section(QLatin1Char(' '), 1).simplified();
        if (id > 0 && !rest.isEmpty()) {
            m_models << QVariantMap{{QStringLiteral("id"), id},
                                    {QStringLiteral("name"), QStringLiteral("%1 \u2014 %2").arg(id).arg(rest.left(40))}};
        }
    }
    return m_models;
}

QStringList RigController::serialPorts() const
{
    QStringList out;
#ifdef Q_OS_WIN
    // Si chiede a Windows l'elenco dei nomi di dispositivo e si tengono le COM.
    // Dal registro non si poteva: i nomi delle voci hanno le barre rovesce
    // (\Device\Silabser0) e QSettings non le sa leggere — infatti l'elenco
    // usciva con le righe giuste di numero ma vuote.
    QVarLengthArray<wchar_t, 65536> buffer(65536);
    const DWORD length = QueryDosDeviceW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    for (DWORD i = 0; i < length;) {
        const QString name = QString::fromWCharArray(buffer.data() + i);
        if (name.isEmpty())
            break;
        if (name.startsWith(QLatin1String("COM")) && name.size() > 3 && name.at(3).isDigit())
            out << name;
        i += static_cast<DWORD>(name.size()) + 1;
    }
#else
    const QDir dev(QStringLiteral("/dev"));
    for (const QString& name : dev.entryList({QStringLiteral("ttyUSB*"), QStringLiteral("ttyACM*"),
                                              QStringLiteral("ttyS*")}, QDir::System))
        out << dev.filePath(name);
#endif
    out.sort();
    return out;
}

// ── Il decoder CW ────────────────────────────────────────────────────────

QStringList RigController::audioInputs() const
{
    QStringList out;
    for (const QAudioDevice& device : QMediaDevices::audioInputs())
        out << device.description();
    return out;
}

void RigController::setAudioInput(const QString& name)
{
    if (name == m_audioInput)
        return;
    m_audioInput = name;
    QSettings().setValue(QStringLiteral("cw/audioInput"), name);
    if (m_decoderOn) {
        stopAudio();
        startAudio();
    }
    emit decoderChanged();
}

void RigController::setDecoderOn(bool on)
{
    if (on == m_decoderOn)
        return;
    m_decoderOn = on;
    if (on)
        startAudio();
    else
        stopAudio();
    emit decoderChanged();
}

void RigController::clearDecoder()
{
    m_decoderText.clear();
    m_decoder.reset();
    emit decoderChanged();
    publishScope(true);
}

void RigController::startAudio()
{
    QAudioDevice chosen = QMediaDevices::defaultAudioInput();
    for (const QAudioDevice& device : QMediaDevices::audioInputs()) {
        if (device.description() == m_audioInput)
            chosen = device;
    }
    if (chosen.isNull()) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CW"), tr("No audio input to listen to"), QStringLiteral("warning"));
        m_decoderOn = false;
        return;
    }

    QAudioFormat format;
    format.setSampleRate(8000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!chosen.isFormatSupported(format))
        format = chosen.preferredFormat();

    m_decoder.setSampleRate(format.sampleRate());
    m_decoder.reset();
    m_audio = std::make_unique<QAudioSource>(chosen, format);
    m_audioDevice = m_audio->start();
    if (!m_audioDevice) {
        m_audio.reset();
        m_decoderOn = false;
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CW"), tr("The audio input did not open"), QStringLiteral("warning"));
        return;
    }
    connect(m_audioDevice, &QIODevice::readyRead, this, [this] {
        consumeAudio(m_audioDevice->readAll());
    });
    if (m_ctx.activity)
        m_ctx.activity(QStringLiteral("CW"), tr("CW decoder listening to %1").arg(chosen.description()),
                       QStringLiteral("info"));
}

void RigController::consumeAudio(const QByteArray& chunk)
{
    if (chunk.isEmpty())
        return;
    m_audioBuffer += chunk;
    const int samples = static_cast<int>(m_audioBuffer.size() / sizeof(qint16));
    if (samples <= 0)
        return;
    const QString text = m_decoder.feed(reinterpret_cast<const qint16*>(m_audioBuffer.constData()), samples);
    m_audioBuffer.remove(0, samples * sizeof(qint16));
    if (!text.isEmpty()) {
        m_decoderText += text;
        // Non si tiene una giornata di CW in memoria: gli ultimi 4000
        // caratteri bastano e avanzano.
        if (m_decoderText.size() > 4000)
            m_decoderText = m_decoderText.right(3000);
    }
    emit decoderChanged();
    publishScope();
}

void RigController::publishScope(bool force)
{
    // Il disegno non ha bisogno di tutti i fotogrammi: a 15 al secondo scorre
    // gia' liscio, e l'interfaccia non si carica per niente.
    if (!force && m_scopeClock.isValid() && m_scopeClock.elapsed() < 66)
        return;
    m_scopeClock.restart();

    const core::CwDecoder::Scope& scope = m_decoder.scope();
    // Una colonna per punto del grafico basta e avanza: si tiene il massimo di
    // ogni gruppo, cosi' anche il punto piu' corto resta visibile.
    constexpr int kPoints = 300;
    QVariantList signal;
    const qsizetype n = scope.signal.size();
    if (n > 0) {
        const int points = static_cast<int>(std::min<qsizetype>(n, kPoints));
        signal.reserve(points);
        for (int i = 0; i < points; ++i) {
            const qsizetype from = n * i / points;
            const qsizetype to = std::max(from + 1, n * (i + 1) / points);
            float peak = 0;
            for (qsizetype j = from; j < to; ++j)
                peak = std::max(peak, scope.signal.at(j));
            signal.append(peak);
        }
    }
    m_scope = QVariantMap{
        {QStringLiteral("signal"), signal},
        {QStringLiteral("level"), scope.level},
        {QStringLiteral("pitch"), scope.pitch},
        {QStringLiteral("wpm"), scope.speed},
        {QStringLiteral("cost"), scope.cost},
        {QStringLiteral("reading"), scope.reading},
    };
    emit decoderScopeChanged();
}

void RigController::playTestAudio(const QByteArray& pcm, int sampleRate)
{
    stopAudio();
    sampleRate = sampleRate > 0 ? sampleRate : 8000;
    m_decoder.setSampleRate(sampleRate);
    m_decoder.reset();
    m_decoderOn = true;
    emit decoderChanged();

    // Venti millisecondi alla volta, al passo del tempo vero.
    auto offset = std::make_shared<qsizetype>(0);
    const qsizetype step = static_cast<qsizetype>(sampleRate / 50) * static_cast<qsizetype>(sizeof(qint16));
    m_testAudio = std::make_unique<QTimer>();
    m_testAudio->setInterval(20);
    connect(m_testAudio.get(), &QTimer::timeout, this, [this, pcm, offset, step] {
        if (*offset >= pcm.size()) {
            m_testAudio->stop();
            return;
        }
        consumeAudio(pcm.mid(*offset, step));
        *offset += step;
    });
    m_testAudio->start();
}

void RigController::stopAudio()
{
    // L'ultima lettera sta ancora nel decodificatore: la finestra di analisi e'
    // lunga tre secondi, e spegnendo si chiuderebbe con una lettera in meno.
    if (m_audio || m_testAudio) {
        const QString last = m_decoder.flush();
        if (!last.isEmpty())
            m_decoderText += last;
    }
    if (m_audio)
        m_audio->stop();
    m_audio.reset();
    m_testAudio.reset();
    m_audioDevice = nullptr;
    m_audioBuffer.clear();
    m_scope.clear();
    emit decoderScopeChanged();
}


void RigController::disconnectNow()
{
    m_rig.disconnectFromRig();
    emit stateChanged();
}

void RigController::tuneTo(qint64 hz, const QString& mode)
{
    if (hz > 0)
        m_rig.setFrequency(hz);
    if (!mode.isEmpty())
        m_rig.setMode(mode);
}

QString RigController::expand(const QString& text, const QVariantMap& context) const
{
    QString out = text;
    const QString mine = m_ctx.stationCallsign ? m_ctx.stationCallsign() : QString();
    auto put = [&out](const QString& token, const QString& value) {
        out.replace(QStringLiteral("{") + token + QStringLiteral("}"), value, Qt::CaseInsensitive);
    };
    put(QStringLiteral("MYCALL"), mine.toUpper());
    put(QStringLiteral("CALL"), context.value(QStringLiteral("call")).toString().toUpper());
    put(QStringLiteral("RST"), context.value(QStringLiteral("rst"), QStringLiteral("599")).toString());
    put(QStringLiteral("NR"), context.value(QStringLiteral("nr")).toString());
    put(QStringLiteral("EXCH"), context.value(QStringLiteral("exch")).toString());
    put(QStringLiteral("NAME"), context.value(QStringLiteral("name")).toString());
    // Quello che resta senza risposta se ne va: in aria non si manda una
    // parentesi graffa.
    static const QRegularExpression leftovers(QStringLiteral("\\{[A-Za-z#]+\\}"));
    out.remove(leftovers);
    return out.simplified();
}

void RigController::sendMacro(int index, const QVariantMap& context)
{
    if (index < 0 || index >= m_macros.size())
        return;
    sendText(m_macros.at(index).toMap().value(QStringLiteral("text")).toString(), context);
}

void RigController::sendText(const QString& text, const QVariantMap& context)
{
    const QString ready = expand(text, context);
    if (ready.isEmpty())
        return;
    // Il manipolatore sulla seriale ha la precedenza: se c'e', e' quello che
    // l'operatore ha attaccato alla radio apposta.
    if (m_keyer.isOpen()) {
        m_keyer.send(ready, wpm());
        return;
    }
    m_rig.sendMorse(ready);
}

void RigController::stop()
{
    m_keyer.stop();
    m_rig.stopMorse();
}

// ── Il manipolatore sulla seriale ───────────────────────────────────────────

void RigController::openKeyer()
{
    m_keyer.close();
    if (m_keyerPort.isEmpty()) {
        emit stateChanged();
        return;
    }
    if (m_keyer.open(m_keyerPort, m_keyerLine) && m_ctx.activity) {
        m_ctx.activity(QStringLiteral("CW"),
                       tr("CW keyer on %1 (%2): it works with the CAT busy elsewhere")
                           .arg(m_keyerPort, m_keyerLine),
                       QStringLiteral("success"));
    }
    emit stateChanged();
}

void RigController::setKeyerPort(const QString& port)
{
    const QString clean = port.trimmed();
    if (clean == m_keyerPort)
        return;
    m_keyerPort = clean;
    QSettings().setValue(QStringLiteral("rig/keyerPort"), m_keyerPort);
    openKeyer();
    emit changed();
}

void RigController::setKeyerLine(const QString& line)
{
    const QString clean = line.trimmed().toUpper() == QLatin1String("RTS") ? QStringLiteral("RTS")
                                                                           : QStringLiteral("DTR");
    if (clean == m_keyerLine)
        return;
    m_keyerLine = clean;
    QSettings().setValue(QStringLiteral("rig/keyerLine"), m_keyerLine);
    openKeyer();
    emit changed();
}

void RigController::testKeyer()
{
    if (!m_keyer.isOpen()) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("CW"), tr("No CW keyer: pick a port first"),
                           QStringLiteral("warning"));
        return;
    }
    m_keyer.send(QStringLiteral("VVV"), wpm());
}

void RigController::setMacro(int index, const QString& label, const QString& text)
{
    if (index < 0 || index >= m_macros.size())
        return;
    QVariantMap macro = m_macros.at(index).toMap();
    macro.insert(QStringLiteral("label"), label.trimmed());
    macro.insert(QStringLiteral("text"), text.trimmed());
    m_macros[index] = macro;
    saveMacros();
    emit macrosChanged();
}

void RigController::resetMacros()
{
    m_macros = defaultMacros();
    saveMacros();
    emit macrosChanged();
}

QVariantList RigController::defaultMacros()
{
    // Quelle di sempre, nell'ordine in cui le tiene ogni log da contest.
    return {
        QVariantMap{{QStringLiteral("label"), QStringLiteral("CQ")},
                    {QStringLiteral("text"), QStringLiteral("CQ TEST {MYCALL} {MYCALL} TEST")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Call")},
                    {QStringLiteral("text"), QStringLiteral("{CALL}")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Exch")},
                    {QStringLiteral("text"), QStringLiteral("{CALL} 5NN {NR}")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("TU")},
                    {QStringLiteral("text"), QStringLiteral("TU {MYCALL} TEST")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("?")},
                    {QStringLiteral("text"), QStringLiteral("?")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("AGN")},
                    {QStringLiteral("text"), QStringLiteral("AGN")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("NR?")},
                    {QStringLiteral("text"), QStringLiteral("NR?")}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("73")},
                    {QStringLiteral("text"), QStringLiteral("73 GL")}},
    };
}

void RigController::loadMacros()
{
    const QString raw = QSettings().value(QStringLiteral("cw/macros")).toString();
    const QJsonArray array = QJsonDocument::fromJson(raw.toUtf8()).array();
    QVariantList out;
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        out << QVariantMap{{QStringLiteral("label"), object.value(QStringLiteral("label")).toString()},
                           {QStringLiteral("text"), object.value(QStringLiteral("text")).toString()}};
    }
    m_macros = out.size() == 8 ? out : defaultMacros();
}

void RigController::saveMacros()
{
    QJsonArray array;
    for (const QVariant& value : std::as_const(m_macros)) {
        const QVariantMap macro = value.toMap();
        array.append(QJsonObject{{QStringLiteral("label"), macro.value(QStringLiteral("label")).toString()},
                                 {QStringLiteral("text"), macro.value(QStringLiteral("text")).toString()}});
    }
    QSettings().setValue(QStringLiteral("cw/macros"),
                         QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

} // namespace decolog::app
