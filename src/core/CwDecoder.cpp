#include "core/CwDecoder.h"

#include <QDebug>
#include <QHash>

#include <algorithm>
#include <utility>

#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

void CwDecoder::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate > 0 ? sampleRate : 8000;
    // Un blocco ogni 8 ms: il punto piu' corto che si usa in aria (50 wpm) ne
    // dura tre, quindi c'e' spazio per misurarlo.
    m_blockSize = qMax(16, m_sampleRate / 125);
    rebuildBins();
    reset();
}

void CwDecoder::setTone(int hz)
{
    m_tone = hz;
    rebuildBins();
}

void CwDecoder::rebuildBins()
{
    m_bins.clear();
    auto add = [this](double frequency) {
        Bin bin;
        bin.frequency = frequency;
        const double k = frequency * m_blockSize / m_sampleRate;
        bin.coeff = 2.0 * std::cos(2.0 * M_PI * k / m_blockSize);
        m_bins << bin;
    };
    if (m_tone > 0) {
        add(m_tone);
    } else {
        for (double f = 400; f <= 1000.5; f += 50)
            add(f);
    }
}

double CwDecoder::magnitudeOf(Bin& bin) const
{
    const double magnitude = std::sqrt(bin.s1 * bin.s1 + bin.s2 * bin.s2 - bin.coeff * bin.s1 * bin.s2);
    return magnitude;
}

int CwDecoder::wpm() const
{
    if (m_dotBlocks <= 0)
        return 0;
    const double dotSeconds = m_dotBlocks * m_blockSize / static_cast<double>(m_sampleRate);
    // PARIS: un punto e' 1.2/wpm secondi.
    return static_cast<int>(std::lround(1.2 / qMax(0.001, dotSeconds)));
}

void CwDecoder::reset()
{
    m_partial.clear();
    m_loud = 0;
    m_quiet = 0;
    m_on = false;
    m_runBlocks = 0;
    m_dotBlocks = 0;
    m_marks.clear();
    m_pendingMarks.clear();
    m_pendingWord = false;
    m_shortestMark = 0;
    m_sawLongMark = false;
    m_output.clear();
    m_wordPending = false;
    m_foundTone = 0;
    for (Bin& bin : m_bins) {
        bin.s1 = 0;
        bin.s2 = 0;
    }
}

QString CwDecoder::feed(const qint16* samples, int count)
{
    m_output.clear();
    m_partial.reserve(m_partial.size() + count);
    for (int i = 0; i < count; ++i)
        m_partial << samples[i];

    while (m_partial.size() >= m_blockSize) {
        double best = 0;
        double bestFrequency = 0;
        for (Bin& bin : m_bins) {
            bin.s1 = 0;
            bin.s2 = 0;
            for (int i = 0; i < m_blockSize; ++i) {
                const double s0 = m_partial.at(i) / 32768.0 + bin.coeff * bin.s1 - bin.s2;
                bin.s2 = bin.s1;
                bin.s1 = s0;
            }
            const double magnitude = magnitudeOf(bin);
            if (magnitude > best) {
                best = magnitude;
                bestFrequency = bin.frequency;
            }
        }
        m_partial.remove(0, m_blockSize);

        // Il tono c'e' se sul suo canale c'e' molta piu' roba che sugli altri:
        // il rumore e' largo e sta dappertutto, il CW e' stretto e sta li'.
        // Confrontare col vicinato, e non con una soglia fissa, e' quello che
        // tiene in piedi il decoder quando la banda e' rumorosa.
        QVector<double> levels;
        levels.reserve(m_bins.size());
        for (Bin& bin : m_bins)
            levels << magnitudeOf(bin);
        std::sort(levels.begin(), levels.end());
        const double floorLevel = levels.at(levels.size() / 2);
        const double ratio = best / qMax(1e-9, floorLevel);

        // Il livello del tono sale svelto e scende piano; il rumore di fondo e'
        // quello che si sente sugli altri canali. La soglia sta in mezzo, e le
        // due soglie diverse (piu' alta per accendere, piu' bassa per spegnere)
        // impediscono che un colpo di rumore diventi un punto.
        m_loud = best > m_loud ? m_loud + (best - m_loud) * 0.5 : m_loud + (best - m_loud) * 0.05;
        m_quiet = m_quiet <= 0 ? floorLevel : m_quiet * 0.95 + floorLevel * 0.05;
        m_ratio = m_ratio * 0.4 + ratio * 0.6;
        // Quanto il tono sta sopra ai suoi vicini: col rumore da solo si sta
        // sotto il doppio, con il CW in mezzo si va oltre il quintuplo. Si
        // accende a 3,5 e si spegne a 2,8, cosi' non traballa nel mezzo.
        const bool worthIt = m_loud > 0.004;
        const bool on = worthIt && (m_on ? m_ratio > 2.8 : m_ratio > 3.5);
        if (qEnvironmentVariableIsSet("DECOLOG_CW_DEBUG")) {
            qDebug("best=%.4f floor=%.4f ratio=%.2f on=%d run=%d dot=%.1f",
                   best, floorLevel, m_ratio, on ? 1 : 0, m_runBlocks, m_dotBlocks);
        }

        if (on && bestFrequency > 0)
            m_foundTone = m_foundTone <= 0 ? bestFrequency : m_foundTone * 0.9 + bestFrequency * 0.1;

        if (on == m_on) {
            ++m_runBlocks;
            // Silenzio lungo: la lettera e' finita, e non si aspetta oltre.
            const double dot = dotGuess();
            if (!m_on && dot > 0 && m_runBlocks > dot * 2.2 && !m_marks.isEmpty())
                closeCharacter();
            if (!m_on && dot > 0 && m_runBlocks > dot * 6 && m_wordPending) {
                if (m_pendingMarks.isEmpty())
                    m_output += QLatin1Char(' ');
                else
                    m_pendingWord = true;
                m_wordPending = false;
            }
            continue;
        }

        // Un segno o un silenzio di un blocco solo non e' Morse: e' rumore.
        // Si butta via e si resta dov'eravamo.
        if (m_runBlocks < 2) {
            m_runBlocks += 1;
            continue;
        }
        pushRun(m_on, m_runBlocks);
        m_on = on;
        m_runBlocks = 1;
    }
    return m_output;
}

double CwDecoder::dotGuess() const
{
    if (m_dotBlocks > 0)
        return m_dotBlocks;
    if (m_shortestMark <= 0)
        return 0;
    return m_shortestMark;
}

void CwDecoder::pushRun(bool mark, int blocks)
{
    if (blocks <= 0)
        return;
    if (mark) {
        m_marks << blocks;
        if (m_shortestMark > 0 && blocks >= m_shortestMark * 2)
            m_sawLongMark = true;
        if (m_shortestMark <= 0 || blocks < m_shortestMark) {
            if (m_shortestMark > 0 && m_shortestMark >= blocks * 2)
                m_sawLongMark = true;
            m_shortestMark = blocks;
        }
        m_wordPending = true;
        return;
    }

    const double dot = dotGuess();
    if (dot <= 0)
        return;
    // Piu' lungo di un punto e mezzo: la lettera e' finita.
    if (blocks > dot * 2.2)
        closeCharacter();
    if (blocks > dot * 5 && m_wordPending) {
        if (m_pendingMarks.isEmpty())
            m_output += QLatin1Char(' ');
        else
            m_pendingWord = true;
        m_wordPending = false;
    }
}

void CwDecoder::closeCharacter()
{
    if (m_marks.isEmpty())
        return;

    // Il punto di questa lettera e' il suo segno piu' corto; se quello che
    // sapevamo dice un'altra cosa, ci si mette d'accordo.
    int shortest = m_marks.first();
    for (const int blocks : std::as_const(m_marks))
        shortest = qMin(shortest, blocks);
    if (m_dotBlocks <= 0) {
        m_dotBlocks = shortest;
    } else if (shortest >= 3 && shortest < m_dotBlocks * 0.9 && shortest > m_dotBlocks * 0.3) {
        // Un segno piu' corto di quello che credevamo il punto vuol dire che il
        // punto era lui: si scende subito. Ma solo se e' un segno vero, non uno
        // sputo di rumore: sotto i tre blocchi non si crede a niente.
        m_dotBlocks = shortest;
    } else if (shortest < m_dotBlocks * 1.8) {
        m_dotBlocks = m_dotBlocks * 0.7 + shortest * 0.3;
    }

    // Esce la lettera di prima, letta con la misura di adesso: e' per questo
    // che anche la prima lettera, quando ancora non si sapeva niente, viene
    // fuori giusta.
    emitPending();
    m_pendingMarks = m_marks;
    m_marks.clear();
}

void CwDecoder::emitPending()
{
    if (m_pendingMarks.isEmpty())
        return;
    QString symbols;
    for (const int blocks : std::as_const(m_pendingMarks))
        symbols += blocks < m_dotBlocks * 2 ? QLatin1Char('.') : QLatin1Char('-');
    m_pendingMarks.clear();
    const QString letter = morseToChar(symbols);
    m_output += letter.isEmpty() ? QStringLiteral("_") : letter;
    if (m_pendingWord) {
        m_output += QLatin1Char(' ');
        m_pendingWord = false;
    }
}

QString CwDecoder::flush()
{
    m_output.clear();
    if (m_on && m_runBlocks > 0) {
        pushRun(true, m_runBlocks);
        m_runBlocks = 0;
        m_on = false;
    }
    closeCharacter();
    emitPending();
    return m_output;
}

} // namespace decolog::core
