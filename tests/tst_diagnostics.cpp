#include "core/Diagnostics.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

    void operationalChecklistNeverRunsUnsafeActions()
    {
        const auto steps = diagnostics::operationalChecklist(true, false, 0, 0);
        QCOMPARE(steps.size(), 5);
        QCOMPARE(steps.at(0).id, QStringLiteral("frequency"));
        QCOMPARE(steps.at(2).id, QStringLiteral("ptt"));
        QCOMPARE(steps.at(2).state, QStringLiteral("not-run"));
        QVERIFY(steps.at(2).detail.contains(QStringLiteral("no transmission")));
        QCOMPARE(steps.at(4).state, QStringLiteral("not-run"));
    }

    void operationalChecklistRecordsStationContext()
    {
        const auto steps = diagnostics::operationalChecklist(
            true, true, 14074000, 1,
            {QStringLiteral("hamlib"), QStringLiteral(" Yaesu FT-991A "), QString()});
        QCOMPARE(steps.size(), 5);
        QVERIFY(steps.at(0).detail.contains(QStringLiteral("backend=hamlib")));
        QVERIFY(steps.at(0).detail.contains(QStringLiteral("model= Yaesu FT-991A ")));
        QVERIFY(steps.at(0).detail.contains(QStringLiteral("firmware=not reported")));
        QCOMPARE(steps.at(2).state, QStringLiteral("not-run"));
    }

    void operationalMatrixSeparatesEvidenceAndDoesNotOverclaim()
    {
        QFile file(QFINDTESTDATA("fixtures/operational_matrix.json"));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
        const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(root.value(QStringLiteral("schema")).toString(),
                 QStringLiteral("decodxlog.operational-matrix/v1"));
        const QJsonArray scenarios = root.value(QStringLiteral("scenarios")).toArray();
        QVERIFY(scenarios.size() >= 4);
        for (const QJsonValue& value : scenarios) {
            const QJsonObject scenario = value.toObject();
            QVERIFY(!scenario.value(QStringLiteral("id")).toString().isEmpty());
            QVERIFY(!scenario.value(QStringLiteral("kind")).toString().isEmpty());
            QVERIFY(!scenario.value(QStringLiteral("evidence")).toString().isEmpty());
            const QString kind = scenario.value(QStringLiteral("kind")).toString();
            const QString status = scenario.value(QStringLiteral("status")).toString();
            if (kind == QLatin1String("ui") || kind == QLatin1String("hardware")
                || kind == QLatin1String("external-data"))
                QVERIFY2(status != QLatin1String("passed"),
                         "manual or external evidence cannot be claimed as automatic pass");
        }
    }
};

QTEST_GUILESS_MAIN(TestDiagnostics)
#include "tst_diagnostics.moc"
