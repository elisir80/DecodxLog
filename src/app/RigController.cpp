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
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>

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

// La radio attaccata col cavo: DecoLog non parla la lingua di ogni radio, ma
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
    const QStringList arguments{QStringLiteral("-m"), QString::number(m_rigModel),
                                QStringLiteral("-r"), m_serialPort,
                                QStringLiteral("-s"), QString::number(m_baud),
                                QStringLiteral("-T"), QStringLiteral("127.0.0.1"),
                                QStringLiteral("-t"), QString::number(chosen)};
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
    // Le porte che Windows dichiara, senza dipendere da altro.
    QSettings ports(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\SERIALCOMM"),
                    QSettings::NativeFormat);
    for (const QString& key : ports.allKeys())
        out << ports.value(key).toString();
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
    connect(m_audioDevice, &QIODevice::readyRead, this, [this, format] {
        const QByteArray chunk = m_audioDevice->readAll();
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
    });
    if (m_ctx.activity)
        m_ctx.activity(QStringLiteral("CW"), tr("CW decoder listening to %1").arg(chosen.description()),
                       QStringLiteral("info"));
}

void RigController::stopAudio()
{
    if (m_audio)
        m_audio->stop();
    m_audio.reset();
    m_audioDevice = nullptr;
    m_audioBuffer.clear();
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
    m_rig.sendMorse(ready);
}

void RigController::stop()
{
    m_rig.stopMorse();
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
