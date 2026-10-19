#pragma once

#include "core/Adif.h"

#include <QByteArray>
#include <QList>
#include <QString>

namespace decolog::core::diagnostics {

// Redacts credentials from support text before it leaves the application.
QString redactSecrets(const QString& text);

// A deterministic, non-public test QSO. It is intentionally marked as a
// diagnostic source so callers can reject it before any upload/relay step.
AdifRecord syntheticQso();

struct OperationalStep {
    QString id;
    QString state; // ok | warning | not-run
    QString detail;
};

struct StationContext {
    QString backend;
    QString model;
    QString firmware;
};

// Safe checklist only: it observes current state and never keys PTT or writes
// a QSO. Hardware-dependent actions deliberately remain not-run.
QList<OperationalStep> operationalChecklist(bool logOpen, bool rigConnected,
                                             qint64 frequencyHz, int rigFeatures);
QList<OperationalStep> operationalChecklist(bool logOpen, bool rigConnected,
                                             qint64 frequencyHz, int rigFeatures,
                                             const StationContext& context);

} // namespace decolog::core::diagnostics
