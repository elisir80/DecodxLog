#include "core/SerialPorts.h"
#include "../StartupTrace.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QSet>
#include <QSettings>

#include <algorithm>

namespace decolog::core {

namespace {

bool isPseudoTerminal(const QString& path)
{
#if defined(Q_OS_MACOS)
    // /dev/ttys000, /dev/ttys001, ... sono le sessioni del terminale macOS.
    // Le porte USB/Bluetooth usabili per CAT sono normalmente /dev/cu.*.
    return path.startsWith(QStringLiteral("/dev/ttys"));
#elif defined(Q_OS_LINUX)
    return path.startsWith(QStringLiteral("/dev/pts/"));
#else
    Q_UNUSED(path)
    return false;
#endif
}

QString portPath(const QSerialPortInfo& info)
{
#if defined(Q_OS_WIN)
    return info.portName().trimmed();
#else
    const QString location = info.systemLocation().trimmed();
    return location.isEmpty() ? info.portName().trimmed() : location;
#endif
}

QStringList withoutReserved(const QStringList& ports)
{
    const QStringList reserved = reservedSerialPorts();
    if (reserved.isEmpty())
        return ports;
    QStringList out;
    for (const QString& port : ports) {
        if (!reserved.contains(normalizedPortName(port)))
            out << port;
    }
    return out;
}

} // namespace

QString normalizedPortName(const QString& port)
{
    QString clean = port.trimmed();
    if (clean.startsWith(QStringLiteral("\\\\.\\")))
        clean = clean.mid(4);
#if defined(Q_OS_WIN)
    clean = clean.toUpper();
#endif
    return clean;
}

QStringList userReservedSerialPorts()
{
    QStringList out;
    const QString text = QSettings().value(QStringLiteral("serial/reserved")).toString();
    for (const QString& part : text.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts)) {
        const QString clean = normalizedPortName(part);
        if (!clean.isEmpty() && !out.contains(clean))
            out << clean;
    }
    return out;
}

void setUserReservedSerialPorts(const QStringList& ports)
{
    QStringList clean;
    for (const QString& port : ports) {
        const QString name = normalizedPortName(port);
        if (!name.isEmpty() && !clean.contains(name))
            clean << name;
    }
    QSettings().setValue(QStringLiteral("serial/reserved"), clean.join(QStringLiteral(", ")));
}

bool leaveDecodiumPort()
{
    return QSettings().value(QStringLiteral("serial/leaveDecodium"), true).toBool();
}

void setLeaveDecodiumPort(bool on)
{
    QSettings().setValue(QStringLiteral("serial/leaveDecodium"), on);
}

QString decodiumCatPort()
{
    // Decodium tiene le sue impostazioni in Decodium3.ini; il CAT sta sotto [Transceiver].
    const QSettings decodium(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"),
                             QStringLiteral("Decodium3"));
    for (const char* key : {"Transceiver/serialPort", "CAT_Native/serialPort"}) {
        const QString port = normalizedPortName(decodium.value(QLatin1String(key)).toString());
        if (!port.isEmpty())
            return port;
    }
    return {};
}

QStringList reservedSerialPorts()
{
    QStringList out = userReservedSerialPorts();
    if (leaveDecodiumPort()) {
        const QString decodium = decodiumCatPort();
        if (!decodium.isEmpty() && !out.contains(decodium))
            out << decodium;
    }
    return out;
}

bool isSerialPortReserved(const QString& port)
{
    const QString clean = normalizedPortName(port);
    return !clean.isEmpty() && reservedSerialPorts().contains(clean);
}

bool isHiddenVirtualPort(const QString& port)
{
    static const QRegularExpression hidden(QStringLiteral("^CNC[AB]\\d+$"),
                                           QRegularExpression::CaseInsensitiveOption);
    return hidden.match(normalizedPortName(port)).hasMatch();
}

QStringList virtualSerialPorts()
{
    QStringList out;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        if (!info.description().contains(QStringLiteral("com0com"), Qt::CaseInsensitive))
            continue;
        const QString name = portPath(info);
        if (name.isEmpty() || isHiddenVirtualPort(name))
            continue;
        if (!out.contains(name))
            out << name;
    }
    return withoutReserved(out);
}

QStringList serialProbeOrder(const QString& current, const QStringList& all, const QStringList& virtualPorts)
{
    QStringList order;
    auto add = [&order](const QString& port) {
        const QString name = port.trimmed();
        if (name.isEmpty() || isSerialPortReserved(name) || isHiddenVirtualPort(name))
            return;
        for (const QString& known : std::as_const(order)) {
            if (normalizedPortName(known) == normalizedPortName(name))
                return;
        }
        order << name;
    };
    add(current);
    for (const QString& port : virtualPorts)
        add(port);
    for (const QString& port : all)
        add(port);
    return order;
}

QStringList availableSerialPorts()
{
    // L'elenco costa una quarantina di millisecondi (SetupAPI) e all'apertura della finestra lo
    // chiedono in tanti, uno dopo l'altro: per un paio di secondi si risponde con quello di prima.
    static QMutex lock;
    static QElapsedTimer age;
    static QStringList last;
    {
        QMutexLocker guard(&lock);
        if (age.isValid() && age.elapsed() < 2000)
            return withoutReserved(last);
    }

    decolog::StartupSpan trace("Serial ports list");
    QStringList ports;
    QSet<QString> seen;

    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        if (info.portName().contains(QStringLiteral("NULL"), Qt::CaseInsensitive))
            continue;

        const QString path = portPath(info);
        if (path.isEmpty() || isPseudoTerminal(path))
            continue;

        // Deduplica i due alias che alcuni driver pubblicano per la stessa USB.
        const QString canonical = QFileInfo(path).canonicalFilePath();
        const QString key = canonical.isEmpty() ? path : canonical;
        if (seen.contains(key))
            continue;
        seen.insert(key);
        ports << path;
    }

    std::sort(ports.begin(), ports.end(), [](const QString& a, const QString& b) {
        return QString::localeAwareCompare(a, b) < 0;
    });
    {
        QMutexLocker guard(&lock);
        last = ports;
        age.restart();
    }
    return withoutReserved(ports);
}

} // namespace decolog::core
