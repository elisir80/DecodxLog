// La radio via rigctld: si parla col demone di Hamlib, e qui c'e' un finto
// rigctld che risponde come quello vero (protocollo esteso, "+f" → "get_freq:
// / Frequency: … / RPRT 0"), cosi' la prova non ha bisogno di una radio.
#include "core/RigControl.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace decolog::core;

namespace {

class FakeRigctld : public QTcpServer {
public:
    QStringList received;
    qint64 frequency{14074000};
    QString mode{"CW"};
    int keyspd{22};
    bool morseWorks{true};
    // Il ponte CAT di Decodium risponde col valore e basta: niente eco del
    // comando, niente RPRT. DecoDXLog deve capire anche quello.
    bool plainAnswers{false};

    FakeRigctld()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* s = nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, s, [this, s] {
                    while (s->canReadLine()) {
                        const QString line = QString::fromUtf8(s->readLine()).trimmed();
                        received << line;
                        s->write(answer(line).toUtf8());
                    }
                });
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });
        listen(QHostAddress::LocalHost);
    }

    QString answer(const QString& line)
    {
        const QString cmd = line.startsWith(QLatin1Char('+')) ? line.mid(1) : line;
        if (plainAnswers) {
            if (cmd == QLatin1String("f"))
                return QStringLiteral("%1\n").arg(frequency);
            if (cmd == QLatin1String("m"))
                return QStringLiteral("%1\n3000\n").arg(mode);
            if (cmd == QLatin1String("l KEYSPD"))
                return QStringLiteral("%1\n").arg(keyspd);
            if (cmd.startsWith(QLatin1String("b ")))
                return QStringLiteral("RPRT -11\n");
            return QStringLiteral("RPRT 0\n");
        }
        if (cmd == QLatin1String("f"))
            return QStringLiteral("get_freq:\nFrequency: %1\nRPRT 0\n").arg(frequency);
        if (cmd == QLatin1String("m"))
            return QStringLiteral("get_mode:\nMode: %1\nPassband: 500\nRPRT 0\n").arg(mode);
        if (cmd == QLatin1String("l KEYSPD"))
            return QStringLiteral("get_level: KEYSPD\n%1\nRPRT 0\n").arg(keyspd);
        if (cmd.startsWith(QLatin1String("F "))) {
            frequency = cmd.mid(2).toLongLong();
            return QStringLiteral("set_freq: %1\nRPRT 0\n").arg(frequency);
        }
        if (cmd.startsWith(QLatin1String("M "))) {
            mode = cmd.section(QLatin1Char(' '), 1, 1);
            return QStringLiteral("set_mode: %1\nRPRT 0\n").arg(mode);
        }
        if (cmd.startsWith(QLatin1String("L KEYSPD "))) {
            keyspd = cmd.section(QLatin1Char(' '), 2, 2).toInt();
            return QStringLiteral("set_level: KEYSPD %1\nRPRT 0\n").arg(keyspd);
        }
        if (cmd.startsWith(QLatin1String("b ")))
            return QStringLiteral("send_morse: %1\nRPRT %2\n").arg(cmd.mid(2)).arg(morseWorks ? 0 : -1);
        if (cmd.contains(QLatin1String("stop_morse")))
            return QStringLiteral("stop_morse:\nRPRT 0\n");
        return QStringLiteral("%1:\nRPRT 0\n").arg(cmd);
    }
};

} // namespace

class TestRig : public QObject {
    Q_OBJECT

private slots:
    void readsWhereTheRadioIs()
    {
        FakeRigctld rig;
        RigControl control;
        QSignalSpy changed(&control, &RigControl::changed);
        control.connectTo(QStringLiteral("127.0.0.1"), rig.serverPort());

        QTRY_VERIFY_WITH_TIMEOUT(control.connected(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(control.frequencyHz(), 14074000LL, 5000);
        QCOMPARE(control.mode(), QStringLiteral("CW"));
        QTRY_COMPARE_WITH_TIMEOUT(control.speedWpm(), 22, 5000);
        QVERIFY(changed.size() > 0);

        // Si comanda anche: VFO, modo e velocita' arrivano alla radio.
        control.setFrequency(7025000);
        control.setMode(QStringLiteral("CW"));
        control.setSpeedWpm(28);
        QTRY_VERIFY_WITH_TIMEOUT(rig.received.contains(QStringLiteral("+F 7025000")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.received.contains(QStringLiteral("+L KEYSPD 28")), 5000);
        QCOMPARE(control.speedWpm(), 28);

        control.disconnectFromRig();
        QVERIFY(!control.connected());
    }

    void keysTheTextInCw()
    {
        FakeRigctld rig;
        RigControl control;
        control.connectTo(QStringLiteral("127.0.0.1"), rig.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(control.connected(), 5000);

        QSignalSpy sent(&control, &RigControl::morseSent);
        control.sendMorse(QStringLiteral("CQ TEST IU8LMC"));
        QVERIFY(sent.wait(5000));
        QCOMPARE(sent.first().at(0).toString(), QStringLiteral("CQ TEST IU8LMC"));
        QVERIFY(rig.received.contains(QStringLiteral("+b CQ TEST IU8LMC")));

        control.stopMorse();
        QTRY_VERIFY_WITH_TIMEOUT(rig.received.contains(QStringLiteral("+\\stop_morse")), 5000);
    }

    // rigctld risponde a "get_level" col valore nudo e *poi* con RPRT. Il
    // valore nudo bastava a chiudere la risposta, e il RPRT che arrivava dopo
    // si prendeva la domanda seguente: da li' in poi ogni risposta finiva sulla
    // domanda sbagliata, e l'errore del CW spariva del tutto.
    //
    // Qui si aspetta apposta che la velocita' sia arrivata — cioe' che quella
    // risposta col valore nudo sia stata chiusa — e solo allora si manda il CW.
    void theTailOfOneAnswerIsNotTheAnswerToTheNext()
    {
        FakeRigctld rig;
        rig.morseWorks = false;
        RigControl control;
        control.connectTo(QStringLiteral("127.0.0.1"), rig.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(control.connected(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(control.speedWpm(), 22, 5000);

        QSignalSpy failed(&control, &RigControl::failed);
        control.sendMorse(QStringLiteral("TEST"));
        QVERIFY2(failed.wait(5000), "la risposta al CW e' stata attribuita a un'altra domanda");
        QVERIFY(failed.first().at(0).toString().contains(QStringLiteral("CW")));

        // E la radio continua a rispondere a tono anche dopo: la coda non e'
        // rimasta sfasata di uno.
        control.setFrequency(7025000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.received.contains(QStringLiteral("+F 7025000")), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(control.frequencyHz(), 7025000LL, 5000);
    }

    void aRadioThatDoesNotKeyCwSaysSo()
    {
        FakeRigctld rig;
        rig.morseWorks = false;
        RigControl control;
        control.connectTo(QStringLiteral("127.0.0.1"), rig.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(control.connected(), 5000);

        QSignalSpy failed(&control, &RigControl::failed);
        control.sendMorse(QStringLiteral("TEST"));
        QVERIFY(failed.wait(5000));
        QVERIFY(failed.first().at(0).toString().contains(QStringLiteral("CW")));
    }

    void withoutTheRadioNothingGoesOnAir()
    {
        RigControl control;
        QSignalSpy failed(&control, &RigControl::failed);
        control.sendMorse(QStringLiteral("CQ"));
        QCOMPARE(failed.size(), 1);
        QVERIFY(!control.connected());
    }

    void understandsARigctldThatAnswersPlainly()
    {
        // Come il ponte CAT di Decodium: "+f" -> "14084000", senza RPRT.
        FakeRigctld rig;
        rig.plainAnswers = true;
        rig.frequency = 14084000;
        rig.mode = QStringLiteral("PKTUSB");
        RigControl control;
        control.connectTo(QStringLiteral("127.0.0.1"), rig.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(control.connected(), 5000);

        QTRY_COMPARE_WITH_TIMEOUT(control.frequencyHz(), 14084000LL, 5000);
        QCOMPARE(control.mode(), QStringLiteral("PKTUSB"));
        // E la coda non si inceppa: al giro dopo chiede ancora.
        const qsizetype before = rig.received.size();
        QTRY_VERIFY_WITH_TIMEOUT(rig.received.size() > before + 2, 6000);
    }

    void aCatBridgeThatCannotKeyCwSaysItClearly()
    {
        FakeRigctld rig;
        rig.plainAnswers = true;
        RigControl control;
        control.connectTo(QStringLiteral("127.0.0.1"), rig.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(control.connected(), 5000);

        QSignalSpy unsupported(&control, &RigControl::morseUnsupported);
        control.sendMorse(QStringLiteral("CQ"));
        QVERIFY(unsupported.wait(5000));
        QVERIFY(control.status().contains(QStringLiteral("CW")));
    }
};

QTEST_MAIN(TestRig)
#include "tst_rig.moc"
