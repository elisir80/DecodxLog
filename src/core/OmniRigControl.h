// DecoDXLog — la radio attraverso OmniRig (VE3NEA), solo su Windows.
//
// OmniRig e' un server COM: tiene la porta della radio e la divide fra tutti i
// programmi che la chiedono (Log4OM, JTDX, CW Skimmer...). Qui si parla con
// "OmniRig.OmniRigX": Rig1 o Rig2, e di ognuno Freq, Mode, Tx, Status. OmniRig
// non manda il CW: quello lo fa il manipolatore seriale o la radio via CAT.
#pragma once

#include "core/RigLink.h"

#include <QTimer>

namespace decolog::core {

namespace omnirig {

// I bit dei parametri di OmniRig (dalla sua libreria dei tipi).
enum Param : long {
    PM_RX = 0x00200000,
    PM_TX = 0x00400000,
    PM_CW_U = 0x00800000,
    PM_CW_L = 0x01000000,
    PM_SSB_U = 0x02000000,
    PM_SSB_L = 0x04000000,
    PM_DIG_U = 0x08000000,
    PM_DIG_L = 0x10000000,
    PM_AM = 0x20000000,
    PM_FM = 0x40000000,
};
enum Status : long { ST_NOTCONFIGURED = 0, ST_DISABLED = 1, ST_PORTBUSY = 2, ST_NOTRESPONDING = 3, ST_ONLINE = 4 };

// Il modo di OmniRig come lo scrive Hamlib, e il contrario.
QString toHamlibMode(long mode);
long fromHamlibMode(const QString& mode);

} // namespace omnirig

class OmniRigControl : public RigLink {
    Q_OBJECT

public:
    explicit OmniRigControl(QObject* parent = nullptr);
    ~OmniRigControl() override;

    // 1 o 2: la radio di OmniRig.
    void connectTo(int rigNumber);

    void disconnectFromRig() override;
    bool connected() const override { return m_connected; }
    qint64 frequencyHz() const override { return m_hz; }
    QString mode() const override { return m_mode; }
    int speedWpm() const override { return m_wpm; }
    QString status() const override { return m_status; }

    void refresh() override;
    void setFrequency(qint64 hz) override;
    void setMode(const QString& mode) override;
    void setPtt(bool on) override;
    void setSpeedWpm(int wpm) override { m_wpm = wpm; }
    void sendMorse(const QString& text) override;
    void stopMorse() override {}

private:
    void release();

    void* m_server{nullptr};    // IDispatch* di OmniRigX
    void* m_rig{nullptr};       // IDispatch* di Rig1/Rig2
    int m_rigNumber{1};
    bool m_connected{false};
    qint64 m_hz{0};
    QString m_mode;
    int m_wpm{24};
    QString m_status;
    QTimer m_poll;
};

} // namespace decolog::core
