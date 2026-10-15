// Portato da iu8lmc/Morsecodium (src/decode/), decodificatore CW neurale CTC. Attivo solo con DECODXLOG_CTC_ONNX.
#include "core/CtcDecoder.h"
#include "core/MorseTables.h"
#include <QtGlobal>
#include <QSettings>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <array>
#include <string>

#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

namespace {
constexpr int    FS8K  = 8000;
constexpr int    NFFT  = 512;
constexpr int    HOP   = 40;              // -> 200 Hz frame (come il training)
constexpr int    BAND  = 13;
constexpr int    FBINS = 2 * BAND + 1;    // 27
constexpr double BINHZ = double(FS8K) / NFFT;
constexpr int    WLEN   = 1024;           // finestra di inferenza (~5.1 s)
constexpr int    KEEPFR = WLEN + 128;     // frame di striscia tenuti
constexpr double PI = 3.14159265358979323846;
#ifdef HAVE_ONNXRUNTIME
constexpr int    TIME_DS = 2;             // downsampling temporale della CNN (->100 Hz)
constexpr int    TAIL   = 160;            // coda instabile non emessa (~0.8 s)
constexpr int    MIN_EMIT = 100;          // nuova inferenza ogni ~0.5 s di frame
constexpr int    FLUSH_SIL = 400;         // silenzio (frame) che chiude l'ultimo carattere
// classi CTC per-ELEMENTO del training (train_ctc.py):
// 0=blank, 1=dit, 2=dah, 3=gap-carattere, 4=gap-parola.
// La conversione elementi -> lettere la fa la tabella Morse (decodeFuzzy).
constexpr int kDit = 1, kDah = 2, kCharGap = 3, kWordGap = 4;
#endif

// ---- ANTI-ALIAS PRIMA DELLA DECIMAZIONE A 8 kHz ---------------------------
// L'interpolazione lineare da sola NON filtra: il rumore sopra i 4 kHz si
// ripiega in banda e sporca la striscia spettrale che la rete deve leggere.
// Passa-basso Butterworth di 6o ordine (3 biquad in cascata, Direct Form II
// trasposta) con taglio a ~3.4 kHz, applicato in streaming sul segnale in
// ingresso: lo stato (z1/z2) persiste tra i chunk, si azzera in reset().
// A 48 kHz l'attenuazione oltre i 7 kHz (cio' che si ripiega nella banda
// della striscia) supera i 35 dB; la banda utile 200-1000 Hz resta piatta.
constexpr double AA_FC  = 3400.0;  // frequenza di taglio del passa-basso
constexpr int    AA_NBQ = 3;       // biquad in cascata (1=2o, 2=4o, 3=6o ordine)

struct AntiAliasLp {
    struct Biq { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0; };
    std::array<Biq, 3> st;
    double fsCur = 0.0;    // frequenza per cui i coefficienti sono calcolati
    bool   active = false; // serve solo quando si decima (fsIn > 8 kHz)

    void design(double fs)
    {
        if (fs == fsCur) return;                 // gia' progettato per questa fs
        fsCur = fs;
        active = fs > double(FS8K) + 1.0 && AA_NBQ > 0;
        reset();
        if (!active) return;
        // Q dei biquad Butterworth per ordine 2/4/6 (poli a coppie coniugate)
        static const double QQ[3][3] = {
            {0.70710678, 0.0,        0.0       },
            {0.54119610, 1.30656296, 0.0       },
            {0.51763809, 0.70710678, 1.93185165}};
        const double w0 = 2.0 * PI * AA_FC / fs;
        const double cw = std::cos(w0), sw = std::sin(w0);
        for (int i = 0; i < AA_NBQ; ++i) {
            const double alpha = sw / (2.0 * QQ[AA_NBQ - 1][i]);
            const double a0 = 1.0 + alpha;
            Biq &b = st[i];
            b.b0 = (1.0 - cw) * 0.5 / a0;
            b.b1 = (1.0 - cw) / a0;
            b.b2 = b.b0;
            b.a1 = -2.0 * cw / a0;
            b.a2 = (1.0 - alpha) / a0;
        }
    }
    void reset() { for (auto &b : st) b.z1 = b.z2 = 0.0; }
    float step(float x)
    {
        if (!active) return x;
        double v = x;
        for (int i = 0; i < AA_NBQ; ++i) {
            Biq &b = st[i];
            const double y = b.b0 * v + b.z1;
            b.z1 = b.b1 * v - b.a1 * y + b.z2;
            b.z2 = b.b2 * v - b.a2 * y;
            v = y;
        }
        return float(v);
    }
};

#ifdef HAVE_ONNXRUNTIME
// ---- PREFIX BEAM SEARCH CTC ------------------------------------------------
// Il greedy argmax decide frame per frame: un dit affogato nel QSB sparisce
// in silenzio e il carattere si trasforma in un ALTRO carattere valido.
// Il prefix beam search tiene le BEAMW ipotesi (prefissi di elementi) piu'
// probabili e le fa competere sulle probabilita' di TUTTI i frame della
// finestra: l'ipotesi col dit debole puo' vincere se il resto la conferma.
constexpr int   BEAMW   = 12;      // ipotesi mantenute (5 classi: basta poco)
constexpr float LOGZERO = -1e30f;  // log(0) operativo

// log(exp(a)+exp(b)) numericamente stabile
inline float logAdd(float a, float b)
{
    if (a <= LOGZERO * 0.5f) return b;
    if (b <= LOGZERO * 0.5f) return a;
    const float m = a > b ? a : b;
    return m + std::log1p(std::exp(-std::abs(a - b)));
}

struct BeamHyp {
    std::vector<unsigned char> labels;  // prefisso (elementi non-blank)
    std::vector<int>           times;   // frame di nascita di ogni elemento
    float pb  = LOGZERO;                // log-P(prefisso, ultimo frame blank)
    float pnb = LOGZERO;                // log-P(prefisso, ultimo frame label)
    float score() const { return logAdd(pb, pnb); }
};

// Decodifica l'intera finestra [T x C] di log-prob e restituisce il miglior
// prefisso con il frame di nascita di ogni elemento (serve allo stitching
// temporale tra finestre, identico al percorso greedy).
void ctcBeamSearch(const float *logp, int T, int C,
                   std::vector<unsigned char> &labels, std::vector<int> &times)
{
    std::vector<BeamHyp> cur(1), nxt;
    cur[0].pb = 0.0f;                          // prefisso vuoto, prob 1
    nxt.reserve(std::size_t(BEAMW) * (C + 1));
    std::vector<unsigned char> ext;            // scratch: prefisso esteso
    for (int t = 0; t < T; ++t) {
        const float *row = logp + std::size_t(t) * C;
        nxt.clear();
        // cerca il prefisso tra i candidati del frame; i genitori arrivano in
        // ordine di score decrescente, quindi chi crea lo slot detta i tempi
        auto slot = [&nxt](const std::vector<unsigned char> &lab) -> BeamHyp * {
            for (auto &h : nxt)
                if (h.labels == lab) return &h;
            return nullptr;
        };
        for (const auto &h : cur) {
            const float tot = logAdd(h.pb, h.pnb);
            // 1) blank e 2) ripetizione dell'ultima label: prefisso invariato
            BeamHyp *s = slot(h.labels);
            if (!s) {
                nxt.emplace_back();
                s = &nxt.back();
                s->labels = h.labels;
                s->times  = h.times;
            }
            s->pb = logAdd(s->pb, row[0] + tot);
            if (!h.labels.empty())
                s->pnb = logAdd(s->pnb, row[h.labels.back()] + h.pnb);
            // 3) estensione con una nuova label c (regola CTC: la stessa
            //    label subito dopo se stessa richiede un blank in mezzo,
            //    quindi in quel caso contribuisce solo pb)
            for (int c = 1; c < C; ++c) {
                const bool same = !h.labels.empty() && h.labels.back() == c;
                const float p = row[c] + (same ? h.pb : tot);
                if (p <= LOGZERO * 0.5f) continue;
                ext = h.labels;
                ext.push_back((unsigned char)c);
                BeamHyp *e = slot(ext);
                if (!e) {
                    nxt.emplace_back();
                    e = &nxt.back();
                    e->labels = ext;
                    e->times  = h.times;
                    e->times.push_back(t);
                }
                e->pnb = logAdd(e->pnb, p);
            }
        }
        std::sort(nxt.begin(), nxt.end(), [](const BeamHyp &a, const BeamHyp &b) {
            return a.score() > b.score();
        });
        if (int(nxt.size()) > BEAMW) nxt.resize(BEAMW);
        cur.swap(nxt);
    }
    const BeamHyp *best = &cur.front();
    for (const auto &h : cur)
        if (h.score() > best->score()) best = &h;
    labels = best->labels;
    times  = best->times;
}
#endif
}

struct CtcDecoder::Impl {
    // ricampionamento streaming -> 8 kHz
    double fsIn = 48000.0, step = 6.0, srcPos = 0.0;
    AntiAliasLp aa;                       // passa-basso prima della decimazione
    std::vector<float> inBuf;  long inAbs0 = 0;
    // ring a 8 kHz
    std::vector<float> ring8k;  long ring8kAbs0 = 0, total8k = 0;
    // tabelle DFT
    std::vector<float> cosT, sinT, hann;  int cbin = -1;
    // striscia (log1p) [frame x FBINS], indicizzazione assoluta
    std::vector<float> strip;  long bufStartFrame = 0, framesComputed = 0;
    // stitching CTC: emesso tutto cio' che inizia prima di questo frame assoluto
    long emittedUntil = 0, lastInferFrame = 0;
    // carattere in costruzione (elementi dit/dah) + stream per lo scope
    QString symbol, morse;
    long lastElemFrame = 0;               // ultimo elemento visto (flush su silenzio)
    // stima WPM: frame assoluti dei caratteri emessi (finestra mobile 10 s)
    std::vector<long> charFrames;
    // scratch
    std::vector<float> win;
    std::vector<unsigned char> beamLab;   // uscita del beam search
    std::vector<int> beamTime;
#ifdef HAVE_ONNXRUNTIME
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "MorseAIctc"};
    std::unique_ptr<Ort::Session> session;
    Ort::AllocatorWithDefaultOptions alloc;
    std::string inName, outName, outWpmName, outMetaName;
    bool hasWpmHead = false;    // il modello espone il probe WPM (2o output)
    bool hasMetaHead = false;   // il modello espone i probe meta (3o output)
#endif
};

CtcDecoder::CtcDecoder() : d_(std::make_unique<Impl>())
{
    d_->hann.resize(NFFT);
    for (int n = 0; n < NFFT; ++n)
        d_->hann[n] = float(0.5 - 0.5 * std::cos(2.0 * PI * n / (NFFT - 1)));
    // beam search CTC (prefix beam) al posto del greedy argmax: letto una
    // volta dall'INI; il greedy resta come fallback ("decoder/ctcBeam").
    useBeam_ = QSettings().value(QStringLiteral("decoder/ctcBeam"), true).toBool();
}
CtcDecoder::~CtcDecoder() = default;

bool CtcDecoder::isCompiled()
{
#ifdef HAVE_ONNXRUNTIME
    return true;
#else
    return false;
#endif
}

QString CtcDecoder::name() const
{
#ifdef HAVE_ONNXRUNTIME
    return loaded_ ? QStringLiteral("AI end-to-end (CTC)")
                   : QStringLiteral("AI end-to-end (nessun modello)");
#else
    return QStringLiteral("AI end-to-end (non compilato)");
#endif
}

bool CtcDecoder::loadModel(const QString &path)
{
    loaded_ = false;
#ifdef HAVE_ONNXRUNTIME
    try {
        Ort::SessionOptions opt;
        opt.SetIntraOpNumThreads(1);
        opt.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        opt.DisableCpuMemArena();
        const std::wstring wp = path.toStdWString();
        d_->session = std::make_unique<Ort::Session>(d_->env, wp.c_str(), opt);
        d_->inName  = d_->session->GetInputNameAllocated(0, d_->alloc).get();
        d_->outName = d_->session->GetOutputNameAllocated(0, d_->alloc).get();
        d_->hasWpmHead = d_->session->GetOutputCount() > 1;
        if (d_->hasWpmHead)
            d_->outWpmName = d_->session->GetOutputNameAllocated(1, d_->alloc).get();
        d_->hasMetaHead = d_->session->GetOutputCount() > 2;
        if (d_->hasMetaHead)
            d_->outMetaName = d_->session->GetOutputNameAllocated(2, d_->alloc).get();
        loaded_ = true;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "CtcDecoder load: %s\n", e.what());
        loaded_ = false;
    }
#else
    Q_UNUSED(path);
#endif
    reset();
    return loaded_;
}

void CtcDecoder::reset()
{
    auto &I = *d_;
    I.srcPos = 0.0; I.inBuf.clear(); I.inAbs0 = 0;
    I.aa.reset();
    I.ring8k.clear(); I.ring8kAbs0 = 0; I.total8k = 0;
    I.strip.clear(); I.bufStartFrame = 0; I.framesComputed = 0;
    I.emittedUntil = 0; I.lastInferFrame = 0;
    I.symbol.clear(); I.morse.clear(); I.lastElemFrame = 0;
    I.charFrames.clear();
    wpm_ = 0.0;
    conf_ = 0.0;
    snrNet_ = 0.0; qrmNet_ = 0.0; fistWeight_ = 0.0; fistJitter_ = 0.0;
}

QString CtcDecoder::takeMorse()
{
    QString m = d_->morse;
    d_->morse.clear();
    return m;
}

void CtcDecoder::retune(double toneHz)
{
    const int cb = int(std::lround(toneHz / BINHZ));
    if (cb == d_->cbin) return;
    d_->cbin = cb;
    d_->cosT.assign(std::size_t(FBINS) * NFFT, 0.0f);
    d_->sinT.assign(std::size_t(FBINS) * NFFT, 0.0f);
    for (int b = 0; b < FBINS; ++b) {
        const int k = cb - BAND + b;
        const double w = 2.0 * PI * k / NFFT;
        for (int s = 0; s < NFFT; ++s) {
            d_->cosT[std::size_t(b) * NFFT + s] = float(std::cos(w * s));
            d_->sinT[std::size_t(b) * NFFT + s] = float(std::sin(w * s));
        }
    }
}

void CtcDecoder::resampleTo8k(const float *mono, int n)
{
    auto &I = *d_;
    // anti-alias PRIMA della decimazione: l'interpolazione lineare campiona
    // il segnale gia' filtrato, cosi' nulla sopra i ~4 kHz si ripiega in banda
    I.inBuf.reserve(I.inBuf.size() + std::size_t(n));
    for (int i = 0; i < n; ++i)
        I.inBuf.push_back(I.aa.step(mono[i]));
    const long inEndAbs = I.inAbs0 + long(I.inBuf.size());
    while (true) {
        const long i0 = long(std::floor(I.srcPos));
        const long i1 = i0 + 1;
        if (i1 >= inEndAbs) break;
        if (i0 < I.inAbs0) { I.srcPos += I.step; continue; }
        const double t = I.srcPos - double(i0);
        const float a = I.inBuf[std::size_t(i0 - I.inAbs0)];
        const float b = I.inBuf[std::size_t(i1 - I.inAbs0)];
        I.ring8k.push_back(float((1.0 - t) * a + t * b));
        ++I.total8k;
        I.srcPos += I.step;
    }
    const long keepFrom = long(std::floor(I.srcPos)) - 1;
    if (keepFrom > I.inAbs0) {
        long drop = std::min(keepFrom - I.inAbs0, long(I.inBuf.size()));
        if (drop > 0) { I.inBuf.erase(I.inBuf.begin(), I.inBuf.begin() + drop); I.inAbs0 += drop; }
    }
}

void CtcDecoder::computeFrames()
{
    auto &I = *d_;
    while (I.framesComputed * HOP + NFFT <= I.total8k) {
        const long start8 = I.framesComputed * HOP;
        if (start8 < I.ring8kAbs0) { ++I.framesComputed; continue; }
        const long ri = start8 - I.ring8kAbs0;
        for (int b = 0; b < FBINS; ++b) {
            const float *ct = &I.cosT[std::size_t(b) * NFFT];
            const float *st = &I.sinT[std::size_t(b) * NFFT];
            double re = 0.0, im = 0.0;
            for (int s = 0; s < NFFT; ++s) {
                const double xv = double(I.ring8k[std::size_t(ri + s)]) * I.hann[s];
                re += xv * ct[s];
                im -= xv * st[s];
            }
            I.strip.push_back(float(std::log1p(std::sqrt(re * re + im * im))));
        }
        ++I.framesComputed;
    }
    const long need8 = I.framesComputed * HOP;
    if (need8 > I.ring8kAbs0) {
        long drop = std::min(need8 - I.ring8kAbs0, long(I.ring8k.size()));
        if (drop > 0) { I.ring8k.erase(I.ring8k.begin(), I.ring8k.begin() + drop); I.ring8kAbs0 += drop; }
    }
    const long have = I.framesComputed - I.bufStartFrame;
    if (have > KEEPFR) {
        const long drop = have - KEEPFR;
        I.strip.erase(I.strip.begin(), I.strip.begin() + drop * FBINS);
        I.bufStartFrame += drop;
    }
}

void CtcDecoder::infer(QString &out, bool force)
{
#ifndef HAVE_ONNXRUNTIME
    Q_UNUSED(force);
#endif
#ifdef HAVE_ONNXRUNTIME
    if (!loaded_ || !d_->session) return;
    auto &I = *d_;
    if (!force && I.framesComputed - I.lastInferFrame < MIN_EMIT) return;
    const long winEnd = I.framesComputed;
    const long winStart = std::max(I.bufStartFrame, winEnd - long(WLEN));
    int winLen = int(winEnd - winStart);
    winLen -= winLen % TIME_DS;                 // multiplo del downsampling CNN
    if (winLen < 16 * TIME_DS) return;
    I.lastInferFrame = I.framesComputed;

    // copia finestra + normalizzazione per-finestra (mediana / p99, come in training)
    const std::size_t cnt = std::size_t(winLen) * FBINS;
    I.win.assign(I.strip.begin() + (winStart - I.bufStartFrame) * FBINS,
                 I.strip.begin() + (winStart - I.bufStartFrame) * FBINS + long(cnt));
    std::vector<float> tmp(I.win);
    std::nth_element(tmp.begin(), tmp.begin() + cnt / 2, tmp.end());
    const float med = tmp[cnt / 2];
    const std::size_t pi = std::size_t(0.99 * (cnt - 1));
    std::nth_element(tmp.begin(), tmp.begin() + pi, tmp.end());
    const float p99 = tmp[pi];
    const float scale = 1.0f / (p99 - med + 1e-6f);
    for (auto &v : I.win) v = std::clamp((v - med) * scale, -1.0f, 3.0f);

    try {
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<int64_t, 3> shape = {1, winLen, FBINS};
        const char *inN[]  = {I.inName.c_str()};
        const char *outN[] = {I.outName.c_str()};
        const char *outN3[] = {I.outName.c_str(), I.outWpmName.c_str(),
                               I.outMetaName.c_str()};
        const size_t nOut = I.hasMetaHead ? 3 : (I.hasWpmHead ? 2 : 1);
        Ort::Value input = Ort::Value::CreateTensor<float>(mem, I.win.data(), I.win.size(),
                                                           shape.data(), shape.size());
        auto res = I.session->Run(Ort::RunOptions{nullptr}, inN, &input, 1,
                                  nOut > 1 ? outN3 : outN, nOut);
        const auto info = res.front().GetTensorTypeAndShapeInfo();
        const auto oshape = info.GetShape();          // [1, T', C]
        if (oshape.size() != 3) return;
        const int Tds = int(oshape[1]);
        const int C   = int(oshape[2]);
        const float *logp = res.front().GetTensorData<float>();
        const float *wph = (I.hasWpmHead && res.size() > 1)
                               ? res[1].GetTensorData<float>() : nullptr;
        const float *mth = (I.hasMetaHead && res.size() > 2)
                               ? res[2].GetTensorData<float>() : nullptr;
        double wpmSum = 0.0; int wpmN = 0;
        double metaSum[4] = {0.0, 0.0, 0.0, 0.0};

        // Decodifica CTC con tempo: collassa le ripetizioni, salta il blank,
        // accumula gli ELEMENTI nel carattere corrente e chiude sul gap.
        // Lo stitching usa il frame assoluto d'inizio di ogni elemento: si
        // emette solo la parte stabile della finestra, il resto si rivaluta.
        long stableEnd = winEnd - long(TAIL);         // oltre: coda instabile
        // CONFINE ALLINEATO AL BLANK: la nascita di un elemento jittera di
        // 1-2 frame tra finestre consecutive (il contesto della CNN cambia);
        // un taglio rigido che cade sul fronte di un elemento lo PERDE (in
        // questa finestra e' coda instabile, nella prossima risulta "gia'
        // emesso") o lo duplica. Il taglio viene quindi arretrato al centro
        // dell'ultima zona di blank prima di winEnd-TAIL, cosi' nessun
        // elemento cade mai a cavallo del confine.
        {
            auto amaxAt = [&](int t) {
                const float *row = logp + std::size_t(t) * C;
                int k = 0; float best = row[0];
                for (int c = 1; c < C; ++c)
                    if (row[c] > best) { best = row[c]; k = c; }
                return k;
            };
            const int tCut = int((stableEnd - winStart) / TIME_DS);
            if (tCut > 0 && tCut <= Tds) {
                int b1 = -1;
                const int tLo = std::max(0, tCut - 100);   // ~2 s di ricerca
                for (int t = std::min(tCut, Tds) - 1; t >= tLo; --t)
                    if (amaxAt(t) == 0) { b1 = t; break; }
                if (b1 >= 0) {
                    int b0 = b1;
                    while (b0 > tLo && amaxAt(b0 - 1) == 0) --b0;
                    stableEnd = winStart + long((b0 + b1 + 1) / 2) * TIME_DS;
                }
            }
        }
        auto flushChar = [&](long frameAbs) {
            if (I.symbol.isEmpty()) return;
            out += morse::decodeFuzzy(I.symbol);
            I.symbol.clear();
            I.charFrames.push_back(frameAbs);
        };
        // Emissione di un elemento (comune a beam e greedy): stessa semantica
        // streaming di sempre: solo la parte nuova e stabile della finestra.
        auto emitElem = [&](int k, long frameAbs, double p) {
            if (frameAbs < I.emittedUntil || frameAbs >= stableEnd) return;
            // confidenza: EMA su TUTTI gli elementi nuovi visti, anche
            // quelli sotto soglia (gli scarti abbassano la fiducia)
            conf_ = conf_ > 0.0 ? 0.9 * conf_ + 0.1 * p : p;
            if (p < minProb_) return;
            switch (k) {
            case kDit:     I.symbol += QLatin1Char('.'); I.morse += QLatin1Char('.'); break;
            case kDah:     I.symbol += QLatin1Char('-'); I.morse += QLatin1Char('-'); break;
            case kCharGap: flushChar(frameAbs); I.morse += QLatin1Char(' '); break;
            case kWordGap: flushChar(frameAbs); out += QLatin1Char(' ');
                           I.morse += QLatin1Char('/'); break;
            }
            I.lastElemFrame = frameAbs;
            if (I.morse.size() > 400) I.morse = I.morse.right(400);
        };
        // probe WPM/meta: media sui frame di segnale (argmax != blank),
        // identica per entrambi i percorsi di decodifica
        if (wph) {
            for (int t = 0; t < Tds; ++t) {
                const float *row = logp + std::size_t(t) * C;
                int k = 0; float best = row[0];
                for (int c = 1; c < C; ++c)
                    if (row[c] > best) { best = row[c]; k = c; }
                if (k == 0) continue;
                wpmSum += wph[t]; ++wpmN;
                if (mth)
                    for (int j = 0; j < 4; ++j)
                        metaSum[j] += mth[std::size_t(t) * 4 + j];
            }
        }
        if (useBeam_) {
            // PREFIX BEAM SEARCH sull'intera finestra: le ipotesi competono
            // sulle probabilita' complete, non sul solo argmax per frame.
            ctcBeamSearch(logp, Tds, C, I.beamLab, I.beamTime);
            for (std::size_t i = 0; i < I.beamLab.size(); ++i) {
                const int k = I.beamLab[i];
                const int t = I.beamTime[i];
                // per soglia/confidenza: posteriore di PICCO della label nei
                // primi frame della sua occupazione (il frame di nascita puo'
                // cadere sul fronte, il picco arriva 1-3 frame dopo)
                float lp = LOGZERO;
                const int tEnd = std::min(t + 4, Tds);
                for (int tt = t; tt < tEnd; ++tt)
                    lp = std::max(lp, logp[std::size_t(tt) * C + k]);
                emitElem(k, winStart + long(t) * TIME_DS, std::exp(double(lp)));
            }
        } else {
            // GREEDY argmax per frame (fallback): un evento a ogni transizione
            int prev = 0;
            for (int t = 0; t < Tds; ++t) {
                const float *row = logp + std::size_t(t) * C;
                int k = 0; float best = row[0];
                for (int c = 1; c < C; ++c)
                    if (row[c] > best) { best = row[c]; k = c; }
                if (k != prev && k != 0)
                    emitElem(k, winStart + long(t) * TIME_DS, std::exp(double(best)));
                prev = k;
            }
        }
        I.emittedUntil = std::max(I.emittedUntil, stableEnd);
        // fine trasmissione: il target non ha un gap finale, quindi l'ultimo
        // carattere va chiuso dopo un tratto di silenzio
        if (!I.symbol.isEmpty() && I.framesComputed - I.lastElemFrame > FLUSH_SIL)
            flushChar(I.lastElemFrame);

        if (wph && wpmN >= 8) {
            // WPM dal PROBE sullo stato interno della rete (tools/probe_wpm.py):
            // informazione che il modello possiede per decodificare ma che il
            // solo testo non riporta. Molto piu' accurata del conteggio caratteri.
            const double w = 40.0 * wpmSum / wpmN;
            wpm_ = wpm_ > 0.0 ? 0.7 * wpm_ + 0.3 * w : w;
            if (mth) {
                // probe meta: SNR, QRM, pugno (weighting + jitter) percepiti
                auto ema = [&](double &v, double x) {
                    v = v > 0.0 ? 0.7 * v + 0.3 * x : x;
                };
                ema(snrNet_,     25.0 * metaSum[0] / wpmN);
                ema(qrmNet_,      1.0 * metaSum[1] / wpmN);
                ema(fistWeight_,  5.0 * metaSum[2] / wpmN);
                ema(fistJitter_,  0.2 * metaSum[3] / wpmN);
            }
        } else if (!I.hasWpmHead) {
            // fallback per modelli senza probe: rate dei caratteri (PARIS=5)
            const long horizon = I.framesComputed - 2000;   // 10 s a 200 Hz
            I.charFrames.erase(std::remove_if(I.charFrames.begin(), I.charFrames.end(),
                                              [horizon](long f) { return f < horizon; }),
                               I.charFrames.end());
            wpm_ = double(I.charFrames.size()) * 1.2;       // (n/5) * (60/10)
        }
    } catch (const std::exception &e) {
        std::fprintf(stderr, "CtcDecoder infer: %s\n", e.what());
        std::fflush(stderr);
    }
#else
    Q_UNUSED(out);
#endif
}

void CtcDecoder::processAudio(const float *mono, int n, double fsIn,
                              double toneHz, QString &out)
{
    if (n <= 0) return;
    if (std::abs(fsIn - d_->fsIn) > 1.0) { d_->fsIn = fsIn; d_->step = fsIn / FS8K; }
    if (d_->step <= 0.0) d_->step = fsIn / FS8K;
    d_->aa.design(d_->fsIn);   // no-op se la frequenza non e' cambiata
    // Salto del tono oltre 1 bin (auto-tono su una nuova stazione): la striscia
    // gia' accumulata e' centrata sul VECCHIO bin e mischiarla ai frame nuovi
    // darebbe alla rete una finestra incoerente. Si spreme prima il testo
    // stabile rimasto (ultima inferenza sulle vecchie tabelle), si chiude il
    // carattere in costruzione, poi si riparte con la striscia vuota.
    const int cb = int(std::lround(toneHz / BINHZ));
    if (d_->cbin >= 0 && std::abs(cb - d_->cbin) > 1) {
        infer(out, true);
        if (!d_->symbol.isEmpty()) {           // chiudi il carattere pendente
            out += morse::decodeFuzzy(d_->symbol);
            d_->symbol.clear();
            d_->charFrames.push_back(d_->framesComputed);
        }
        if (d_->lastElemFrame > 0) {
            // confine di parola: cio' che segue il salto e' un'altra stazione
            // (o la stessa ri-agganciata), non la coda della parola corrente
            out += QLatin1Char(' ');
            d_->morse += QLatin1Char('/');
        }
        d_->strip.clear();
        d_->bufStartFrame  = d_->framesComputed;
        d_->emittedUntil   = d_->framesComputed;
        d_->lastInferFrame = d_->framesComputed;
    }
    retune(toneHz);
    resampleTo8k(mono, n);
    computeFrames();
    infer(out);
}
