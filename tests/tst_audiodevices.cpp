// Scegliere una scheda audio in modo preciso: ritrovarla per identificativo o
// per nome, e non ripiegare mai in silenzio su un'altra.
#include "app/AudioDevices.h"

#include <QTest>

using namespace decolog::app::audiodev;

namespace {

QList<Entry> cards()
{
    return {
        {"{0.0.1.00000000}.{aaa}", "Microfono (Realtek Audio)"},
        {"{0.0.1.00000000}.{bbb}", "Microfono (2- USB Audio CODEC )"},
        {"{0.0.1.00000000}.{ccc}", "Gruppo microfoni (USB Audio CODEC)"},
    };
}

} // namespace

class TestAudioDevices : public QObject {
    Q_OBJECT

private slots:
    void systemDefaultIsAChoice()
    {
        const Resolution r = resolve(cards(), {});
        QCOMPARE(r.kind, Resolution::SystemDefault);
        QCOMPARE(comboIndex(r), 0);
    }

    void findsByIdentifier()
    {
        const Resolution r = resolve(cards(), {"{0.0.1.00000000}.{bbb}", "Microfono (2- USB Audio CODEC )"});
        QCOMPARE(r.kind, Resolution::Found);
        QCOMPARE(r.index, 1);
        QCOMPARE(comboIndex(r), 2);
    }

    // Windows rinomina la scheda (il numero davanti cambia con le porte USB):
    // l'identificativo la ritrova lo stesso.
    void survivesARename()
    {
        const Resolution r = resolve(cards(), {"{0.0.1.00000000}.{bbb}", "Microfono (3- USB Audio CODEC )"});
        QCOMPARE(r.kind, Resolution::Found);
        QCOMPARE(r.index, 1);
    }

    // L'identificativo e' cambiato (un'altra porta) ma il nome e' lo stesso e
    // uno solo: e' quella.
    void fallsBackToAUniqueName()
    {
        const Resolution r = resolve(cards(), {"{0.0.1.00000000}.{old}", "Gruppo microfoni (USB Audio CODEC)"});
        QCOMPARE(r.kind, Resolution::Found);
        QCOMPARE(r.index, 2);
    }

    // Le impostazioni vecchie hanno solo il nome.
    void legacyNameOnly()
    {
        QCOMPARE(resolve(cards(), {{}, "Microfono (Realtek Audio)"}).index, 0);
        QCOMPARE(resolve(cards(), {{}, "Microfono (Realtek Audio)"}).kind, Resolution::Found);
    }

    // Quello che e' sparito non diventa un'altra scheda: ne' il predefinito, ne'
    // la prima riga dell'elenco.
    void missingStaysMissing()
    {
        const Resolution r = resolve(cards(), {"{0.0.1.00000000}.{gone}", "Microfono (9- Scheda che non c'e')"});
        QCOMPARE(r.kind, Resolution::Missing);
        QCOMPARE(r.index, -1);
        QCOMPARE(comboIndex(r), -1);
        QCOMPARE(resolve({}, {"x", "y"}).kind, Resolution::Missing);
    }

    void twoCardsWithTheSameName()
    {
        QList<Entry> twins = {{"{1}", "USB Audio CODEC"}, {"{2}", "USB Audio CODEC"}, {"{3}", "Altra"}};
        // Con l'identificativo non c'e' dubbio.
        QCOMPARE(resolve(twins, {"{2}", "USB Audio CODEC"}).kind, Resolution::Found);
        QCOMPARE(resolve(twins, {"{2}", "USB Audio CODEC"}).index, 1);
        // Solo col nome: si dice che e' ambiguo, e si prende la prima.
        const Resolution r = resolve(twins, {{}, "USB Audio CODEC"});
        QCOMPARE(r.kind, Resolution::Ambiguous);
        QCOMPARE(r.index, 0);
        // E nell'elenco si distinguono.
        QCOMPARE(labels(twins), QStringList({"USB Audio CODEC", "USB Audio CODEC (2)", "Altra"}));
    }
};

QTEST_MAIN(TestAudioDevices)
#include "tst_audiodevices.moc"
