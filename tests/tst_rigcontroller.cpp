// Il pulsante Stop deve fermare la radio che sta realmente trasmettendo.
#include "app/RigController.h"

#include <QSettings>
#include <QTest>

using namespace decolog;

namespace {

class FakeRig final : public core::RigLink {
public:
    bool connected() const override { return true; }
    qint64 frequencyHz() const override { return 0; }
    QString mode() const override { return QStringLiteral("CW"); }
    int speedWpm() const override { return speed; }
    QString status() const override { return {}; }
    void disconnectFromRig() override {}
    void refresh() override {}
    void setFrequency(qint64) override {}
    void setMode(const QString&) override {}
    void setPtt(bool on) override { pttStates << on; }
    void setSpeedWpm(int value) override { speed = value; }
    void sendMorse(const QString& value) override { sent << value; }
    void stopMorse() override { ++stops; }

    int speed{0};
    int stops{0};
    QStringList sent;
    QList<bool> pttStates;
};

} // namespace

class TestRigController : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("DecoDXLogTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_rigcontroller"));
        QSettings().clear();
    }

    void stopTargetsTheRadioThatReceivedTheMacro()
    {
        FakeRig radio2;
        app::RigController::Context context;
        context.alternateRig = [&radio2]() { return &radio2; };
        app::RigController controller(context);

        controller.sendText(QStringLiteral("CQ TEST"), {});
        QCOMPARE(radio2.sent, QStringList({QStringLiteral("CQ TEST")}));

        controller.stop();
        QCOMPARE(radio2.stops, 1);
        QCOMPARE(radio2.pttStates, QList<bool>({false}));
    }

    void everyMacroKeepsItsOwnIndexAndStopClearsIt()
    {
        FakeRig radio2;
        app::RigController::Context context;
        context.alternateRig = [&radio2]() { return &radio2; };
        app::RigController controller(context);
        controller.setMacro(0, QStringLiteral("F1 CQ"), QStringLiteral("CQ"));
        controller.setMacro(1, QStringLiteral("F2 Call"), QStringLiteral("{CALL}"));

        controller.sendMacro(1, {{QStringLiteral("call"), QStringLiteral("IU8LMC")}});
        QCOMPARE(radio2.sent, QStringList({QStringLiteral("IU8LMC")}));
        QCOMPARE(controller.activeMacroIndex(), 1);

        controller.stop();
        QCOMPARE(controller.activeMacroIndex(), -1);
        QCOMPARE(radio2.stops, 1);
        QCOMPARE(radio2.pttStates, QList<bool>({false}));
    }
};

QTEST_GUILESS_MAIN(TestRigController)
#include "tst_rigcontroller.moc"
