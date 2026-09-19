// DecoLog — il decoder CW: dall'audio alle lettere.
//
// Non serve una radio che decodifichi: basta l'audio che esce dalla radio.
// Si guarda quanta energia c'e' sul tono del CW (Goertzel, che e' un filtro
// stretto e costa poco), si segna quando il tono c'e' e quando non c'e', e da
// quei tempi si tirano fuori punti, linee e spazi. La velocita' non si chiede a
// nessuno: si impara dai punti che arrivano.
#pragma once

#include <QString>
#include <QVector>

namespace decolog::core {

class CwDecoder {
public:
    explicit CwDecoder(int sampleRate = 8000);

    void setSampleRate(int sampleRate);
    int sampleRate() const { return m_sampleRate; }

    // Il tono da ascoltare, in hertz. 0: lo cerca da solo fra 400 e 1000 Hz,
    // che e' dove sta il CW di chiunque.
    void setTone(int hz);
    int tone() const { return m_tone; }
    double toneHz() const { return m_foundTone; }

    // La velocita' che ha imparato, in parole al minuto.
    int wpm() const;

    // Manda dentro l'audio (mono, 16 bit) e torna il testo nuovo, se ne e'
    // uscito. Si puo' chiamare a pezzi piccoli: lo stato resta.
    QString feed(const qint16* samples, int count);
    // Quello che resta da dire quando il segnale finisce (l'ultima lettera).
    QString flush();
    void reset();

private:
    struct Bin {
        double frequency{0};
        double coeff{0};
        double s1{0};
        double s2{0};
    };

    void rebuildBins();
    double magnitudeOf(Bin& bin) const;
    void pushRun(bool mark, int blocks);
    void closeCharacter();
    void emitPending();
    double dotGuess() const;

    int m_sampleRate{8000};
    int m_tone{0};
    double m_foundTone{0};
    int m_blockSize{64};
    QVector<Bin> m_bins;
    QVector<qint16> m_partial;

    // Soglia che si adatta: il rumore sale e scende, il CW resta leggibile.
    double m_loud{0};
    double m_quiet{0};
    double m_ratio{0};
    bool m_on{false};
    int m_runBlocks{0};
    double m_dotBlocks{0};

    // I segni della lettera in corso, ancora in blocchi: si trasformano in
    // punti e linee solo quando la lettera e' finita, quando cioe' si sa
    // quanto dura un punto.
    QVector<int> m_marks;
    // La lettera di prima, ancora in blocchi: si scrive quando la prossima e'
    // finita, con la velocita' imparata nel frattempo.
    QVector<int> m_pendingMarks;
    bool m_pendingWord{false};
    int m_shortestMark{0};
    bool m_sawLongMark{false};
    QString m_output;
    bool m_wordPending{false};
};

// La tavola del Morse, per chi deve scrivere e per chi deve leggere.
QString morseToChar(const QString& symbols);
QString charToMorse(QChar c);

} // namespace decolog::core
