#pragma once

#include <QCoreApplication>
#include <QThread>
#include <QtGlobal>
#include <atomic>
#include <chrono>
#include <cstdio>

namespace decolog {
// Opt-in, monotonic timings. stderr is flushed even before Qt's event loop.
inline void startupTrace(const char* phase)
{
    static const bool enabled = qEnvironmentVariableIntValue("DECODXLOG_TRACE_STARTUP") != 0;
    if (!enabled)
        return;
    static const auto start = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    // [bg] quando non e' il filo della finestra: quello che blocca e' l'altro.
    const bool ui = !QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread();
    std::fprintf(stderr, "[Startup +%lld ms]%s %s\n", static_cast<long long>(ms), ui ? "" : " [bg]", phase);
    std::fflush(stderr);
}

// L'ultima operazione lunga fatta sul filo della finestra: serve al controllo dei blocchi
// (DecoLogController::startFreezeWatch) per dire *cosa* stava facendo il programma quando la
// finestra non rispondeva, non solo per quanto. Sempre attivo (non dipende dalla variabile
// d'ambiente): costa tre scritture atomiche, e solo per le operazioni sopra i 300 ms.
struct SlowOp {
    std::atomic<const char*> name{nullptr};
    std::atomic<long long> endMs{0};       // fine, in ms dal primo uso
    std::atomic<long long> durationMs{0};
};
inline SlowOp& lastSlowOp()
{
    static SlowOp op;
    return op;
}
inline long long steadyNowMs()
{
    static const auto origin = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - origin).count();
}

class StartupSpan {
public:
    explicit StartupSpan(const char* name) : m_name(name) { steadyNowMs(); startupTrace(m_name); }
    ~StartupSpan() {
        char message[256];
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_start).count();
        if (elapsed >= 300 && QCoreApplication::instance()
            && QThread::currentThread() == QCoreApplication::instance()->thread()) {
            SlowOp& slow = lastSlowOp();
            const long long now = steadyNowMs();
            // Resta quella piu' lunga delle ultime secondi: e' lei la colpevole del blocco.
            if (elapsed >= slow.durationMs.load() || now - slow.endMs.load() > 10000) {
                slow.name.store(m_name);
                slow.durationMs.store(elapsed);
                slow.endMs.store(now);
            }
        }
        std::snprintf(message, sizeof(message), "done: %s (duration %lld ms)", m_name,
                      static_cast<long long>(elapsed));
        startupTrace(message);
    }
private:
    const char* m_name;
    const std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
};
}
