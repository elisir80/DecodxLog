#include "core/Diagnostics.h"

#include <QTest>

using namespace decolog::core;

class TestDiagnostics : public QObject {
    Q_OBJECT

private slots:
    void supportTextRedactsCredentials()
    {
        const QString input = QStringLiteral(
            "Authorization: Bearer abc123 X-DecoDXLog-Token: deadbeef api_key=secret password: hunter2");
        const QString safe = diagnostics::redactSecrets(input);
        QVERIFY(!safe.contains(QStringLiteral("abc123")));
        QVERIFY(!safe.contains(QStringLiteral("deadbeef")));
        QVERIFY(!safe.contains(QStringLiteral("secret")));
        QVERIFY(!safe.contains(QStringLiteral("hunter2")));
        QVERIFY(safe.count(QStringLiteral("<redacted>")) == 4);
    }

    void syntheticQsoIsExplicitlyDiagnostic()
    {
        const AdifRecord record = diagnostics::syntheticQso();
        QCOMPARE(record.value(QStringLiteral("CALL")), QStringLiteral("DIAG-TEST"));
        QCOMPARE(record.value(QStringLiteral("APP_DECOLOG_DIAGNOSTIC")), QStringLiteral("1"));
    }
};

QTEST_GUILESS_MAIN(TestDiagnostics)
#include "tst_diagnostics.moc"
