// DecoLog — le impostazioni che viaggiano nel Cloud.
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
    void everythingTravelsButTheMachine()
    {
        QTemporaryDir dir;
        QScopedPointer<QSettings> s(fresh(dir, QStringLiteral("a.ini")));
        s->setValue(QStringLiteral("theme/current"), QStringLiteral("Darkcodium"));
        s->setValue(QStringLiteral("udp/port"), 2238);
        s->setValue(QStringLiteral("rotor/host"), QStringLiteral("127.0.0.1"));
        s->setValue(QStringLiteral("backup/folder"), QStringLiteral("D:/log/backup"));
        s->setValue(QStringLiteral("credentials/cloud/stored"), true);
        s->setValue(QStringLiteral("cloud/lastSync"), QStringLiteral("2026-09-18 19:53"));
        s->setValue(QStringLiteral("cloud/server"), QStringLiteral("https://cloud.ft2.it"));

        const QVariantMap out = cloudsettings::collect(*s);
        // Porte, percorsi e indirizzi dei programmi accanto sono impostazioni
        // come le altre: su un altro computer la stazione dev'essere la stessa.
        QCOMPARE(out.value(QStringLiteral("theme/current")).toString(), QStringLiteral("Darkcodium"));
        QCOMPARE(out.value(QStringLiteral("udp/port")).toInt(), 2238);
        QCOMPARE(out.value(QStringLiteral("rotor/host")).toString(), QStringLiteral("127.0.0.1"));
        QCOMPARE(out.value(QStringLiteral("backup/folder")).toString(), QStringLiteral("D:/log/backup"));
        QCOMPARE(out.value(QStringLiteral("cloud/server")).toString(), QStringLiteral("https://cloud.ft2.it"));
        // Il promemoria del portachiavi e il quaderno del sync restano qui.
        QVERIFY(!out.contains(QStringLiteral("credentials/cloud/stored")));
        QVERIFY(!out.contains(QStringLiteral("cloud/lastSync")));
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

        const int written = cloudsettings::apply(
            *s, QVariantMap{{QStringLiteral("credentials/cloud/stored"), true},
                            {QStringLiteral("cloud/callsign"), QStringLiteral("XX0XXX")},
                            {QStringLiteral("theme/current"), QStringLiteral("Stellar Light")}});
        QCOMPARE(written, 1);
        QCOMPARE(s->value(QStringLiteral("credentials/cloud/stored")).toBool(), false);
        QVERIFY(!s->contains(QStringLiteral("cloud/callsign")));
        QCOMPARE(s->value(QStringLiteral("theme/current")).toString(), QStringLiteral("Stellar Light"));
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
