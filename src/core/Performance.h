#pragma once

#include <QList>
#include <QString>

namespace decolog::core::performance {

struct Summary {
    int samples{0};
    qint64 p50Ms{0};
    qint64 p95Ms{0};
    qint64 maxMs{0};
    bool valid{false};
};

Summary summarize(QList<qint64> samples);
QString toText(const Summary& summary, const QString& label);

} // namespace decolog::core::performance
