// DecoDXLog — il rotore: lo stato di DecoRotor e le risposte di rotctld.
#include "core/RotorLink.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace decolog::core;

class TestRotor : public QObject {
    Q_OBJECT

private:
    static QJsonObject json(const char* text)
    {
        return QJsonDocument::fromJson(text).object();
    }

private slots:
    void degreesComeBackInRange()
    {
        QCOMPARE(rotor::normalize(0.0), 0.0);
        QCOMPARE(rotor::normalize(359.5), 359.5);
        QCOMPARE(rotor::normalize(360.0), 0.0);
        QCOMPARE(rotor::normalize(370.0), 10.0);
        QCOMPARE(rotor::normalize(-10.0), 350.0);
        QCOMPARE(rotor::normalize(-370.0), 350.0);
    }

    void decoRotorState()
    {
        const RotorState s = rotor::parseState(json(
            "{\"type\": \"state\", \"connected\": true, \"port\": \"COM4\","
            " \"model\": \"d_azel\", \"model_label\": \"Control box D - azimut + elevazione\","
            " \"has_az\": true, \"has_el\": true,"
            " \"az\": 128.4, \"az_target\": 130.0, \"az_moving\": true,"
            " \"el\": 12.0, \"el_target\": null, \"el_moving\": false,"
            " \"moving\": true, \"error\": null, \"ts\": 1787141355.85}"));
        QVERIFY(s.connected);
        QVERIFY(s.hasAz);
        QVERIFY(s.hasEl);
        QVERIFY(s.moving);
        QCOMPARE(s.az, 128.4);
        QCOMPARE(s.azTarget, 130.0);
        QCOMPARE(s.el, 12.0);
        // Nessun bersaglio in elevazione: null diventa -1, non 0, che sarebbe
        // una richiesta di puntare all'orizzonte.
        QCOMPARE(s.elTarget, -1.0);
        QCOMPARE(s.port, QStringLiteral("COM4"));
        QCOMPARE(s.model, QStringLiteral("d_azel"));
        QVERIFY(s.error.isEmpty());
    }

    void decoRotorStateWithoutElevation()
    {
        const RotorState s = rotor::parseState(json(
            "{\"connected\": true, \"has_az\": true, \"has_el\": false, \"az\": 300.0,"
            " \"az_target\": null, \"moving\": false}"));
        QVERIFY(s.connected);
        QVERIFY(!s.hasEl);
        QVERIFY(!s.moving);
        QCOMPARE(s.az, 300.0);
        QCOMPARE(s.azTarget, -1.0);
    }

    void rotctldPosition()
    {
        double az = 0.0;
        double el = 0.0;
        QVERIFY(rotor::parsePosition(QStringLiteral("128.40\n12.00\n"), &az, &el));
        QCOMPARE(az, 128.4);
        QCOMPARE(el, 12.0);

        // La forma lunga di "+p".
        az = el = 0.0;
        QVERIFY(rotor::parsePosition(QStringLiteral("Azimuth: 90.00\nElevation: 5.00\n"), &az, &el));
        QCOMPARE(az, 90.0);
        QCOMPARE(el, 5.0);

        // Solo azimut: l'elevazione resta com'era.
        az = 0.0;
        el = 7.0;
        QVERIFY(rotor::parsePosition(QStringLiteral("45.00\n"), &az, &el));
        QCOMPARE(az, 45.0);
        QCOMPARE(el, 7.0);
    }

    void rotctldErrors()
    {
        double az = 0.0;
        double el = 0.0;
        QVERIFY(!rotor::parsePosition(QStringLiteral("RPRT -8\n"), &az, &el));
        QVERIFY(!rotor::parsePosition(QStringLiteral(""), &az, &el));
        QVERIFY(!rotor::parsePosition(QStringLiteral("boh\n"), &az, &el));
        // Niente e' stato toccato.
        QCOMPARE(az, 0.0);
        QCOMPARE(el, 0.0);
    }

    void linkStartsAndStopsWithoutAGateway()
    {
        // Senza nessuno dall'altra parte non deve succedere niente di brutto:
        // il collegamento resta spento e lo stato non e' collegato.
        RotorLink link;
        link.start(RotorLink::Backend::Rotctld, QStringLiteral("127.0.0.1"), 4599);
        QVERIFY(link.running());
        QVERIFY(!link.state().connected);
        link.goTo(120.0);
        link.halt();
        link.park();
        link.stop();
        QVERIFY(!link.running());
    }
};

QTEST_MAIN(TestRotor)
#include "tst_rotor.moc"
