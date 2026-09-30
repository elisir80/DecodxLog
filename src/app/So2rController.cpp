#include "app/So2rController.h"
#include "core/SerialPorts.h"

#include <QSerialPort>
#include <QSettings>

namespace decolog::core::otrsp {

QStringList commands(int focus, bool stereo)
{
    const QString n = focus == 2 ? QStringLiteral("2") : QStringLiteral("1");
    return {QStringLiteral("TX") + n, QStringLiteral("RX") + n + (stereo ? QStringLiteral("S") : QString())};
}

} // namespace decolog::core::otrsp

namespace decolog::app {

So2rController::So2rController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
    QSettings s;
    m_enabled = s.value(QStringLiteral("so2r/enabled"), false).toBool();
    m_otrspPort = s.value(QStringLiteral("so2r/otrspPort")).toString();
    m_link = s.value(QStringLiteral("so2r/link"), m_link).toString();
    m_address = s.value(QStringLiteral("so2r/address"), m_address).toString();
    m_stereo = s.value(QStringLiteral("so2r/stereo"), false).toBool();
    for (core::RigLink* l : {static_cast<core::RigLink*>(&m_hamlib), static_cast<core::RigLink*>(&m_tci),
                             static_cast<core::RigLink*>(&m_flrig), static_cast<core::RigLink*>(&m_omni)}) {
        connect(l, &core::RigLink::changed, this, &So2rController::stateChanged);
        connect(l, &core::RigLink::failed, this, [this](const QString& why) {
            if (m_ctx.activity)
                m_ctx.activity(QStringLiteral("SO2R"), tr("Radio 2: %1").arg(why), QStringLiteral("warning"));
        });
    }
}

So2rController::~So2rController()
{
    if (m_serial)
        m_serial->close();
}

void So2rController::start()
{
    if (!m_enabled)
        return;
    openOtrsp();
    connectRadio2();
    sendOtrsp();
}

void So2rController::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    QSettings().setValue(QStringLiteral("so2r/enabled"), on);
    if (on) {
        start();
    } else {
        if (m_radio2)
            m_radio2->disconnectFromRig();
        m_radio2 = nullptr;
        if (m_serial)
            m_serial->close();
    }
    emit changed();
    emit focusChanged();
    emit stateChanged();
}

void So2rController::setFocus(int radio)
{
    const int r = radio == 2 ? 2 : 1;
    if (!m_enabled || r == m_focus)
        return;
    m_focus = r;
    sendOtrsp();
    emit focusChanged();
}

void So2rController::setStereo(bool on)
{
    if (on == m_stereo)
        return;
    m_stereo = on;
    QSettings().setValue(QStringLiteral("so2r/stereo"), on);
    sendOtrsp();
    emit focusChanged();
}

void So2rController::setOtrspPort(const QString& port)
{
    if (port == m_otrspPort)
        return;
    m_otrspPort = port;
    QSettings().setValue(QStringLiteral("so2r/otrspPort"), port);
    if (m_enabled) {
        openOtrsp();
        sendOtrsp();
    }
    emit changed();
}

void So2rController::setLink(const QString& link)
{
    const QString l = link == QLatin1String("tci") || link == QLatin1String("flrig") || link == QLatin1String("omnirig")
                          ? link : QStringLiteral("network");
    if (l == m_link)
        return;
    m_link = l;
    QSettings().setValue(QStringLiteral("so2r/link"), l);
    if (m_enabled)
        connectRadio2();
    emit changed();
}

void So2rController::setAddress(const QString& address)
{
    if (address.trimmed() == m_address)
        return;
    m_address = address.trimmed();
    QSettings().setValue(QStringLiteral("so2r/address"), m_address);
    if (m_enabled)
        connectRadio2();
    emit changed();
}

bool So2rController::otrspOpen() const
{
    return m_serial && m_serial->isOpen();
}

QStringList So2rController::serialPorts() const
{
    return core::availableSerialPorts();
}

void So2rController::openOtrsp()
{
    if (m_serial)
        m_serial->close();
    if (m_otrspPort.isEmpty()) {
        emit stateChanged();
        return;
    }
    if (!m_serial)
        m_serial = new QSerialPort(this);
    m_serial->setPortName(m_otrspPort);
    // OTRSP: 9600 8N1, senza controllo di flusso.
    m_serial->setBaudRate(QSerialPort::Baud9600);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);
    if (!m_serial->open(QIODevice::ReadWrite) && m_ctx.activity)
        m_ctx.activity(QStringLiteral("SO2R"), tr("Cannot open the SO2R box on %1: %2").arg(m_otrspPort, m_serial->errorString()),
                       QStringLiteral("warning"));
    emit stateChanged();
}

void So2rController::sendOtrsp()
{
    if (!otrspOpen())
        return;
    for (const QString& c : core::otrsp::commands(focus(), m_stereo))
        m_serial->write(c.toLatin1() + '\r');
    m_serial->flush();
}

void So2rController::connectRadio2()
{
    for (core::RigLink* l : {static_cast<core::RigLink*>(&m_hamlib), static_cast<core::RigLink*>(&m_tci),
                             static_cast<core::RigLink*>(&m_flrig), static_cast<core::RigLink*>(&m_omni)})
        l->disconnectFromRig();
    if (m_link == QLatin1String("tci")) {
        m_radio2 = &m_tci;
        m_tci.connectTo(m_address.isEmpty() ? QStringLiteral("127.0.0.1:40001") : m_address, 1);
    } else if (m_link == QLatin1String("flrig")) {
        m_radio2 = &m_flrig;
        m_flrig.connectTo(m_address);
    } else if (m_link == QLatin1String("omnirig")) {
        m_radio2 = &m_omni;
        m_omni.connectTo(m_address.trimmed() == QLatin1String("1") ? 1 : 2);
    } else {
        m_radio2 = &m_hamlib;
        const QString host = m_address.section(QLatin1Char(':'), 0, 0);
        const int port = m_address.section(QLatin1Char(':'), 1, 1).toInt();
        m_hamlib.connectTo(host.isEmpty() ? QStringLiteral("127.0.0.1") : host, static_cast<quint16>(port > 0 ? port : 4533));
    }
    emit stateChanged();
}

} // namespace decolog::app
