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
    // Split, VFO, RIT e XIT; una radio senza RIT risponde -11.
    bool split{false};
    qint64 txFrequency{0};
    QString vfo{"VFOA"};
    int rit{0};
    int xit{0};
    bool hasRit{true};

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
        if (cmd == QLatin1String("s"))
            return QStringLiteral("get_split_vfo:\nSplit: %1\nTX VFO: VFOB\nRPRT 0\n").arg(split ? 1 : 0);
        if (cmd.startsWith(QLatin1String("S "))) {
            split = cmd.section(QLatin1Char(' '), 1, 1) == QLatin1String("1");
            return QStringLiteral("set_split_vfo:\nRPRT 0\n");
        }
        if (cmd == QLatin1String("i"))
            return QStringLiteral("get_split_freq:\nTX Frequency: %1\nRPRT 0\n").arg(txFrequency);
        if (cmd.startsWith(QLatin1String("I "))) {
            txFrequency = cmd.mid(2).toLongLong();
            return QStringLiteral("set_split_freq:\nRPRT 0\n");
        }
        if (cmd == QLatin1String("v"))
            return QStringLiteral("get_vfo:\nVFO: %1\nRPRT 0\n").arg(vfo);
        if (cmd.startsWith(QLatin1String("V "))) {
            vfo = cmd.mid(2);
            return QStringLiteral("set_vfo:\nRPRT 0\n");
        }
        if (cmd == QLatin1String("j") || cmd.startsWith(QLatin1String("J "))) {
            if (!hasRit)
                return QStringLiteral("RPRT -11\n");
            if (cmd.startsWith(QLatin1String("J ")))
                rit = cmd.mid(2).toInt();
            return QStringLiteral("get_rit:\nRIT: %1\nRPRT 0\n").arg(rit);
        }
        if (cmd == QLatin1String("z"))
            return QStringLiteral("get_xit:\nXIT: %1\nRPRT 0\n").arg(xit);
        if (cmd.startsWith(QLatin1String("Z "))) {
            xit = cmd.mid(2).toInt();
            return QStringLiteral("set_xit:\nRPRT 0\n");
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

    void splitVfoRitAndXit()
    {
        FakeRigctld rig;
        RigControl control;
        control.connectTo(QStringLiteral("127.0.0.1"), rig.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(control.connected(), 5000);
        QCOMPARE(control.features(), int(RigLink::Split | RigLink::VfoSelect | RigLink::Rit | RigLink::Xit));

        // Split: in su di 1 kHz, sul VFO B.
        control.setSplit(true, 14075000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.received.contains(QStringLiteral("+S 1 VFOB")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.received.contains(QStringLiteral("+I 14075000")), 5000);
        QVERIFY(control.split());
        // Quello che dice la radio si legge al giro dopo.
        rig.txFrequency = 14076000;
        QTRY_COMPARE_WITH_TIMEOUT(control.txFrequencyHz(), 14076000LL, 8000);
        control.setSplit(false);
        QTRY_VERIFY_WITH_TIMEOUT(rig.received.contains(QStringLiteral("+S 0 VFOA")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!control.split(), 8000);

        control.setVfo(QStringLiteral("VFOB"));
        QTRY_VERIFY_WITH_TIMEOUT(rig.received.contains(QStringLiteral("+V VFOB")), 5000);
        QCOMPARE(control.vfo(), QString("VFOB"));

        control.setRit(150);
        control.setXit(-200);
        QTRY_VERIFY_WITH_TIMEOUT(rig.received.contains(QStringLiteral("+J 150")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.received.contains(QStringLiteral("+Z -200")), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(control.ritHz(), 150, 8000);
        QCOMPARE(control.xitHz(), -200);
    }

    void aRadioWithoutRitStopsBeingAsked()
    {
        FakeRigctld rig;
        rig.hasRit = false;
        RigControl control;
        control.connectTo(QStringLiteral("127.0.0.1"), rig.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(control.connected(), 5000);
        // Al primo "j" risponde -11: il RIT sparisce, il resto resta.
        QTRY_VERIFY_WITH_TIMEOUT(!(control.features() & RigLink::Rit), 8000);
        QVERIFY(control.features() & RigLink::Split);
        QSignalSpy failed(&control, &RigControl::failed);
        control.setRit(100);
        QTRY_VERIFY_WITH_TIMEOUT(failed.count() > 0, 5000);
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
