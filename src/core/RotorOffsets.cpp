#include "core/RotorOffsets.h"

#include "core/RotorLink.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

namespace decolog::core::rotoroffsets {

namespace {

// Lo scostamento come lo si legge: da -180 a +180, che +270 e -90 sono la stessa cosa.
int tidy(int offset)
{
    int o = offset % 360;
    if (o > 180)
        o -= 360;
    if (o <= -180)
        o += 360;
    return o;
}

} // namespace

QList<Entry> fromJson(const QString& json)
{
    QList<Entry> out;
    for (const QJsonValue& v : QJsonDocument::fromJson(json.toUtf8()).array()) {
        const QJsonObject o = v.toObject();
        Entry e;
        e.band = o.value(QStringLiteral("band")).toString().trimmed().toLower();
        e.antenna = o.value(QStringLiteral("antenna")).toString().trimmed();
        e.offset = tidy(o.value(QStringLiteral("offset")).toInt());
        out << e;
    }
    return out;
}

QString toJson(const QList<Entry>& entries)
{
    QJsonArray array;
    for (const Entry& e : entries)
        array.append(QJsonObject{{QStringLiteral("band"), e.band},
                                 {QStringLiteral("antenna"), e.antenna},
                                 {QStringLiteral("offset"), tidy(e.offset)}});
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QVariantList toVariant(const QList<Entry>& entries)
{
    QVariantList out;
    for (const Entry& e : entries)
        out << QVariantMap{{QStringLiteral("band"), e.band},
                           {QStringLiteral("antenna"), e.antenna},
                           {QStringLiteral("offset"), tidy(e.offset)}};
    return out;
}

const Entry* forBand(const QList<Entry>& entries, const QString& band)
{
    const QString b = band.trimmed().toLower();
    if (b.isEmpty())
        return nullptr;
    for (const Entry& e : entries) {
        if (e.band == b)
            return &e;
    }
    return nullptr;
}

double antennaAz(double rotorAz, int offset)
{
    return rotor::normalize(rotorAz + tidy(offset));
}

double rotorAz(double antennaAz, int offset)
{
    return rotor::normalize(antennaAz - tidy(offset));
}

} // namespace decolog::core::rotoroffsets
