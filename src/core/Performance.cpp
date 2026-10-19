#include "core/Performance.h"

#include <algorithm>

namespace decolog::core::performance {

namespace {
qint64 nearestRank(const QList<qint64>& values, int percentile)
{
    if (values.isEmpty())
        return 0;
    const qsizetype rank = std::max<qsizetype>(1, (values.size() * percentile + 99) / 100);
    return values.at(std::min<qsizetype>(values.size() - 1, rank - 1));
}
} // namespace

Summary summarize(QList<qint64> samples)
{
    Summary out;
    if (samples.isEmpty())
        return out;
    for (qint64& sample : samples)
        sample = std::max<qint64>(0, sample);
    std::sort(samples.begin(), samples.end());
    out.samples = samples.size();
    out.p50Ms = nearestRank(samples, 50);
    out.p95Ms = nearestRank(samples, 95);
    out.maxMs = samples.back();
    out.valid = true;
    return out;
}

QString toText(const Summary& summary, const QString& label)
{
    if (!summary.valid)
        return QStringLiteral("%1: no samples").arg(label);
    return QStringLiteral("%1: n=%2 p50=%3 ms p95=%4 ms max=%5 ms")
        .arg(label).arg(summary.samples).arg(summary.p50Ms).arg(summary.p95Ms).arg(summary.maxMs);
}

} // namespace decolog::core::performance
