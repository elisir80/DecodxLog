#include "core/ReleaseSignature.h"

#include "monocypher-ed25519.h"
#include "monocypher.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

namespace decolog::core::releasesig {

namespace {

constexpr char kFormat[] = "decodxlog-release/1";

const uint8_t* bytes(const QByteArray& data)
{
    return reinterpret_cast<const uint8_t*>(data.constData());
}

QString cleanVersion(QString version)
{
    version = version.trimmed();
    if (version.startsWith(QLatin1Char('v')) || version.startsWith(QLatin1Char('V')))
        version.remove(0, 1);
    return version;
}

} // namespace

QList<TrustedKey> trustedKeys()
{
    // Le chiavi di chi pubblica, in base64. La prima e' quella di tutti i
    // giorni; la seconda e' di scorta, tenuta fuori dal computer: se la prima
    // si perde, le versioni firmate con la seconda si installano lo stesso.
    // Un fork che pubblica i suoi pacchetti aggiunge qui le sue, col suo
    // repository: la firma di uno non vale per le release dell'altro.
    struct Entry {
        const char* repository;
        const char* key;
    };
    static const Entry entries[] = {
        // 9194bfce15e1a728, chiave dedicata alle release del fork elisir80.
        {"elisir80/DecodxLog", "NKqwHmHypOygT4oWhhj/+J8U2rExbrR2Kdf8Pg9OTqk="},
        // ad518c5309aee583, quella di tutti i giorni (portachiavi di IU8LMC)
        {"iu8lmc/DecoDXLog", "E5xzin16Px/dO4DFHhaoVO1HfR/6R00YX3vGSzjIKgQ="},
        // 411be55f604c83ef, quella di scorta (fuori dal computer)
        {"iu8lmc/DecoDXLog", "XpjAue2FJWljh1HS3AGR7Ng9JKtKw6mOl0N3e6th990="},
    };
    QList<TrustedKey> out;
    for (const Entry& e : entries) {
        const QByteArray key = QByteArray::fromBase64(QByteArray(e.key));
        if (key.size() == 32)
            out.append({QString::fromLatin1(e.repository), key});
    }
    return out;
}

QString keyId(const QByteArray& publicKey)
{
    return QString::fromLatin1(QCryptographicHash::hash(publicKey, QCryptographicHash::Sha256).left(8).toHex());
}

SignedFile Manifest::file(const QString& name) const
{
    for (const SignedFile& f : files) {
        if (f.name == name)
            return f;
    }
    return {};
}

QByteArray buildManifest(const QString& repository, const QString& version, const QList<SignedFile>& files)
{
    QJsonArray list;
    for (const SignedFile& f : files) {
        list.append(QJsonObject{{QStringLiteral("name"), f.name},
                                {QStringLiteral("size"), static_cast<double>(f.size)},
                                {QStringLiteral("sha256"), QString::fromLatin1(f.sha256.toHex())}});
    }
    const QJsonObject root{
        {QStringLiteral("format"), QLatin1String(kFormat)},
        {QStringLiteral("repository"), repository},
        {QStringLiteral("version"), cleanVersion(version)},
        {QStringLiteral("signed"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("files"), list},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray signManifest(const QByteArray& manifest, const QByteArray& secretKey)
{
    if (secretKey.size() != 64)
        return {};
    const QByteArray signature = ed25519Sign(manifest, secretKey);
    const QJsonObject root{{QStringLiteral("key"), keyId(secretKey.mid(32))},
                           {QStringLiteral("signature"), QString::fromLatin1(signature.toBase64())}};
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

Manifest verify(const QByteArray& manifest, const QByteArray& signature, const QString& repository,
                const QString& version, const QList<TrustedKey>& keys)
{
    Manifest out;
    if (manifest.isEmpty() || signature.isEmpty())
        return out;   // Missing

    // La firma: di quale chiave, e i 64 byte.
    const QJsonObject sig = QJsonDocument::fromJson(signature).object();
    out.keyId = sig.value(QStringLiteral("key")).toString().trimmed().toLower();
    const QByteArray raw = QByteArray::fromBase64(sig.value(QStringLiteral("signature")).toString().toLatin1());
    if (raw.size() != 64) {
        out.state = Manifest::State::Invalid;
        return out;
    }

    // La chiave dev'essere di questo repository: quella di un altro, anche se
    // conosciuta, non firma le release di qui.
    const TrustedKey* key = nullptr;
    for (const TrustedKey& k : keys) {
        if (k.repository.compare(repository, Qt::CaseInsensitive) == 0 && keyId(k.publicKey) == out.keyId) {
            key = &k;
            break;
        }
    }
    if (!key) {
        out.state = Manifest::State::Untrusted;
        return out;
    }
    if (!ed25519Check(raw, key->publicKey, manifest)) {
        out.state = Manifest::State::Invalid;
        return out;
    }

    // Firmato davvero: adesso si legge, e dev'essere proprio questa release.
    out.state = Manifest::State::Invalid;
    const QJsonObject root = QJsonDocument::fromJson(manifest).object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(kFormat))
        return out;
    out.repository = root.value(QStringLiteral("repository")).toString();
    out.version = cleanVersion(root.value(QStringLiteral("version")).toString());
    if (out.repository.compare(repository, Qt::CaseInsensitive) != 0 || out.version != cleanVersion(version))
        return out;
    for (const QJsonValue& v : root.value(QStringLiteral("files")).toArray()) {
        const QJsonObject f = v.toObject();
        SignedFile file;
        file.name = f.value(QStringLiteral("name")).toString();
        file.size = static_cast<qint64>(f.value(QStringLiteral("size")).toDouble());
        file.sha256 = QByteArray::fromHex(f.value(QStringLiteral("sha256")).toString().toLatin1());
        if (file.name.isEmpty() || file.size <= 0 || file.sha256.size() != 32)
            return out;
        out.files.append(file);
    }
    out.state = Manifest::State::Verified;
    return out;
}

Manifest verify(const QByteArray& manifest, const QByteArray& signature, const QString& repository,
                const QString& version)
{
    return verify(manifest, signature, repository, version, trustedKeys());
}

bool generateKeyPair(QByteArray* secretKey, QByteArray* publicKey)
{
    QByteArray seed(32, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(seed.data()), 8);
    *secretKey = keyPairFromSeed(seed, publicKey);
    crypto_wipe(seed.data(), static_cast<size_t>(seed.size()));
    return secretKey->size() == 64 && publicKey->size() == 32;
}

QByteArray keyPairFromSeed(const QByteArray& seed, QByteArray* publicKey)
{
    if (seed.size() != 32)
        return {};
    // Monocypher cancella il seme che riceve: gli si passa una copia.
    QByteArray copy = seed;
    QByteArray secret(64, '\0');
    QByteArray pub(32, '\0');
    crypto_ed25519_key_pair(reinterpret_cast<uint8_t*>(secret.data()), reinterpret_cast<uint8_t*>(pub.data()),
                            reinterpret_cast<uint8_t*>(copy.data()));
    if (publicKey)
        *publicKey = pub;
    return secret;
}

QByteArray ed25519Sign(const QByteArray& message, const QByteArray& secretKey)
{
    if (secretKey.size() != 64)
        return {};
    QByteArray signature(64, '\0');
    crypto_ed25519_sign(reinterpret_cast<uint8_t*>(signature.data()), bytes(secretKey), bytes(message),
                        static_cast<size_t>(message.size()));
    return signature;
}

bool ed25519Check(const QByteArray& signature, const QByteArray& publicKey, const QByteArray& message)
{
    if (signature.size() != 64 || publicKey.size() != 32)
        return false;
    return crypto_ed25519_check(bytes(signature), bytes(publicKey), bytes(message),
                                static_cast<size_t>(message.size())) == 0;
}

} // namespace decolog::core::releasesig
