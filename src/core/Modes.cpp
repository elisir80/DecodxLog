#include "core/Modes.h"

#include <QHash>

namespace decolog::core::modes {

namespace {

// L'ordine e' quello di chi opera: prima CW, poi la fonia, poi i digitali dal
// piu' usato al meno usato. I nomi sono quelli ADIF, cosi' finiscono nel log
// come stanno.
const QVector<Entry>& table()
{
    static const QVector<Entry> entries = {
        {QStringLiteral("CW"),      QStringLiteral("CW"),     QStringLiteral("cw")},
        {QStringLiteral("CW-R"),    QStringLiteral("CWR"),    QStringLiteral("cw")},
        {QStringLiteral("USB"),     QStringLiteral("USB"),    QStringLiteral("voice")},
        {QStringLiteral("LSB"),     QStringLiteral("LSB"),    QStringLiteral("voice")},
        {QStringLiteral("AM"),      QStringLiteral("AM"),     QStringLiteral("voice")},
        {QStringLiteral("FM"),      QStringLiteral("FM"),     QStringLiteral("voice")},
        {QStringLiteral("FT8"),     QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("FT4"),     QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("FT2"),     QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("JS8"),     QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("JT65"),    QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("JT9"),     QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("Q65"),     QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("MSK144"),  QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("WSPR"),    QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("RTTY"),    QStringLiteral("RTTY"),   QStringLiteral("data")},
        {QStringLiteral("RTTY-R"),  QStringLiteral("RTTYR"),  QStringLiteral("data")},
        {QStringLiteral("PSK31"),   QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("PSK63"),   QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("OLIVIA"),  QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("MFSK"),    QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("CONTESTI"), QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("HELL"),    QStringLiteral("PKTUSB"), QStringLiteral("data")},
        {QStringLiteral("SSTV"),    QStringLiteral("USB"),    QStringLiteral("data")},
        {QStringLiteral("PACKET"),  QStringLiteral("PKTFM"),  QStringLiteral("data")},
        {QStringLiteral("DIGITALVOICE"), QStringLiteral("PKTUSB"), QStringLiteral("data")},
    };
    return entries;
}

} // namespace

QVector<Entry> all()
{
    return table();
}

QString catFor(const QString& mode)
{
    const QString wanted = mode.trimmed().toUpper();
    if (wanted.isEmpty())
        return QStringLiteral("USB");
    for (const Entry& e : table()) {
        if (e.name == wanted)
            return e.cat;
    }
    // Chi arriva da Decodium puo' portare un sottomodo che qui non c'e'
    // (FT8-DX, PSK125…): se e' scritto come uno dei nostri, vale quello.
    for (const Entry& e : table()) {
        if (e.group == QLatin1String("data") && wanted.startsWith(e.name))
            return e.cat;
    }
    // Anche i nomi che la radio usa gia' vanno bene cosi' come sono.
    for (const Entry& e : table()) {
        if (e.cat == wanted)
            return e.cat;
    }
    if (wanted == QLatin1String("SSB") || wanted == QLatin1String("PHONE"))
        return QStringLiteral("USB");
    if (wanted == QLatin1String("DATA") || wanted == QLatin1String("DIGI"))
        return QStringLiteral("PKTUSB");
    return QStringLiteral("USB");
}

} // namespace decolog::core::modes
