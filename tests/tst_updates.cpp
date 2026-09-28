// C'e' una versione nuova? Due cose da non sbagliare: confrontare i numeri
// (1.10 viene dopo 1.9, non prima) e leggere la risposta di GitHub senza
// proporre a chi opera una bozza o un pre-rilascio.
//
// E una terza: installare solo quello che chi pubblica ha firmato.
//
// Il JSON qui sotto si scrive con gli apici singoli e si scambiano alla fine:
// moc non digerisce le stringhe grezze, e un test che non si compila non serve.
#include "core/ReleaseSignature.h"
#include "core/Updates.h"

#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

using namespace decolog::core;

namespace {

QByteArray json(const char* text)
{
    return QByteArray(text).replace('\'', '"');
}

QByteArray hex(const char* text)
{
    return QByteArray::fromHex(QByteArray(text));
}

// Un server HTTP da due soldi: risponde con il contenuto del percorso chiesto,
// o con `body` per tutti gli altri.
class TinyHttp : public QTcpServer {
public:
    explicit TinyHttp(QByteArray body)
        : m_body(std::move(body))
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* s = nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, s, [this, s] {
                    const QByteArray request = s->readAll();
                    if (!request.contains("\r\n\r\n"))
                        return;
                    const QByteArray path = request.split(' ').value(1);
                    const QByteArray body = m_routes.value(path, m_body);
                    s->write("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: "
                             + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                    s->disconnectFromHost();
                });
            }
        });
        listen(QHostAddress::LocalHost);
    }
    QUrl url(const char* path = "/pkg") const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(QLatin1String(path)));
    }
    void route(const QByteArray& path, const QByteArray& body) { m_routes.insert(path, body); }

private:
    QByteArray m_body;
    QHash<QByteArray, QByteArray> m_routes;
};

} // namespace

class TestUpdates : public QObject {
    Q_OBJECT

private slots:
    void theSameVersionIsNotNew()
    {
        QCOMPARE(updates::compareVersions(QStringLiteral("1.1.0"), QStringLiteral("1.1.0")), 0);
        QCOMPARE(updates::compareVersions(QStringLiteral("v1.1.0"), QStringLiteral("1.1.0")), 0);
        QCOMPARE(updates::compareVersions(QStringLiteral("1.1"), QStringLiteral("1.1.0")), 0);
    }

    void numbersAreComparedAsNumbers()
    {
        // Il caso che rompe i confronti fatti da lettere.
        QCOMPARE(updates::compareVersions(QStringLiteral("1.10.0"), QStringLiteral("1.9.0")), 1);
        QCOMPARE(updates::compareVersions(QStringLiteral("1.9.0"), QStringLiteral("1.10.0")), -1);
        QCOMPARE(updates::compareVersions(QStringLiteral("2.0.0"), QStringLiteral("1.99.99")), 1);
        QCOMPARE(updates::compareVersions(QStringLiteral("1.1.1"), QStringLiteral("1.1.0")), 1);
    }

    void aPreReleaseCountsAsItsNumber()
    {
        QCOMPARE(updates::compareVersions(QStringLiteral("1.2.0-beta1"), QStringLiteral("1.2.0")), 0);
    }

    void theAnswerFromGitHubIsRead()
    {
        const ReleaseInfo info = updates::parseRelease(json(
            "{'tag_name': 'v1.2.0',"
            " 'html_url': 'https://github.com/iu8lmc/DecoDXLog/releases/tag/v1.2.0',"
            " 'body': 'Cose nuove', 'draft': false, 'prerelease': false,"
            " 'assets': ["
            "   {'name': 'DecoDXLog-1.2.0-win64.zip', 'size': 111111111,"
            "    'browser_download_url': 'https://example.invalid/DecoDXLog-1.2.0-win64.zip'},"
            "   {'name': 'DecoDXLog-1.2.0-windows-x64-setup.exe', 'size': 99000000,"
            "    'browser_download_url': 'https://example.invalid/DecoDXLog-1.2.0-setup.exe'}]}"),
            {QStringLiteral("windows"), QStringLiteral("x86_64")});
        QVERIFY(info.valid);
        QCOMPARE(info.version, QStringLiteral("1.2.0"));   // la v davanti si toglie
        QCOMPARE(info.notes, QStringLiteral("Cose nuove"));
        QCOMPARE(info.package.toString(), QStringLiteral("https://example.invalid/DecoDXLog-1.2.0-setup.exe"));
        QCOMPARE(info.packageName, QStringLiteral("DecoDXLog-1.2.0-windows-x64-setup.exe"));
        QCOMPARE(info.packageBytes, 99000000LL);
    }

    void aDraftOrAPreReleaseIsNotProposed()
    {
        const UpdateTarget mac{QStringLiteral("macos"), QStringLiteral("aarch64")};
        QVERIFY(!updates::parseRelease(json(
            "{'tag_name': 'v9.9.9', 'draft': true, 'assets': ["
            "{'name': 'DecoDXLog-macos-arm64.dmg', 'browser_download_url': 'https://example.invalid/a'}]}"), mac).valid);
        QVERIFY(!updates::parseRelease(json(
            "{'tag_name': 'v9.9.9', 'prerelease': true, 'assets': ["
            "{'name': 'DecoDXLog-macos-arm64.dmg', 'browser_download_url': 'https://example.invalid/a'}]}"), mac).valid);
    }

    void rubbishIsNotARelease()
    {
        QVERIFY(!updates::parseRelease(QByteArray("<html>404</html>")).valid);
        QVERIFY(!updates::parseRelease(json("{}")).valid);
        QVERIFY(!updates::parseRelease(QByteArray()).valid);
    }

    void onlyAPackageForThisComputerCounts()
    {
        const QByteArray release = json(
            "{'tag_name': 'v1.0.0', 'html_url': 'https://example.invalid/r', 'assets': ["
            "{'name': 'DecoDXLog-1.0.0-windows-x64-portable.zip', 'size': 1,"
            " 'browser_download_url': 'https://example.invalid/z.zip'},"
            "{'name': 'DecoDXLog-1.0.0-windows-x64-setup.exe', 'size': 2,"
            " 'browser_download_url': 'https://example.invalid/z.exe'}]}");
        QVERIFY(!updates::parseRelease(
                     release, {QStringLiteral("macos"), QStringLiteral("aarch64")}).valid);
        const ReleaseInfo windows = updates::parseRelease(
            release, {QStringLiteral("windows"), QStringLiteral("x86_64")});
        QVERIFY(windows.valid);
        QCOMPARE(windows.packageName, QStringLiteral("DecoDXLog-1.0.0-windows-x64-setup.exe"));
    }

    void architectureSpecificAssetsBeatGenericOrWrongOnes()
    {
        const QByteArray release = json(
            "{'tag_name': 'v1.3.0', 'assets': ["
            "{'name': 'DecoDXLog-1.3.0-macos-x86_64.dmg', 'size': 11, 'browser_download_url': 'https://example.invalid/intel'},"
            "{'name': 'DecoDXLog-1.3.0-macos-arm64.dmg', 'size': 12, 'browser_download_url': 'https://example.invalid/arm'},"
            "{'name': 'DecoDXLog-1.3.0-linux-x86_64.AppImage', 'size': 13, 'browser_download_url': 'https://example.invalid/linux-intel'},"
            "{'name': 'DecoDXLog-1.3.0-linux-aarch64.AppImage', 'size': 14, 'browser_download_url': 'https://example.invalid/linux-arm'}]}");

        QCOMPARE(updates::parseRelease(release, {QStringLiteral("macos"), QStringLiteral("aarch64")}).packageName,
                 QStringLiteral("DecoDXLog-1.3.0-macos-arm64.dmg"));
        QCOMPARE(updates::parseRelease(release, {QStringLiteral("macos"), QStringLiteral("x86_64")}).packageName,
                 QStringLiteral("DecoDXLog-1.3.0-macos-x86_64.dmg"));
        QCOMPARE(updates::parseRelease(release, {QStringLiteral("linux"), QStringLiteral("aarch64")}).packageName,
                 QStringLiteral("DecoDXLog-1.3.0-linux-aarch64.AppImage"));
        QCOMPARE(updates::parseRelease(release, {QStringLiteral("linux"), QStringLiteral("x86_64")}).packageName,
                 QStringLiteral("DecoDXLog-1.3.0-linux-x86_64.AppImage"));
    }

    void releaseListSkipsNewerOtherPlatformReleases()
    {
        const QByteArray releases = json(
            "["
            "{'tag_name': 'v2.0.0', 'assets': ["
            " {'name': 'DecoDXLog-2.0.0-windows-x64-setup.exe', 'browser_download_url': 'https://example.invalid/win'}]},"
            "{'tag_name': 'v1.9.0', 'assets': ["
            " {'name': 'DecoDXLog-1.9.0-macos-arm64.dmg', 'browser_download_url': 'https://example.invalid/mac'}]},"
            "{'tag_name': 'v1.8.0', 'assets': ["
            " {'name': 'DecoDXLog-1.8.0-macos-arm64.dmg', 'browser_download_url': 'https://example.invalid/mac-old'}]}"
            "]");
        const ReleaseInfo mac = updates::parseReleases(
            releases, {QStringLiteral("macos"), QStringLiteral("aarch64")});
        QVERIFY(mac.valid);
        QCOMPARE(mac.version, QStringLiteral("1.9.0"));
        QCOMPARE(mac.packageName, QStringLiteral("DecoDXLog-1.9.0-macos-arm64.dmg"));
    }

    void rfc8032Vectors()
    {
        // I vettori dell'RFC 8032 (7.1, test 1-3): se Monocypher li sbaglia, la
        // firma non vale niente.
        struct Vector { const char* seed; const char* pub; const char* msg; const char* sig; };
        const Vector vectors[] = {
            {"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
             "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
             "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"},
            {"4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
             "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
             "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"},
            {"c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
             "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
             "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a"},
        };
        for (const Vector& v : vectors) {
            QByteArray pub;
            const QByteArray secret = releasesig::keyPairFromSeed(hex(v.seed), &pub);
            QCOMPARE(pub, hex(v.pub));
            QCOMPARE(releasesig::ed25519Sign(hex(v.msg), secret), hex(v.sig));
            QVERIFY(releasesig::ed25519Check(hex(v.sig), hex(v.pub), hex(v.msg)));
            QByteArray bad = hex(v.sig);
            bad[5] = char(bad.at(5) ^ 0x01);
            QVERIFY(!releasesig::ed25519Check(bad, hex(v.pub), hex(v.msg)));
        }
    }

    void theProgramKnowsTheKeysOfItsPublisher()
    {
        // Senza una chiave per iu8lmc/DecoDXLog nessun aggiornamento si
        // installerebbe da solo.
        const QList<releasesig::TrustedKey> keys = releasesig::trustedKeys();
        QVERIFY(!keys.isEmpty());
        for (const releasesig::TrustedKey& k : keys) {
            QCOMPARE(k.publicKey.size(), 32);
            QCOMPARE(k.repository, QStringLiteral("iu8lmc/DecoDXLog"));
        }
    }

    void onlyASignedListIsTrusted()
    {
        QByteArray secret, pub, otherSecret, otherPub;
        QVERIFY(releasesig::generateKeyPair(&secret, &pub));
        QVERIFY(releasesig::generateKeyPair(&otherSecret, &otherPub));
        QVERIFY(pub != otherPub);
        const QString repo = QStringLiteral("iu8lmc/DecoDXLog");
        const QList<releasesig::TrustedKey> keys{{repo, pub}, {QStringLiteral("elisir80/DecodxLog"), otherPub}};
        const releasesig::SignedFile setup{QStringLiteral("DecoDXLog-1.2.0-setup.exe"), 1234,
                                           QCryptographicHash::hash("setup", QCryptographicHash::Sha256)};
        const QByteArray manifest = releasesig::buildManifest(repo, QStringLiteral("v1.2.0"), {setup});
        const QByteArray signature = releasesig::signManifest(manifest, secret);

        // Firmato con la chiave giusta: si legge l'elenco.
        releasesig::Manifest m = releasesig::verify(manifest, signature, repo, QStringLiteral("1.2.0"), keys);
        QCOMPARE(m.state, releasesig::Manifest::State::Verified);
        QCOMPARE(m.keyId, releasesig::keyId(pub));
        QCOMPARE(m.file(setup.name).sha256, setup.sha256);
        QCOMPARE(m.file(setup.name).size, qint64(1234));
        QVERIFY(m.file(QStringLiteral("altro.exe")).sha256.isEmpty());

        // Un byte cambiato nell'elenco: la firma non torna.
        QByteArray tampered = manifest;
        tampered.replace("1234", "1235");
        QCOMPARE(releasesig::verify(tampered, signature, repo, QStringLiteral("1.2.0"), keys).state,
                 releasesig::Manifest::State::Invalid);
        // L'elenco di un'altra versione, riattaccato a questa release.
        QCOMPARE(releasesig::verify(manifest, signature, repo, QStringLiteral("1.3.0"), keys).state,
                 releasesig::Manifest::State::Invalid);
        // Firmato con la chiave di un altro repository: per questo non vale.
        const QByteArray foreign = releasesig::signManifest(manifest, otherSecret);
        QCOMPARE(releasesig::verify(manifest, foreign, repo, QStringLiteral("1.2.0"), keys).state,
                 releasesig::Manifest::State::Untrusted);
        // E la firma giusta non vale per le release dell'altro repository.
        QCOMPARE(releasesig::verify(manifest, signature, QStringLiteral("elisir80/DecodxLog"),
                                    QStringLiteral("1.2.0"), keys).state,
                 releasesig::Manifest::State::Untrusted);
        // Niente elenco o niente firma; una firma che non e' una firma.
        QCOMPARE(releasesig::verify({}, signature, repo, QStringLiteral("1.2.0"), keys).state,
                 releasesig::Manifest::State::Missing);
        QCOMPARE(releasesig::verify(manifest, "rubbish", repo, QStringLiteral("1.2.0"), keys).state,
                 releasesig::Manifest::State::Invalid);
        // Con le sole chiavi scritte nel programma, una chiave nuova non passa.
        QCOMPARE(releasesig::verify(manifest, signature, repo, QStringLiteral("1.2.0")).state,
                 releasesig::Manifest::State::Untrusted);
    }

    void thePackageMustBeTheSignedOne()
    {
        ReleaseInfo info = updates::parseRelease(json(
            "{'tag_name': 'v1.2.0', 'assets': ["
            "{'name': 'DecoDXLog-1.2.0-setup.exe', 'size': 1234, 'browser_download_url': 'https://example.invalid/setup'},"
            "{'name': 'decodxlog-release.json', 'size': 300, 'browser_download_url': 'https://example.invalid/m'},"
            "{'name': 'decodxlog-release.json.sig', 'size': 120, 'browser_download_url': 'https://example.invalid/s'}]}"),
            {QStringLiteral("windows"), QStringLiteral("x86_64")});
        QVERIFY(info.valid);
        QCOMPARE(info.packageName, QStringLiteral("DecoDXLog-1.2.0-setup.exe"));
        QCOMPARE(info.manifest, QUrl(QStringLiteral("https://example.invalid/m")));
        QCOMPARE(info.signature, QUrl(QStringLiteral("https://example.invalid/s")));
        QVERIFY(!info.verified());

        releasesig::Manifest m;
        m.state = releasesig::Manifest::State::Verified;
        m.files = {{QStringLiteral("DecoDXLog-1.2.0-setup.exe"), 1234, QByteArray(32, 'x')}};
        ReleaseInfo ok = info;
        updates::applyManifest(ok, m);
        QVERIFY(ok.verified());
        QCOMPARE(ok.packageSha256, QByteArray(32, 'x'));

        // Una dimensione diversa da quella firmata, o un pacchetto che
        // nell'elenco non c'e': non si installa.
        ReleaseInfo bigger = info;
        bigger.packageBytes = 9999;
        updates::applyManifest(bigger, m);
        QCOMPARE(bigger.signatureState, releasesig::Manifest::State::Invalid);
        QVERIFY(!bigger.verified());
        ReleaseInfo absent = info;
        absent.packageName = QStringLiteral("DecoDXLog-1.2.0-other-setup.exe");
        updates::applyManifest(absent, m);
        QVERIFY(!absent.verified());
    }

    void aDownloadThatIsNotTheSignedFileIsDiscarded()
    {
        const QByteArray body("the real installer");
        TinyHttp server(body);
        QVERIFY(server.isListening());
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("setup.exe"));

        // Lo SHA-256 giusto: il file resta.
        {
            UpdateFetcher fetcher;
            QSignalSpy done(&fetcher, &UpdateFetcher::downloaded);
            QSignalSpy failed(&fetcher, &UpdateFetcher::downloadFailed);
            fetcher.download(server.url(), path, body.size(), QCryptographicHash::hash(body, QCryptographicHash::Sha256));
            QTRY_VERIFY(done.size() + failed.size() > 0);
            QCOMPARE(done.size(), 1);
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadOnly));
            QCOMPARE(f.readAll(), body);
        }
        QVERIFY(QFile::remove(path));
        // Uno SHA-256 diverso: non resta niente, e lo si dice.
        {
            UpdateFetcher fetcher;
            QSignalSpy done(&fetcher, &UpdateFetcher::downloaded);
            QSignalSpy failed(&fetcher, &UpdateFetcher::downloadFailed);
            fetcher.download(server.url(), path, body.size(), QCryptographicHash::hash("other", QCryptographicHash::Sha256));
            QTRY_VERIFY(done.size() + failed.size() > 0);
            QCOMPARE(failed.size(), 1);
            QVERIFY(!QFile::exists(path));
        }
        // Senza SHA-256 non si comincia nemmeno.
        {
            UpdateFetcher fetcher;
            QSignalSpy failed(&fetcher, &UpdateFetcher::downloadFailed);
            fetcher.download(server.url(), path, body.size(), QByteArray());
            QCOMPARE(failed.size(), 1);
            QVERIFY(!QFile::exists(path));
        }
    }

    void theCheckOffersOnlySignedPackagesForInstall()
    {
        // Tutta la strada: GitHub dice che c'e' la 2.0.0, l'elenco firmato e la
        // firma si scaricano e si controllano, e il pacchetto risulta firmato.
        const QByteArray package("installer 2.0.0");
        TinyHttp server(package);
        // Il pacchetto del sistema dove gira la prova: su Linux un AppImage, su
        // macOS un dmg. Con il solo setup.exe fuori da Windows non c'era niente
        // da proporre, e la prova falliva.
        const QString platform = updates::currentTarget().platform;
        const QString packageName = platform == QLatin1String("windows") ? QStringLiteral("DecoDXLog-2.0.0-setup.exe")
                                    : platform == QLatin1String("macos") ? QStringLiteral("DecoDXLog-2.0.0.dmg")
                                                                         : QStringLiteral("DecoDXLog-2.0.0.AppImage");
        QByteArray secret, pub;
        QVERIFY(releasesig::generateKeyPair(&secret, &pub));
        const QString repo = QStringLiteral("test endpoint");
        const QByteArray manifest = releasesig::buildManifest(
            repo, QStringLiteral("2.0.0"),
            {{packageName, package.size(), QCryptographicHash::hash(package, QCryptographicHash::Sha256)}});
        const auto release = [&server, &packageName](bool withSignature) {
            QByteArray assets = "{'name': '" + packageName.toLatin1() + "', 'size': 15, 'browser_download_url': '"
                                + server.url("/pkg").toString().toLatin1() + "'}";
            if (withSignature)
                assets += ",{'name': 'decodxlog-release.json', 'browser_download_url': '"
                          + server.url("/m").toString().toLatin1() + "'},"
                          "{'name': 'decodxlog-release.json.sig', 'browser_download_url': '"
                          + server.url("/s").toString().toLatin1() + "'}";
            return json(("[{'tag_name': 'v2.0.0', 'assets': [" + assets + "]}]").constData());
        };
        server.route("/m", manifest);
        server.route("/s", releasesig::signManifest(manifest, secret));

        auto check = [&server](const QList<releasesig::TrustedKey>& keys) {
            UpdateFetcher fetcher;
            fetcher.setUrl(server.url("/api"));
            fetcher.setTrustedKeys(keys);
            QSignalSpy found(&fetcher, &UpdateFetcher::finished);
            QSignalSpy failed(&fetcher, &UpdateFetcher::failed);
            fetcher.fetch(QStringLiteral("1.0.0"));
            QElapsedTimer t;
            t.start();
            while (found.isEmpty() && failed.isEmpty() && t.elapsed() < 5000)
                QTest::qWait(20);
            return found.isEmpty() ? ReleaseInfo{} : found.first().first().value<ReleaseInfo>();
        };

        // Con la chiave giusta: firmato, con lo SHA-256 del pacchetto.
        server.route("/api", release(true));
        ReleaseInfo info = check({{repo, pub}});
        QVERIFY(info.valid);
        QVERIFY(info.verified());
        QCOMPARE(info.packageSha256, QCryptographicHash::hash(package, QCryptographicHash::Sha256));

        // Con una chiave che non e' quella: si propone, ma non si installa.
        QByteArray otherSecret, otherPub;
        QVERIFY(releasesig::generateKeyPair(&otherSecret, &otherPub));
        info = check({{repo, otherPub}});
        QVERIFY(info.valid);
        QVERIFY(!info.verified());
        QCOMPARE(info.signatureState, releasesig::Manifest::State::Untrusted);

        // Senza elenco firmato: idem.
        server.route("/api", release(false));
        info = check({{repo, pub}});
        QVERIFY(info.valid);
        QCOMPARE(info.signatureState, releasesig::Manifest::State::Missing);
    }

    void newerReleasesComeNewestFirst()
    {
        ReleaseInfo fork = updates::parseRelease(json(
            "{'tag_name': 'v2.0.0', 'assets': ["
            "{'name': 'DecoDXLog-2.0.0-macos-arm64.dmg', 'browser_download_url': 'https://example.invalid/fork'}]}"),
            {QStringLiteral("macos"), QStringLiteral("aarch64")});
        fork.repository = QStringLiteral("elisir80/DecodxLog");
        ReleaseInfo upstream = updates::parseRelease(json(
            "{'tag_name': 'v3.0.0', 'assets': ["
            "{'name': 'DecoDXLog-3.0.0-macos-arm64.dmg', 'browser_download_url': 'https://example.invalid/upstream'}]}"),
            {QStringLiteral("macos"), QStringLiteral("aarch64")});
        upstream.repository = QStringLiteral("iu8lmc/DecoDXLog");

        // Si provano dalla piu' nuova: la prima firmata vince, e se nessuna lo
        // e' si propone la piu' nuova, da scaricare a mano.
        const QList<ReleaseInfo> list = updates::newerReleases({fork, upstream}, QStringLiteral("1.0.0"));
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.at(0).version, QStringLiteral("3.0.0"));
        QCOMPARE(list.at(1).repository, QStringLiteral("elisir80/DecodxLog"));
        // Una versione non piu' nuova di quella che gira non c'e'.
        QCOMPARE(updates::newerReleases({fork, upstream}, QStringLiteral("2.5.0")).size(), 1);
        QVERIFY(updates::newerReleases({fork, upstream}, QStringLiteral("3.0.0")).isEmpty());
        // A pari versione, prima la sorgente che viene prima.
        ReleaseInfo same = upstream;
        same.version = QStringLiteral("2.0.0");
        QCOMPARE(updates::newerReleases({fork, same}, QStringLiteral("1.0.0")).first().repository,
                 QStringLiteral("elisir80/DecodxLog"));
    }
};

QTEST_MAIN(TestUpdates)
#include "tst_updates.moc"
