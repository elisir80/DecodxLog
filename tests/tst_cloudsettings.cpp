// DecoDXLog — le impostazioni che viaggiano nel Cloud.
//
// Il patto e' semplice: va tutto, meno le poche cose che parlano solo di questa
// macchina. Qui si prova che ci vada davvero tutto — anche un filtro salvato,
// che testo non e' — che il profilo attivo si ritrovi per uuid e non per numero
// di riga, e che quello che deve restare a casa resti a casa anche se il server
// prova a mandarlo.
#include "core/CloudSettings.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace decolog::core;

namespace {

// Un file di impostazioni per ogni prova: niente tocca quelle di chi lavora.
QSettings* fresh(QTemporaryDir& dir, const QString& name)
{
    return new QSettings(dir.filePath(name), QSettings::IniFormat);
}

} // namespace

class TestCloudSettings : public QObject {
    Q_OBJECT

private slots:
    void scopePolicyIsVersionedAndClassified()
    {
        QCOMPARE(cloudsettings::kScopeVersion, 1);
        QCOMPARE(cloudsettings::scopeOf(QStringLiteral("theme/current")), cloudsettings::Scope::Portable);
        QCOMPARE(cloudsettings::scopeOf(QStringLiteral("cw/macros")), cloudsettings::Scope::Portable);
        QCOMPARE(cloudsettings::scopeOf(QStringLiteral("station/profile/home")), cloudsettings::Scope::Station);
        QCOMPARE(cloudsettings::scopeOf(QStringLiteral("rig/serialPort")), cloudsettings::Scope::Device);
        QCOMPARE(cloudsettings::scopeOf(QStringLiteral("udp/port")), cloudsettings::Scope::Device);
        QCOMPARE(cloudsettings::scopeOf(QStringLiteral("station/activeProfile")), cloudsettings::Scope::Device);
    }

    void everythingTravelsButTheMachine()
    {
        QTemporaryDir dir;
        QScopedPointer<QSettings> s(fresh(dir, QStringLiteral("a.ini")));
        s->setValue(QStringLiteral("theme/current"), QStringLiteral("Darkcodium"));
        s->setValue(QStringLiteral("cw/macros"), QStringLiteral("CQ|CALL|TU"));
        s->setValue(QStringLiteral("layout/savedFilters"), QStringList{QStringLiteral("20m"), QStringLiteral("CW")});
        s->setValue(QStringLiteral("udp/port"), 2238);
        s->setValue(QStringLiteral("rig/serialPort"), QStringLiteral("/dev/cu.usbserial-1"));
        s->setValue(QStringLiteral("cw/audioInput"), QStringLiteral("USB Audio CODEC"));
        s->setValue(QStringLiteral("rotor/host"), QStringLiteral("127.0.0.1"));
        s->setValue(QStringLiteral("backup/folder"), QStringLiteral("D:/log/backup"));
        s->setValue(QStringLiteral("credentials/cloud/stored"), true);
        s->setValue(QStringLiteral("cloud/lastSync"), QStringLiteral("2026-09-18 19:53"));
        s->setValue(QStringLiteral("cloud/server"), QStringLiteral("https://cloud.ft2.it"));

        const QVariantMap out = cloudsettings::collect(*s);
        QCOMPARE(out.value(QStringLiteral("theme/current")).toString(), QStringLiteral("Darkcodium"));
        QCOMPARE(out.value(QStringLiteral("cw/macros")).toString(), QStringLiteral("CQ|CALL|TU"));
        QCOMPARE(out.value(QStringLiteral("layout/savedFilters")).toStringList(), QStringList({QStringLiteral("20m"), QStringLiteral("CW")}));
        QVERIFY(!out.contains(QStringLiteral("udp/port")));
        QVERIFY(!out.contains(QStringLiteral("rig/serialPort")));
        QVERIFY(!out.contains(QStringLiteral("cw/audioInput")));
        QVERIFY(!out.contains(QStringLiteral("rotor/host")));
        QVERIFY(!out.contains(QStringLiteral("backup/folder")));
        QCOMPARE(out.value(QStringLiteral("cloud/server")).toString(), QStringLiteral("https://cloud.ft2.it"));
        // Il promemoria del portachiavi e il quaderno del sync restano qui.
        QVERIFY(!out.contains(QStringLiteral("credentials/cloud/stored")));
        QVERIFY(!out.contains(QStringLiteral("cloud/lastSync")));
    }

    void networkIdentityStaysOnThisMachine()
    {
        QTemporaryDir dir;
        QScopedPointer<QSettings> source(fresh(dir, QStringLiteral("net-source.ini")));
        source->setValue(QStringLiteral("net/id"), QStringLiteral("source-device"));

        const QVariantMap document = cloudsettings::collect(*source);
        // L'identita' UDP e' strettamente locale: non deve diventare quella
        // del primo computer dopo un ripristino Cloud.
        QVERIFY(!document.contains(QStringLiteral("net/id")));

        QScopedPointer<QSettings> target(fresh(dir, QStringLiteral("net-target.ini")));
        target->setValue(QStringLiteral("net/id"), QStringLiteral("target-device"));
        cloudsettings::apply(*target, document);
        QCOMPARE(target->value(QStringLiteral("net/id")).toString(), QStringLiteral("target-device"));
    }

    void aSavedFilterSurvivesTheTrip()
    {
        QTemporaryDir dir;
        QScopedPointer<QSettings> from(fresh(dir, QStringLiteral("b.ini")));
        const QVariantMap filter{{QStringLiteral("bands"), QStringList{QStringLiteral("20m")}},
                                 {QStringLiteral("minSnr"), -12}};
        from->setValue(QStringLiteral("layout/savedFilters"), QVariant(filter));

        const QVariantMap document = cloudsettings::collect(*from);
        // Un QVariantMap non e' testo: viaggia impacchettato, non a pezzi.
        QVERIFY(document.value(QStringLiteral("layout/savedFilters")).toMap()
                    .contains(QStringLiteral("__qvariant__")));

        QScopedPointer<QSettings> to(fresh(dir, QStringLiteral("c.ini")));
        QCOMPARE(cloudsettings::apply(*to, document), document.size());
        QCOMPARE(to->value(QStringLiteral("layout/savedFilters")).toMap(), filter);
    }

    void theActiveProfileTravelsByUuid()
    {
        QTemporaryDir dir;
        QScopedPointer<QSettings> from(fresh(dir, QStringLiteral("d.ini")));
        from->setValue(QStringLiteral("station/activeProfile"), 7);

        const QVariantMap document = cloudsettings::collect(
            *from, [](qint64 id) { return id == 7 ? QStringLiteral("uuid-casa") : QString(); });
        QVERIFY(!document.contains(QStringLiteral("station/activeProfile")));
        QCOMPARE(document.value(cloudsettings::kActiveProfileUuid).toString(), QStringLiteral("uuid-casa"));

        // Sull'altro computer lo stesso profilo ha un altro numero di riga.
        QScopedPointer<QSettings> to(fresh(dir, QStringLiteral("e.ini")));
        to->setValue(QStringLiteral("station/activeProfile"), 1);
        cloudsettings::apply(*to, document, false,
                             [](const QString& uuid) -> qint64 {
                                 return uuid == QLatin1String("uuid-casa") ? 3 : 0;
                             });
        QCOMPARE(to->value(QStringLiteral("station/activeProfile")).toInt(), 3);
    }

    void whatBelongsToThisMachineIsNotAcceptedFromOutside()
    {
        QTemporaryDir dir;
        QScopedPointer<QSettings> s(fresh(dir, QStringLiteral("f.ini")));
        s->setValue(QStringLiteral("credentials/cloud/stored"), false);
        s->setValue(QStringLiteral("rig/serialPort"), QStringLiteral("local-serial"));
        s->setValue(QStringLiteral("udp/port"), 2238);
        s->setValue(QStringLiteral("backup/folder"), QStringLiteral("/local/backup"));

        const int written = cloudsettings::apply(
            *s, QVariantMap{{QStringLiteral("credentials/cloud/stored"), true},
                            {QStringLiteral("rig/serialPort"), QStringLiteral("remote-serial")},
                            {QStringLiteral("udp/port"), 9999},
                            {QStringLiteral("backup/folder"), QStringLiteral("D:/remote")},
                            {QStringLiteral("cloud/callsign"), QStringLiteral("XX0XXX")},
                            {QStringLiteral("theme/current"), QStringLiteral("Stellar Light")}});
        QCOMPARE(written, 1);
        QCOMPARE(s->value(QStringLiteral("credentials/cloud/stored")).toBool(), false);
        QCOMPARE(s->value(QStringLiteral("rig/serialPort")).toString(), QStringLiteral("local-serial"));
        QCOMPARE(s->value(QStringLiteral("udp/port")).toInt(), 2238);
        QCOMPARE(s->value(QStringLiteral("backup/folder")).toString(), QStringLiteral("/local/backup"));
        QVERIFY(!s->contains(QStringLiteral("cloud/callsign")));
        QCOMPARE(s->value(QStringLiteral("theme/current")).toString(), QStringLiteral("Stellar Light"));
    }

    // Le macro CW cambiate qui e non ancora mandate non tornano quelle di
    // prima quando dal Cloud arrivano le impostazioni: vince la modifica di
    // qui. Quello che qui non e' cambiato arriva come sempre.
    void whatChangedHereAndWasNotSentStays()
    {
        QTemporaryDir dir;
        QScopedPointer<QSettings> s(fresh(dir, QStringLiteral("m.ini")));
        s->setValue(QStringLiteral("cw/macros"), QStringLiteral("[old]"));
        s->setValue(QStringLiteral("theme/current"), QStringLiteral("Darkcodium"));
        s->setValue(QStringLiteral("udp/port"), 2237);
        const QVariantMap base = cloudsettings::collect(*s);

        // Qui si cambiano le macro...
        s->setValue(QStringLiteral("cw/macros"), QStringLiteral("[mine]"));
        // ...e dal Cloud arriva un'altra versione, con le macro vecchie e un
        // tema nuovo scelto sull'altro computer. La porta letta dal file e'
        // testo, quella in memoria un numero: e' la stessa, non una modifica.
        QVariantMap remote = base;
        remote.insert(QStringLiteral("theme/current"), QStringLiteral("Stellar Light"));
        remote.insert(QStringLiteral("udp/port"), QStringLiteral("2237"));

        QStringList kept;
        const int written = cloudsettings::apply(*s, remote, false, {}, &base, &kept);
        QCOMPARE(written, 1);
        QCOMPARE(kept, QStringList{QStringLiteral("cw/macros")});
        QCOMPARE(s->value(QStringLiteral("cw/macros")).toString(), QStringLiteral("[mine]"));
        QCOMPARE(s->value(QStringLiteral("theme/current")).toString(), QStringLiteral("Stellar Light"));

        // Senza base (come prima) arriva tutto.
        const int all = cloudsettings::apply(*s, remote);
        QCOMPARE(all, 1);
        QCOMPARE(s->value(QStringLiteral("cw/macros")).toString(), QStringLiteral("[old]"));
    }

    void sameValueReadsTextAndNumbersAlike()
    {
        QVERIFY(cloudsettings::sameValue(2237, QStringLiteral("2237")));
        QVERIFY(cloudsettings::sameValue(true, QStringLiteral("true")));
        QVERIFY(cloudsettings::sameValue(QStringList{QStringLiteral("a")}, QVariantList{QStringLiteral("a")}));
        QVERIFY(!cloudsettings::sameValue(QStringLiteral("[old]"), QStringLiteral("[mine]")));
    }

    void nothingToChangeNothingWritten()
    {
        QTemporaryDir dir;
        QScopedPointer<QSettings> s(fresh(dir, QStringLiteral("g.ini")));
        s->setValue(QStringLiteral("theme/current"), QStringLiteral("Darkcodium"));
        const QVariantMap same = cloudsettings::collect(*s);

        QCOMPARE(cloudsettings::apply(*s, same), 0);
        // E l'impronta e' la stessa: al giro dopo non si rimanda niente.
        QCOMPARE(cloudsettings::fingerprint(same), cloudsettings::fingerprint(cloudsettings::collect(*s)));
    }

    void aDryRunCountsWithoutTouchingAnything()
    {
        QTemporaryDir dir;
        QScopedPointer<QSettings> s(fresh(dir, QStringLiteral("h.ini")));
        s->setValue(QStringLiteral("theme/current"), QStringLiteral("Ocean Blue"));

        const int written = cloudsettings::apply(
            *s, QVariantMap{{QStringLiteral("theme/current"), QStringLiteral("Darkcodium")}}, true);
        QCOMPARE(written, 1);
        QCOMPARE(s->value(QStringLiteral("theme/current")).toString(), QStringLiteral("Ocean Blue"));
    }
};

QTEST_MAIN(TestCloudSettings)
#include "tst_cloudsettings.moc"
