#include "core/SecretVault.h"

#ifdef DECOLOG_HAS_OPENSSL
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#endif

#include <QCryptographicHash>

namespace decolog::core::vault {

namespace {

constexpr int kIterations = 200'000;
constexpr int kKeyBytes = 32;   // AES-256
constexpr int kIvBytes = 12;    // il nonce di GCM
constexpr int kTagBytes = 16;

// Il sale: non e' segreto e non puo' esserlo — il secondo computer deve poterlo
// rifare avendo solo la password. Legato al nominativo, cosi' due stazioni con
// la stessa password non hanno la stessa chiave.
QByteArray saltFor(const QString& callsign)
{
    const QByteArray seed = QByteArrayLiteral("decolog-vault|") + callsign.trimmed().toUpper().toUtf8();
    return QCryptographicHash::hash(seed, QCryptographicHash::Sha256).left(16);
}

} // namespace

#ifdef DECOLOG_HAS_OPENSSL

bool available() { return true; }

QByteArray deriveKey(const QString& password, const QString& callsign)
{
    if (password.isEmpty() || callsign.trimmed().isEmpty())
        return {};
    const QByteArray pass = password.toUtf8();
    const QByteArray salt = saltFor(callsign);
    QByteArray key(kKeyBytes, '\0');
    const int ok = PKCS5_PBKDF2_HMAC(pass.constData(), pass.size(),
                                     reinterpret_cast<const unsigned char*>(salt.constData()), salt.size(),
                                     kIterations, EVP_sha256(), kKeyBytes,
                                     reinterpret_cast<unsigned char*>(key.data()));
    return ok == 1 ? key : QByteArray{};
}

QString seal(const QByteArray& key, const QByteArray& plain)
{
    if (key.size() != kKeyBytes)
        return {};

    QByteArray iv(kIvBytes, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), kIvBytes) != 1)
        return {};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return {};

    QByteArray out(plain.size() + kTagBytes, '\0');
    QByteArray tag(kTagBytes, '\0');
    int len = 0;
    int total = 0;
    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kIvBytes, nullptr) == 1
        && EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                              reinterpret_cast<const unsigned char*>(key.constData()),
                              reinterpret_cast<const unsigned char*>(iv.constData())) == 1
        && EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &len,
                             reinterpret_cast<const unsigned char*>(plain.constData()),
                             static_cast<int>(plain.size())) == 1;
    if (ok) {
        total = len;
        ok = EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + total, &len) == 1;
        total += len;
    }
    if (ok) {
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagBytes,
                                 reinterpret_cast<unsigned char*>(tag.data())) == 1;
    }
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        return {};

    out.truncate(total);
    return QString::fromLatin1((iv + tag + out).toBase64());
}

std::optional<QByteArray> unseal(const QByteArray& key, const QString& sealed)
{
    if (key.size() != kKeyBytes)
        return std::nullopt;
    const QByteArray blob = QByteArray::fromBase64(sealed.toLatin1());
    if (blob.size() < kIvBytes + kTagBytes)
        return std::nullopt;

    const QByteArray iv = blob.left(kIvBytes);
    const QByteArray tag = blob.mid(kIvBytes, kTagBytes);
    const QByteArray body = blob.mid(kIvBytes + kTagBytes);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return std::nullopt;

    QByteArray out(body.size() + kTagBytes, '\0');
    int len = 0;
    int total = 0;
    bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kIvBytes, nullptr) == 1
        && EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                              reinterpret_cast<const unsigned char*>(key.constData()),
                              reinterpret_cast<const unsigned char*>(iv.constData())) == 1
        && EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &len,
                             reinterpret_cast<const unsigned char*>(body.constData()),
                             static_cast<int>(body.size())) == 1;
    if (ok) {
        total = len;
        ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagBytes,
                                 const_cast<char*>(tag.constData())) == 1;
    }
    if (ok) {
        // Qui GCM controlla il sigillo: se non torna, il blocco non e' nostro o
        // e' stato toccato.
        ok = EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + total, &len) == 1;
        total += len;
    }
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        return std::nullopt;

    out.truncate(total);
    return out;
}

#else   // senza OpenSSL non si finge: le credenziali restano dove sono

bool available() { return false; }
QByteArray deriveKey(const QString&, const QString&) { return {}; }
QString seal(const QByteArray&, const QByteArray&) { return {}; }
std::optional<QByteArray> unseal(const QByteArray&, const QString&) { return std::nullopt; }

#endif

} // namespace decolog::core::vault
