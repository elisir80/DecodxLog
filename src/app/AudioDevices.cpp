#include "app/AudioDevices.h"

#include <QHash>
#include <QMediaDevices>
#include <QVariantMap>

namespace decolog::app::audiodev {

Resolution resolve(const QList<Entry>& entries, const Saved& saved)
{
    if (saved.id.isEmpty() && saved.name.isEmpty())
        return {Resolution::SystemDefault, -1};

    // Prima l'identificativo: e' quello che non cambia quando Windows rinomina
    // la scheda.
    if (!saved.id.isEmpty()) {
        for (int i = 0; i < entries.size(); ++i) {
            if (QString::fromUtf8(entries.at(i).id) == saved.id)
                return {Resolution::Found, i};
        }
    }
    // Poi il nome com'era scritto, ma solo se non lascia dubbi.
    QList<int> byName;
    if (!saved.name.isEmpty()) {
        for (int i = 0; i < entries.size(); ++i) {
            if (entries.at(i).name == saved.name)
                byName << i;
        }
    }
    if (byName.size() == 1)
        return {Resolution::Found, byName.first()};
    if (byName.size() > 1)
        return {Resolution::Ambiguous, byName.first()};
    return {Resolution::Missing, -1};
}

QStringList labels(const QList<Entry>& entries)
{
    QStringList out;
    QHash<QString, int> seen;
    for (const Entry& e : entries) {
        const int n = ++seen[e.name];
        out << (n == 1 ? e.name : QStringLiteral("%1 (%2)").arg(e.name).arg(n));
    }
    return out;
}

int comboIndex(const Resolution& resolution)
{
    switch (resolution.kind) {
    case Resolution::SystemDefault:
        return 0;
    case Resolution::Found:
    case Resolution::Ambiguous:
        return resolution.index + 1;
    case Resolution::Missing:
        return -1;
    }
    return -1;
}

QList<QAudioDevice> inputDevices()
{
    return QMediaDevices::audioInputs();
}

QList<QAudioDevice> outputDevices()
{
    return QMediaDevices::audioOutputs();
}

QList<Entry> entriesOf(const QList<QAudioDevice>& devices)
{
    QList<Entry> out;
    out.reserve(devices.size());
    for (const QAudioDevice& d : devices)
        out.append(Entry{d.id(), d.description()});
    return out;
}

QVariantList deviceList(const QList<QAudioDevice>& devices)
{
    const QList<Entry> entries = entriesOf(devices);
    const QStringList shown = labels(entries);
    QVariantList out;
    for (int i = 0; i < entries.size(); ++i) {
        out << QVariantMap{{QStringLiteral("id"), QString::fromUtf8(entries.at(i).id)},
                           {QStringLiteral("name"), entries.at(i).name},
                           {QStringLiteral("label"), shown.at(i)}};
    }
    return out;
}

Saved savedFor(const QList<QAudioDevice>& devices, int comboRow)
{
    if (comboRow <= 0 || comboRow > devices.size())
        return {};
    const QAudioDevice& d = devices.at(comboRow - 1);
    return Saved{QString::fromUtf8(d.id()), d.description()};
}

} // namespace decolog::app::audiodev
