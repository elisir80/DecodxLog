#pragma once

#include <QtGlobal>
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
    std::fprintf(stderr, "[Startup +%lld ms] %s\n", static_cast<long long>(ms), phase);
    std::fflush(stderr);
}

class StartupSpan {
public:
    explicit StartupSpan(const char* name) : m_name(name) { startupTrace(m_name); }
    ~StartupSpan() {
        char message[256];
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_start).count();
        std::snprintf(message, sizeof(message), "done: %s (duration %lld ms)", m_name,
                      static_cast<long long>(elapsed));
        startupTrace(message);
    }
private:
    const char* m_name;
    const std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
};
}
