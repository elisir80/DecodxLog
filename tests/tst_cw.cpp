// Il decoder CW: si genera il segnale che uscirebbe dalla radio — un tono che
// va e viene coi tempi del Morse — e si guarda se quello che torna e' il testo
// di partenza. Niente radio, niente scheda audio: solo numeri.
#include "core/CwDecoder.h"

#include <QTest>

#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace decolog::core;

namespace {

// Il testo, in tono a 700 Hz, alla velocita' chiesta, con un po' di rumore.
QVector<qint16> morseAudio(const QString& text, int wpm, int sampleRate = 8000,
                           double toneHz = 700, double noise = 0.0, int dahDots = 3)
{
    const int dotSamples = static_cast<int>(1.2 / wpm * sampleRate);
    QVector<qint16> out;
    double phase = 0;
    const double step = 2.0 * M_PI * toneHz / sampleRate;

    auto add = [&](int samples, bool on) {
        for (int i = 0; i < samples; ++i) {
            double value = on ? std::sin(phase) * 0.5 : 0.0;
            if (noise > 0)
                value += ((std::rand() % 2000) / 1000.0 - 1.0) * noise;
            phase += step;
            out << static_cast<qint16>(qBound(-1.0, value, 1.0) * 32000);
        }
    };

    add(dotSamples * 8, false);   // un po' di silenzio, come in aria
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char(' ')) {
            add(dotSamples * 4, false);   // fra le parole: 7 punti in tutto
            continue;
        }
        const QString symbols = charToMorse(c);
        for (int s = 0; s < symbols.size(); ++s) {
            add(dotSamples * (symbols.at(s) == QLatin1Char('.') ? 1 : dahDots), true);
            add(dotSamples, false);       // dentro la lettera
        }
        add(dotSamples * 2, false);       // fra le lettere: 3 punti in tutto
    }
    add(dotSamples * 12, false);
    return out;
}

QString decode(const QVector<qint16>& audio, int sampleRate = 8000)
{
    CwDecoder decoder(sampleRate);
    QString text;
    // A pezzi, come arriverebbero dalla scheda audio.
    const int chunk = 512;
    for (int i = 0; i < audio.size(); i += chunk)
        text += decoder.feed(audio.constData() + i, qMin(chunk, audio.size() - i));
    text += decoder.flush();
    return text;
}

} // namespace

class TestCw : public QObject {
    Q_OBJECT

private slots:
    void theTableGoesBothWays()
    {
        QCOMPARE(charToMorse(QLatin1Char('A')), QStringLiteral(".-"));
        QCOMPARE(charToMorse(QLatin1Char('5')), QStringLiteral("....."));
        QCOMPARE(morseToChar(QStringLiteral("-.-")), QStringLiteral("K"));
        QCOMPARE(morseToChar(QStringLiteral("...-.-")), QStringLiteral("<SK>"));
        QVERIFY(morseToChar(QStringLiteral("........")).isEmpty());
    }

    void readsACallFromTheAudio()
    {
        const QString text = decode(morseAudio(QStringLiteral("CQ TEST IU8LMC"), 20));
        QVERIFY2(text.contains(QStringLiteral("CQ")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("TEST")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("IU8LMC")), qPrintable(text));
    }

    void followsTheSpeed()
    {
        for (int wpm : {15, 25, 35}) {
            CwDecoder decoder(8000);
            const QVector<qint16> audio = morseAudio(QStringLiteral("PARIS PARIS"), wpm);
            QString text;
            const int chunk = 512;
            for (int i = 0; i < audio.size(); i += chunk)
                text += decoder.feed(audio.constData() + i, qMin(chunk, audio.size() - i));
            text += decoder.flush();
            QVERIFY2(text.contains(QStringLiteral("PARIS")), qPrintable(QStringLiteral("%1 wpm: %2").arg(wpm).arg(text)));
            // La velocita' che ha imparato e' quella giusta, a spanne.
            // ggmorse la misura, non la indovina: due parole al minuto di
            // scarto sono gia' larghe.
            QVERIFY2(qAbs(decoder.wpm() - wpm) <= 2,
                     qPrintable(QStringLiteral("%1 wpm letti come %2").arg(wpm).arg(decoder.wpm())));
        }
    }

    void honoursALockedSpeed()
    {
        // La velocita' puo' essere fissata quando Auto trova un ritmo spurio
        // in mezzo a un segnale intermittente. Deve restare un'opzione del
        // ricevitore, non confondersi con la velocita' del keyer TX.
        for (int wpm : {15, 25, 35}) {
            CwDecoder decoder(8000);
            decoder.setTone(700);
            decoder.setSpeed(wpm);
            const QVector<qint16> audio = morseAudio(QStringLiteral("PARIS PARIS"), wpm);
            QString text;
            for (int i = 0; i < audio.size(); i += 512)
                text += decoder.feed(audio.constData() + i, qMin(512, audio.size() - i));
            text += decoder.flush();
            QVERIFY2(text.contains(QStringLiteral("PARIS")),
                     qPrintable(QStringLiteral("%1 WPM: %2").arg(wpm).arg(text)));
            QCOMPARE(decoder.speed(), wpm);
        }
    }

    void readsNativeRateAudio()
    {
        // Le schede USB moderne lavorano normalmente a 44,1 o 48 kHz. Il
        // decoder deve leggerle nel loro formato nativo, senza dipendere dal
        // ricampionatore del backend audio, e lasciare a ggmorse la
        // conversione alla sua frequenza interna.
        CwDecoder decoder(48000);
        const QVector<qint16> audio = morseAudio(QStringLiteral("CQ DE IU8LMC"), 20, 48000, 620);
        QString text;
        for (int i = 0; i < audio.size(); i += 2048)
            text += decoder.feed(audio.constData() + i, qMin(2048, audio.size() - i));
        text += decoder.flush();
        QVERIFY2(text.contains(QStringLiteral("IU8LMC")), qPrintable(text));
        QVERIFY2(qAbs(decoder.toneHz() - 620) < 20, qPrintable(QString::number(decoder.toneHz())));
    }

    void survivesSomeNoise()
    {
        // Col rumore in banda il decoder tiene: le prime lettere dopo un lungo
        // silenzio possono uscire storte — finche' non ha imparato quanto dura
        // un punto non puo' saperlo, e succede a tutti i decoder — ma da li' in
        // poi legge, nominativo compreso.
        std::srand(7);
        const QString text = decode(morseAudio(QStringLiteral("TEST DE IU8LMC"), 20, 8000, 700, 0.02));
        QVERIFY2(text.contains(QStringLiteral("IU8LMC")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("DI")) || text.contains(QStringLiteral("DE")), qPrintable(text));
    }

    void findsTheToneByItself()
    {
        // Il tono non si dice a nessuno: lo trova guardando dove sta l'energia.
        // Tre lettere sole non bastano — ggmorse misura i tempi su una finestra
        // di tre secondi, e finche' non ha agganciato la velocita' le prime
        // lettere possono restare indietro: si manda quello che si manderebbe
        // in aria davvero.
        CwDecoder decoder(8000);
        const QVector<qint16> audio = morseAudio(QStringLiteral("SOS SOS SOS"), 18, 8000, 550);
        QString text;
        for (int i = 0; i < audio.size(); i += 512)
            text += decoder.feed(audio.constData() + i, qMin(512, audio.size() - i));
        text += decoder.flush();
        QVERIFY2(text.contains(QStringLiteral("SOS SOS")), qPrintable(text));
        QVERIFY2(qAbs(decoder.toneHz() - 550) < 20, qPrintable(QString::number(decoder.toneHz())));
    }

    void showsWhatItHears()
    {
        // Il grafico: a meta' del messaggio c'e' il segnale degli ultimi tre
        // secondi, fra 0 e 1, con dei toni sopra la soglia e dei vuoti sotto.
        CwDecoder decoder(8000);
        const QVector<qint16> audio = morseAudio(QStringLiteral("CQ CQ DE IU8LMC IU8LMC K"), 22);
        const int half = audio.size() / 2;
        for (int i = 0; i < half; i += 512)
            decoder.feed(audio.constData() + i, qMin(512, half - i));
        const CwDecoder::Scope& scope = decoder.scope();
        QVERIFY(scope.signal.size() > 100);
        QVERIFY(scope.reading);
        QVERIFY(scope.level > 0.0f && scope.level < 1.0f);
        QVERIFY2(qAbs(scope.pitch - 700) < 20, qPrintable(QString::number(scope.pitch)));
        int above = 0;
        for (const float v : scope.signal) {
            QVERIFY(v >= 0.0f && v <= 1.0f);
            if (v > scope.level)
                ++above;
        }
        QVERIFY(above > 0 && above < scope.signal.size());

        decoder.reset();
        QVERIFY(decoder.scope().signal.isEmpty());
    }

    // La fiducia: su un segnale pulito e' alta, sulla banda vuota e' zero — il pannello la
    // colora, e un valore alto sul rumore farebbe fidare di lettere che non ci sono.
    void confidenceFollowsTheSignal()
    {
        CwDecoder decoder(8000);
        const QVector<qint16> audio = morseAudio(QStringLiteral("CQ CQ DE IU8LMC IU8LMC K"), 22);
        for (int i = 0; i < audio.size() / 2; i += 512)
            decoder.feed(audio.constData() + i, qMin(512, audio.size() / 2 - i));
        QVERIFY(decoder.scope().reading);
        QVERIFY2(decoder.scope().confidence > 0.5f, qPrintable(QString::number(decoder.scope().confidence)));
        QVERIFY(decoder.scope().confidence <= 1.0f);
        QVERIFY(!decoder.scope().neural);

        std::srand(11);
        CwDecoder quiet(8000);
        QVector<qint16> chunk(512);
        for (int block = 0; block < 10 * 8000 / 512; ++block) {
            for (int i = 0; i < chunk.size(); ++i)
                chunk[i] = static_cast<qint16>(((std::rand() % 2000) / 1000.0 - 1.0) * 0.2 * 32000);
            quiet.feed(chunk.constData(), chunk.size());
        }
        QVERIFY2(quiet.scope().confidence < 0.3f, qPrintable(QString::number(quiet.scope().confidence)));
        // E sul rumore il decodificatore non si inventa un pugno ne' dei segni.
        QCOMPARE(quiet.scope().fist, 0.0f);
        QVERIFY2(quiet.scope().elements.size() < 3, qPrintable(QString::number(quiet.scope().elements.size())));
    }

    // I segni e il pugno: il Morse da manuale ha la linea lunga tre punti, e il decodificatore
    // deve vedere i segni (punti e linee) e dare un rapporto vicino a 3. Con la linea piu' lunga
    // (4 punti) il rapporto sale: e' il pugno dell'operatore.
    void seesTheElementsAndTheFist()
    {
        CwDecoder decoder(8000);
        const QVector<qint16> audio = morseAudio(QStringLiteral("CQ CQ DE IU8LMC IU8LMC K"), 22);
        for (int i = 0; i < audio.size() / 2; i += 512)
            decoder.feed(audio.constData() + i, qMin(512, audio.size() / 2 - i));
        const CwDecoder::Scope& scope = decoder.scope();
        const float standard = scope.fist;
        QVERIFY2(scope.elements.size() >= 6, qPrintable(QString::number(scope.elements.size())));
        int dits = 0;
        int dahs = 0;
        for (const CwDecoder::Scope::Element& e : scope.elements) {
            QVERIFY(e.at >= 0.0f && e.at + e.length <= 1.0001f);
            (e.dah ? dahs : dits)++;
        }
        QVERIFY(dits > 0 && dahs > 0);
        QVERIFY2(scope.fist > 2.3f && scope.fist < 3.8f, qPrintable(QString::number(scope.fist)));
        // Mano pesante: linee da quattro punti, rapporto piu' alto.
        CwDecoder heavy(8000);
        const QVector<qint16> slow = morseAudio(QStringLiteral("CQ CQ DE IU8LMC IU8LMC K"), 22, 8000, 700, 0.0, 4);
        for (int i = 0; i < slow.size() / 2; i += 512)
            heavy.feed(slow.constData() + i, qMin(512, slow.size() / 2 - i));
        QVERIFY2(heavy.scope().fist > standard + 0.4f,
                 qPrintable(QString("%1 contro %2").arg(heavy.scope().fist).arg(standard)));

        decoder.reset();
        QVERIFY(decoder.scope().elements.isEmpty());
        QCOMPARE(decoder.scope().fist, 0.0f);
    }

    void staysQuietOnAnEmptyBand()
    {
        // Mezzo minuto di solo rumore, come quando la radio e' accesa e non
        // trasmette nessuno: dal riquadro non deve uscire niente. Il rumore a
        // tratti somiglia al Morse, e un decoder che non lo riconosce riempie
        // lo schermo di lettere che nessuno ha mandato.
        std::srand(11);
        CwDecoder decoder(8000);
        QVector<qint16> chunk(512);
        QString text;
        for (int block = 0; block < 30 * 8000 / 512; ++block) {
            for (int i = 0; i < chunk.size(); ++i)
                chunk[i] = static_cast<qint16>(((std::rand() % 2000) / 1000.0 - 1.0) * 0.2 * 32000);
            text += decoder.feed(chunk.constData(), chunk.size());
        }
        text += decoder.flush();
        QVERIFY2(text.trimmed().isEmpty(), qPrintable(text));
    }

    void listensOnlyWhereItIsTold()
    {
        // Col tono fissato non va a cercare: ascolta li' e basta.
        CwDecoder decoder(8000);
        decoder.setTone(600);
        const QVector<qint16> audio = morseAudio(QStringLiteral("CQ CQ DE IU8LMC"), 22, 8000, 600);
        QString text;
        for (int i = 0; i < audio.size(); i += 512)
            text += decoder.feed(audio.constData() + i, qMin(512, audio.size() - i));
        text += decoder.flush();
        QVERIFY2(text.contains(QStringLiteral("IU8LMC")), qPrintable(text));
        QCOMPARE(decoder.tone(), 600);
        QVERIFY2(qAbs(decoder.toneHz() - 600) < 5, qPrintable(QString::number(decoder.toneHz())));
    }

};

QTEST_MAIN(TestCw)
#include "tst_cw.moc"
