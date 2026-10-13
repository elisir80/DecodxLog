#include "core/CwSkimmer.h"

#include "core/CwCalls.h"

#include <algorithm>
#include <cmath>

namespace decolog::core {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Quanto deve stare sopra il rumore di fondo (la mediana della banda) una portante per contare.
constexpr float kPeakRatio = 14.0f;
// Due portanti piu' vicine di cosi' sono la stessa stazione (o una sola con i clic del tasto).
constexpr double kMinSeparationHz = 40.0;
// Quanti battiti (un quarto di secondo l'uno) di fila servono per aprire un decoder.
constexpr int kHitsToOpen = 5;
// Una stazione sparita resta in elenco per un po': un QSO ha pause fra un passaggio e l'altro.
constexpr qint64 kKeepTicks = 120;     // 30 s
constexpr qint64 kAbsentTicks = 10;    // dopo questi battiti senza portante non e' piu' «presente»
constexpr double kHistorySeconds = 4.0;
constexpr qsizetype kTextKeep = 240;

void fft(std::vector<std::complex<float>>& a, const std::vector<std::complex<float>>& twiddle)
{
    const size_t n = a.size();
    // Inversione dei bit.
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const size_t step = n / len;
        for (size_t i = 0; i < n; i += len) {
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<float> w = twiddle[k * step];
                const std::complex<float> u = a[i + k];
                const std::complex<float> v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
            }
        }
    }
}

} // namespace

CwSkimmer::CwSkimmer(int sampleRate)
{
    setSampleRate(sampleRate);
}

CwSkimmer::~CwSkimmer() = default;

void CwSkimmer::setSampleRate(int sampleRate)
{
    m_rate = sampleRate > 0 ? sampleRate : 8000;
    rebuild();
}

void CwSkimmer::setBand(int lowHz, int highHz)
{
    m_low = std::max(100, lowHz);
    m_high = std::max(m_low + 100, highHz);
    m_view = QVector<float>(kSpectrumPoints, 0.0f);
    m_candidates.clear();
}

void CwSkimmer::setLexicon(bool on)
{
    m_lexicon = on;
    for (auto& s : m_slots) {
        if (s->decoder)
            s->decoder->setLexicon(on);
    }
}

void CwSkimmer::reset()
{
    rebuild();
}

void CwSkimmer::rebuild()
{
    // Una finestra di circa due decimi di secondo: righe larghe 4-6 Hz, abbastanza per separare
    // due stazioni a 50 Hz e abbastanza corta da seguire un CW veloce.
    int n = 1;
    while (n < m_rate / 5)
        n <<= 1;
    m_fftSize = std::max(512, n);
    m_hop = std::max(256, m_rate / 4);
    m_window.assign(static_cast<size_t>(m_fftSize), 0.0f);
    for (int i = 0; i < m_fftSize; ++i)
        m_window[static_cast<size_t>(i)] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * kPi * i / (m_fftSize - 1)));
    m_twiddle.assign(static_cast<size_t>(m_fftSize / 2), {});
    for (int k = 0; k < m_fftSize / 2; ++k) {
        const double angle = -2.0 * kPi * k / m_fftSize;
        m_twiddle[static_cast<size_t>(k)] = {static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle))};
    }
    m_ring.assign(static_cast<size_t>(m_fftSize), 0.0f);
    m_ringAt = 0;
    m_total = 0;
    m_sinceHop = 0;
    m_ticks = 0;
    m_power.assign(static_cast<size_t>(m_fftSize / 2 + 1), 0.0f);
    m_history.assign(static_cast<size_t>(m_rate * kHistorySeconds), 0);
    m_historyAt = 0;
    m_historyFull = false;
    m_candidates.clear();
    m_slots.clear();
    m_view = QVector<float>(kSpectrumPoints, 0.0f);
    m_nextId = 1;
    m_changed = true;
}

void CwSkimmer::feed(const qint16* samples, int count)
{
    if (!samples || count <= 0)
        return;
    int done = 0;
    while (done < count) {
        const int take = static_cast<int>(std::min<qint64>(count - done, m_hop - m_sinceHop));
        for (int i = 0; i < take; ++i) {
            const qint16 s = samples[done + i];
            m_ring[static_cast<size_t>(m_ringAt)] = s / 32768.0f;
            m_ringAt = (m_ringAt + 1) % m_fftSize;
            m_history[static_cast<size_t>(m_historyAt)] = s;
            m_historyAt = (m_historyAt + 1) % static_cast<qsizetype>(m_history.size());
            if (m_historyAt == 0)
                m_historyFull = true;
        }
        m_total += take;
        m_sinceHop += take;

        // L'audio ai decoder gia' aperti.
        for (auto& slot : m_slots) {
            if (!slot->decoder)
                continue;
            const QString out = slot->decoder->feed(samples + done, take);
            const int retract = slot->decoder->takeRetract();
            if (retract > 0)
                slot->text.chop(retract);
            if (!out.isEmpty() || retract > 0) {
                slot->text += out;
                if (slot->text.size() > kTextKeep)
                    slot->text = slot->text.right(kTextKeep - 40);
                if (!out.isEmpty())
                    slot->lastChar = m_total;
                refreshCall(*slot);
                m_changed = true;
            }
        }
        done += take;
        if (m_sinceHop >= m_hop) {
            m_sinceHop = 0;
            analyse();
        }
    }
}

void CwSkimmer::refreshCall(Slot& s) const
{
    const double idle = static_cast<double>(m_total - s.lastChar) / m_rate;
    const cwcalls::Found f = cwcalls::find(s.text, idle > 2.0);
    if (f.call.isEmpty())
        return;
    s.call = f.call;
    s.cq = f.cq;
}

void CwSkimmer::analyse()
{
    ++m_ticks;
    // La finestra nell'ordine del tempo, con la sua curva.
    std::vector<std::complex<float>> buf(static_cast<size_t>(m_fftSize));
    for (int i = 0; i < m_fftSize; ++i) {
        const qsizetype at = (m_ringAt + i) % m_fftSize;
        buf[static_cast<size_t>(i)] = m_ring[static_cast<size_t>(at)] * m_window[static_cast<size_t>(i)];
    }
    fft(buf, m_twiddle);
    const bool first = m_ticks <= 1;
    for (size_t k = 0; k < m_power.size(); ++k) {
        const float p = std::norm(buf[k]);
        m_power[k] = first ? p : 0.65f * m_power[k] + 0.35f * p;
    }

    const double binHz = static_cast<double>(m_rate) / m_fftSize;
    const int kLow = std::max(2, static_cast<int>(std::ceil(m_low / binHz)));
    const int kHigh = std::min(static_cast<int>(m_power.size()) - 3, static_cast<int>(std::floor(m_high / binHz)));
    if (kHigh - kLow < 8)
        return;

    // Il rumore di fondo e' la mediana: qualche portante non la sposta.
    std::vector<float> band(m_power.begin() + kLow, m_power.begin() + kHigh + 1);
    std::nth_element(band.begin(), band.begin() + static_cast<long>(band.size() / 2), band.end());
    const float floor = std::max(band[band.size() / 2], 1e-12f);

    // Lo spettro per la finestra: il massimo di ogni gruppo di righe, in decibel sopra il fondo
    // (0 = fondo, 1 = 30 dB sopra).
    m_view.resize(kSpectrumPoints);
    for (int i = 0; i < kSpectrumPoints; ++i) {
        const int from = kLow + (kHigh - kLow + 1) * i / kSpectrumPoints;
        const int to = std::max(from + 1, kLow + (kHigh - kLow + 1) * (i + 1) / kSpectrumPoints);
        float peak = 0;
        for (int k = from; k < to && k <= kHigh; ++k)
            peak = std::max(peak, m_power[static_cast<size_t>(k)]);
        const float db = 10.0f * std::log10(std::max(peak / floor, 1.0f));
        m_view[i] = std::clamp(db / 30.0f, 0.0f, 1.0f);
    }

    // Le portanti: massimi locali sopra il fondo.
    struct Peak {
        double freq;
        double db;
        float power;
    };
    std::vector<Peak> peaks;
    const int guard = std::max(2, static_cast<int>(std::ceil(20.0 / binHz)));
    for (int k = kLow; k <= kHigh; ++k) {
        const float p = m_power[static_cast<size_t>(k)];
        if (p < kPeakRatio * floor)
            continue;
        bool top = true;
        for (int j = std::max(0, k - guard); j <= std::min<int>(static_cast<int>(m_power.size()) - 1, k + guard); ++j) {
            if (m_power[static_cast<size_t>(j)] > p) {
                top = false;
                break;
            }
        }
        if (!top)
            continue;
        // Interpolazione parabolica sul logaritmo: dice dove sta la portante fra due righe.
        const double a = std::log(std::max(m_power[static_cast<size_t>(k - 1)], 1e-12f));
        const double b = std::log(std::max(p, 1e-12f));
        const double c = std::log(std::max(m_power[static_cast<size_t>(k + 1)], 1e-12f));
        const double denom = a - 2.0 * b + c;
        const double shift = std::abs(denom) > 1e-9 ? 0.5 * (a - c) / denom : 0.0;
        peaks.push_back({(k + std::clamp(shift, -0.5, 0.5)) * binHz, 10.0 * std::log10(p / floor), p});
    }
    std::sort(peaks.begin(), peaks.end(), [](const Peak& x, const Peak& y) { return x.power > y.power; });
    std::vector<Peak> kept;
    for (const Peak& p : peaks) {
        const bool near = std::any_of(kept.begin(), kept.end(),
                                      [&](const Peak& q) { return std::abs(q.freq - p.freq) < kMinSeparationHz; });
        if (!near)
            kept.push_back(p);
        if (kept.size() >= 12)
            break;
    }

    // Le candidate: una portante deve restare per qualche battito prima che le si dia un decoder.
    std::vector<bool> used(kept.size(), false);
    for (Candidate& c : m_candidates) {
        int best = -1;
        double bestDist = 20.0;
        for (size_t i = 0; i < kept.size(); ++i) {
            const double d = std::abs(kept[i].freq - c.freq);
            if (!used[i] && d < bestDist) {
                bestDist = d;
                best = static_cast<int>(i);
            }
        }
        if (best >= 0) {
            used[static_cast<size_t>(best)] = true;
            c.freq = 0.5 * c.freq + 0.5 * kept[static_cast<size_t>(best)].freq;
            c.level = kept[static_cast<size_t>(best)].db;
            c.hits = std::min(c.hits + 1, 3 * kHitsToOpen);
            c.misses = 0;
        } else {
            c.hits = std::max(0, c.hits - 1);
            c.misses += 1;
        }
    }
    for (size_t i = 0; i < kept.size(); ++i) {
        if (!used[i])
            m_candidates.append({kept[i].freq, kept[i].db, 1, 0});
    }
    m_candidates.erase(std::remove_if(m_candidates.begin(), m_candidates.end(),
                                      [](const Candidate& c) { return c.hits == 0 && c.misses > 3; }),
                       m_candidates.end());

    // I decoder aperti seguono la loro portante.
    for (auto& slot : m_slots) {
        double bestDist = 30.0;
        const Peak* match = nullptr;
        for (const Peak& p : kept) {
            const double d = std::abs(p.freq - slot->freq);
            if (d < bestDist) {
                bestDist = d;
                match = &p;
            }
        }
        const bool was = slot->present;
        if (match) {
            slot->present = true;
            slot->lastSeen = m_ticks;
            slot->freq = 0.6 * slot->freq + 0.4 * match->freq;
            slot->level = match->db;
            if (slot->decoder && std::abs(slot->freq - slot->decoder->tone()) > 12.0)
                slot->decoder->setTone(static_cast<int>(std::lround(slot->freq)));
        } else if (m_ticks - slot->lastSeen > kAbsentTicks) {
            slot->present = false;
        }
        if (was != slot->present)
            m_changed = true;
    }
    const size_t before = m_slots.size();
    m_slots.erase(std::remove_if(m_slots.begin(), m_slots.end(),
                                 [&](const std::unique_ptr<Slot>& s) {
                                     return m_ticks - s->lastSeen > kKeepTicks;
                                 }),
                  m_slots.end());
    if (before != m_slots.size())
        m_changed = true;

    // Una portante nuova che e' rimasta: le si apre un decoder.
    for (const Candidate& c : m_candidates) {
        // Deve esserci adesso: una portante sparita non riapre un decoder.
        if (c.hits < kHitsToOpen || c.misses > 0)
            continue;
        const bool known = std::any_of(m_slots.begin(), m_slots.end(), [&](const std::unique_ptr<Slot>& s) {
            return std::abs(s->freq - c.freq) < kMinSeparationHz * 0.75;
        });
        if (!known)
            open(c);
    }
}

void CwSkimmer::open(const Candidate& c)
{
    if (static_cast<int>(m_slots.size()) >= kMaxStations) {
        // Pieno: si libera la stazione assente da piu' tempo, ma solo se e' davvero andata via.
        auto victim = m_slots.end();
        qint64 oldest = m_ticks;
        for (auto it = m_slots.begin(); it != m_slots.end(); ++it) {
            if (!(*it)->present && (*it)->lastSeen < oldest && m_ticks - (*it)->lastSeen > kAbsentTicks * 2) {
                oldest = (*it)->lastSeen;
                victim = it;
            }
        }
        if (victim == m_slots.end())
            return;
        m_slots.erase(victim);
    }
    auto slot = std::make_unique<Slot>();
    slot->id = m_nextId++;
    slot->freq = c.freq;
    slot->level = c.level;
    slot->lastSeen = m_ticks;
    slot->opened = m_ticks;
    slot->present = true;
    slot->lastChar = m_total;
    slot->decoder = std::make_unique<CwDecoder>(m_rate);
    slot->decoder->setLexicon(m_lexicon);
    slot->decoder->setTone(static_cast<int>(std::lround(c.freq)));
    // Ogni stazione ascolta solo il suo tono: le vicine non devono entrare nel suo decoder.
    slot->decoder->setNarrowFilter(true);

    // Gli ultimi secondi di audio, perche' il decoder impari subito velocita' e ritmo.
    const size_t size = m_history.size();
    const size_t have = m_historyFull ? size : static_cast<size_t>(m_historyAt);
    if (have > 0) {
        std::vector<qint16> replay(have);
        const size_t start = m_historyFull ? static_cast<size_t>(m_historyAt) : 0;
        for (size_t i = 0; i < have; ++i)
            replay[i] = m_history[(start + i) % size];
        const size_t chunk = 1024;
        for (size_t at = 0; at < replay.size(); at += chunk) {
            const QString out = slot->decoder->feed(replay.data() + at, static_cast<int>(std::min(chunk, replay.size() - at)));
            const int retract = slot->decoder->takeRetract();
            if (retract > 0)
                slot->text.chop(retract);
            slot->text += out;
        }
        if (!slot->text.isEmpty())
            slot->lastChar = m_total;
    }
    refreshCall(*slot);
    m_slots.push_back(std::move(slot));
    m_changed = true;
}

QVector<CwSkimmer::Station> CwSkimmer::stations() const
{
    QVector<Station> out;
    for (const auto& s : m_slots) {
        Station st;
        st.id = s->id;
        st.freq = s->freq;
        st.level = s->level;
        st.wpm = s->decoder ? s->decoder->wpm() : 0;
        st.confidence = s->decoder ? s->decoder->scope().confidence : 0.0f;
        st.text = s->text;
        st.call = s->call;
        st.cq = s->cq;
        st.idleSeconds = static_cast<double>(m_total - s->lastChar) / m_rate;
        st.present = s->present;
        out.append(st);
    }
    std::sort(out.begin(), out.end(), [](const Station& a, const Station& b) { return a.freq < b.freq; });
    return out;
}

bool CwSkimmer::takeChanged()
{
    const bool c = m_changed;
    m_changed = false;
    return c;
}

} // namespace decolog::core
