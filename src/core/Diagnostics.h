#pragma once

#include "core/Adif.h"

#include <QByteArray>
#include <QString>

namespace decolog::core::diagnostics {

// Redacts credentials from support text before it leaves the application.
QString redactSecrets(const QString& text);

// A deterministic, non-public test QSO. It is intentionally marked as a
// diagnostic source so callers can reject it before any upload/relay step.
AdifRecord syntheticQso();

} // namespace decolog::core::diagnostics
