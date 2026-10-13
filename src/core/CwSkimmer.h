// DecoDXLog — le stazioni CW in banda, tutte insieme (uno «skimmer» sull'audio della radio).
//
// Il decoder principale ascolta un tono solo. Ma nei 2-3 kHz di audio di una radio ci sono
// spesso piu' stazioni CW: qui si guarda lo spettro, si cercano le portanti che restano accese,
// e ogni portante ha il suo decoder (lo stesso di CwDecoder, fissato sul suo tono). Per ogni
// stazione si sa tono, velocita', fiducia, ultimo testo e — se l'ha detto — il nominativo.
//
// Ispirato allo skimmer multi-stazione di iu8lmc/Morsecodium; qui gira sull'audio stesso del
// decoder e si appoggia a ggmorse, quindi non serve nessun modello.
#pragma once

#include "core/CwDecoder.h"

#include <QString>
#include <QVector>

#include <complex>
#include <memory>
#include <vector>

namespace decolog::core {

class CwSkimmer {
public:
    static constexpr int kMaxStations = 5;
    // I punti dello spettro che si danno alla finestra, da lowHz a highHz.
    static constexpr int kSpectrumPoints = 160;

    struct Station {
        int id{0};
        double freq{0};         // Hz, nell'audio
        double level{0};        // dB sopra il rumore di fondo
        int wpm{0};
        float confidence{0};    // da 0 a 1
        QString text;           // gli ultimi caratteri decodificati
        QString call;           // il nominativo, se la stazione l'ha detto
        bool cq{false};         // sta chiamando CQ
        double idleSeconds{0};  // da quanti secondi non esce un carattere
        bool present{false};    // la portante c'e' adesso
    };

    explicit CwSkimmer(int sampleRate = 8000);
    ~CwSkimmer();

    CwSkimmer(const CwSkimmer&) = delete;
    CwSkimmer& operator=(const CwSkimmer&) = delete;

    void setSampleRate(int sampleRate);
    // La parte di audio dove si cercano le stazioni: dove sta il CW di chiunque.
    void setBand(int lowHz, int highHz);
    int lowHz() const { return m_low; }
    int highHz() const { return m_high; }
    void setLexicon(bool on);
    void reset();

    // L'audio (mono, 16 bit), a pezzi come arriva dalla scheda.
    void feed(const qint16* samples, int count);

    QVector<Station> stations() const;
    // Lo spettro della banda, da 0 a 1 (rispetto al rumore di fondo), kSpectrumPoints punti.
    const QVector<float>& spectrum() const { return m_view; }
    // Vero se da ultima chiamata e' cambiato qualcosa che vale la pena mostrare.
    bool takeChanged();

private:
    struct Slot {
        int id{0};
        double freq{0};
        double level{0};
        qint64 lastSeen{0};     // in battiti di analisi
        qint64 lastChar{0};     // in campioni
        qint64 opened{0};
        bool present{false};
        QString text;
        QString call;
        bool cq{false};
        std::unique_ptr<CwDecoder> decoder;
    };
    struct Candidate {
        double freq{0};
        double level{0};
        int hits{0};
        int misses{0};
    };

    void rebuild();
    void analyse();
    void open(const Candidate& c);
    void refreshCall(Slot& s) const;

    int m_rate{8000};
    int m_low{300};
    int m_high{1450};
    bool m_lexicon{true};

    int m_fftSize{2048};
    int m_hop{2000};
    std::vector<float> m_window;
    std::vector<std::complex<float>> m_twiddle;
    std::vector<float> m_ring;      // gli ultimi m_fftSize campioni
    qsizetype m_ringAt{0};
    qint64 m_total{0};              // campioni ricevuti dall'ultimo reset
    qint64 m_sinceHop{0};
    qint64 m_ticks{0};
    std::vector<float> m_power;     // spettro medio, tutte le righe fino a Nyquist
    std::vector<qint16> m_history;  // gli ultimi secondi, per far partire in fretta un nuovo decoder
    qsizetype m_historyAt{0};
    bool m_historyFull{false};

    QVector<Candidate> m_candidates;
    std::vector<std::unique_ptr<Slot>> m_slots;
    QVector<float> m_view;
    int m_nextId{1};
    bool m_changed{false};
};

} // namespace decolog::core
