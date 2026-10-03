#include "core/Gs232.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace decolog::core::gs232 {

QByteArray encode(const QString& command)
{
    return command.toLatin1() + QByteArray(1, prosistel::kCr);
}

QByteArray queryPosition()
{
    return encode(QStringLiteral("C"));
}

QByteArray gotoAngle(double degrees)
{
    const long raw = std::clamp(std::lround(degrees), 0L, static_cast<long>(kMaxAzimuth));
    return encode(QStringLiteral("M%1").arg(raw, 3, 10, QLatin1Char('0')));
}

QByteArray stop()
{
    return encode(QStringLiteral("S"));
}

std::optional<prosistel::Reply> decode(const QByteArray& frame)
{
    const QString text = QString::fromLatin1(frame).trimmed().toUpper();
    // "+0290", "+0290+0045" (C2), "AZ=290", "AZ=290 EL=045".
    static const QRegularExpression re(QStringLiteral(R"(^(?:AZ\s*=\s*|\+)(\d{1,4})(?!\d))"));
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch())
        return std::nullopt;
    prosistel::Reply r;
    r.axis = prosistel::kAzimuth;
    r.verb = QLatin1Char('C');
    r.value = m.captured(1).toDouble();
    r.status = QLatin1Char('R');
    return r;
}

} // namespace decolog::core::gs232
