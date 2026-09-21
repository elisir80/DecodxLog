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
            "   {'name': 'DecoDXLog-1.2.0-setup.exe', 'size': 99000000,"
            "    'browser_download_url': 'https://example.invalid/DecoDXLog-1.2.0-setup.exe'}]}"));
        QVERIFY(info.valid);
        QCOMPARE(info.version, QStringLiteral("1.2.0"));   // la v davanti si toglie
        QCOMPARE(info.notes, QStringLiteral("Cose nuove"));
        QCOMPARE(info.installer.toString(), QStringLiteral("https://example.invalid/DecoDXLog-1.2.0-setup.exe"));
        QCOMPARE(info.archive.toString(), QStringLiteral("https://example.invalid/DecoDXLog-1.2.0-win64.zip"));
        QCOMPARE(info.installerBytes, 99000000LL);
    }

    void aDraftOrAPreReleaseIsNotProposed()
    {
        QVERIFY(!updates::parseRelease(json("{'tag_name': 'v9.9.9', 'draft': true, 'assets': []}")).valid);
        QVERIFY(!updates::parseRelease(json("{'tag_name': 'v9.9.9', 'prerelease': true, 'assets': []}")).valid);
    }

    void rubbishIsNotARelease()
    {
        QVERIFY(!updates::parseRelease(QByteArray("<html>404</html>")).valid);
        QVERIFY(!updates::parseRelease(json("{}")).valid);
        QVERIFY(!updates::parseRelease(QByteArray()).valid);
    }

    void aReleaseWithoutInstallerStillCounts()
    {
        // Le versioni vecchie avevano solo lo zip: si dice lo stesso che c'e'
        // qualcosa di nuovo, e si apre la pagina.
        const ReleaseInfo info = updates::parseRelease(json(
            "{'tag_name': 'v1.0.0', 'html_url': 'https://example.invalid/r',"
            " 'assets': [{'name': 'DecoDXLog-1.0.0-win64.zip', 'size': 1,"
            "             'browser_download_url': 'https://example.invalid/z.zip'}]}"));
        QVERIFY(info.valid);
        QVERIFY(info.installer.isEmpty());
        QVERIFY(!info.archive.isEmpty());
    }
};

QTEST_MAIN(TestUpdates)
#include "tst_updates.moc"
