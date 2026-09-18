#include "core/CloudSettings.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStringList>

namespace decolog::core::cloudsettings {

namespace {

// Quello che non si porta dietro, e perche':
//
//  * `credentials/...` — non sono i segreti (quelli stanno nel portachiavi, e
//    nel portachiavi restano): sono il promemoria di cosa e' stato salvato
//    *qui*. Portarlo altrove farebbe credere a DecoLog di avere una password
//    che non ha.
//  * `cloud/callsign` e `cloud/lastSync` — il quaderno del sync stesso.
//  * `station/activeProfile` — il numero di riga del profilo su questo
//    computer: al suo posto viaggia l'uuid.
const QStringList kNeverSynced{
    QStringLiteral("credentials"),
    QStringLiteral("cloud/callsign"),
    QStringLiteral("cloud/lastSync"),
    QStringLiteral("station/activeProfile"),
};

constexpr auto kPacked = "__qvariant__";

} // namespace

bool isMachineOnly(const QString& key)
{
    for (const QString& deny : kNeverSynced) {
        if (key == deny || key.startsWith(deny + QLatin1Char('/')))
            return true;
    }
    return false;
}

QVariant pack(const QVariant& value)
{
    switch (value.typeId()) {
    case QMetaType::QString:
    case QMetaType::Bool:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::QStringList:
        return value;
    default:
        break;
    }
    QByteArray blob;
    QDataStream stream(&blob, QIODevice::WriteOnly);
    stream << value;
    return QVariantMap{{QLatin1String(kPacked), QString::fromLatin1(blob.toBase64())}};
}

QVariant unpack(const QVariant& value)
{
    if (value.typeId() != QMetaType::QVariantMap)
        return value;
    const QVariantMap wrapper = value.toMap();
    const auto it = wrapper.constFind(QLatin1String(kPacked));
    if (it == wrapper.constEnd())
        return value;
    QByteArray blob = QByteArray::fromBase64(it.value().toString().toLatin1());
    QDataStream stream(&blob, QIODevice::ReadOnly);
    QVariant out;
    stream >> out;
    return out;
}

QVariantMap collect(QSettings& settings, const std::function<QString(qint64)>& uuidForProfile)
{
    QVariantMap values;
    for (const QString& key : settings.allKeys()) {
        if (isMachineOnly(key))
            continue;
        values.insert(key, pack(settings.value(key)));
    }
    const qint64 active = settings.value(QStringLiteral("station/activeProfile")).toLongLong();
    if (active > 0 && uuidForProfile) {
        const QString uuid = uuidForProfile(active);
        if (!uuid.isEmpty())
            values.insert(kActiveProfileUuid, uuid);
    }
    return values;
}

int apply(QSettings& settings, const QVariantMap& values, bool dryRun,
          const std::function<qint64(const QString&)>& profileForUuid)
{
    int written = 0;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        // Le cose di questa macchina non si lasciano riscrivere da fuori,
        // qualunque cosa arrivi dal server.
        if (isMachineOnly(it.key()))
            continue;
        if (it.key() == kActiveProfileUuid) {
            if (!profileForUuid)
                continue;
            const qint64 id = profileForUuid(it.value().toString());
            if (id <= 0 || settings.value(QStringLiteral("station/activeProfile")).toLongLong() == id)
                continue;
            if (!dryRun)
                settings.setValue(QStringLiteral("station/activeProfile"), id);
            ++written;
            continue;
        }
        const QVariant value = unpack(it.value());
        if (settings.value(it.key()) == value)
            continue;
        if (!dryRun)
            settings.setValue(it.key(), value);
        ++written;
    }
    if (!dryRun && written > 0)
        settings.sync();
    return written;
}

QString fingerprint(const QVariantMap& values)
{
    const QByteArray json = QJsonDocument(QJsonObject::fromVariantMap(values)).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(QCryptographicHash::hash(json, QCryptographicHash::Sha256).toHex());
}

} // namespace decolog::core::cloudsettings
