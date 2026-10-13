#include "core/SerialPorts.h"
#include "../StartupTrace.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QSerialPortInfo>
#include <QSet>

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

} // namespace

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
            return last;
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
    return ports;
}

} // namespace decolog::core
