#include "core/CloudSettings.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QStringList>

namespace decolog::core::cloudsettings {

namespace {

// Quello che non si porta dietro, e perche':
//
//  * `credentials/...` — non sono i segreti (quelli stanno nel portachiavi, e
//    nel portachiavi restano): sono il promemoria di cosa e' stato salvato
//    *qui*. Portarlo altrove farebbe credere a DecoDXLog di avere una password
//    che non ha.
//  * `cloud/callsign` e `cloud/lastSync` — il quaderno del sync stesso.
//  * `station/activeProfile` — il numero di riga del profilo su questo
//    computer: al suo posto viaggia l'uuid.
//  * `net/id` — l'identita' UDP di questa macchina; copiarla creerebbe una
//    collisione e farebbe scartare i messaggi dell'altro dispositivo.
//
// La versione 1 dell'ambito portabile tiene inoltre locali le risorse legate
// all'installazione: porte e dispositivi audio/seriali, indirizzi dei servizi
// locali e percorsi. Un percorso Windows non deve finire nelle impostazioni di
// un Mac/Linux, e una porta occupata su un computer non descrive la stazione.
// Le chiavi portabili (tema, macro, filtri, layout e profilo per UUID) restano
// invece sincronizzabili; le chiavi non riconosciute mantengono il precedente
// comportamento per compatibilita' con impostazioni future.
const QStringList kNeverSynced{
    QStringLiteral("credentials"),
    QStringLiteral("cloud/callsign"),
    QStringLiteral("cloud/lastSync"),
    QStringLiteral("station/activeProfile"),
    QStringLiteral("net/id"),
    QStringLiteral("rig/serialPort"),
    QStringLiteral("rig/pttPort"),
    QStringLiteral("rig/port"),
    QStringLiteral("rig/host"),
    QStringLiteral("rig/baud"),
    QStringLiteral("rig/dataBits"),
    QStringLiteral("rig/stopBits"),
    QStringLiteral("rig/parity"),
    QStringLiteral("rig/handshake"),
    QStringLiteral("rig/civAddress"),
    QStringLiteral("rig/keyerPort"),
    QStringLiteral("rig/share/port"),
    QStringLiteral("cw/audioInput"),
    QStringLiteral("cw/audioInputId"),
    QStringLiteral("cw/neuralModel"),
    QStringLiteral("rotor/serialPort"),
    QStringLiteral("rotor/arcoSerialPort"),
    QStringLiteral("rotor/host"),
    QStringLiteral("rotor/port"),
    QStringLiteral("rotor/httpPort"),
    QStringLiteral("rotorGateway/serialPort"),
    QStringLiteral("rotorGateway/rotctldPort"),
    QStringLiteral("rotorGateway/wsPort"),
    QStringLiteral("decolink/port"),
    QStringLiteral("n1mm/port"),
    QStringLiteral("so2r/otrspPort"),
    QStringLiteral("api/port"),
    QStringLiteral("net/port"),
    QStringLiteral("udp/port"),
    QStringLiteral("udp/multicastGroup"),
    QStringLiteral("backup"),
    QStringLiteral("decodium/logPath"),
    QStringLiteral("qsl/tqslPath"),
    QStringLiteral("path"),
};

bool denied(const QString& key)
{
    for (const QString& deny : kNeverSynced) {
        if (key == deny || key.startsWith(deny + QLatin1Char('/')))
            return true;
    }
    return false;
}

constexpr auto kPacked = "__qvariant__";

// Un valore detto sempre allo stesso modo: i numeri e i si'/no come testo,
// perche' il file INI li rende come testo e il programma a volte come numeri.
QJsonValue canonical(const QJsonValue& value)
{
    if (value.isDouble())
        return QString::number(value.toDouble(), 'g', 17);
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.isArray()) {
        QJsonArray out;
        for (const QJsonValue& item : value.toArray())
            out.append(canonical(item));
        return out;
    }
    if (value.isObject()) {
        QJsonObject out;
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            out.insert(it.key(), canonical(it.value()));
        return out;
    }
    return value;
}

} // namespace

bool sameValue(const QVariant& a, const QVariant& b)
{
    return canonical(QJsonValue::fromVariant(a)) == canonical(QJsonValue::fromVariant(b));
}

Scope scopeOf(const QString& key)
{
    if (denied(key) || key == QLatin1String("station/activeProfile"))
        return Scope::Device;
    if (key == QLatin1String("station/activeProfileUuid")
        || key == QLatin1String("station/profiles")
        || key.startsWith(QStringLiteral("station/profile/")))
        return Scope::Station;
    return Scope::Portable;
}

bool isMachineOnly(const QString& key)
{
    return scopeOf(key) == Scope::Device;
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
          const std::function<qint64(const QString&)>& profileForUuid,
          const QVariantMap* base, QStringList* keptLocal)
{
    int written = 0;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        // Le cose di questa macchina non si lasciano riscrivere da fuori,
        // qualunque cosa arrivi dal server.
        if (isMachineOnly(it.key()))
            continue;
        // Cambiata qui e non ancora mandata: resta com'e'. Era il guaio delle
        // macro CW che tornavano quelle di prima dopo un giro di sync.
        if (base && it.key() != kActiveProfileUuid && settings.contains(it.key())) {
            const QVariant local = pack(settings.value(it.key()));
            const bool changedHere = !base->contains(it.key()) || !sameValue(local, base->value(it.key()));
            if (changedHere && !sameValue(local, it.value())) {
                if (keptLocal)
                    keptLocal->append(it.key());
                continue;
            }
        }
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
