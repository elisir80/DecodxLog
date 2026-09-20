// DecoDXLog — la cassaforte delle credenziali.
//
// Quello che deve valere: con la password giusta si riapre identico, con
// un'altra password non si apre, e se qualcuno tocca il blocco non si apre
// nemmeno con la chiave giusta. Due stazioni con la stessa password non hanno
// la stessa chiave.
#include "core/SecretVault.h"

#include <QTest>

using namespace decolog::core;

class TestVault : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!vault::available())
            QSKIP("compilato senza OpenSSL: le credenziali restano sul computer");
    }

    void whatGoesInComesBackOut()
    {
        const QByteArray key = vault::deriveKey(QStringLiteral("una password lunga"),
                                                QStringLiteral("IU8LMC"));
        QCOMPARE(key.size(), 32);

        const QByteArray plain = QByteArrayLiteral("{\"qrz\":{\"account\":\"IU8LMC\",\"secret\":\"segreto\"}}");
        const QString sealed = vault::seal(key, plain);
        QVERIFY(!sealed.isEmpty());
        // Sul server non si deve leggere niente in chiaro.
        QVERIFY(!sealed.contains(QLatin1String("segreto")));
        QVERIFY(!sealed.contains(QLatin1String("IU8LMC")));

        const auto back = vault::unseal(key, sealed);
        QVERIFY(back.has_value());
        QCOMPARE(*back, plain);
    }

    void thesameSecretLooksDifferentEveryTime()
    {
        const QByteArray key = vault::deriveKey(QStringLiteral("una password lunga"),
                                                QStringLiteral("IU8LMC"));
        const QByteArray plain = QByteArrayLiteral("sempre lo stesso");
        // Ogni chiusura ha il suo nonce: da fuori non si vede nemmeno che il
        // contenuto non e' cambiato.
        QVERIFY(vault::seal(key, plain) != vault::seal(key, plain));
    }

    void anotherPasswordDoesNotOpenIt()
    {
        const QByteArray mine = vault::deriveKey(QStringLiteral("una password lunga"),
                                                 QStringLiteral("IU8LMC"));
        const QByteArray other = vault::deriveKey(QStringLiteral("un'altra password"),
                                                  QStringLiteral("IU8LMC"));
        const QString sealed = vault::seal(mine, QByteArrayLiteral("roba mia"));
        QVERIFY(!vault::unseal(other, sealed).has_value());
    }

    void twoStationsWithTheSamePasswordHaveDifferentKeys()
    {
        const QByteArray mine = vault::deriveKey(QStringLiteral("una password lunga"),
                                                 QStringLiteral("IU8LMC"));
        const QByteArray yours = vault::deriveKey(QStringLiteral("una password lunga"),
                                                  QStringLiteral("DL9ZZT"));
        QVERIFY(mine != yours);
        // E il nominativo si scrive come viene: iu8lmc e IU8LMC sono la stessa
        // stazione.
        QCOMPARE(vault::deriveKey(QStringLiteral("una password lunga"), QStringLiteral(" iu8lmc ")), mine);
    }

    void aTouchedBlockDoesNotOpen()
    {
        const QByteArray key = vault::deriveKey(QStringLiteral("una password lunga"),
                                                QStringLiteral("IU8LMC"));
        QString sealed = vault::seal(key, QByteArrayLiteral("roba mia, non toccare"));
        QByteArray blob = QByteArray::fromBase64(sealed.toLatin1());
        blob[blob.size() - 1] = static_cast<char>(blob.at(blob.size() - 1) ^ 0x01);
        QVERIFY(!vault::unseal(key, QString::fromLatin1(blob.toBase64())).has_value());
    }

    void nothingToOpenIsNotACrash()
    {
        const QByteArray key = vault::deriveKey(QStringLiteral("una password lunga"),
                                                QStringLiteral("IU8LMC"));
        QVERIFY(!vault::unseal(key, QString()).has_value());
        QVERIFY(!vault::unseal(key, QStringLiteral("non e' nemmeno base64 valido!!")).has_value());
        QVERIFY(!vault::unseal(QByteArray(), QStringLiteral("AAAA")).has_value());
        QVERIFY(vault::deriveKey(QString(), QStringLiteral("IU8LMC")).isEmpty());
        QVERIFY(vault::deriveKey(QStringLiteral("password"), QString()).isEmpty());
    }
};

QTEST_MAIN(TestVault)
#include "tst_vault.moc"
