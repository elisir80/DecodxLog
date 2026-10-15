#include "core/Diagnostics.h"

#include <QRegularExpression>

namespace decolog::core::diagnostics {

QString redactSecrets(const QString& input)
{
    QString out = input;
    static const QList<QRegularExpression> patterns{
        QRegularExpression(QStringLiteral("(?i)(authorization\\s*:\\s*bearer\\s+)[^\\s,;]+")),
        QRegularExpression(QStringLiteral("(?i)(x-decodxlog-token\\s*:\\s*)[^\\s,;]+")),
        QRegularExpression(QStringLiteral("(?i)((?:api[_ -]?key|password|secret|token)\\s*[=:]\\s*)[^\\s,;]+")),
    };
    for (const QRegularExpression& pattern : patterns)
        out.replace(pattern, QStringLiteral("\\1<redacted>"));
    return out;
}

AdifRecord syntheticQso()
{
    return AdifRecord{{QStringLiteral("CALL"), QStringLiteral("DIAG-TEST")},
                      {QStringLiteral("QSO_DATE"), QStringLiteral("20000101")},
                      {QStringLiteral("TIME_ON"), QStringLiteral("000000")},
                      {QStringLiteral("BAND"), QStringLiteral("2m")},
                      {QStringLiteral("MODE"), QStringLiteral("TEST")},
                      {QStringLiteral("APP_DECOLOG_DIAGNOSTIC"), QStringLiteral("1")}};
}

} // namespace decolog::core::diagnostics
