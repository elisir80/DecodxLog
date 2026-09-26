// C'e' una versione nuova? Due cose da non sbagliare: confrontare i numeri
// (1.10 viene dopo 1.9, non prima) e leggere la risposta di GitHub senza
// proporre a chi opera una bozza o un pre-rilascio.
//
// Il JSON qui sotto si scrive con gli apici singoli e si scambiano alla fine:
// moc non digerisce le stringhe grezze, e un test che non si compila non serve.
#include "core/Updates.h"

#include <QTest>

using namespace decolog::core;

namespace {

QByteArray json(const char* text)
{
    return QByteArray(text).replace('\'', '"');
}

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

    void forkIsPreferredAndUpstreamIsFallback()
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

        const ReleaseInfo preferred = updates::selectPreferredUpdate(
            fork, upstream, QStringLiteral("1.0.0"));
        QCOMPARE(preferred.version, QStringLiteral("2.0.0"));
        QCOMPARE(preferred.repository, QStringLiteral("elisir80/DecodxLog"));

        const ReleaseInfo fallback = updates::selectPreferredUpdate(
            {}, upstream, QStringLiteral("1.0.0"));
        QCOMPARE(fallback.version, QStringLiteral("3.0.0"));
        QCOMPARE(fallback.repository, QStringLiteral("iu8lmc/DecoDXLog"));
    }
};

QTEST_MAIN(TestUpdates)
#include "tst_updates.moc"
