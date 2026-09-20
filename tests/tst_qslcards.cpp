// DecoDXLog — le etichette delle QSL di carta: raggruppamento e PDF.
#include "core/QslCards.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTest>

using namespace decolog::core;

class TestQslCards : public QObject {
    Q_OBJECT

private:
    static QVariantMap qso(const QString& call, const QString& date, const QString& via = {})
    {
        return QVariantMap{{QStringLiteral("call"), call},
                           {QStringLiteral("date"), date},
                           {QStringLiteral("time"), QStringLiteral("1830")},
                           {QStringLiteral("band"), QStringLiteral("20m")},
                           {QStringLiteral("mode"), QStringLiteral("FT2")},
                           {QStringLiteral("rst"), QStringLiteral("599")},
                           {QStringLiteral("via"), via}};
    }

private slots:
    void sheetsAreThere()
    {
        const QList<qslcard::Sheet> all = qslcard::sheets();
        QVERIFY(all.size() >= 3);
        // Un id sconosciuto non lascia senza foglio: si ripiega sul primo.
        QCOMPARE(qslcard::sheetById(QStringLiteral("boh")).id, all.first().id);
        QCOMPARE(qslcard::sheetById(QStringLiteral("l7163")).columns, 2);
    }

    void oneLabelPerCorrespondent()
    {
        const QList<QVariantMap> qsos{qso("IK0ABC", "2026-01-02"), qso("IK0ABC", "2026-01-03"),
                                      qso("DL9ZZT", "2026-01-04", "DL1MGR"), qso("IK0ABC", "2026-01-05")};
        const QList<qslcard::Label> labels = qslcard::group(qsos, 4);
        QCOMPARE(labels.size(), 2);
        QCOMPARE(labels[0].call, QStringLiteral("IK0ABC"));
        QCOMPARE(labels[0].lines.size(), 3);
        QCOMPARE(labels[1].call, QStringLiteral("DL9ZZT"));
        QCOMPARE(labels[1].via, QStringLiteral("DL1MGR"));
    }

    void spillsOverToAnotherLabel()
    {
        QList<QVariantMap> qsos;
        for (int i = 0; i < 5; ++i)
            qsos << qso(QStringLiteral("IK0ABC"), QStringLiteral("2026-01-0%1").arg(i + 1));
        const QList<qslcard::Label> labels = qslcard::group(qsos, 2);
        QCOMPARE(labels.size(), 3);
        QCOMPARE(labels[0].lines.size(), 2);
        QCOMPARE(labels[2].lines.size(), 1);
        for (const qslcard::Label& l : labels)
            QCOMPARE(l.call, QStringLiteral("IK0ABC"));
    }

    void writesAPdf()
    {
        const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(QStringLiteral("decolog-test-labels.pdf"));
        QFile::remove(path);
        const QList<qslcard::Label> labels = qslcard::group(
            {qso("IK0ABC", "2026-01-02"), qso("DL9ZZT", "2026-01-04")}, 4);
        QString error;
        const QVariantMap station{{QStringLiteral("call"), QStringLiteral("IU8LMC")},
                                  {QStringLiteral("grid"), QStringLiteral("JN70")}};
        QVERIFY2(qslcard::writePdf(path, labels, qslcard::sheetById(QStringLiteral("l7160")),
                                   station, true, &error),
                 qPrintable(error));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.size() > 500);
        QCOMPARE(file.read(4), QByteArray("%PDF"));
        file.close();
        QFile::remove(path);
    }

    void noLabelsNoFile()
    {
        QString error;
        const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(QStringLiteral("decolog-test-empty.pdf"));
        QFile::remove(path);
        QVERIFY(!qslcard::writePdf(path, {}, qslcard::sheetById(QString()), {}, false, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(path));
    }
};

QTEST_MAIN(TestQslCards)
#include "tst_qslcards.moc"
