// DecoDXLog — SO2R: due radio, un operatore.
//
// La radio 1 e' quella di sempre (Impostazioni → Radio); la radio 2 si collega
// qui, per rigctld, TCI, flrig o OmniRig. Una delle due ha il "fuoco": e' quella
// su cui va l'inserimento, la sintonia, il CW. La scatola SO2R (MK2R, SO2RDuino,
// YCCC SO2R+...) si comanda con OTRSP su una porta seriale: TX1/TX2 per il
// trasmettitore, RX1/RX2 per le cuffie, RX1S/RX2S per l'ascolto stereo.
#pragma once

#include "core/FlrigControl.h"
#include "core/OmniRigControl.h"
#include "core/RigControl.h"
#include "core/TciControl.h"

#include <QObject>
#include <QString>
#include <functional>

class QSerialPort;

namespace decolog::core {

namespace otrsp {
// Il comando OTRSP per mettere trasmettitore e cuffie: {"TX2", "RX2S"}.
QStringList commands(int focus, bool stereo);
} // namespace otrsp

} // namespace decolog::core

namespace decolog::app {

class So2rController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(int focus READ focus WRITE setFocus NOTIFY focusChanged)
    Q_PROPERTY(bool stereo READ stereo WRITE setStereo NOTIFY focusChanged)
    Q_PROPERTY(QString otrspPort READ otrspPort WRITE setOtrspPort NOTIFY changed)
    Q_PROPERTY(bool otrspOpen READ otrspOpen NOTIFY stateChanged)
    Q_PROPERTY(QString link READ link WRITE setLink NOTIFY changed)          // network, tci, flrig, omnirig
    Q_PROPERTY(QString address READ address WRITE setAddress NOTIFY changed) // host:port, o 1/2 per OmniRig
    Q_PROPERTY(bool radio2Connected READ radio2Connected NOTIFY stateChanged)
    Q_PROPERTY(QString radio2Status READ radio2Status NOTIFY stateChanged)
    Q_PROPERTY(qint64 radio2Hz READ radio2Hz NOTIFY stateChanged)
    Q_PROPERTY(QString radio2Mode READ radio2Mode NOTIFY stateChanged)

public:
    struct Context {
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
    };

    explicit So2rController(Context context, QObject* parent = nullptr);
    ~So2rController() override;

    void start();

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    int focus() const { return m_enabled ? m_focus : 1; }
    void setFocus(int radio);
    bool stereo() const { return m_stereo; }
    void setStereo(bool on);
    QString otrspPort() const { return m_otrspPort; }
    void setOtrspPort(const QString& port);
    bool otrspOpen() const;
    QString link() const { return m_link; }
    void setLink(const QString& link);
    QString address() const { return m_address; }
    void setAddress(const QString& address);

    bool radio2Connected() const { return m_radio2 && m_radio2->connected(); }
    QString radio2Status() const { return m_radio2 ? m_radio2->status() : QString(); }
    qint64 radio2Hz() const { return m_radio2 ? m_radio2->frequencyHz() : 0; }
    QString radio2Mode() const { return m_radio2 ? m_radio2->mode() : QString(); }
    // La radio 2, quando ha il fuoco: la sintonia e il CW vanno a lei.
    core::RigLink* radio2() const { return m_radio2; }
    bool radio2HasFocus() const { return m_enabled && m_focus == 2; }

    Q_INVOKABLE void toggleFocus() { setFocus(focus() == 1 ? 2 : 1); }
    Q_INVOKABLE void toggleStereo() { setStereo(!m_stereo); }
    Q_INVOKABLE void connectRadio2();
    Q_INVOKABLE QStringList serialPorts() const;

signals:
    void changed();
    void focusChanged();
    void stateChanged();

private:
    void sendOtrsp();
    void openOtrsp();

    Context m_ctx;
    bool m_enabled{false};
    int m_focus{1};
    bool m_stereo{false};
    QString m_otrspPort;
    QString m_link{QStringLiteral("network")};
    QString m_address{QStringLiteral("127.0.0.1:4533")};
    QSerialPort* m_serial{nullptr};
    core::RigControl m_hamlib;
    core::TciControl m_tci;
    core::FlrigControl m_flrig;
    core::OmniRigControl m_omni;
    core::RigLink* m_radio2{nullptr};
};

} // namespace decolog::app
