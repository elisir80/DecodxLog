// La correzione delle parole del decoder CW: una parola a un errore da una voce del lessico si
// corregge, le cifre e i nominativi no, e le lettere gia' mostrate si ritirano.
#include "core/CwDecoder.h"
#include "core/Lexicon.h"

using namespace decolog::core;

#include <QTest>

using decolog::core::CwDecoder;

namespace {

// Quello che vede chi consuma il testo: ritira le lettere, poi aggiunge.
struct Screen {
    CwDecoder decoder{8000};
    QString text;
    void feed(const QString& chunk)
    {
        const QString out = decoder.applyLexicon(chunk);
        text.chop(decoder.takeRetract());
        text += out;
    }
};

} // namespace

class TestLexicon : public QObject {
    Q_OBJECT

private slots:
    void correctsAWordOffByOneMorseElement()
    {
        QCOMPARE(lexicon::correct(QStringLiteral("TMX")), QStringLiteral("TNX"));
        // Gia' del lessico, o troppo corta, o con le cifre: non si tocca.
        QCOMPARE(lexicon::correct(QStringLiteral("TNX")), QStringLiteral("TNX"));
        QCOMPARE(lexicon::correct(QStringLiteral("DE")), QStringLiteral("DE"));
        QCOMPARE(lexicon::correct(QStringLiteral("5NN")), QStringLiteral("5NN"));
        QCOMPARE(lexicon::correct(QStringLiteral("IU8LMC")), QStringLiteral("IU8LMC"));
    }

    void aWordIsCorrectedWhenItCloses_andTheLettersAreTakenBack()
    {
        Screen s;
        s.feed(QStringLiteral("CQ T"));                    // la parola e' ancora aperta
        QCOMPARE(s.text, QStringLiteral("CQ T"));
        s.feed(QStringLiteral("MX "));                     // si chiude: TMX diventa TNX
        QCOMPARE(s.text, QStringLiteral("CQ TNX "));
    }

    void aWordInsideOneChunkIsCorrectedInPlace()
    {
        Screen s;
        s.feed(QStringLiteral("CQ DE IU8LMC TMX 5NN K"));
        QCOMPARE(s.text, QStringLiteral("CQ DE IU8LMC TNX 5NN K"));
    }

    void callsignsRapportsAndSymbolsAreLeftAlone()
    {
        Screen s;
        s.feed(QStringLiteral("DL1ABC 599 TMX, UR RST "));
        // «TMX,» ha una virgola: parola con simboli, non si corregge.
        QCOMPARE(s.text, QStringLiteral("DL1ABC 599 TMX, UR RST "));
    }

    void switchedOffItChangesNothing()
    {
        Screen s;
        s.decoder.setLexicon(false);
        s.feed(QStringLiteral("CQ TMX "));
        QCOMPARE(s.text, QStringLiteral("CQ TMX "));
        QCOMPARE(s.decoder.takeRetract(), 0);
    }
};

QTEST_GUILESS_MAIN(TestLexicon)
#include "tst_lexicon.moc"
