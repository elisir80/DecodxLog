// Lo skimmer: piu' stazioni CW insieme nella stessa audio, ognuna al suo tono, ognuna col suo
// ritmo. Si genera quello che uscirebbe da una radio con tre stazioni in banda piu' rumore, e si
// guarda se ne escono tre portanti con i loro nominativi.
#include "core/CwSkimmer.h"

#include <QElapsedTimer>
#include <QTest>

#define _USE_MATH_DEFINES
#include <cmath>
#include <random>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace decolog::core;

namespace {

// Una stazione: il testo, il tono, la velocita' e quando comincia.
struct Station {
    QString text;
    double toneHz;
    int wpm;
    double startS;
    double amplitude;
};

// Un tono acceso e spento coi tempi del Morse, a partire da `startS`, ripetuto `repeat` volte.
void addStation(std::vector<double>& mix, const Station& st, int rate, int repeat)
{
    const int dot = static_cast<int>(1.2 / st.wpm * rate);
    size_t at = static_cast<size_t>(st.startS * rate);
    double phase = 0;
    const double step = 2.0 * M_PI * st.toneHz / rate;
    auto tone = [&](int samples, bool on) {
        for (int i = 0; i < samples; ++i, ++at) {
            if (at >= mix.size())
                return;
            if (on) {
                // Un attacco morbido: una portante vera non fa clic da una parte all'altra.
                const double edge = std::min({1.0, i / 24.0, (samples - i) / 24.0});
                mix[at] += st.amplitude * edge * std::sin(phase);
            }
            phase += step;
        }
    };
    for (int r = 0; r < repeat; ++r) {
        for (const QChar c : st.text) {
            if (c == QLatin1Char(' ')) {
                tone(dot * 4, false);
                continue;
            }
            const QString symbols = charToMorse(c);
            for (const QChar s : symbols) {
                tone(dot * (s == QLatin1Char('.') ? 1 : 3), true);
                tone(dot, false);
            }
            tone(dot * 2, false);
        }
        tone(dot * 12, false);
    }
}

std::vector<qint16> render(const std::vector<Station>& stations, double seconds, int rate, double noise, int repeat)
{
    std::vector<double> mix(static_cast<size_t>(seconds * rate), 0.0);
    for (const Station& st : stations)
        addStation(mix, st, rate, repeat);
    std::mt19937 rng(7);
    std::normal_distribution<double> gauss(0.0, noise);
    std::vector<qint16> pcm(mix.size());
    for (size_t i = 0; i < mix.size(); ++i) {
        const double v = std::clamp(mix[i] + gauss(rng), -1.0, 1.0);
        pcm[i] = static_cast<qint16>(std::lround(v * 30000.0));
    }
    return pcm;
}

void run(CwSkimmer& skimmer, const std::vector<qint16>& pcm)
{
    const size_t chunk = 800;   // a pezzi, come dalla scheda audio
    for (size_t at = 0; at < pcm.size(); at += chunk)
        skimmer.feed(pcm.data() + at, static_cast<int>(std::min(chunk, pcm.size() - at)));
}

} // namespace

class TestCwSkimmer : public QObject {
    Q_OBJECT

private slots:
    void silenceAndNoiseGiveNoStations()
    {
        CwSkimmer skimmer(8000);
        run(skimmer, render({}, 30.0, 8000, 0.05, 1));
        QVERIFY(skimmer.stations().isEmpty());
    }

    void findsEveryCarrierAndItsCall()
    {
        CwSkimmer skimmer(8000);
        // Tre stazioni in 600 Hz di banda: ognuna manda il suo CQ piu' volte.
        const std::vector<Station> stations{
            {QStringLiteral("CQ CQ DE K1ABC K1ABC K"), 450.0, 18, 0.5, 0.18},
            {QStringLiteral("CQ TEST IU8LMC IU8LMC"), 800.0, 24, 1.3, 0.12},
            {QStringLiteral("CQ DE JA1XYZ JA1XYZ K"), 1050.0, 15, 0.9, 0.15},
        };
        const std::vector<qint16> pcm = render(stations, 45.0, 8000, 0.02, 3);
        QElapsedTimer timer;
        timer.start();
        run(skimmer, pcm);
        const qint64 ms = timer.elapsed();
        qInfo("CPU: 45 s di audio con 3 stazioni in %lld ms", static_cast<long long>(ms));

        const QVector<CwSkimmer::Station> found = skimmer.stations();
        for (const auto& s : found)
            qInfo("  %.1f Hz  %d wpm  %.0f dB  conf %.2f  call \"%s\"  \"%s\"", s.freq, s.wpm, s.level,
                  static_cast<double>(s.confidence), qPrintable(s.call), qPrintable(s.text.right(60)));
        QCOMPARE(found.size(), 3);
        QVERIFY(std::abs(found.at(0).freq - 450.0) < 10.0);
        QVERIFY(std::abs(found.at(1).freq - 800.0) < 10.0);
        QVERIFY(std::abs(found.at(2).freq - 1050.0) < 10.0);
        QCOMPARE(found.at(0).call, QStringLiteral("K1ABC"));
        QCOMPARE(found.at(1).call, QStringLiteral("IU8LMC"));
        QCOMPARE(found.at(2).call, QStringLiteral("JA1XYZ"));
        QVERIFY(found.at(0).cq);
    }

    // Due stazioni a 130 Hz una dall'altra, con ritmi diversi: ognuna si legge senza il testo
    // dell'altra.
    void closeStationsStaySeparate()
    {
        CwSkimmer skimmer(8000);
        const std::vector<Station> stations{
            {QStringLiteral("CQ DE K1ABC K1ABC K"), 650.0, 20, 0.5, 0.15},
            {QStringLiteral("CQ DE JA1XYZ JA1XYZ K"), 780.0, 16, 0.9, 0.15},
        };
        run(skimmer, render(stations, 45.0, 8000, 0.02, 3));
        const QVector<CwSkimmer::Station> found = skimmer.stations();
        QCOMPARE(found.size(), 2);
        QCOMPARE(found.at(0).call, QStringLiteral("K1ABC"));
        QCOMPARE(found.at(1).call, QStringLiteral("JA1XYZ"));
        QVERIFY2(!found.at(0).text.contains(QStringLiteral("JA1")), qPrintable(found.at(0).text));
        QVERIFY2(!found.at(1).text.contains(QStringLiteral("K1A")), qPrintable(found.at(1).text));
    }

    // Il passa-banda stretto: un vicino a 150 Hz e dieci volte piu' forte copre la stazione che
    // si vuole sentire, e col filtro si legge.
    void theNarrowFilterKeepsAStrongNeighbourOut()
    {
        const std::vector<Station> stations{
            {QStringLiteral("CQ DE K1ABC K1ABC K"), 700.0, 20, 0.5, 0.04},
            {QStringLiteral("CQ DE JA1XYZ JA1XYZ K 599 TU"), 850.0, 24, 0.9, 0.4},
        };
        const std::vector<qint16> pcm = render(stations, 40.0, 8000, 0.01, 3);
        auto hits = [&](bool narrow) {
            CwDecoder d(8000);
            d.setTone(700);
            d.setNarrowFilter(narrow);
            QString text;
            for (size_t at = 0; at < pcm.size(); at += 800)
                text += d.feed(pcm.data() + at, static_cast<int>(std::min<size_t>(800, pcm.size() - at)));
            text += d.flush();
            return text.count(QStringLiteral("K1ABC"));
        };
        QVERIFY(hits(true) >= 4);
        QVERIFY(hits(true) > hits(false));
    }

    void aStationThatLeavesStaysThenGoes()
    {
        CwSkimmer skimmer(8000);
        const std::vector<Station> one{{QStringLiteral("CQ DE K1ABC K1ABC K"), 600.0, 20, 0.5, 0.2}};
        // Quaranta secondi di segnale e poi due minuti di sola banda.
        std::vector<qint16> pcm = render(one, 40.0, 8000, 0.02, 3);
        const std::vector<qint16> quiet = render({}, 125.0, 8000, 0.02, 1);
        pcm.insert(pcm.end(), quiet.begin(), quiet.end());
        const size_t cut = static_cast<size_t>(40.0 * 8000) + 8000 * 10;
        run(skimmer, std::vector<qint16>(pcm.begin(), pcm.begin() + static_cast<long>(cut)));
        QVERIFY(skimmer.stations().size() == 1);
        QVERIFY(!skimmer.stations().at(0).present);   // e' andata via, ma resta in elenco
        run(skimmer, std::vector<qint16>(pcm.begin() + static_cast<long>(cut), pcm.end()));
        QVERIFY(skimmer.stations().isEmpty());        // dopo mezzo minuto non c'e' piu'
    }
};

QTEST_GUILESS_MAIN(TestCwSkimmer)
#include "tst_cwskimmer.moc"
