// Portato da iu8lmc/Morsecodium (src/decode/), decodificatore CW neurale CTC. Attivo solo con DECODXLOG_CTC_ONNX.
#pragma once
#include <QString>
#include <vector>
#include <memory>

// DECODER END-TO-END (CTC) — nessun timing, nessuna soglia.
// Riceve l'AUDIO RAW + il tono della portante, calcola in streaming la stessa
// striscia spettrale dell'orecchio (8 kHz, Hann 512, hop 40 -> 200 Hz, 27 bin,
// log1p, norm mediana/p99) e la passa a finestre a un CRNN+CTC (ONNX) che
// emette DIRETTAMENTE i caratteri: la rete ha imparato ritmo, weighting, QSB
// e QRM dal dataset, come un operatore umano. Lo stitching tra finestre
// avviene sul tempo (frame) di ogni carattere: si emette solo la parte
// "stabile" della finestra, il resto viene rivalutato alla successiva.
class CtcDecoder {
public:
    CtcDecoder();
    ~CtcDecoder();

    static bool isCompiled();
    bool loadModel(const QString &path);
    bool isLoaded() const { return loaded_; }
    QString name() const;

    // Soglia di confidenza per emettere un elemento (anti-allucinazioni sul
    // rumore): piu' alta = piu' silenzioso sulla banda vuota, a rischio di
    // perdere elementi deboli. Regolabile via INI ("decoder/ctcMinProb").
    void setMinProb(double p) { minProb_ = qBound(0.0, p, 0.95); }

    void reset();
    double wpm() const { return wpm_; }   // stima dal rate dei caratteri emessi
    QString takeMorse();                  // dit/dah emessi, per lo scope CW

    // Confidenza 0..1 del modello sugli ultimi elementi decodificati: quanto
    // la rete e' "sicura" di cio' che scrive (probabilita' dei picchi CTC,
    // informazione interna che il greedy da solo butterebbe via).
    double confidence() const { return conf_; }

    // Probe sullo stato interno (tools/probe_wpm.py): grandezze che la rete
    // possiede ma non scrive, rese riportabili da teste lineari congelate.
    double snrNet() const { return snrNet_; }         // SNR percepito (dB)
    double qrmNet() const { return qrmNet_; }         // QRM percepito 0..1
    double fistWeight() const { return fistWeight_; } // pugno: rapporto dah/dit
    double fistJitter() const { return fistJitter_; } // pugno: jitter 0..~0.15

    // Elabora n campioni mono a frequenza fsIn, con portante toneHz.
    void processAudio(const float *mono, int n, double fsIn, double toneHz, QString &out);

private:
    void retune(double toneHz);
    void resampleTo8k(const float *mono, int n);   // con anti-alias in testa
    void computeFrames();
    // finestra -> ONNX -> beam/greedy CTC -> testo. force salta il gate
    // MIN_EMIT (usato per spremere la striscia prima di invalidarla al retune).
    void infer(QString &out, bool force = false);

    struct Impl;
    std::unique_ptr<Impl> d_;
    bool loaded_ = false;
    // prefix beam search al posto del greedy argmax: il greedy per-frame
    // scarta in silenzio gli elementi deboli e mutila i simboli; il beam fa
    // competere le ipotesi complete. INI "decoder/ctcBeam" (letto una volta).
    bool useBeam_ = true;
    double wpm_ = 0.0;
    double conf_ = 0.0;
    double minProb_ = 0.50;
    double snrNet_ = 0.0, qrmNet_ = 0.0, fistWeight_ = 0.0, fistJitter_ = 0.0;
};
