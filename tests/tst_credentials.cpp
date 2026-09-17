// Credenziali: il segreto va nel portachiavi e torna uguale, nel file delle
// impostazioni resta solo il nome utente.
//
// Usa il portachiavi vero dell'utente, sotto un nome di servizio proprio del
// test, e alla fine cancella quello che ha scritto. Dove il portachiavi non c'e'
// (CI Linux senza Secret Service) il test si salta, non fallisce.
#include "core/CredentialStore.h"

#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

using namespace decolog::core;

class TestCredentials : public QObject {
    Q_OBJECT

    CredentialStore* m_store{nullptr};
    const QString m_secret = QStringLiteral("s3cr3t-äöü-%1").arg(QCoreApplication::applicationPid());

    bool waitFinished(QSignalSpy& spy) { return spy.count() > 0 || spy.wait(10000); }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("Decodium"));
        QCoreApplication::setApplicationName(QStringLiteral("DecoLog-test"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings().clear();
        m_store = new CredentialStore(QStringLiteral("DecoLog-test-%1").arg(QCoreApplication::applicationPid()), this);
    }

    void cleanupTestCase()
    {
        if (m_store && m_store->available()) {
            QSignalSpy spy(m_store, &CredentialStore::finished);
            m_store->remove(QStringLiteral("qrz"));
            waitFinished(spy);
        }
        const QString file = QSettings().fileName();
        QSettings().clear();
        QFile::remove(file);
    }

    void knownServices()
    {
        const auto services = CredentialStore::knownServices();
        QStringList ids;
        for (const auto& s : services)
            ids << s.id;
        for (const char* id : {"cloud", "qrz", "lotw", "clublog", "eqsl"})
            QVERIFY2(ids.contains(QLatin1String(id)), id);
    }

    void saveReadRemove()
    {
        if (!m_store->available())
            QSKIP("built without qtkeychain");

        QSignalSpy spy(m_store, &CredentialStore::finished);
        m_store->save(QStringLiteral("qrz"), QStringLiteral(" IU8LMC "), m_secret);
        QVERIFY(waitFinished(spy));
        if (!spy.first().at(1).toBool())
            QSKIP(qPrintable("no usable system keystore: " + spy.first().at(2).toString()));

        QCOMPARE(m_store->account(QStringLiteral("qrz")), QString("IU8LMC"));
        QVERIFY(m_store->hasSecret(QStringLiteral("qrz")));

        // Nel file delle impostazioni il segreto non c'e'.
        QSettings().sync();
        QFile ini(QSettings().fileName());
        QVERIFY(ini.open(QIODevice::ReadOnly));
        QVERIFY(!QString::fromUtf8(ini.readAll()).contains(QStringLiteral("s3cr3t")));

        QString read, error;
        bool done = false;
        m_store->readSecret(QStringLiteral("qrz"), [&](const QString& s, const QString& e) {
            read = s;
            error = e;
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(read, m_secret);

        // Un segreto vuoto cambia l'account e lascia il segreto.
        spy.clear();
        m_store->save(QStringLiteral("qrz"), QStringLiteral("IU8LMC/P"), QString());
        QVERIFY(waitFinished(spy));
        QCOMPARE(m_store->account(QStringLiteral("qrz")), QString("IU8LMC/P"));
        QVERIFY(m_store->hasSecret(QStringLiteral("qrz")));

        spy.clear();
        m_store->remove(QStringLiteral("qrz"));
        QVERIFY(waitFinished(spy));
        QVERIFY(spy.first().at(1).toBool());
        QVERIFY(!m_store->hasSecret(QStringLiteral("qrz")));
        QVERIFY(m_store->account(QStringLiteral("qrz")).isEmpty());

        // Dopo la rimozione verify() non lo dichiara piu'.
        spy.clear();
        m_store->verify(QStringLiteral("qrz"));
        QVERIFY(waitFinished(spy));
        QVERIFY(!spy.first().at(1).toBool());
        QVERIFY(!m_store->hasSecret(QStringLiteral("qrz")));
    }
};

QTEST_GUILESS_MAIN(TestCredentials)
#include "tst_credentials.moc"
