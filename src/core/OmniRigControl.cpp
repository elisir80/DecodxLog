#include "core/OmniRigControl.h"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <objbase.h>
#include <oleauto.h>
#include <windows.h>
#endif

namespace decolog::core {

namespace omnirig {

QString toHamlibMode(long mode)
{
    if (mode & PM_CW_U) return QStringLiteral("CW");
    if (mode & PM_CW_L) return QStringLiteral("CWR");
    if (mode & PM_SSB_U) return QStringLiteral("USB");
    if (mode & PM_SSB_L) return QStringLiteral("LSB");
    if (mode & PM_DIG_U) return QStringLiteral("PKTUSB");
    if (mode & PM_DIG_L) return QStringLiteral("PKTLSB");
    if (mode & PM_AM) return QStringLiteral("AM");
    if (mode & PM_FM) return QStringLiteral("FM");
    return {};
}

long fromHamlibMode(const QString& mode)
{
    const QString m = mode.trimmed().toUpper();
    if (m == QLatin1String("CW")) return PM_CW_U;
    if (m == QLatin1String("CWR")) return PM_CW_L;
    if (m == QLatin1String("USB")) return PM_SSB_U;
    if (m == QLatin1String("LSB")) return PM_SSB_L;
    if (m == QLatin1String("PKTUSB") || m == QLatin1String("RTTYR")) return PM_DIG_U;
    if (m == QLatin1String("PKTLSB") || m == QLatin1String("RTTY")) return PM_DIG_L;
    if (m == QLatin1String("AM")) return PM_AM;
    if (m == QLatin1String("FM") || m == QLatin1String("PKTFM")) return PM_FM;
    return 0;
}

} // namespace omnirig

#ifdef Q_OS_WIN
namespace {

IDispatch* dispatch(void* p) { return static_cast<IDispatch*>(p); }

bool getProperty(IDispatch* obj, const wchar_t* name, VARIANT* out)
{
    if (!obj)
        return false;
    DISPID id = 0;
    LPOLESTR n = const_cast<LPOLESTR>(name);
    if (FAILED(obj->GetIDsOfNames(IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id)))
        return false;
    DISPPARAMS none{nullptr, nullptr, 0, 0};
    VariantInit(out);
    return SUCCEEDED(obj->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &none, out, nullptr, nullptr));
}

long getLong(IDispatch* obj, const wchar_t* name, bool* ok = nullptr)
{
    VARIANT v;
    const bool good = getProperty(obj, name, &v) && SUCCEEDED(VariantChangeType(&v, &v, 0, VT_I4));
    if (ok)
        *ok = good;
    const long out = good ? v.lVal : 0;
    VariantClear(&v);
    return out;
}

bool putLong(IDispatch* obj, const wchar_t* name, long value)
{
    if (!obj)
        return false;
    DISPID id = 0;
    LPOLESTR n = const_cast<LPOLESTR>(name);
    if (FAILED(obj->GetIDsOfNames(IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id)))
        return false;
    VARIANT arg;
    VariantInit(&arg);
    arg.vt = VT_I4;
    arg.lVal = value;
    DISPID put = DISPID_PROPERTYPUT;
    DISPPARAMS params{&arg, &put, 1, 1};
    return SUCCEEDED(obj->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &params, nullptr, nullptr, nullptr));
}

} // namespace
#endif

OmniRigControl::OmniRigControl(QObject* parent)
    : RigLink(parent)
{
    m_poll.setInterval(500);
    connect(&m_poll, &QTimer::timeout, this, &OmniRigControl::refresh);
    m_status = QCoreApplication::translate("OmniRigControl", "not connected");
}

OmniRigControl::~OmniRigControl()
{
    release();
}

void OmniRigControl::release()
{
#ifdef Q_OS_WIN
    if (m_rig)
        dispatch(m_rig)->Release();
    if (m_server)
        dispatch(m_server)->Release();
#endif
    m_rig = nullptr;
    m_server = nullptr;
}

void OmniRigControl::connectTo(int rigNumber)
{
    disconnectFromRig();
    m_rigNumber = rigNumber == 2 ? 2 : 1;
#ifdef Q_OS_WIN
    // Il thread dell'interfaccia ha gia' OLE acceso da Qt: qui basta chiederlo.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CLSID clsid;
    if (FAILED(CLSIDFromProgID(L"OmniRig.OmniRigX", &clsid))) {
        m_status = QCoreApplication::translate("OmniRigControl", "OmniRig is not installed");
        emit failed(m_status);
        emit changed();
        return;
    }
    IDispatch* server = nullptr;
    if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER, IID_IDispatch,
                                reinterpret_cast<void**>(&server)))
        || !server) {
        m_status = QCoreApplication::translate("OmniRigControl", "cannot start OmniRig");
        emit failed(m_status);
        emit changed();
        return;
    }
    m_server = server;
    VARIANT v;
    if (!getProperty(server, m_rigNumber == 2 ? L"Rig2" : L"Rig1", &v) || v.vt != VT_DISPATCH || !v.pdispVal) {
        VariantClear(&v);
        m_status = QCoreApplication::translate("OmniRigControl", "OmniRig has no Rig%1").arg(m_rigNumber);
        emit failed(m_status);
        release();
        emit changed();
        return;
    }
    m_rig = v.pdispVal;      // il riferimento passa a noi
    m_status = QCoreApplication::translate("OmniRigControl", "OmniRig Rig%1…").arg(m_rigNumber);
    refresh();
    m_poll.start();
#else
    m_status = QCoreApplication::translate("OmniRigControl", "OmniRig exists only on Windows");
    emit failed(m_status);
    emit changed();
#endif
}

void OmniRigControl::disconnectFromRig()
{
    m_poll.stop();
    release();
    const bool was = m_connected;
    m_connected = false;
    m_hz = 0;
    m_mode.clear();
    m_status = QCoreApplication::translate("OmniRigControl", "not connected");
    if (was)
        emit changed();
}

void OmniRigControl::refresh()
{
#ifdef Q_OS_WIN
    IDispatch* rig = dispatch(m_rig);
    if (!rig)
        return;
    bool ok = false;
    const long st = getLong(rig, L"Status", &ok);
    const bool online = ok && st == omnirig::ST_ONLINE;
    bool dirty = online != m_connected;
    m_connected = online;
    if (!online) {
        const QString text = st == omnirig::ST_NOTCONFIGURED ? QCoreApplication::translate("OmniRigControl", "Rig%1 not configured in OmniRig")
                           : st == omnirig::ST_DISABLED      ? QCoreApplication::translate("OmniRigControl", "Rig%1 disabled in OmniRig")
                           : st == omnirig::ST_PORTBUSY      ? QCoreApplication::translate("OmniRigControl", "Rig%1: the port is busy")
                                                             : QCoreApplication::translate("OmniRigControl", "Rig%1 does not answer");
        const QString s = text.arg(m_rigNumber);
        dirty = dirty || s != m_status;
        m_status = s;
        if (dirty)
            emit changed();
        return;
    }
    long hz = getLong(rig, L"Freq");
    if (hz <= 0)
        hz = getLong(rig, L"FreqA");
    const QString mode = omnirig::toHamlibMode(getLong(rig, L"Mode"));
    const QString s = QCoreApplication::translate("OmniRigControl", "OmniRig Rig%1").arg(m_rigNumber);
    if (hz != m_hz || mode != m_mode || s != m_status)
        dirty = true;
    m_hz = hz;
    m_mode = mode;
    m_status = s;
    if (dirty)
        emit changed();
#endif
}

void OmniRigControl::setFrequency(qint64 hz)
{
#ifdef Q_OS_WIN
    if (hz > 0 && putLong(dispatch(m_rig), L"Freq", static_cast<long>(hz))) {
        m_hz = hz;
        emit changed();
    }
#else
    Q_UNUSED(hz);
#endif
}

void OmniRigControl::setMode(const QString& mode)
{
#ifdef Q_OS_WIN
    if (const long m = omnirig::fromHamlibMode(mode))
        putLong(dispatch(m_rig), L"Mode", m);
#else
    Q_UNUSED(mode);
#endif
}

void OmniRigControl::setPtt(bool on)
{
#ifdef Q_OS_WIN
    putLong(dispatch(m_rig), L"Tx", on ? omnirig::PM_TX : omnirig::PM_RX);
#else
    Q_UNUSED(on);
#endif
}

void OmniRigControl::sendMorse(const QString& text)
{
    Q_UNUSED(text);
    emit morseUnsupported();
}

} // namespace decolog::core
