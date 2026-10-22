// DecoDXLog — i log della stazione: crearne un altro, ritrovarli, dimenticarli.
//
// Quello che conta qui e' che un log nuovo sia un log vero (si apre e ha le sue
// tabelle) e che l'elenco non perda niente e non raddoppi niente: un contest
// comincia creando un log, e se il log non si apre ci si accorge a gara
// iniziata.
#include "app/LogLibrary.h"
#include "core/LogDatabase.h"

#include <QDate>
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

    void aLogRemembersWhatItIsFor()
    {
        LogLibrary library;
        const QString file = QDir(m_dir.path()).filePath(QStringLiteral("vucc.sqlite"));
        QCOMPARE(library.createLog(QStringLiteral("VUCC 2026"), QUrl::fromLocalFile(file),
                                   QStringLiteral("award"), QStringLiteral("VUCC")), QString());
        // Un tipo che non si conosce diventa un log generico, non un'etichetta a caso.
        const QString other = QDir(m_dir.path()).filePath(QStringLiteral("boh.sqlite"));
        QCOMPARE(library.createLog(QStringLiteral("boh"), QUrl::fromLocalFile(other),
                                   QStringLiteral("whatever")), QString());

        LogLibrary again;   // dalle impostazioni, come al riavvio
        QMap<QString, QVariantMap> byName;
        for (const QVariant& row : again.logs())
            byName.insert(row.toMap().value(QStringLiteral("name")).toString(), row.toMap());
        QCOMPARE(byName.value(QStringLiteral("VUCC 2026")).value(QStringLiteral("kind")).toString(),
                 QStringLiteral("award"));
        QCOMPARE(byName.value(QStringLiteral("VUCC 2026")).value(QStringLiteral("event")).toString(),
                 QStringLiteral("VUCC"));
        QCOMPARE(byName.value(QStringLiteral("boh")).value(QStringLiteral("kind")).toString(),
                 QStringLiteral("general"));
    }

    void anEventWithoutANameNamesTheLogAfterItAndTheYear()
    {
        LogLibrary library;
        QCOMPARE(library.createLog(QString(), QUrl(), QStringLiteral("contest"),
                                   QStringLiteral("CQ WW SSB")), QString());
        const QString name = library.logs().first().toMap().value(QStringLiteral("name")).toString();
        QCOMPARE(name, QStringLiteral("CQ WW SSB %1").arg(QDate::currentDate().year()));
        // Senza nome e senza evento non c'e' niente da chiamare.
        QVERIFY(!library.createLog(QString(), QUrl(), QStringLiteral("contest"), QString()).isEmpty());
    }

    void theLogInUseIsNeverDeleted()
    {
        LogLibrary library;
        const QString file = QDir(m_dir.path()).filePath(QStringLiteral("inuso.sqlite"));
        QCOMPARE(library.createLog(QStringLiteral("inuso"), QUrl::fromLocalFile(file)), QString());
        library.setCurrent(file);
        QVERIFY(!library.deleteLog(file).isEmpty());
        QVERIFY(QFile::exists(file));
        QCOMPARE(library.logs().size(), 1);
    }

    void deletingTakesTheFileAndTheListEntry()
    {
        LogLibrary library;
        const QString keep = QDir(m_dir.path()).filePath(QStringLiteral("resta.sqlite"));
        const QString gone = QDir(m_dir.path()).filePath(QStringLiteral("va.sqlite"));
        QCOMPARE(library.createLog(QStringLiteral("resta"), QUrl::fromLocalFile(keep)), QString());
        QCOMPARE(library.createLog(QStringLiteral("va"), QUrl::fromLocalFile(gone)), QString());
        library.setCurrent(keep);

        const QString error = library.deleteLog(gone);
        if (!error.isEmpty())
            QSKIP(qPrintable(QStringLiteral("no Recycle Bin here: ") + error));
        QVERIFY(!QFile::exists(gone));
        QVERIFY(QFile::exists(keep));
        QCOMPARE(library.logs().size(), 1);
        // E il nome torna libero: si puo' rifare un log con lo stesso nome.
        QCOMPARE(library.createLog(QStringLiteral("va"), QUrl::fromLocalFile(gone)), QString());
    }

    void aLogAlreadyMissingJustLeavesTheList()
    {
        LogLibrary library;
        const QString keep = QDir(m_dir.path()).filePath(QStringLiteral("aperto.sqlite"));
        const QString lost = QDir(m_dir.path()).filePath(QStringLiteral("perso.sqlite"));
        QCOMPARE(library.createLog(QStringLiteral("aperto"), QUrl::fromLocalFile(keep)), QString());
        QCOMPARE(library.createLog(QStringLiteral("perso"), QUrl::fromLocalFile(lost)), QString());
        library.setCurrent(keep);
        QVERIFY(QFile::remove(lost));
        QCOMPARE(library.deleteLog(lost), QString());
        QCOMPARE(library.logs().size(), 1);
        QVERIFY(!library.deleteLog(lost).isEmpty());   // non c'e' piu' nemmeno in elenco
    }
};

QTEST_MAIN(TestLogLibrary)
#include "tst_loglibrary.moc"
