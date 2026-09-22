// DecoDXLog — i log della stazione: crearne un altro, ritrovarli, dimenticarli.
//
// Quello che conta qui e' che un log nuovo sia un log vero (si apre e ha le sue
// tabelle) e che l'elenco non perda niente e non raddoppi niente: un contest
// comincia creando un log, e se il log non si apre ci si accorge a gara
// iniziata.
#include "app/LogLibrary.h"
#include "core/LogDatabase.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace decolog;
using namespace decolog::app;

class TestLogLibrary : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        // Senza questo, un log creato senza percorso finirebbe nella cartella
        // dei dati vera di chi fa girare la prova.
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DecodiumTest"));
        QCoreApplication::setApplicationName(QStringLiteral("LogLibraryTest"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
    }

    void init()
    {
        QSettings().clear();
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
    }

    void aNewLogIsARealLog()
    {
        LogLibrary library;
        const QString file = QDir(m_dir.path()).filePath(QStringLiteral("CQ WW SSB 2026.sqlite"));
        QCOMPARE(library.createLog(QStringLiteral("CQ WW SSB 2026"),
                                   QUrl::fromLocalFile(file)), QString());
        QVERIFY(QFile::exists(file));

        // Si apre, e dentro ci sono le tabelle: un file vuoto non e' un log.
        core::LogDatabase db;
        QVERIFY2(db.open(file), qPrintable(db.lastError()));
        QCOMPARE(db.qsoCount(), 0);
        QVERIFY(db.schemaVersion() > 0);

        const QVariantList logs = library.logs();
        QCOMPARE(logs.size(), 1);
        QCOMPARE(logs.first().toMap().value(QStringLiteral("name")).toString(),
                 QStringLiteral("CQ WW SSB 2026"));
    }

    void aNameWithSlashesDoesNotBecomeAFolder()
    {
        LogLibrary library;
        // "IARU R1 / 2026" con le barre finirebbe in una cartella che non c'e'.
        QCOMPARE(library.createLog(QStringLiteral("IARU R1 / 2026")), QString());
        const QString path = library.logs().first().toMap().value(QStringLiteral("path")).toString();
        QVERIFY2(QFile::exists(path), qPrintable(path));
        QVERIFY(!path.contains(QLatin1String("R1 / 2026")));
    }

    void thereIsNoSecondLogWithTheSameFile()
    {
        LogLibrary library;
        const QString file = QDir(m_dir.path()).filePath(QStringLiteral("doppio.sqlite"));
        QCOMPARE(library.createLog(QStringLiteral("doppio"), QUrl::fromLocalFile(file)), QString());
        // Lo stesso file un'altra volta: non si crea sopra a quello che c'e'.
        QVERIFY(!library.createLog(QStringLiteral("doppio"), QUrl::fromLocalFile(file)).isEmpty());
        // E metterlo nell'elenco quando c'e' gia' non lo raddoppia.
        QCOMPARE(library.addExisting(QUrl::fromLocalFile(file)), QString());
        QCOMPARE(library.logs().size(), 1);
    }

    void aFileThatIsNotALogIsRefused()
    {
        LogLibrary library;
        const QString path = QDir(m_dir.path()).filePath(QStringLiteral("niente.txt"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("non sono un log");
        f.close();
        QVERIFY(!library.addExisting(QUrl::fromLocalFile(path)).isEmpty());
        QCOMPARE(library.logs().size(), 0);
    }

    void theOpenOneIsMarkedAndComesFirst()
    {
        LogLibrary library;
        const QString first = QDir(m_dir.path()).filePath(QStringLiteral("uno.sqlite"));
        const QString second = QDir(m_dir.path()).filePath(QStringLiteral("due.sqlite"));
        QCOMPARE(library.createLog(QStringLiteral("uno"), QUrl::fromLocalFile(first)), QString());
        QCOMPARE(library.createLog(QStringLiteral("due"), QUrl::fromLocalFile(second)), QString());

        library.setCurrent(second);
        const QVariantList logs = library.logs();
        QCOMPARE(logs.size(), 2);
        // Il piu' usato di recente sta in cima, ed e' segnato come aperto.
        QCOMPARE(logs.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("due"));
        QCOMPARE(logs.first().toMap().value(QStringLiteral("current")).toBool(), true);
        QCOMPARE(logs.at(1).toMap().value(QStringLiteral("current")).toBool(), false);
        // E di quello che non e' aperto si dice quanti QSO ha dentro.
        QCOMPARE(logs.at(1).toMap().value(QStringLiteral("qsos")).toInt(), 0);
    }

    void aLogOpenedFromTheCommandLineEntersTheList()
    {
        const QString file = QDir(m_dir.path()).filePath(QStringLiteral("dariga.sqlite"));
        {
            core::LogDatabase db;
            QVERIFY(db.open(file));
        }
        LogLibrary library;
        QCOMPARE(library.logs().size(), 0);
        library.setCurrent(file);
        // Chi apre un log una volta lo ritrova, invece di ricercarlo a mano.
        QCOMPARE(library.logs().size(), 1);
        QCOMPARE(library.current(), QDir::toNativeSeparators(QFileInfo(file).canonicalFilePath()));

        // Una libreria nuova lo rilegge dalle impostazioni: l'elenco sopravvive
        // alla chiusura del programma.
        LogLibrary again;
        QCOMPARE(again.logs().size(), 1);
    }

    void forgettingLeavesTheFileWhereItIs()
    {
        LogLibrary library;
        const QString file = QDir(m_dir.path()).filePath(QStringLiteral("scordato.sqlite"));
        QCOMPARE(library.createLog(QStringLiteral("scordato"), QUrl::fromLocalFile(file)), QString());
        library.forget(file);
        QCOMPARE(library.logs().size(), 0);
        // Dimenticare non e' cancellare: il log e' ancora sul disco.
        QVERIFY(QFile::exists(file));
    }

    void aMissingFileIsSaidSoInsteadOfDisappearing()
    {
        LogLibrary library;
        const QString file = QDir(m_dir.path()).filePath(QStringLiteral("sparito.sqlite"));
        QCOMPARE(library.createLog(QStringLiteral("sparito"), QUrl::fromLocalFile(file)), QString());
        QVERIFY(QFile::remove(file));

        const QVariantMap row = library.logs().first().toMap();
        QCOMPARE(row.value(QStringLiteral("missing")).toBool(), true);
        // E aprirlo dice cosa e' successo, invece di riavviare su niente.
        QVERIFY(!library.openLog(file).isEmpty());
    }
};

QTEST_MAIN(TestLogLibrary)
#include "tst_loglibrary.moc"
