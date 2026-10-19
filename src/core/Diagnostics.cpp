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

QList<OperationalStep> operationalChecklist(bool logOpen, bool rigConnected,
                                             qint64 frequencyHz, int rigFeatures)
{
    return operationalChecklist(logOpen, rigConnected, frequencyHz, rigFeatures, {});
}

QList<OperationalStep> operationalChecklist(bool logOpen, bool rigConnected,
                                             qint64 frequencyHz, int rigFeatures,
                                             const StationContext& context)
{
    const QString backend = context.backend.isEmpty() ? QStringLiteral("unknown") : context.backend;
    const QString model = context.model.isEmpty() ? QStringLiteral("unknown") : context.model;
    const QString firmware = context.firmware.isEmpty() ? QStringLiteral("not reported") : context.firmware;
    const QString station = QStringLiteral("backend=%1, model=%2, firmware=%3")
                                .arg(backend, model, firmware);
    return {
        {QStringLiteral("frequency"), rigConnected && frequencyHz > 0 ? QStringLiteral("ok") : QStringLiteral("warning"),
         QStringLiteral("%1; %2").arg(rigConnected && frequencyHz > 0
                                          ? QStringLiteral("frequency read from the connected radio")
                                          : QStringLiteral("radio frequency not verified"), station)},
        {QStringLiteral("split"), rigConnected ? QStringLiteral("ok") : QStringLiteral("warning"),
         QStringLiteral("%1; %2").arg(rigConnected ? ((rigFeatures & 1)
                                                           ? QStringLiteral("split capability reported by the backend")
                                                           : QStringLiteral("backend reports no split capability"))
                                             : QStringLiteral("split not verified without a connected radio"), station)},
        {QStringLiteral("ptt"), QStringLiteral("not-run"),
         QStringLiteral("PTT test requires explicit operator approval and a named radio; no transmission was started; %1").arg(station)},
        {QStringLiteral("logging"), logOpen ? QStringLiteral("ok") : QStringLiteral("warning"),
         logOpen ? QStringLiteral("local log is open; write path is available") : QStringLiteral("local log is not open")},
        {QStringLiteral("qsl"), QStringLiteral("not-run"),
         QStringLiteral("QSL send is not exercised by diagnostics; use the service test explicitly")}
    };
}

} // namespace decolog::core::diagnostics
