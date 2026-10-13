// Il decodificatore neurale (CTC) come opzione del decodificatore CW. Senza ONNX Runtime nella
// build la prova verifica che il programma resti su ggmorse; con ONNX Runtime, e con il modello
// indicato in DECODXLOG_CTC_MODEL, decodifica un CW sintetico e pretende le lettere giuste.
#include "core/CwDecoder.h"

#include <QFile>
#include <QProcessEnvironment>
#include <QTest>

#include <cmath>
#include <vector>

using decolog::core::CwDecoder;

namespace {

// Il codice Morse delle lettere che il test usa: punti e linee.
QString morseOf(QChar c)
{
    static const QHash<QChar, QString> table{
        {'C', "-.-."}, {'Q', "--.-"}, {'D', "-.."}, {'E', "."}, {'I', ".."}, {'U', "..-"},
        {'8', "---.."}, {'L', ".-.."}, {'M', "--"}, {'K', "-.-"},
    };
    return table.value(c);
}

// CW sintetico: un tono sinusoidale acceso e spento come lo manda un tasto, a `wpm` parole al minuto.
std::vector<qint16> synthesize(const QString& text, int wpm, double toneHz, int rate)
{
    const double dit = 1.2 / wpm;   // secondi
    std::vector<bool> keyed;        // un campione ogni 1/rate secondi, a blocchi
    auto push = [&](bool on, double seconds) {
        const auto n = static_cast<int>(std::lround(seconds * rate));
        keyed.insert(keyed.end(), static_cast<size_t>(n), on);
    };
    push(false, 1.0);
    for (qsizetype w = 0; w < text.size(); ++w) {
        const QChar c = text.at(w);
        if (c == ' ') {
            push(false, 7 * dit);
            continue;
        }
        const QString code = morseOf(c);
        for (qsizetype i = 0; i < code.size(); ++i) {
            push(true, code.at(i) == '.' ? dit : 3 * dit);
            if (i + 1 < code.size())
                push(false, dit);
        }
        push(false, 3 * dit);
    }
    push(false, 1.0);

    std::vector<qint16> pcm(keyed.size());
    for (size_t i = 0; i < keyed.size(); ++i) {
        const double t = static_cast<double>(i) / rate;
        const double v = keyed[i] ? 0.5 * std::sin(2.0 * M_PI * toneHz * t) : 0.0;
        pcm[i] = static_cast<qint16>(std::lround(v * 32767.0));
    }
    return pcm;
}

} // namespace

class TestCtcDecoder : public QObject {
    Q_OBJECT

private slots:
    // In ogni build: senza la rete il modello non si carica, e ggmorse resta.
    void withoutTheNetworkTheModelIsRefused()
    {
        if (CwDecoder::neuralAvailable())
            QSKIP("questa build ha ONNX Runtime: la prova sta nel caso successivo");
        CwDecoder d(8000);
        QVERIFY(!d.setNeuralModel(QStringLiteral("nonesiste.onnx")));
        QVERIFY(!d.neural());
    }

    // Con ONNX Runtime e un modello: il file non esiste, o non e' un modello, si rifiuta; il file
    // buono decodifica il CW sintetico.
    void theNetworkDecodesSyntheticCw()
    {
        if (!CwDecoder::neuralAvailable())
            QSKIP("questa build non ha ONNX Runtime");
        CwDecoder missing(8000);
        QVERIFY(!missing.setNeuralModel(QStringLiteral("nonesiste.onnx")));

        const QString model = QProcessEnvironment::systemEnvironment().value(QStringLiteral("DECODXLOG_CTC_MODEL"));
        if (model.isEmpty())
            QSKIP("serve DECODXLOG_CTC_MODEL con il percorso di un modello .onnx");
        CwDecoder d(8000);
        QVERIFY2(d.setNeuralModel(model), qPrintable(model));
        QVERIFY(d.neural());

        const std::vector<qint16> pcm = synthesize(QStringLiteral("CQ DE IU8LMC K"), 20, 700.0, 8000);
        QString text;
        const size_t chunk = 800;   // a pezzi, come arriva dalla scheda audio
        for (size_t at = 0; at < pcm.size(); at += chunk)
            text += d.feed(pcm.data() + at, static_cast<int>(std::min(chunk, pcm.size() - at)));
        qInfo("fiducia della rete a fine segnale: %.2f", static_cast<double>(d.scope().confidence));
        QVERIFY(d.scope().neural);
        text += d.flush();
        qInfo("CTC: \"%s\"", qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("CQ")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("IU8LMC")), qPrintable(text));
    }
};

QTEST_GUILESS_MAIN(TestCtcDecoder)
#include "tst_ctcdecoder.moc"
