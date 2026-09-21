// La cartolina QSL: i campi, quello che ci finisce dentro, e il disegno vero.
//
// Non si prova solo che il file esca: si guarda il pixel. Un campo posato a
// meta' cartolina deve lasciare un segno li' e non altrove, se no l'anteprima
// dice una cosa e la stampante ne fa un'altra.
#include "core/QslDesign.h"

#include <QDir>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace decolog::core;

namespace {

qsldesign::Field field(const QString& key, double x, double y, int size = 100)
{
    qsldesign::Field f;
    f.key = key;
    f.x = x;
    f.y = y;
    f.size = size;
    return f;
}

QVariantMap sampleQso()
{
    return QVariantMap{{"call", "DL9ZZT"}, {"date", "2026-02-08"}, {"time", "2204"},
                       {"band", "40m"},   {"mode", "CW"},         {"freq", "7.0741"},
                       {"rst", "599"},    {"name", "Klaus"}};
}

// Quanti pixel non bianchi ci sono dentro un riquadro dell'immagine: e' il modo
// piu' diretto per sapere se qualcosa e' stato scritto li'.
int inkIn(const QImage& image, const QRect& box)
{
    int ink = 0;
    for (int y = box.top(); y <= box.bottom() && y < image.height(); ++y) {
        for (int x = box.left(); x <= box.right() && x < image.width(); ++x) {
            if (qGray(image.pixel(x, y)) < 128)
                ++ink;
        }
    }
    return ink;
}

} // namespace

class TestQslDesign : public QObject {
    Q_OBJECT

private slots:
    void theValuesComeOutFormatted()
    {
        const QVariantMap qso = sampleQso();
        const QVariantMap station{{"call", "IU8LMC"}, {"grid", "JN71DC"}, {"name", "Martino"}};

        QCOMPARE(qsldesign::valueFor(field("call", 0, 0), qso, station), QString("DL9ZZT"));
        // La data si spezza come la vuole la cartolina, con lo zero davanti.
        QCOMPARE(qsldesign::valueFor(field("day", 0, 0), qso, station), QString("08"));
        QCOMPARE(qsldesign::valueFor(field("month", 0, 0), qso, station), QString("02"));
        QCOMPARE(qsldesign::valueFor(field("monthName", 0, 0), qso, station), QString("FEB"));
        QCOMPARE(qsldesign::valueFor(field("year", 0, 0), qso, station), QString("2026"));
        // L'ora coi due punti, la frequenza a tre decimali.
        QCOMPARE(qsldesign::valueFor(field("time", 0, 0), qso, station), QString("22:04"));
        QCOMPARE(qsldesign::valueFor(field("freq", 0, 0), qso, station), QString("7.074"));
        // I campi "my" vengono dal profilo di stazione, non dal QSO.
        QCOMPARE(qsldesign::valueFor(field("myCall", 0, 0), qso, station), QString("IU8LMC"));
        QCOMPARE(qsldesign::valueFor(field("myGrid", 0, 0), qso, station), QString("JN71DC"));

        qsldesign::Field free = field("text", 0, 0);
        free.text = QStringLiteral("TNX FB QSO");
        QCOMPARE(qsldesign::valueFor(free, qso, station), QString("TNX FB QSO"));
        // Un QSO senza data non inventa niente.
        QVERIFY(qsldesign::valueFor(field("day", 0, 0), QVariantMap{}, station).isEmpty());
    }

    void theLayoutSurvivesAFileAndBadNumbers()
    {
        qsldesign::Card card;
        card.templatePath = QStringLiteral("C:/qsl/mia.jpg");
        card.fields << field("call", 0.25, 0.75, 60);
        card.fields.last().align = QStringLiteral("right");
        card.fields.last().color = QStringLiteral("#c00000");
        card.fields.last().bold = false;

        const qsldesign::Card back = qsldesign::fromJson(qsldesign::toJson(card));
        QCOMPARE(back.templatePath, card.templatePath);
        QCOMPARE(back.fields.size(), 1);
        QCOMPARE(back.fields.first().key, QString("call"));
        QCOMPARE(back.fields.first().x, 0.25);
        QCOMPARE(back.fields.first().y, 0.75);
        QCOMPARE(back.fields.first().size, 60);
        QCOMPARE(back.fields.first().align, QString("right"));
        QCOMPARE(back.fields.first().color, QString("#c00000"));
        QCOMPARE(back.fields.first().bold, false);

        // Un file scritto a mano, o rovinato, non deve far male: quello che e'
        // fuori misura si riporta dentro, e un campo senza chiave si butta.
        const QByteArray wild = QByteArray(
            "{'fields': [{'key': 'call', 'x': 9, 'y': -3, 'size': 9999, 'align': 'sideways',"
            " 'color': 'non un colore'}, {'x': 0.5, 'y': 0.5}]}").replace('\'', '"');
        const qsldesign::Card fixed = qsldesign::fromJson(QJsonDocument::fromJson(wild).object());
        QCOMPARE(fixed.fields.size(), 1);
        QCOMPARE(fixed.fields.first().x, 1.0);
        QCOMPARE(fixed.fields.first().y, 0.0);
        QCOMPARE(fixed.fields.first().size, 400);
        QCOMPARE(fixed.fields.first().align, QString("left"));
        QCOMPARE(fixed.fields.first().color, QString("#000000"));
    }

    void theTextLandsWhereItWasPut()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        qsldesign::Card card;
        // Senza modello la cartolina viene bianca a misura di cartolina vera:
        // cosi' l'inchiostro che si conta e' solo quello del campo.
        card.fields << field("call", 0.5, 0.5, 120);
        card.fields.last().align = QStringLiteral("center");

        QStringList written;
        QString error;
        QVERIFY2(qsldesign::writePng(dir.path(), card, {sampleQso()}, {}, &written, &error),
                 qPrintable(error));
        QCOMPARE(written.size(), 1);
        // Il nome del file dice di chi e' la cartolina e di quando.
        QVERIFY2(written.first().contains(QStringLiteral("DL9ZZT-20260208-2204")),
                 qPrintable(written.first()));

        const QImage card1(written.first());
        QVERIFY(!card1.isNull());
        // 148 x 105 mm a 300 punti per pollice.
        QCOMPARE(card1.width(), 1748);
        QCOMPARE(card1.height(), 1240);

        // Il nominativo sta in mezzo, e solo in mezzo.
        const QRect middle(card1.width() / 2 - 300, card1.height() / 2 - 20, 600, 160);
        QVERIFY2(inkIn(card1, middle) > 200, qPrintable(QString::number(inkIn(card1, middle))));
        QCOMPARE(inkIn(card1, QRect(20, 20, 400, 200)), 0);

        // Spostato in alto a sinistra, l'inchiostro si sposta con lui.
        card.fields.first().x = 0.15;
        card.fields.first().y = 0.08;
        card.fields.first().align = QStringLiteral("left");
        QStringList second;
        QVERIFY2(qsldesign::writePng(dir.filePath(QStringLiteral("due")), card, {sampleQso()},
                                     {}, &second, &error), qPrintable(error));
        const QImage card2(second.first());
        QVERIFY(inkIn(card2, QRect(int(card2.width() * 0.15), int(card2.height() * 0.08), 600, 160)) > 200);
        QCOMPARE(inkIn(card2, middle), 0);
    }

    void thePdfHasOnePageEveryTwoCards()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        qsldesign::Card card;
        card.fields << field("call", 0.5, 0.4, 80);

        const QList<QVariantMap> three{sampleQso(), sampleQso(), sampleQso()};
        const QString path = dir.filePath(QStringLiteral("cartoline.pdf"));
        QString error;
        QVERIFY2(qsldesign::writePdf(path, card, three, {}, 2, &error), qPrintable(error));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray raw = file.readAll();
        QVERIFY(raw.startsWith("%PDF"));
        // Tre cartoline, due per foglio: due fogli.
        QVERIFY2(raw.contains("/Count 2"), "il PDF non ha due pagine");
        // A4, in punti tipografici.
        QVERIFY(raw.contains("595.000000 842.000000"));

        // Senza QSO non si scrive un file vuoto: si dice di no.
        QVERIFY(!qsldesign::writePdf(dir.filePath(QStringLiteral("vuoto.pdf")), card, {}, {}, 2, &error));
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("vuoto.pdf"))));
    }
};

QTEST_MAIN(TestQslDesign)
#include "tst_qsldesign.moc"
