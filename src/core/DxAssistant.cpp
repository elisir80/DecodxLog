#include "core/DxAssistant.h"

#include "core/Spots.h"

#include <algorithm>

namespace decolog::core::dxassistant {

namespace {
constexpr int kNewDxcc = 1 << 0;
constexpr int kNewBand = 1 << 1;
constexpr int kNewMode = 1 << 2;
constexpr int kNewReference = 1 << 11;
}

QList<Recommendation> rank(const QList<Candidate>& candidates,
                           const Preferences& p,
                           const QDateTime& now)
{
    QList<Recommendation> out;
    out.reserve(candidates.size());
    for (const Candidate& c : candidates) {
        Recommendation r;
        r.candidate = c;
        if (c.call.trimmed().isEmpty()) {
            r.limitations << QStringLiteral("missing callsign");
            continue;
        }
        if (c.status & kNewDxcc) {
            r.score += p.newEntityWeight;
            r.reasons << QStringLiteral("new DXCC");
        }
        if (c.status & kNewBand) {
            r.score += p.newBandWeight;
            r.reasons << QStringLiteral("new band");
        }
        if (c.status & kNewMode) {
            r.score += p.newModeWeight;
            r.reasons << QStringLiteral("new mode");
        }
        if (c.status & kNewReference) {
            r.score += p.newReferenceWeight;
            r.reasons << QStringLiteral("new reference");
        }
        if (c.hasPropagation && c.propagationQuality >= 0) {
            const int quality = std::clamp(c.propagationQuality, 0, 3);
            r.score += p.propagationWeight * (quality / 3.0);
            r.reasons << QStringLiteral("propagation quality %1/3").arg(quality);
        } else {
            r.limitations << QStringLiteral("propagation unavailable");
        }
        if (!c.hasHistory)
            r.limitations << QStringLiteral("personal history unavailable");
        if (!c.spottedAt.isValid()) {
            r.limitations << QStringLiteral("spot age unavailable");
        } else {
            const qint64 age = std::max<qint64>(0, c.spottedAt.secsTo(now) / 60);
            if (age > p.freshnessMinutes)
                r.limitations << QStringLiteral("spot is %1 minutes old").arg(age);
            else
                r.reasons << QStringLiteral("spot is %1 minutes old").arg(age);
        }
        if (r.reasons.isEmpty())
            r.reasons << QStringLiteral("no positive objective matched");
        out.append(r);
    }
    std::stable_sort(out.begin(), out.end(), [](const Recommendation& a, const Recommendation& b) {
        return a.score > b.score;
    });
    return out;
}

} // namespace decolog::core::dxassistant
