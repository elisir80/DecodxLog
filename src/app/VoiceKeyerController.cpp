#include "app/VoiceKeyerController.h"
#include "app/AudioDevices.h"

#include <QAudioDevice>
#include <QAudioOutput>
#include <QAudioSource>
#include <QDir>
#include <QFileInfo>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QtEndian>

namespace decolog::app {

namespace wav {

QByteArray header(int sampleRate, int channels, int bitsPerSample, quint32 dataBytes)
{
    QByteArray h;
    auto u32 = [&h](quint32 v) { char b[4]; qToLittleEndian(v, b); h.append(b, 4); };
    auto u16 = [&h](quint16 v) { char b[2]; qToLittleEndian(v, b); h.append(b, 2); };
    const quint16 block = static_cast<quint16>(channels * bitsPerSample / 8);
    h.append("RIFF", 4);
    u32(36 + dataBytes);
    h.append("WAVEfmt ", 8);
    u32(16);
    u16(1);                                 // PCM
    u16(static_cast<quint16>(channels));
    u32(static_cast<quint32>(sampleRate));
    u32(static_cast<quint32>(sampleRate) * block);
    u16(block);
    u16(static_cast<quint16>(bitsPerSample));
    h.append("data", 4);
    u32(dataBytes);
    return h;
}

} // namespace wav

namespace {

// La scheda scelta, e solo quella. Se non si trova NON si ripiega su un'altra:
// un messaggio vocale sulla scheda sbagliata finisce negli altoparlanti mentre
// la radio e' in trasmissione. `ok` dice se si puo' andare avanti; il
// predefinito di sistema e' una scelta scritta, non un ripiego.
QAudioDevice pickDevice(const QList<QAudioDevice>& list, const QString& id, const QString& name,
                        const QAudioDevice& systemDefault, bool* ok, bool* ambiguous)
{
    const auto r = audiodev::resolve(audiodev::entriesOf(list), {id, name});
    *ambiguous = r.kind == audiodev::Resolution::Ambiguous;
    switch (r.kind) {
    case audiodev::Resolution::SystemDefault:
        *ok = !systemDefault.isNull();
        return systemDefault;
    case audiodev::Resolution::Found:
    case audiodev::Resolution::Ambiguous:
        *ok = true;
        return list.at(r.index);
    case audiodev::Resolution::Missing:
        break;
    }
    *ok = false;
    return {};
}

QStringList defaultLabels()
{
    return {QStringLiteral("CQ"), QStringLiteral("Call"), QStringLiteral("Exch"), QStringLiteral("TU"),
            QStringLiteral("?"), QStringLiteral("AGN"), QStringLiteral("NR?"), QStringLiteral("73")};
}

} // namespace

VoiceKeyerController::VoiceKeyerController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    QSettings s;
    m_output = s.value(QStringLiteral("dvk/output")).toString();
    m_outputId = s.value(QStringLiteral("dvk/outputId")).toString();
    m_input = s.value(QStringLiteral("dvk/input")).toString();
    m_inputId = s.value(QStringLiteral("dvk/inputId")).toString();
    m_usePtt = s.value(QStringLiteral("dvk/ptt"), true).toBool();
    m_repeat = s.value(QStringLiteral("dvk/repeat"), 0).toInt();
    loadLabels();
    // Il PTT va su un attimo prima dell'audio: le radio ci mettono qualche
    // decina di millisecondi a passare in trasmissione.
    m_pttLead.setSingleShot(true);
    m_pttLead.setInterval(150);
    connect(&m_pttLead, &QTimer::timeout, this, [this] {
        if (m_player)
            m_player->play();
    });
    m_repeatTimer.setSingleShot(true);
    connect(&m_repeatTimer, &QTimer::timeout, this, [this] {
        if (m_repeating && m_playing < 0 && m_lastSlot >= 0)
            play(m_lastSlot);
    });
}

VoiceKeyerController::~VoiceKeyerController()
{
    stop();
    stopRecording();
}

QString VoiceKeyerController::filePath(int slot)
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("dvk/F%1.wav").arg(slot + 1));
}

void VoiceKeyerController::loadLabels()
{
    m_labels = QSettings().value(QStringLiteral("dvk/labels")).toStringList();
    const QStringList defaults = defaultLabels();
    while (m_labels.size() < kSlots)
        m_labels << defaults.value(m_labels.size());
}

QVariantList VoiceKeyerController::messages() const
{
    QVariantList out;
    for (int i = 0; i < kSlots; ++i) {
        const QFileInfo f(filePath(i));
        out << QVariantMap{{QStringLiteral("slot"), i},
                           {QStringLiteral("key"), QStringLiteral("F%1").arg(i + 1)},
                           {QStringLiteral("label"), m_labels.value(i)},
                           {QStringLiteral("present"), f.exists() && f.size() > 44},
                           {QStringLiteral("seconds"), f.exists() ? qMax(0.0, (f.size() - 44) / 32000.0) : 0.0}};
    }
    return out;
}

QStringList VoiceKeyerController::outputs() const
{
    QStringList out;
    for (const QAudioDevice& d : QMediaDevices::audioOutputs())
        out << d.description();
    return out;
}

QStringList VoiceKeyerController::inputs() const
{
    QStringList out;
    for (const QAudioDevice& d : QMediaDevices::audioInputs())
        out << d.description();
    return out;
}

void VoiceKeyerController::watchDevices() const
{
    // L'elenco cambia quando cambiano le schede: si ascolta il sistema.
    if (m_mediaDevices)
        return;
    auto* self = const_cast<VoiceKeyerController*>(this);
    m_mediaDevices = new QMediaDevices(self);
    auto changed = [self] {
        emit self->devicesChanged();
        emit self->settingsChanged();
    };
    connect(m_mediaDevices, &QMediaDevices::audioOutputsChanged, self, changed);
    connect(m_mediaDevices, &QMediaDevices::audioInputsChanged, self, changed);
}

QVariantList VoiceKeyerController::outputDevices() const
{
    watchDevices();
    return audiodev::deviceList(audiodev::outputDevices());
}

QVariantList VoiceKeyerController::inputDevices() const
{
    watchDevices();
    return audiodev::deviceList(audiodev::inputDevices());
}

int VoiceKeyerController::outputIndex() const
{
    return audiodev::comboIndex(
        audiodev::resolve(audiodev::entriesOf(audiodev::outputDevices()), {m_outputId, m_output}));
}

int VoiceKeyerController::inputIndex() const
{
    return audiodev::comboIndex(
        audiodev::resolve(audiodev::entriesOf(audiodev::inputDevices()), {m_inputId, m_input}));
}

void VoiceKeyerController::chooseOutput(int row)
{
    const audiodev::Saved saved = audiodev::savedFor(audiodev::outputDevices(), row);
    if (saved.id == m_outputId && saved.name == m_output)
        return;
    m_outputId = saved.id;
    m_output = saved.name;
    QSettings s;
    s.setValue(QStringLiteral("dvk/outputId"), m_outputId);
    s.setValue(QStringLiteral("dvk/output"), m_output);
    emit settingsChanged();
}

void VoiceKeyerController::chooseInput(int row)
{
    const audiodev::Saved saved = audiodev::savedFor(audiodev::inputDevices(), row);
    if (saved.id == m_inputId && saved.name == m_input)
        return;
    m_inputId = saved.id;
    m_input = saved.name;
    QSettings s;
    s.setValue(QStringLiteral("dvk/inputId"), m_inputId);
    s.setValue(QStringLiteral("dvk/input"), m_input);
    emit settingsChanged();
}

void VoiceKeyerController::setOutput(const QString& name)
{
    if (name == m_output && m_outputId.isEmpty())
        return;
    m_output = name;
    m_outputId.clear();
    QSettings s;
    s.setValue(QStringLiteral("dvk/output"), name);
    s.setValue(QStringLiteral("dvk/outputId"), QString());
    emit settingsChanged();
}

void VoiceKeyerController::setInput(const QString& name)
{
    if (name == m_input && m_inputId.isEmpty())
        return;
    m_input = name;
    m_inputId.clear();
    QSettings s;
    s.setValue(QStringLiteral("dvk/input"), name);
    s.setValue(QStringLiteral("dvk/inputId"), QString());
    emit settingsChanged();
}

void VoiceKeyerController::setUsePtt(bool on)
{
    if (on == m_usePtt)
        return;
    m_usePtt = on;
    QSettings().setValue(QStringLiteral("dvk/ptt"), on);
    emit settingsChanged();
}

void VoiceKeyerController::setRepeatSeconds(int s)
{
    s = qBound(0, s, 120);
    if (s == m_repeat)
        return;
    m_repeat = s;
    QSettings().setValue(QStringLiteral("dvk/repeat"), s);
    emit settingsChanged();
}

bool VoiceKeyerController::hasMessage(int slot) const
{
    const QFileInfo f(filePath(slot));
    return slot >= 0 && slot < kSlots && f.exists() && f.size() > 44;
}

void VoiceKeyerController::play(int slot)
{
    if (!hasMessage(slot) || m_recording >= 0) {
        if (m_ctx.activity && m_recording < 0)
            m_ctx.activity(QStringLiteral("DVK"), tr("F%1 has no recorded message").arg(slot + 1), QStringLiteral("warning"));
        return;
    }
    stop();
    m_lastSlot = slot;
    if (!m_player) {
        m_player = new QMediaPlayer(this);
        m_audioOut = new QAudioOutput(this);
        m_player->setAudioOutput(m_audioOut);
        connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s) {
            if (s == QMediaPlayer::EndOfMedia || s == QMediaPlayer::InvalidMedia)
                finishPlayback();
        });
    }
    bool found = false;
    bool ambiguous = false;
    const QAudioDevice out = pickDevice(QMediaDevices::audioOutputs(), m_outputId, m_output,
                                        QMediaDevices::defaultAudioOutput(), &found, &ambiguous);
    if (!found) {
        // Niente PTT e niente audio: meglio un messaggio che non parte di uno
        // che esce dalla scheda sbagliata.
        m_playing = -1;
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("DVK"),
                           tr("The audio output \"%1\" is not available: choose another one in the DVK panel. "
                              "Nothing was sent.")
                               .arg(m_output.isEmpty() ? tr("System default") : m_output),
                           QStringLiteral("warning"));
        emit stateChanged();
        return;
    }
    if (ambiguous && m_ctx.activity) {
        m_ctx.activity(QStringLiteral("DVK"),
                       tr("Two audio outputs are called \"%1\": using the first one. Choose it again in the DVK panel "
                          "to say which.")
                           .arg(m_output),
                       QStringLiteral("warning"));
    }
    m_audioOut->setDevice(out);
    m_player->setSource(QUrl::fromLocalFile(filePath(slot)));
    m_playing = slot;
    emit stateChanged();
    if (m_usePtt && m_ctx.ptt) {
        m_ctx.ptt(true);
        m_pttLead.start();
    } else {
        m_player->play();
    }
}

void VoiceKeyerController::playRepeating(int slot)
{
    m_repeating = m_repeat > 0;
    play(slot);
}

void VoiceKeyerController::finishPlayback()
{
    if (m_playing < 0)
        return;
    m_playing = -1;
    if (m_usePtt && m_ctx.ptt)
        m_ctx.ptt(false);
    emit stateChanged();
    if (m_repeating && m_repeat > 0)
        m_repeatTimer.start(m_repeat * 1000);
}

void VoiceKeyerController::stop()
{
    m_repeating = false;
    m_repeatTimer.stop();
    m_pttLead.stop();
    if (m_player)
        m_player->stop();
    if (m_playing >= 0) {
        m_playing = -1;
        if (m_usePtt && m_ctx.ptt)
            m_ctx.ptt(false);
        emit stateChanged();
    }
}

void VoiceKeyerController::startRecording(int slot)
{
    if (slot < 0 || slot >= kSlots)
        return;
    stop();
    stopRecording();
    bool inputFound = false;
    bool inputAmbiguous = false;
    const QAudioDevice device = pickDevice(QMediaDevices::audioInputs(), m_inputId, m_input,
                                           QMediaDevices::defaultAudioInput(), &inputFound, &inputAmbiguous);
    if (!inputFound) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("DVK"),
                           tr("The microphone \"%1\" is not available: choose another one in the DVK panel.")
                               .arg(m_input.isEmpty() ? tr("System default") : m_input),
                           QStringLiteral("warning"));
        return;
    }
    QAudioFormat f;
    f.setSampleRate(16000);
    f.setChannelCount(1);
    f.setSampleFormat(QAudioFormat::Int16);
    if (!device.isFormatSupported(f)) {
        if (m_ctx.activity)
            m_ctx.activity(QStringLiteral("DVK"), tr("The microphone does not record 16 kHz mono"), QStringLiteral("warning"));
        return;
    }
    QDir().mkpath(QFileInfo(filePath(slot)).absolutePath());
    m_recordFile.setFileName(filePath(slot) + QStringLiteral(".part"));
    if (!m_recordFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    m_recordFile.write(wav::header(16000, 1, 16, 0));
    m_recordFormat = f;
    m_recordBytes = 0;
    m_source = new QAudioSource(device, f, this);
    QIODevice* in = m_source->start();
    connect(in, &QIODevice::readyRead, this, [this, in] {
        const QByteArray data = in->readAll();
        m_recordFile.write(data);
        m_recordBytes += static_cast<quint32>(data.size());
    });
    m_recording = slot;
    emit stateChanged();
}

void VoiceKeyerController::stopRecording()
{
    if (m_recording < 0)
        return;
    const int slot = m_recording;
    m_recording = -1;
    if (m_source) {
        m_source->stop();
        m_source->deleteLater();
        m_source = nullptr;
    }
    // L'intestazione si riscrive con la lunghezza vera.
    m_recordFile.seek(0);
    m_recordFile.write(wav::header(16000, 1, 16, m_recordBytes));
    m_recordFile.close();
    QFile::remove(filePath(slot));
    QFile::rename(m_recordFile.fileName(), filePath(slot));
    if (m_ctx.activity)
        m_ctx.activity(QStringLiteral("DVK"), tr("F%1 recorded: %2 s").arg(slot + 1).arg(m_recordBytes / 32000.0, 0, 'f', 1),
                       QStringLiteral("success"));
    emit stateChanged();
    emit messagesChanged();
}

QString VoiceKeyerController::importFile(int slot, const QUrl& file)
{
    if (slot < 0 || slot >= kSlots)
        return tr("No such key");
    const QString source = file.isLocalFile() ? file.toLocalFile() : file.toString();
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly))
        return tr("Cannot read %1").arg(source);
    const QByteArray head = in.peek(12);
    if (!head.startsWith("RIFF") || head.mid(8, 4) != "WAVE")
        return tr("Not a WAV file");
    QDir().mkpath(QFileInfo(filePath(slot)).absolutePath());
    QFile::remove(filePath(slot));
    in.close();
    if (!QFile::copy(source, filePath(slot)))
        return tr("Cannot copy the file");
    emit messagesChanged();
    return {};
}

void VoiceKeyerController::setLabel(int slot, const QString& label)
{
    if (slot < 0 || slot >= kSlots)
        return;
    m_labels[slot] = label.trimmed();
    QSettings().setValue(QStringLiteral("dvk/labels"), m_labels);
    emit messagesChanged();
}

void VoiceKeyerController::clear(int slot)
{
    if (slot < 0 || slot >= kSlots)
        return;
    QFile::remove(filePath(slot));
    emit messagesChanged();
}

} // namespace decolog::app
