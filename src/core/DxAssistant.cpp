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
        if (p.prioritizeWorkedUnconfirmed && c.worked && !c.confirmed) {
            r.score += p.newReferenceWeight;
            r.reasons << QStringLiteral("worked but unconfirmed");
        }
        if (!p.awardAccount.isEmpty() && c.awardAccount != p.awardAccount) {
            r.limitations << QStringLiteral("outside selected award account");
            if (p.requireFeasible)
                continue;
        }
        const bool bandAvailable = p.availableBands.isEmpty()
                                   || p.availableBands.contains(c.band, Qt::CaseInsensitive);
        const bool modeAvailable = p.availableModes.isEmpty()
                                  || p.availableModes.contains(c.mode, Qt::CaseInsensitive);
        const bool radioAvailable = p.availableRadios.isEmpty() || c.radio.isEmpty()
                                    || p.availableRadios.contains(c.radio, Qt::CaseInsensitive);
        const bool antennaAvailable = p.availableAntennas.isEmpty() || c.antenna.isEmpty()
                                      || p.availableAntennas.contains(c.antenna, Qt::CaseInsensitive);
        if (!bandAvailable || !modeAvailable || !radioAvailable || !antennaAvailable
            || !c.radioAvailable || !c.antennaAvailable) {
            r.limitations << QStringLiteral("station feasibility unavailable");
            if (p.requireFeasible)
                continue;
        } else if (!p.availableBands.isEmpty() || !p.availableModes.isEmpty()) {
            r.reasons << QStringLiteral("station feasible");
        }
        if (!c.observed) {
            r.evidence = QStringLiteral("forecast/inferred");
            r.limitations << QStringLiteral("forecast or inferred data");
        } else if (c.historical) {
            r.evidence = QStringLiteral("historical");
            r.limitations << QStringLiteral("historical observation");
        } else {
            r.evidence = QStringLiteral("observed");
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
            if (age > p.freshnessMinutes) {
                r.limitations << QStringLiteral("spot is %1 minutes old").arg(age);
                if (p.rejectStale) {
                    r.limitations << QStringLiteral("stale spot rejected");
                    continue;
                }
                r.score *= std::clamp(p.stalePenalty, 0.0, 1.0);
                r.reasons << QStringLiteral("stale spot penalty %1").arg(p.stalePenalty, 0, 'f', 2);
            } else
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
