// DecoLog — la radio e le macro in CW.
//
// Il collegamento lo fa Hamlib: DecoLog parla a rigctld, che sta gia' sul
// computer di chi opera. Qui sopra ci sono le macro del contest — otto tasti
// con dentro il testo che si manda, con i buchi da riempire ({CALL}, {NR},
// {MYCALL}) — e la velocita' del manipolatore.
#pragma once

#include "core/CwDecoder.h"
#include "core/RigControl.h"

#include <QAudioSource>
#include <QProcess>
#include <memory>

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

namespace decolog::app {

class RigController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY changed)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(qint64 frequencyHz READ frequencyHz NOTIFY stateChanged)
    Q_PROPERTY(QString frequencyLabel READ frequencyLabel NOTIFY stateChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY stateChanged)
    Q_PROPERTY(int wpm READ wpm WRITE setWpm NOTIFY stateChanged)
    // Le otto macro, come {label, text}.
    Q_PROPERTY(QVariantList macros READ macros NOTIFY macrosChanged)
    // Il decoder CW: ascolta l'audio che esce dalla radio e scrive quello che
    // sente. Non serve che la radio sappia decodificare.
    Q_PROPERTY(bool decoderOn READ decoderOn WRITE setDecoderOn NOTIFY decoderChanged)
    Q_PROPERTY(QString decoderText READ decoderText NOTIFY decoderChanged)
    Q_PROPERTY(int decoderWpm READ decoderWpm NOTIFY decoderChanged)
    Q_PROPERTY(int decoderTone READ decoderTone NOTIFY decoderChanged)
    Q_PROPERTY(QStringList audioInputs READ audioInputs NOTIFY decoderChanged)
    Q_PROPERTY(QString audioInput READ audioInput WRITE setAudioInput NOTIFY decoderChanged)
    // Come si arriva alla radio: "network" (un rigctld gia' acceso) oppure
    // "serial" (la porta della radio, e rigctld lo avvia DecoLog).
    Q_PROPERTY(QString link READ link WRITE setLink NOTIFY changed)
    Q_PROPERTY(QString serialPort READ serialPort WRITE setSerialPort NOTIFY changed)
    Q_PROPERTY(int rigModel READ rigModel WRITE setRigModel NOTIFY changed)
    Q_PROPERTY(int baud READ baud WRITE setBaud NOTIFY changed)

public:
    struct Context {
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
        // Il nominativo con cui si sta operando, per {MYCALL}.
        std::function<QString()> stationCallsign;
    };

    explicit RigController(Context context, QObject* parent = nullptr);

    void start();
    // Per le prove da riga di comando: si collega a quel rigctld solo per
    // questa volta, senza scrivere niente nelle impostazioni dell'operatore.
    void overrideConnection(const QString& host, int port);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    QString host() const { return m_host; }
    void setHost(const QString& host);
    int port() const { return m_port; }
    void setPort(int port);
    bool connected() const { return m_rig.connected(); }
    QString status() const { return m_rig.status(); }
    qint64 frequencyHz() const { return m_rig.frequencyHz(); }
    QString frequencyLabel() const;
    QString mode() const { return m_rig.mode(); }
    int wpm() const { return m_rig.speedWpm() > 0 ? m_rig.speedWpm() : m_wpm; }
    void setWpm(int wpm);
    QVariantList macros() const { return m_macros; }

    // Le macro: testo con i buchi, e i buchi riempiti da quello che sta
    // succedendo adesso ({CALL}, {RST}, {NR}, {EXCH}, {NAME}, {MYCALL}).
    Q_INVOKABLE QString expand(const QString& text, const QVariantMap& context) const;
    Q_INVOKABLE void sendMacro(int index, const QVariantMap& context);
    Q_INVOKABLE void sendText(const QString& text, const QVariantMap& context);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void setMacro(int index, const QString& label, const QString& text);
    Q_INVOKABLE void resetMacros();
    Q_INVOKABLE void connectNow();
    Q_INVOKABLE void disconnectNow();
    // Porta la radio dove dice lo spot: frequenza in Hz e, se c'e', il modo.
    Q_INVOKABLE void tuneTo(qint64 hz, const QString& mode = QString());

    bool decoderOn() const { return m_decoderOn; }
    void setDecoderOn(bool on);
    QString decoderText() const { return m_decoderText; }
    int decoderWpm() const { return m_decoder.wpm(); }
    int decoderTone() const { return static_cast<int>(m_decoder.toneHz()); }
    QStringList audioInputs() const;
    QString audioInput() const { return m_audioInput; }
    void setAudioInput(const QString& name);
    Q_INVOKABLE void clearDecoder();

    QString link() const { return m_link; }
    void setLink(const QString& link);
    QString serialPort() const { return m_serialPort; }
    void setSerialPort(const QString& port);
    int rigModel() const { return m_rigModel; }
    void setRigModel(int model);
    int baud() const { return m_baud; }
    void setBaud(int baud);
    // I modelli che conosce Hamlib, letti da "rigctl -l": {id, name}.
    Q_INVOKABLE QVariantList rigModels();
    // Le porte seriali di questo computer.
    Q_INVOKABLE QStringList serialPorts() const;

signals:
    void changed();
    void stateChanged();
    void macrosChanged();
    void decoderChanged();

private:
    void loadMacros();
    void saveMacros();
    static QVariantList defaultMacros();

    void startAudio();
    void stopAudio();
    void startLocalRigctld();

    Context m_ctx;
    core::RigControl m_rig;
    core::CwDecoder m_decoder{8000};
    std::unique_ptr<QAudioSource> m_audio;
    QIODevice* m_audioDevice{nullptr};
    QByteArray m_audioBuffer;
    QString m_decoderText;
    QString m_audioInput;
    bool m_decoderOn{false};
    // rigctld avviato da noi quando la radio sta su una seriale.
    std::unique_ptr<QProcess> m_rigctld;
    QString m_link{QStringLiteral("network")};
    QString m_serialPort;
    int m_rigModel{0};
    int m_baud{38400};
    QVariantList m_models;
    QVariantList m_macros;
    QString m_host{QStringLiteral("127.0.0.1")};
    int m_port{4532};
    int m_wpm{24};
    bool m_enabled{false};
};

} // namespace decolog::app
