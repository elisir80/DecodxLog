#include "core/CwDecoder.h"

#include "core/CtcDecoder.h"
#include "core/Lexicon.h"
#include "ggmorse/ggmorse.h"

#include <QHash>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace decolog::core {

namespace {

const QHash<QString, QString>& table()
{
    static const QHash<QString, QString> map{
        {".-", "A"},    {"-...", "B"},  {"-.-.", "C"},  {"-..", "D"},   {".", "E"},
        {"..-.", "F"},  {"--.", "G"},   {"....", "H"},  {"..", "I"},    {".---", "J"},
        {"-.-", "K"},   {".-..", "L"},  {"--", "M"},    {"-.", "N"},    {"---", "O"},
        {".--.", "P"},  {"--.-", "Q"},  {".-.", "R"},   {"...", "S"},   {"-", "T"},
        {"..-", "U"},   {"...-", "V"},  {".--", "W"},   {"-..-", "X"},  {"-.--", "Y"},
        {"--..", "Z"},
        {"-----", "0"}, {".----", "1"}, {"..---", "2"}, {"...--", "3"}, {"....-", "4"},
        {".....", "5"}, {"-....", "6"}, {"--...", "7"}, {"---..", "8"}, {"----.", "9"},
        {"-..-.", "/"}, {"-...-", "="}, {".-.-.", "+"}, {"-....-", "-"}, {"..--..", "?"},
        {".-.-.-", "."}, {"--..--", ","}, {"---...", ":"}, {".----.", "'"}, {".--.-.", "@"},
        {"-.--.", "("}, {"-.--.-", ")"}, {"...-.-", "<SK>"}, {"-.-.-", "<KA>"},
        {"...-.", "<SN>"}, {".-...", "<AS>"},
    };
    return map;
}

// Quanti campioni di silenzio servono in coda perche' ggmorse si convinca che
// la parola e' finita: la sua finestra di analisi e' lunga tre secondi.
constexpr double kFlushSeconds = 3.2;

// Sopra questo costo quello che esce non e' Morse: e' il rumore che a tratti
// gli somiglia. Misurato: un segnale, anche sepolto nel rumore, sta sotto 0.01;
// il solo rumore di banda sta fra 0.68 e 1.3.
constexpr float kMaxCost = 0.2f;

} // namespace

QString morseToChar(const QString& symbols)
{
    return table().value(symbols);
}

QString charToMorse(QChar c)
{
    const QString upper = QString(c).toUpper();
    for (auto it = table().cbegin(); it != table().cend(); ++it) {
        if (it.value() == upper)
            return it.key();
    }
    return {};
}

CwDecoder::CwDecoder(int sampleRate)
{
    setSampleRate(sampleRate);
}

CwDecoder::~CwDecoder() = default;

void CwDecoder::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate > 0 ? sampleRate : 8000;
    rebuild();
}

void CwDecoder::setTone(int hz)
{
    m_tone = hz > 0 ? hz : 0;
    applyDecodeParameters();
}

void CwDecoder::setSpeed(int wpm)
{
    // ggmorse cerca velocita' fino a 59 WPM; 0 mantiene il suo rilevamento
    // automatico. Non mandargli valori fuori dal suo intervallo di ricerca.
    m_speed = wpm > 0 ? std::clamp(wpm, 5, 59) : 0;
    applyDecodeParameters();
}

void CwDecoder::applyDecodeParameters()
{
    if (!m_morse)
        return;
    GGMorse::ParametersDecode decode = GGMorse::getDefaultParametersDecode();
    // Con -1 se lo cerca da solo; con un numero ascolta solo li'.
    decode.frequency_hz = m_tone > 0 ? static_cast<float>(m_tone) : -1.0f;
    // Con -1 cerca anche la velocita'; un valore positivo la blocca.
    decode.speed_wpm = m_speed > 0 ? static_cast<float>(m_speed) : -1.0f;
    m_morse->setParametersDecode(decode);
}

void CwDecoder::rebuild()
{
    GGMorse::Parameters parameters{
        static_cast<float>(m_sampleRate),
        static_cast<float>(m_sampleRate),
        GGMorse::kDefaultSamplesPerFrame,
        GGMORSE_SAMPLE_FORMAT_I16,
        GGMORSE_SAMPLE_FORMAT_I16,
    };
    m_morse = std::make_unique<GGMorse>(parameters);
    m_pending.clear();
    m_taken = 0;
    m_fedSamples = 0;
    m_toneHz = 0;
    m_wpm = 0;
    m_scope = Scope();
    m_last = QChar();
    m_word.clear();
    m_retract = 0;
    setTone(m_tone);
    if (m_neural)
        m_neural->reset();
}

int CwDecoder::takeRetract()
{
    const int r = m_retract;
    m_retract = 0;
    return r;
}

QString CwDecoder::applyLexicon(const QString& chunk)
{
    if (!m_lexicon)
        return chunk;
    QString out;
    qsizetype wordInOut = 0;   // le lettere della parola in corso che stanno in questo `out`
    for (const QChar ch : chunk) {
        if (ch != QLatin1Char(' ') && ch != QLatin1Char('\n')) {
            m_word += ch;
            out += ch;
            ++wordInOut;
            continue;
        }
        if (!m_word.isEmpty()) {
            // Solo parole di sole lettere: virgole e simboli restano com'erano.
            bool lettersOnly = true;
            for (const QChar c : m_word)
                lettersOnly = lettersOnly && c.isLetter();
            const QString upper = m_word.toUpper();
            const QString fixed = lettersOnly ? lexicon::correct(upper) : upper;
            if (fixed != upper) {
                const qsizetype take = m_word.size();
                const qsizetype fromOut = std::min(take, wordInOut);
                out.chop(fromOut);
                m_retract += static_cast<int>(take - fromOut);
                out += fixed;
            }
            m_word.clear();
            wordInOut = 0;
        }
        out += ch;
    }
    return out;
}

bool CwDecoder::neuralAvailable()
{
    return CtcDecoder::isCompiled();
}

bool CwDecoder::setNeuralModel(const QString& path)
{
    m_neural.reset();
    if (path.isEmpty() || !neuralAvailable())
        return false;
    auto net = std::make_unique<CtcDecoder>();
    if (!net->loadModel(path))
        return false;
    m_neural = std::move(net);
    reset();
    return true;
}

// Il testo della rete per un pezzo di audio. ggmorse va avanti lo stesso (tono, pannello), ma il
// suo testo si butta: col modello neurale il testo viene da qui.
static QString neuralText(CtcDecoder& net, const std::vector<float>& mono, int rate, int tone, double toneHz)
{
    QString out;
    const double hz = tone > 0 ? tone : (toneHz > 0 ? toneHz : 700.0);
    net.processAudio(mono.data(), static_cast<int>(mono.size()), rate, hz, out);
    return out;
}

void CwDecoder::reset()
{
    rebuild();
}

QString CwDecoder::drain()
{
    QString out;
    // Un fotogramma per volta: cosi' il costo che si legge dopo e' quello di
    // quel pezzo di audio, e si sa se le lettere appena uscite valgono.
    bool served = false;
    GGMorse::CBWaveformInp feeder = [this, &served](void* data, uint32_t maxBytes) -> uint32_t {
        if (served)
            return 0;
        const qsizetype have = m_pending.size() - m_taken;
        if (have < static_cast<qsizetype>(maxBytes))
            return 0;
        std::memcpy(data, m_pending.constData() + m_taken, maxBytes);
        m_taken += maxBytes;
        served = true;
        return maxBytes;
    };

    // Il segnale lo rifa' a ogni fotogramma sull'intera finestra: basta
    // l'ultimo. La soglia invece la accoda, e se non la si prende cresce.
    GGMorse::SignalF signal;
    GGMorse::ThresholdF thresholds;
    bool fresh = false;

    for (;;) {
        served = false;
        m_morse->decode(feeder);
        if (!served)
            break;   // l'audio in mano e' finito: il resto arriva dopo
        if (m_morse->takeSignalF(signal) > 0)
            fresh = true;
        m_morse->takeThresholdF(thresholds);

        // Il costo dice quanto i tempi misurati somigliano a del Morse vero.
        // Su un segnale, anche brutto, sta sotto il centesimo; sul solo rumore
        // di banda sta intorno a uno. In mezzo c'e' tutto lo spazio per dire di
        // no: senza questo controllo il riquadro si riempie di lettere finte
        // appena la radio e' accesa e nessuno sta trasmettendo.
        const GGMorse::Statistics& stats = m_morse->getStatistics();
        const bool reading = stats.costFunction < kMaxCost && stats.estimatedPitch_Hz > 0;
        if (reading) {
            m_toneHz = stats.estimatedPitch_Hz;
            m_wpm = static_cast<int>(std::lround(stats.estimatedSpeed_wpm));
        }

        // Si svuota comunque: quello che non vale si butta, non si accumula.
        GGMorse::TxRx decoded;
        if (m_morse->takeRxData(decoded) <= 0 || !reading)
            continue;
        for (const std::uint8_t c : decoded) {
            // Quando il tono cambia di colpo ggmorse va a capo: nel pannello una
            // riga nuova ci sta, ma non due di fila, e non come prima cosa.
            const QChar ch = QLatin1Char(static_cast<char>(c));
            if (ch == QLatin1Char('\n') && (m_last.isNull() || m_last == QLatin1Char('\n')))
                continue;
            out += ch;
            m_last = ch;
        }
    }

    if (fresh)
        updateScope(signal);

    // Quello che e' stato consumato non serve piu'; il resto aspetta il pezzo
    // successivo di audio.
    if (m_taken > 0) {
        m_pending.remove(0, m_taken);
        m_taken = 0;
    }
    return out;
}

void CwDecoder::updateScope(const std::vector<float>& signal)
{
    const GGMorse::Statistics& stats = m_morse->getStatistics();
    // ggmorse decide acceso/spento con la media del segnale per la soglia: la
    // stessa riga, riportata sulla scala del disegno.
    double mean = 0;
    float peak = 0;
    for (const float v : signal) {
        mean += v;
        peak = std::max(peak, v);
    }
    mean = signal.empty() ? 0 : mean / static_cast<double>(signal.size());

    m_scope.signal.resize(static_cast<qsizetype>(signal.size()));
    for (std::size_t i = 0; i < signal.size(); ++i)
        m_scope.signal[static_cast<qsizetype>(i)] = peak > 0 ? signal[i] / peak : 0.0f;
    m_scope.level = peak > 0 ? std::min(1.0f, static_cast<float>(mean * stats.signalThreshold) / peak) : 0.0f;
    m_scope.pitch = stats.estimatedPitch_Hz;
    m_scope.speed = stats.estimatedSpeed_wpm;
    m_scope.cost = stats.costFunction;
    m_scope.reading = stats.costFunction < kMaxCost && stats.estimatedPitch_Hz > 0;
    // La fiducia di ggmorse: il costo sta fra 0 (Morse perfetto) e la soglia di lettura. Con la
    // rete neurale la aggiorna feed(), che ha le sue probabilita'.
    if (!m_neural) {
        const float raw = m_scope.reading ? std::clamp(1.0f - stats.costFunction / kMaxCost, 0.0f, 1.0f) : 0.0f;
        m_scope.confidence = raw == 0.0f ? 0.0f : 0.7f * m_scope.confidence + 0.3f * raw;
    }
    m_scope.neural = m_neural != nullptr;

    // I segni: le corse di «acceso» sopra la soglia, quelle che non toccano i bordi della finestra.
    // Il rapporto fra linea e punto si ricava dai segni stessi — due gruppi di durate — e non
    // dalla velocita' di ggmorse: con una mano pesante ggmorse non legge piu', ma i segni
    // restano misurabili.
    m_scope.elements.clear();
    const int n = static_cast<int>(m_scope.signal.size());
    const double window = std::min(3.0, static_cast<double>(m_fedSamples) / std::max(1, m_sampleRate));
    struct Run {
        int from;
        int to;
        double seconds;
    };
    std::vector<Run> runs;
    if (n > 8 && window > 0.3) {
        for (int i = 0; i < n;) {
            const bool on = m_scope.signal[i] > m_scope.level;
            int j = i;
            while (j < n && (m_scope.signal[j] > m_scope.level) == on)
                ++j;
            if (on && i > 0 && j < n)
                runs.push_back({i, j, (j - i) * window / n});
            i = j;
        }
    }
    bool measured = false;
    if (runs.size() >= 4) {
        std::vector<double> d;
        for (const Run& r : runs)
            d.push_back(r.seconds);
        std::sort(d.begin(), d.end());
        // Il salto piu' grande fra due durate vicine: sotto i punti, sopra le linee.
        size_t split = 0;
        double bestJump = 1.0;
        for (size_t k = 0; k + 1 < d.size(); ++k) {
            const double jump = d[k + 1] / std::max(d[k], 1e-6);
            if (jump > bestJump) {
                bestJump = jump;
                split = k + 1;
            }
        }
        auto median = [](const std::vector<double>& v, size_t from, size_t to) {
            return v[from + (to - from) / 2];
        };
        double dah = 0;      // soglia di classificazione: sopra e' una linea
        double ditMed = 0;
        double dahMed = 0;
        bool clusters = false;
        if (bestJump >= 1.5 && split > 0 && split < d.size()) {
            ditMed = median(d, 0, split);
            dahMed = median(d, split, d.size());
            const double spreadDit = d[split - 1] / std::max(d.front(), 1e-6);
            const double spreadDah = d.back() / std::max(d[split], 1e-6);
            const double ratio = dahMed / ditMed;
            // Due gruppi veri: ciascuno stretto, e il rapporto di una mano umana. Fuori da
            // qui e' rumore che a caso ha qualche tono acceso.
            clusters = ratio >= 1.5 && ratio <= 6.0 && spreadDit <= 2.0 && spreadDah <= 2.0;
            if (clusters)
                dah = std::sqrt(ditMed * dahMed);
        }
        // Un gruppo solo (tutti punti, o tutte linee): lo dice la velocita' di ggmorse, se c'e'.
        const double wpm = m_scope.speed;
        if (!clusters && wpm >= 5.0 && m_scope.reading) {
            dah = 2.0 * 1.2 / wpm;
            measured = true;
        } else if (clusters) {
            measured = true;
        }
        if (measured && (clusters || m_scope.reading)) {
            std::vector<double> dits;
            std::vector<double> dahs;
            for (const Run& r : runs) {
                // Meno di un terzo del punto e' un disturbo, non un segno.
                const double dit = clusters ? ditMed : 1.2 / std::max(wpm, 5.0);
                if (r.seconds < dit / 3.0)
                    continue;
                Scope::Element e;
                e.at = static_cast<float>(static_cast<double>(r.from) / n);
                e.length = static_cast<float>(static_cast<double>(r.to - r.from) / n);
                e.dah = r.seconds > dah;
                m_scope.elements << e;
                (e.dah ? dahs : dits).push_back(r.seconds);
            }
            if (clusters && dits.size() >= 3 && dahs.size() >= 2) {
                std::sort(dits.begin(), dits.end());
                std::sort(dahs.begin(), dahs.end());
                const double ratio = std::clamp(dahs[dahs.size() / 2] / dits[dits.size() / 2], 1.5, 6.0);
                m_scope.fist = m_scope.fist > 0 ? 0.6f * m_scope.fist + 0.4f * static_cast<float>(ratio)
                                                : static_cast<float>(ratio);
            }
        }
    }
    if (!measured && !m_scope.reading)
        m_scope.fist = 0;
    // Con i segni misurati il segnale e' Morse anche se ggmorse non lo legge: il pannello non
    // deve dire «rumore» davanti a una mano pesante che la rete sta decodificando.
    if (!m_scope.reading && measured && m_neural)
        m_scope.reading = true;
}

QString CwDecoder::feed(const qint16* samples, int count)
{
    if (!m_morse || !samples || count <= 0)
        return {};
    m_pending.append(reinterpret_cast<const char*>(samples),
                     static_cast<qsizetype>(count) * static_cast<qsizetype>(sizeof(qint16)));
    m_fedSamples += count;
    const QString fromGgmorse = drain();
    if (!m_neural)
        return applyLexicon(fromGgmorse);
    std::vector<float> mono(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        mono[static_cast<size_t>(i)] = samples[i] / 32768.0f;
    const QString text = neuralText(*m_neural, mono, m_sampleRate, m_tone, m_toneHz);
    const float raw = m_scope.reading ? std::clamp(static_cast<float>(m_neural->confidence()), 0.0f, 1.0f) : 0.0f;
    m_scope.confidence = raw == 0.0f ? 0.0f : 0.7f * m_scope.confidence + 0.3f * raw;
    return applyLexicon(text);
}

QString CwDecoder::flush()
{
    if (!m_morse)
        return {};
    // Il silenzio che in aria ci sarebbe comunque: senza, l'ultima lettera
    // resterebbe in bocca al decodificatore.
    const int silence = static_cast<int>(kFlushSeconds * m_sampleRate);
    m_pending.append(static_cast<qsizetype>(silence) * static_cast<qsizetype>(sizeof(qint16)), '\0');
    QString out = drain();
    if (m_neural) {
        const std::vector<float> quiet(static_cast<size_t>(silence), 0.0f);
        out = neuralText(*m_neural, quiet, m_sampleRate, m_tone, m_toneHz);
    }
    // L'ultima parola si chiude qui: senza uno spazio in coda non verrebbe corretta.
    out = applyLexicon(out + QLatin1Char(' '));
    while (out.endsWith(QLatin1Char('\n')) || out.endsWith(QLatin1Char(' ')))
        out.chop(1);
    return out;
}

} // namespace decolog::core
