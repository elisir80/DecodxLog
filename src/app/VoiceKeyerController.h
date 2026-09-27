// DecoDXLog — il DVK, il keyer vocale per la fonia: otto messaggi registrati
// (CQ, report, grazie...) sui tasti F1-F8, come i tasti del CW. Un messaggio
// si registra dal microfono qui dentro o si sceglie un file WAV; quando parte,
// il PTT della radio va su, l'audio esce sulla scheda collegata alla radio, e
// alla fine il PTT torna giu'. Esc ferma tutto.
#pragma once

#include <QAudioFormat>
#include <QFile>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <functional>

class QAudioOutput;
class QAudioSource;
class QMediaPlayer;

namespace decolog::app {

namespace wav {
// L'intestazione di un file WAV PCM: 44 byte.
QByteArray header(int sampleRate, int channels, int bitsPerSample, quint32 dataBytes);
} // namespace wav

class VoiceKeyerController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(QStringList outputs READ outputs CONSTANT)
    Q_PROPERTY(QStringList inputs READ inputs CONSTANT)
    Q_PROPERTY(QString output READ output WRITE setOutput NOTIFY settingsChanged)
    Q_PROPERTY(QString input READ input WRITE setInput NOTIFY settingsChanged)
    Q_PROPERTY(bool usePtt READ usePtt WRITE setUsePtt NOTIFY settingsChanged)
    Q_PROPERTY(int playing READ playing NOTIFY stateChanged)       // -1 = niente
    Q_PROPERTY(int recording READ recording NOTIFY stateChanged)   // -1 = niente
    Q_PROPERTY(int repeatSeconds READ repeatSeconds WRITE setRepeatSeconds NOTIFY settingsChanged)

public:
    static constexpr int kSlots = 8;
    struct Context {
        std::function<void(bool on)> ptt;
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
    };

    explicit VoiceKeyerController(Context context, QObject* parent = nullptr);
    ~VoiceKeyerController() override;

    QVariantList messages() const;
    QStringList outputs() const;
    QStringList inputs() const;
    QString output() const { return m_output; }
    void setOutput(const QString& name);
    QString input() const { return m_input; }
    void setInput(const QString& name);
    bool usePtt() const { return m_usePtt; }
    void setUsePtt(bool on);
    int playing() const { return m_playing; }
    int recording() const { return m_recording; }
    int repeatSeconds() const { return m_repeat; }
    void setRepeatSeconds(int s);

    Q_INVOKABLE bool hasMessage(int slot) const;
    Q_INVOKABLE void play(int slot);
    // Ripete il messaggio (di solito il CQ) ogni `repeatSeconds`, finche' non si ferma.
    Q_INVOKABLE void playRepeating(int slot);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void startRecording(int slot);
    Q_INVOKABLE void stopRecording();
    // Un file WAV scelto dall'operatore diventa il messaggio del tasto.
    Q_INVOKABLE QString importFile(int slot, const QUrl& file);
    Q_INVOKABLE void setLabel(int slot, const QString& label);
    Q_INVOKABLE void clear(int slot);
    static QString filePath(int slot);

signals:
    void messagesChanged();
    void settingsChanged();
    void stateChanged();

private:
    void finishPlayback();
    void loadLabels();

    Context m_ctx;
    QString m_output;
    QString m_input;
    bool m_usePtt{true};
    int m_repeat{0};
    int m_playing{-1};
    int m_recording{-1};
    bool m_repeating{false};
    int m_lastSlot{-1};
    QStringList m_labels;
    QMediaPlayer* m_player{nullptr};
    QAudioOutput* m_audioOut{nullptr};
    QAudioSource* m_source{nullptr};
    QFile m_recordFile;
    QAudioFormat m_recordFormat;
    quint32 m_recordBytes{0};
    QTimer m_pttLead;
    QTimer m_repeatTimer;
};

} // namespace decolog::app
