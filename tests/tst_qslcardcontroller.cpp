// I campi di serie della cartolina QSL: dove finiscono, e cosa succede a
// premere il pulsante due volte.
//
// Le posizioni sono misurate sulla cartolina vera — righe e colonne della
// tabella trovate guardando i pixel — e qui si controlla che cadano dentro i
// riquadri, non che siano un certo numero: un numero non direbbe niente.
#include "app/QslCardController.h"

#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace decolog::app;

namespace {

// I riquadri della cartolina di prova (1920 x 1080), in frazione: la fascia del
// nominativo e le sette caselle della tabella.
struct Box {
    const char* key;
    double left, right, top, bottom;
};

const Box kBoxes[] = {
    // "Confirming QSO/SWL to:" finisce a 1253, la fascia arriva a 1910.
    {"call",  1253 / 1920.0, 1910 / 1920.0, 518 / 1080.0, 604 / 1080.0},
    // La riga vuota della tabella: da 738 a 851, colonne fra le righe verticali.
    {"day",      3 / 1920.0,  274 / 1920.0, 738 / 1080.0, 851 / 1080.0},
    {"month",  274 / 1920.0,  554 / 1920.0, 738 / 1080.0, 851 / 1080.0},
    {"year",   554 / 1920.0,  779 / 1920.0, 738 / 1080.0, 851 / 1080.0},
    {"time",   779 / 1920.0, 1029 / 1920.0, 738 / 1080.0, 851 / 1080.0},
    {"freq",  1029 / 1920.0, 1306 / 1920.0, 738 / 1080.0, 851 / 1080.0},
    {"mode",  1306 / 1920.0, 1589 / 1920.0, 738 / 1080.0, 851 / 1080.0},
    {"rst",   1589 / 1920.0, 1916 / 1920.0, 738 / 1080.0, 851 / 1080.0},
};

QVariantMap fieldFor(const QVariantList& fields, const QString& key)
{
    for (const QVariant& value : fields) {
        const QVariantMap field = value.toMap();
        if (field.value(QStringLiteral("key")).toString() == key)
            return field;
    }
    return {};
}

} // namespace

class TestQslCardController : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private slots:
    void initTestCase()
    {
        // Le impostazioni vanno in un temporaneo: una prova non tocca quelle di
        // chi la sta facendo girare.
        QVERIFY(m_dir.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("DecodiumTest"));
        QCoreApplication::setApplicationName(QStringLiteral("QslCardTest"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
    }

    void init()
    {
        QSettings().clear();
    }

    void theUsualFieldsLandInTheirBoxes()
    {
        QslCardController cards{QslCardController::Context{}};
        cards.addStandardCardFields();

        const QVariantList fields = cards.cardFields();
        QCOMPARE(fields.size(), 8);

        for (const Box& box : kBoxes) {
            const QVariantMap field = fieldFor(fields, QString::fromLatin1(box.key));
            QVERIFY2(!field.isEmpty(), box.key);
            const double x = field.value(QStringLiteral("x")).toDouble();
            const double y = field.value(QStringLiteral("y")).toDouble();
            QVERIFY2(x > box.left && x < box.right,
                     qPrintable(QStringLiteral("%1: x %2 fuori da %3..%4")
                                    .arg(QLatin1String(box.key)).arg(x).arg(box.left).arg(box.right)));
            // La y e' il bordo di sopra del testo: deve stare nel riquadro, e il
            // testo che ne esce (circa 58 pixel su 1080) pure.
            QVERIFY2(y > box.top && y + 58.0 / 1080.0 < box.bottom,
                     qPrintable(QStringLiteral("%1: y %2 fuori da %3..%4")
                                    .arg(QLatin1String(box.key)).arg(y).arg(box.top).arg(box.bottom)));
            // Dentro una casella un dato sta in mezzo.
            QCOMPARE(field.value(QStringLiteral("align")).toString(), QStringLiteral("center"));
        }
    }

    void pressingItTwiceDoesNotDoubleTheFields()
    {
        QslCardController cards{QslCardController::Context{}};
        cards.addStandardCardFields();
        cards.addStandardCardFields();
        cards.addStandardCardFields();
        QCOMPARE(cards.cardFields().size(), 8);
    }

    void aFieldAlreadyThereMovesInsteadOfMultiplying()
    {
        QslCardController cards{QslCardController::Context{}};
        // Uno messo a mano nel posto sbagliato, come capita mentre si prova.
        cards.addCardField(QStringLiteral("rst"));
        cards.moveCardField(0, 0.05, 0.05);
        QCOMPARE(cards.cardFields().size(), 1);

        cards.addStandardCardFields();
        // Non se ne trova un secondo: quello che c'era si e' spostato.
        QCOMPARE(cards.cardFields().size(), 8);
        const QVariantMap rst = fieldFor(cards.cardFields(), QStringLiteral("rst"));
        QVERIFY(rst.value(QStringLiteral("x")).toDouble() > 0.8);
    }

    void whatWasPutByHandAndIsNotUsualStays()
    {
        QslCardController cards{QslCardController::Context{}};
        cards.addCardField(QStringLiteral("text"));
        cards.updateCardField(0, {{QStringLiteral("text"), QStringLiteral("TNX FB QSO")}});
        cards.addStandardCardFields();

        // Gli otto di serie piu' il testo libero, che nessuno ha chiesto di
        // toccare.
        QCOMPARE(cards.cardFields().size(), 9);
        const QVariantMap free = fieldFor(cards.cardFields(), QStringLiteral("text"));
        QCOMPARE(free.value(QStringLiteral("text")).toString(), QStringLiteral("TNX FB QSO"));
    }
};

QTEST_MAIN(TestQslCardController)
#include "tst_qslcardcontroller.moc"
