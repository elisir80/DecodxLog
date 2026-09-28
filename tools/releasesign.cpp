// decodxlog_sign — firma le release di DecoDXLog.
//
// La chiave segreta sta nel portachiavi di chi pubblica (Gestione credenziali su
// Windows), mai in un file del repository e mai stampata. Lo strumento non la
// mostra: la crea, la usa, e al massimo ne scrive una copia di scorta dove gli
// si dice.
//
//   decodxlog_sign keygen [--name main]           crea la chiave e la mette nel portachiavi
//   decodxlog_sign keygen --name recovery --export F
//                                                 crea la chiave di scorta in un file, da
//                                                 portare fuori dal computer
//   decodxlog_sign export --name main F           copia di scorta della chiave del portachiavi
//   decodxlog_sign import --name main F           rimette nel portachiavi una copia di scorta
//   decodxlog_sign public [--name main]           chiave pubblica e id, da scrivere nel codice
//   decodxlog_sign sign --repository R --version V [--name main | --key-file F]
//                       [--out DIR] FILE...       scrive decodxlog-release.json e .sig
//   decodxlog_sign verify --repository R --version V [--dir DIR] FILE...
//                                                 controlla firma e SHA-256 come fara' il programma
#include "core/ReleaseSignature.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

#ifdef DECODXLOG_HAVE_KEYCHAIN
#include <qt6keychain/keychain.h>
#endif

using namespace decolog::core;

namespace {

// Dove sta la chiave nel portachiavi: separata dalle credenziali dei servizi.
const QString kKeychainService = QStringLiteral("DecoDXLog release signing");

QTextStream& out()
{
    static QTextStream s(stdout);
    return s;
}

QTextStream& err()
{
    static QTextStream s(stderr);
    return s;
}

int fail(const QString& message)
{
    err() << "decodxlog_sign: " << message << Qt::endl;
    return 1;
}

// Il segreto come si conserva: base64 dei 64 byte di Monocypher.
bool keychainWrite(const QString& name, const QByteArray& secret, QString* error)
{
#ifdef DECODXLOG_HAVE_KEYCHAIN
    QKeychain::WritePasswordJob job(kKeychainService);
    job.setAutoDelete(false);
    job.setKey(name);
    job.setBinaryData(secret.toBase64());
    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error() != QKeychain::NoError) {
        *error = job.errorString();
        return false;
    }
    return true;
#else
    Q_UNUSED(name);
    Q_UNUSED(secret);
    *error = QStringLiteral("this build has no system keystore");
    return false;
#endif
}

QByteArray keychainRead(const QString& name, QString* error)
{
#ifdef DECODXLOG_HAVE_KEYCHAIN
    QKeychain::ReadPasswordJob job(kKeychainService);
    job.setAutoDelete(false);
    job.setKey(name);
    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error() != QKeychain::NoError) {
        *error = job.errorString();
        return {};
    }
    return QByteArray::fromBase64(job.binaryData());
#else
    Q_UNUSED(name);
    *error = QStringLiteral("this build has no system keystore");
    return {};
#endif
}

QByteArray readKeyFile(const QString& path, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = f.errorString();
        return {};
    }
    // Una riga di commento e poi il base64: la copia di scorta si riconosce.
    QByteArray base64;
    for (const QByteArray& line : f.readAll().split('\n')) {
        const QByteArray t = line.trimmed();
        if (!t.isEmpty() && !t.startsWith('#'))
            base64 = t;
    }
    return QByteArray::fromBase64(base64);
}

bool writeKeyFile(const QString& path, const QString& name, const QByteArray& secret, QString* error)
{
    if (QFileInfo::exists(path)) {
        *error = QStringLiteral("%1 already exists: it is not overwritten").arg(path);
        return false;
    }
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        *error = f.errorString();
        return false;
    }
    const QByteArray pub = secret.mid(32);
    f.write("# DecoDXLog release signing key \"" + name.toUtf8() + "\" - SECRET, keep it offline\n");
    f.write("# public key " + pub.toBase64() + " id " + releasesig::keyId(pub).toLatin1() + "\n");
    f.write(secret.toBase64() + "\n");
    if (!f.commit()) {
        *error = f.errorString();
        return false;
    }
    return true;
}

bool validSecret(const QByteArray& secret)
{
    if (secret.size() != 64)
        return false;
    // Il seme deve dare proprio la chiave pubblica che ci sta attaccata.
    QByteArray pub;
    releasesig::keyPairFromSeed(secret.left(32), &pub);
    return pub == secret.mid(32);
}

void printPublic(const QString& name, const QByteArray& secret)
{
    const QByteArray pub = secret.mid(32);
    out() << "key \"" << name << "\"" << Qt::endl
          << "  public key: " << QString::fromLatin1(pub.toBase64()) << Qt::endl
          << "  id:         " << releasesig::keyId(pub) << Qt::endl;
}

QByteArray loadSecret(const QCommandLineParser& p, QString* error)
{
    const QByteArray secret = p.isSet(QStringLiteral("key-file"))
                                  ? readKeyFile(p.value(QStringLiteral("key-file")), error)
                                  : keychainRead(p.value(QStringLiteral("name")), error);
    if (error->isEmpty() && !validSecret(secret))
        *error = QStringLiteral("the key is damaged");
    return secret;
}

releasesig::SignedFile describe(const QString& path, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("%1: %2").arg(path, f.errorString());
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) {
        *error = QStringLiteral("%1: cannot read it").arg(path);
        return {};
    }
    return {QFileInfo(path).fileName(), f.size(), hash.result()};
}

int verifyFiles(const QString& repository, const QString& version, const QString& dir,
                const QStringList& files)
{
    QFile manifestFile(QDir(dir).filePath(QLatin1String(releasesig::kManifestName)));
    QFile signatureFile(QDir(dir).filePath(QLatin1String(releasesig::kSignatureName)));
    if (!manifestFile.open(QIODevice::ReadOnly) || !signatureFile.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("%1 or %2 is missing in %3")
                        .arg(QLatin1String(releasesig::kManifestName), QLatin1String(releasesig::kSignatureName), dir));
    const releasesig::Manifest m =
        releasesig::verify(manifestFile.readAll(), signatureFile.readAll(), repository, version);
    if (!m.verified())
        return fail(QStringLiteral("the signature is not valid for %1 %2 with the keys in the program").arg(repository, version));
    for (const QString& path : files) {
        QString error;
        const releasesig::SignedFile have = describe(path, &error);
        if (!error.isEmpty())
            return fail(error);
        const releasesig::SignedFile want = m.file(have.name);
        if (want.sha256 != have.sha256 || want.size != have.size)
            return fail(QStringLiteral("%1 is not the signed file").arg(have.name));
        out() << "ok  " << have.name << "  " << QString::fromLatin1(have.sha256.toHex()) << Qt::endl;
    }
    out() << "signed by key " << m.keyId << " for " << repository << " " << m.version << Qt::endl;
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("decodxlog_sign"));

    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Signs DecoDXLog releases (Ed25519)."));
    p.addHelpOption();
    p.addPositionalArgument(QStringLiteral("command"), QStringLiteral("keygen, export, import, public, sign, verify"));
    p.addOption({QStringLiteral("name"), QStringLiteral("Key name in the keystore."), QStringLiteral("name"),
                 QStringLiteral("main")});
    p.addOption({QStringLiteral("export"), QStringLiteral("keygen: write the new key to this file instead of the keystore."),
                 QStringLiteral("file")});
    p.addOption({QStringLiteral("key-file"), QStringLiteral("sign: use this backup file instead of the keystore."),
                 QStringLiteral("file")});
    p.addOption({QStringLiteral("repository"), QStringLiteral("owner/project of the release."), QStringLiteral("repo")});
    p.addOption({QStringLiteral("version"), QStringLiteral("Version of the release."), QStringLiteral("version")});
    p.addOption({QStringLiteral("out"), QStringLiteral("sign: where to write the signed list."), QStringLiteral("dir"),
                 QStringLiteral(".")});
    p.addOption({QStringLiteral("dir"), QStringLiteral("verify: where the signed list is."), QStringLiteral("dir"),
                 QStringLiteral(".")});
    p.addOption({QStringLiteral("force"), QStringLiteral("keygen: replace a key already in the keystore.")});
    p.process(app);

    const QStringList args = p.positionalArguments();
    if (args.isEmpty())
        p.showHelp(1);
    const QString command = args.first();
    const QString name = p.value(QStringLiteral("name"));
    QString error;

    if (command == QLatin1String("keygen")) {
        const bool toFile = p.isSet(QStringLiteral("export"));
        if (!toFile && !p.isSet(QStringLiteral("force"))) {
            QString none;
            if (validSecret(keychainRead(name, &none)))
                return fail(QStringLiteral("the keystore already has a key \"%1\": use --force to replace it").arg(name));
        }
        QByteArray secret, pub;
        if (!releasesig::generateKeyPair(&secret, &pub))
            return fail(QStringLiteral("cannot create the key"));
        const bool ok = toFile ? writeKeyFile(p.value(QStringLiteral("export")), name, secret, &error)
                               : keychainWrite(name, secret, &error);
        if (!ok)
            return fail(error);
        printPublic(name, secret);
        out() << (toFile ? "  written to " + p.value(QStringLiteral("export")) : QStringLiteral("  stored in the keystore"))
              << Qt::endl;
        return 0;
    }
    if (command == QLatin1String("export") || command == QLatin1String("import")) {
        if (args.size() < 2)
            return fail(QStringLiteral("which file?"));
        if (command == QLatin1String("export")) {
            const QByteArray secret = keychainRead(name, &error);
            if (!error.isEmpty() || !validSecret(secret))
                return fail(error.isEmpty() ? QStringLiteral("no valid key \"%1\" in the keystore").arg(name) : error);
            if (!writeKeyFile(args.at(1), name, secret, &error))
                return fail(error);
            out() << "written to " << args.at(1) << Qt::endl;
            return 0;
        }
        const QByteArray secret = readKeyFile(args.at(1), &error);
        if (!error.isEmpty() || !validSecret(secret))
            return fail(error.isEmpty() ? QStringLiteral("the file does not hold a valid key") : error);
        if (!keychainWrite(name, secret, &error))
            return fail(error);
        printPublic(name, secret);
        out() << "  stored in the keystore" << Qt::endl;
        return 0;
    }
    if (command == QLatin1String("public")) {
        const QByteArray secret = loadSecret(p, &error);
        if (!error.isEmpty())
            return fail(error);
        printPublic(name, secret);
        return 0;
    }

    const QString repository = p.value(QStringLiteral("repository")).trimmed();
    const QString version = p.value(QStringLiteral("version")).trimmed();
    const QStringList files = args.mid(1);
    if (repository.isEmpty() || version.isEmpty() || files.isEmpty())
        return fail(QStringLiteral("--repository, --version and at least one file are needed"));

    if (command == QLatin1String("sign")) {
        const QByteArray secret = loadSecret(p, &error);
        if (!error.isEmpty())
            return fail(error);
        QList<releasesig::SignedFile> list;
        for (const QString& path : files) {
            const releasesig::SignedFile f = describe(path, &error);
            if (!error.isEmpty())
                return fail(error);
            list.append(f);
        }
        const QByteArray manifest = releasesig::buildManifest(repository, version, list);
        const QByteArray signature = releasesig::signManifest(manifest, secret);
        const QDir dir(p.value(QStringLiteral("out")));
        for (const auto& [fileName, data] : {std::pair{QLatin1String(releasesig::kManifestName), manifest},
                                             std::pair{QLatin1String(releasesig::kSignatureName), signature}}) {
            QSaveFile f(dir.filePath(fileName));
            if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
                return fail(QStringLiteral("%1: %2").arg(dir.filePath(fileName), f.errorString()));
        }
        // Subito la prova, con le chiavi scritte nel programma: una firma che
        // il programma non riconosce non si pubblica.
        return verifyFiles(repository, version, dir.path(), files);
    }
    if (command == QLatin1String("verify"))
        return verifyFiles(repository, version, p.value(QStringLiteral("dir")), files);

    return fail(QStringLiteral("unknown command %1").arg(command));
}
