// I campi di serie della cartolina QSL: dove finiscono, e cosa succede a
// premere il pulsante due volte.
//
// Le posizioni sono misurate sulla cartolina vera — righe e colonne della
// tabella trovate guardando i pixel — e qui si controlla che cadano dentro i
// riquadri, non che siano un certo numero: un numero non direbbe niente.
#include "app/QslCardController.h"
#include "core/LogDatabase.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace decolog;
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

// Un Cloud finto: ascolta su una porta qualunque, risponde 200 e si tiene
// quello che ha ricevuto. Serve a provare che il programma parli davvero la
// lingua che il server si aspetta, invece di fidarsi che la parli.
class FakeCloud : public QTcpServer {
public:
    QByteArray request;
    QByteArray body;

    explicit FakeCloud(QObject* parent = nullptr) : QTcpServer(parent)
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* socket = nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                request += socket->readAll();
                const int head = request.indexOf("\r\n\r\n");
                if (head < 0)
                    return;
                // Si aspetta tutto il corpo: una cartolina non sta in un pacchetto.
                body = request.mid(head + 4);
                if (body.size() < contentLength())
                    return;
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                              "Content-Length: 29\r\n\r\n{\"sent\":true,\"remaining\":99}");
                socket->disconnectFromHost();
            });
        });
    }

    QString url() const { return QStringLiteral("http://127.0.0.1:%1").arg(serverPort()); }

    int contentLength() const { return header("Content-Length").toInt(); }

    QByteArray header(const QByteArray& name) const
    {
        const QList<QByteArray> lines = request.left(request.indexOf("\r\n\r\n")).split('\n');
        for (const QByteArray& line : lines) {
            if (line.toLower().startsWith(name.toLower() + ":"))
                return line.mid(name.size() + 1).trimmed();
        }
        return {};
    }
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

    // La via del Cloud
    //
    // La ragione di tutto questo e' che la password di una casella non stia sul
    // computer di chi opera. Quindi la prova guarda proprio quello: che parta
    // una richiesta al Cloud, col token, la cartolina dentro, e nessun segreto.

    void theCloudRouteIsTheOneOutOfTheBox()
    {
        QslCardController cards{QslCardController::Context{}};
        QCOMPARE(cards.mail().value(QStringLiteral("route")).toString(), QStringLiteral("cloud"));
        // Senza Cloud collegato non si manda: non c'e' da dove.
        QCOMPARE(cards.mail().value(QStringLiteral("ready")).toBool(), false);
    }

    void choosingOnesOwnMailboxLooksAtTheMailboxInstead()
    {
        QslCardController::Context ctx;
        ctx.cloudAccess = [] {
            return QPair<QString, QString>{QStringLiteral("http://x"), QStringLiteral("token")};
        };
        QslCardController cards{ctx};
        QCOMPARE(cards.mail().value(QStringLiteral("ready")).toBool(), true);

        cards.setMailRoute(QStringLiteral("mailbox"));
        QCOMPARE(cards.mail().value(QStringLiteral("route")).toString(), QStringLiteral("mailbox"));
        // Il Cloud c'e' lo stesso, ma adesso conta la casella, che non c'e'.
        QCOMPARE(cards.mail().value(QStringLiteral("cloudReady")).toBool(), true);
        QCOMPARE(cards.mail().value(QStringLiteral("ready")).toBool(), false);

        // Una via che non esiste non lascia il programma senza via d'uscita.
        cards.setMailRoute(QStringLiteral("piccioni"));
        QCOMPARE(cards.mail().value(QStringLiteral("route")).toString(), QStringLiteral("cloud"));
    }

    void theCardGoesToTheCloudWithTheTokenAndNoPassword()
    {
        core::LogDatabase db;
        QVERIFY2(db.open(QStringLiteral(":memory:")), qPrintable(db.lastError()));
        const core::InsertResult added = db.insertQso(
            {{"CALL", "dl9zzt"}, {"QSO_DATE", "20260218"}, {"TIME_ON", "101500"},
             {"FREQ", "14.084"}, {"MODE", "FT2"}, {"RST_SENT", "-10"},
             {"STATION_CALLSIGN", "IU8LMC"}},
            QStringLiteral("test"));
        QCOMPARE(added.status, core::InsertResult::Status::Inserted);

        FakeCloud cloud;
        QVERIFY(cloud.listen(QHostAddress::LocalHost));

        QslCardController::Context ctx;
        ctx.db = &db;
        ctx.station = [] {
            return QVariantMap{{QStringLiteral("call"), QStringLiteral("IU8LMC")}};
        };
        ctx.cloudAccess = [&cloud] {
            return QPair<QString, QString>{cloud.url(), QStringLiteral("un-token")};
        };
        ctx.replyTo = [] { return QStringLiteral("iu8lmc@example.it"); };
        ctx.emailFor = [](const QString&, std::function<void(const QString&, const QString&)> done) {
            done(QStringLiteral("dl9zzt@example.de"), QString());
        };

        QslCardController cards{ctx};
        cards.addStandardCardFields();
        cards.enqueue({QVariant(added.id)}, QStringLiteral("E"));

        QSignalSpy done(&cards, &QslCardController::changed);
        cards.sendCardsByEmail({});
        QVERIFY2(done.wait(15000), "il Cloud non ha ricevuto niente");

        const QJsonObject posted = QJsonDocument::fromJson(cloud.body).object();
        QVERIFY(cloud.request.startsWith("POST /v1/qsl/mail "));
        QCOMPARE(cloud.header("Authorization"), QByteArray("Bearer un-token"));
        QCOMPARE(posted.value(QStringLiteral("to")).toString(), QStringLiteral("dl9zzt@example.de"));
        // Chi risponde arriva all'operatore, non alla casella del servizio.
        QCOMPARE(posted.value(QStringLiteral("replyTo")).toString(),
                 QStringLiteral("iu8lmc@example.it"));
        QVERIFY(posted.value(QStringLiteral("attachmentName")).toString()
                    .startsWith(QStringLiteral("DL9ZZT-20260218")));
        // Quello che parte e' una cartolina vera: il server rifiuta tutto il resto.
        const QByteArray png = QByteArray::fromBase64(
            posted.value(QStringLiteral("attachment")).toString().toLatin1());
        QVERIFY(png.startsWith(QByteArray("\x89PNG\r\n\x1a\n", 8)));

        // E il QSO risulta mandato per via elettronica.
        QCOMPARE(cards.rows(QStringLiteral("queue")).size(), 0);
        QCOMPARE(cards.rows(QStringLiteral("sent")).size(), 1);
    }

    void withoutACloudNothingLeavesAndItSaysSo()
    {
        core::LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        const core::InsertResult added = db.insertQso(
            {{"CALL", "dl9zzt"}, {"QSO_DATE", "20260218"}, {"TIME_ON", "101500"},
             {"FREQ", "14.084"}, {"MODE", "FT2"}, {"STATION_CALLSIGN", "IU8LMC"}},
            QStringLiteral("test"));
        QslCardController::Context ctx;
        ctx.db = &db;
        QslCardController cards{ctx};
        cards.addStandardCardFields();
        cards.enqueue({QVariant(added.id)}, QStringLiteral("E"));

        cards.sendCardsByEmail({});
        // Non si prova nemmeno, e non si dice "mandata": si dice cosa manca.
        QVERIFY(!cards.mailStatus().isEmpty());
        QCOMPARE(cards.rows(QStringLiteral("queue")).size(), 1);
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
