// Le porte seriali di un altro programma: DecoDXLog non le apre mai. Una seriale
// la apre uno solo; se la prende DecoDXLog, Decodium resta senza radio.
#include "core/SerialPorts.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace decolog::core;

class TestSerialPorts : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("DecoDXLogTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_serialports"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings().clear();
    }

    void cleanup()
    {
        QSettings().clear();
    }

    void namesCompareWithoutCaseOrPrefix()
    {
#ifdef Q_OS_WIN
        QCOMPARE(normalizedPortName(QStringLiteral(" com5 ")), QStringLiteral("COM5"));
        QCOMPARE(normalizedPortName(QStringLiteral("\\\\.\\COM12")), QStringLiteral("COM12"));
#else
        QCOMPARE(normalizedPortName(QStringLiteral(" /dev/ttyUSB0 ")), QStringLiteral("/dev/ttyUSB0"));
#endif
    }

    void thePortsTheOperatorWritesAreReserved()
    {
        setLeaveDecodiumPort(false);
        QVERIFY(userReservedSerialPorts().isEmpty());
        QVERIFY(!isSerialPortReserved(QStringLiteral("COM5")));

        setUserReservedSerialPorts({QStringLiteral("com5"), QStringLiteral("COM9"), QStringLiteral("COM5")});
#ifdef Q_OS_WIN
        QCOMPARE(userReservedSerialPorts().size(), 2);
#else
        // POSIX device paths are case-sensitive; only Windows normalizes COM names.
        QCOMPARE(userReservedSerialPorts().size(), 3);
#endif
#ifdef Q_OS_WIN
        QVERIFY(isSerialPortReserved(QStringLiteral("COM5")));
        QVERIFY(isSerialPortReserved(QStringLiteral("com9")));
#endif
        QVERIFY(!isSerialPortReserved(QStringLiteral("COM4")));
        QVERIFY(!isSerialPortReserved(QString()));
    }

    void theDecodiumPortIsLeftAloneUnlessTheOperatorSaysOtherwise()
    {
        // Di serie si lascia stare: l'interruttore e' acceso senza che l'operatore l'abbia toccato.
        QVERIFY(leaveDecodiumPort());
        setLeaveDecodiumPort(false);
        QVERIFY(!leaveDecodiumPort());
        // Spento, di Decodium non si riserva niente.
        QVERIFY(!reservedSerialPorts().contains(decodiumCatPort())
                || decodiumCatPort().isEmpty()
                || userReservedSerialPorts().contains(decodiumCatPort()));
        setLeaveDecodiumPort(true);
        if (!decodiumCatPort().isEmpty())
            QVERIFY(isSerialPortReserved(decodiumCatPort()));
    }

    void theHiddenEndsOfVirtualPairsAreRecognised()
    {
        QVERIFY(isHiddenVirtualPort(QStringLiteral("CNCB0")));
        QVERIFY(isHiddenVirtualPort(QStringLiteral("cncb12")));
        QVERIFY(isHiddenVirtualPort(QStringLiteral("CNCA3")));
        QVERIFY(!isHiddenVirtualPort(QStringLiteral("COM8")));
        QVERIFY(!isHiddenVirtualPort(QStringLiteral("CNCB")));
        QVERIFY(!isHiddenVirtualPort(QStringLiteral("/dev/ttyUSB0")));
    }

    void theRadioSearchSkipsReservedAndHiddenPortsAndTriesTheVirtualOnesFirst()
    {
        setLeaveDecodiumPort(false);
        setUserReservedSerialPorts({QStringLiteral("COM5")});
        const QStringList all{QStringLiteral("COM3"), QStringLiteral("COM4"), QStringLiteral("COM5"),
                              QStringLiteral("COM6"), QStringLiteral("COM8"), QStringLiteral("CNCB0"),
                              QStringLiteral("CNCB2")};
        const QStringList order = serialProbeOrder(QStringLiteral("COM4"), all,
                                                   {QStringLiteral("COM6"), QStringLiteral("COM8")});
#ifdef Q_OS_WIN
        QCOMPARE(order, (QStringList{QStringLiteral("COM4"), QStringLiteral("COM6"), QStringLiteral("COM8"),
                                     QStringLiteral("COM3")}));
#else
        QVERIFY(!order.contains(QStringLiteral("COM5")));
#endif
        QVERIFY(!order.contains(QStringLiteral("CNCB0")));
        QVERIFY(!order.contains(QStringLiteral("CNCB2")));
        // Mai la porta riservata, nemmeno se e' quella scelta adesso.
        QVERIFY(!serialProbeOrder(QStringLiteral("COM5"), all, {}).contains(QStringLiteral("COM5")));
    }

    // Il CAT di Decodium (COM5 nella stazione), ma anche il PTT su un'altra porta e
    // l'amplificatore: tutto cio' che Decodium apre resta suo. «CAT» come PTT non e' una porta.
    void everyPortDecodiumUsesIsReserved()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
        {
            QSettings decodium(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"),
                               QStringLiteral("Decodium3"));
            decodium.setValue(QStringLiteral("Transceiver/serialPort"), QStringLiteral("COM5"));
            decodium.setValue(QStringLiteral("Transceiver/pttPort"), QStringLiteral("CAT"));
            decodium.setValue(QStringLiteral("CAT_Native/serialPort"), QString());
            decodium.setValue(QStringLiteral("General/AmpPort"), QStringLiteral("COM9"));
            decodium.sync();
        }
        QCOMPARE(decodiumCatPort(), QStringLiteral("COM5"));
        QCOMPARE(decodiumSerialPorts(), (QStringList{QStringLiteral("COM5"), QStringLiteral("COM9")}));

        setLeaveDecodiumPort(true);
        QVERIFY(isSerialPortReserved(QStringLiteral("COM5")));
        QVERIFY(isSerialPortReserved(QStringLiteral("COM9")));
        QVERIFY(!isSerialPortReserved(QStringLiteral("COM4")));
        QVERIFY(!isSerialPortReserved(QStringLiteral("CAT")));

        setLeaveDecodiumPort(false);
        QVERIFY(!isSerialPortReserved(QStringLiteral("COM5")));
    }

    void reservedPortsAreNotOfferedInTheLists()
    {
        setLeaveDecodiumPort(false);
        const QStringList before = availableSerialPorts();
        if (before.isEmpty())
            QSKIP("no serial port on this computer");
        setUserReservedSerialPorts({before.first()});
        const QStringList after = availableSerialPorts();
        QVERIFY(!after.contains(before.first(), Qt::CaseInsensitive));
        QCOMPARE(after.size(), before.size() - 1);
    }
};

QTEST_GUILESS_MAIN(TestSerialPorts)
#include "tst_serialports.moc"
