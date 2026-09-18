// DecoLog — il log Cabrillo: colonne al posto giusto, modi e frequenze come li
// vuole chi riceve il log.
#include "core/Cabrillo.h"

#include <QTest>

using namespace decolog::core;

class TestCabrillo : public QObject {
    Q_OBJECT

private:
    static AdifRecord qso(const QString& call, const QString& band, const QString& freq,
                          const QString& mode, const QString& submode = {},
                          const QString& stx = QStringLiteral("001"),
                          const QString& srx = QStringLiteral("034"))
    {
        AdifRecord r;
        r.set(QStringLiteral("CALL"), call);
        r.set(QStringLiteral("QSO_DATE"), QStringLiteral("20261102"));
        r.set(QStringLiteral("TIME_ON"), QStringLiteral("071122"));
        r.set(QStringLiteral("BAND"), band);
        r.set(QStringLiteral("FREQ"), freq);
        r.set(QStringLiteral("MODE"), mode);
        if (!submode.isEmpty())
            r.set(QStringLiteral("SUBMODE"), submode);
        r.set(QStringLiteral("RST_SENT"), QStringLiteral("599"));
        r.set(QStringLiteral("RST_RCVD"), QStringLiteral("599"));
        r.set(QStringLiteral("STX_STRING"), stx);
        r.set(QStringLiteral("SRX_STRING"), srx);
        return r;
    }

    static cabrillo::Info info()
    {
        cabrillo::Info i;
        i.contest = QStringLiteral("CQ-WW-CW");
        i.callsign = QStringLiteral("IU8LMC");
        i.gridLocator = QStringLiteral("JN70");
        i.operators = QStringLiteral("IU8LMC");
        return i;
    }

private slots:
    void modes()
    {
        QCOMPARE(cabrillo::modeCode(QStringLiteral("CW"), QString()), QStringLiteral("CW"));
        QCOMPARE(cabrillo::modeCode(QStringLiteral("SSB"), QString()), QStringLiteral("PH"));
        QCOMPARE(cabrillo::modeCode(QStringLiteral("FM"), QString()), QStringLiteral("FM"));
        QCOMPARE(cabrillo::modeCode(QStringLiteral("RTTY"), QString()), QStringLiteral("RY"));
        // FT2 e gli altri digitali finiscono tutti in DG.
        QCOMPARE(cabrillo::modeCode(QStringLiteral("MFSK"), QStringLiteral("FT2")), QStringLiteral("DG"));
        QCOMPARE(cabrillo::modeCode(QStringLiteral("MFSK"), QStringLiteral("FT8")), QStringLiteral("DG"));
    }

    void frequencies()
    {
        QCOMPARE(cabrillo::frequencyField(QStringLiteral("14.074"), QStringLiteral("20m")),
                 QStringLiteral("14074"));
        QCOMPARE(cabrillo::frequencyField(QStringLiteral("3.573"), QStringLiteral("80m")),
                 QStringLiteral("3573"));
        // Dai 6 metri in su non va la frequenza, va il numero della banda.
        QCOMPARE(cabrillo::frequencyField(QStringLiteral("50.313"), QStringLiteral("6m")),
                 QStringLiteral("50"));
        QCOMPARE(cabrillo::frequencyField(QStringLiteral("144.174"), QStringLiteral("2m")),
                 QStringLiteral("144"));
        QCOMPARE(cabrillo::frequencyField(QString(), QString()), QStringLiteral("0"));
    }

    void qsoLineHasTheRightColumns()
    {
        const QString line = cabrillo::qsoLine(info(), qso(QStringLiteral("DL9ZZT"), QStringLiteral("20m"),
                                                           QStringLiteral("14.074"), QStringLiteral("CW")));
        QVERIFY(line.startsWith(QStringLiteral("QSO: ")));
        // QSO: <5 freq> <2 modo> <10 data> <4 ora> <13 call> <3 rst> <6 scambio> ...
        QCOMPARE(line.mid(5, 5), QStringLiteral("14074"));
        QCOMPARE(line.mid(11, 2), QStringLiteral("CW"));
        QCOMPARE(line.mid(14, 10), QStringLiteral("2026-11-02"));
        QCOMPARE(line.mid(25, 4), QStringLiteral("0711"));
        QCOMPARE(line.mid(30, 13), QStringLiteral("IU8LMC       "));
        QCOMPARE(line.mid(44, 3), QStringLiteral("599"));
        QCOMPARE(line.mid(48, 6), QStringLiteral("001   "));
        QCOMPARE(line.mid(55, 13), QStringLiteral("DL9ZZT       "));
        QCOMPARE(line.mid(69, 3), QStringLiteral("599"));
        QCOMPARE(line.mid(73, 6), QStringLiteral("034   "));
    }

    void qsoWithoutDateIsSkipped()
    {
        AdifRecord broken;
        broken.set(QStringLiteral("CALL"), QStringLiteral("DL9ZZT"));
        QVERIFY(cabrillo::qsoLine(info(), broken).isEmpty());
    }

    void wholeFile()
    {
        QString error;
        const QByteArray out = cabrillo::write(info(),
                                               {qso(QStringLiteral("DL9ZZT"), QStringLiteral("20m"),
                                                    QStringLiteral("14.074"), QStringLiteral("CW")),
                                                qso(QStringLiteral("EA5XYZ"), QStringLiteral("40m"),
                                                    QStringLiteral("7.030"), QStringLiteral("CW"))},
                                               &error);
        QVERIFY2(!out.isEmpty(), qPrintable(error));
        const QString text = QString::fromUtf8(out);
        QVERIFY(text.startsWith(QStringLiteral("START-OF-LOG: 3.0\n")));
        QVERIFY(text.contains(QStringLiteral("CONTEST: CQ-WW-CW\n")));
        QVERIFY(text.contains(QStringLiteral("CALLSIGN: IU8LMC\n")));
        QVERIFY(text.contains(QStringLiteral("CATEGORY-OPERATOR: SINGLE-OP\n")));
        QVERIFY(text.contains(QStringLiteral("CREATED-BY: DecoLog")));
        QVERIFY(text.endsWith(QStringLiteral("END-OF-LOG:\n")));
        QCOMPARE(text.count(QStringLiteral("QSO: ")), 2);
    }

    void refusesWithoutContestOrCall()
    {
        cabrillo::Info i = info();
        i.contest.clear();
        QString error;
        QVERIFY(cabrillo::write(i, {}, &error).isEmpty());
        QVERIFY(!error.isEmpty());

        i = info();
        i.callsign.clear();
        error.clear();
        QVERIFY(cabrillo::write(i, {}, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestCabrillo)
#include "tst_cabrillo.moc"
