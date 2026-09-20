// DecoDXLog — il lato log del sync: cosa parte, cosa si scrive quando arriva, e
// chi vince quando due dispositivi hanno scritto sullo stesso QSO.
#include "core/CloudSync.h"
#include "core/LogDatabase.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace decolog::core;

namespace {

AdifRecord qso(const QString& call, const QString& time = QStringLiteral("183000"),
               const QString& name = {})
{
    AdifRecord r;
    r.set(QStringLiteral("CALL"), call);
    r.set(QStringLiteral("QSO_DATE"), QStringLiteral("20260918"));
    r.set(QStringLiteral("TIME_ON"), time);
    r.set(QStringLiteral("BAND"), QStringLiteral("20m"));
    r.set(QStringLiteral("MODE"), QStringLiteral("MFSK"));
    r.set(QStringLiteral("SUBMODE"), QStringLiteral("FT2"));
    if (!name.isEmpty())
        r.set(QStringLiteral("NAME"), name);
    return r;
}

// Un server che risponde sempre come gli si dice: serve per provare cosa fa
// DecoDXLog davanti a un Cloud piu' vecchio di lui.
class FakeServer : public QObject {
    Q_OBJECT

public:
    explicit FakeServer(const QByteArray& response)
        : m_response(response)
    {
        m_server.listen(QHostAddress::LocalHost, 0);
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* client = m_server.nextPendingConnection();
            connect(client, &QTcpSocket::readyRead, client, [this, client] {
                client->readAll();
                client->write(m_response);
                client->disconnectFromHost();
            });
        });
    }

    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort())); }

private:
    QTcpServer m_server;
    QByteArray m_response;
};

} // namespace

class TestCloud : public QObject {
    Q_OBJECT

private slots:
    void newQsosAreQueued()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        const auto first = db.insertQso(qso(QStringLiteral("DL9ZZT")), QStringLiteral("udp"));
        QCOMPARE(first.status, InsertResult::Status::Inserted);

        const QList<qint64> queue = db.dirtyQsos();
        QCOMPARE(queue.size(), 1);
        QCOMPARE(queue.first(), first.id);

        // Spedito: esce dalla coda, con la revisione che dice il server.
        QVERIFY(db.markSynced(first.id, 3));
        QVERIFY(db.dirtyQsos().isEmpty());
        QCOMPARE(db.meta(first.id)->revision, 3);
    }

    void theRecordCarriesWhatTheServerNeeds()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        const auto inserted = db.insertQso(qso(QStringLiteral("EA5XYZ")), QStringLiteral("udp"));
        const QVariantMap record = db.syncRecord(inserted.id);

        QVERIFY(!record.value(QStringLiteral("uuid")).toString().isEmpty());
        QCOMPARE(record.value(QStringLiteral("call")).toString(), QStringLiteral("EA5XYZ"));
        QCOMPARE(record.value(QStringLiteral("band")).toString(), QStringLiteral("20m"));
        QCOMPARE(record.value(QStringLiteral("submode")).toString(), QStringLiteral("FT2"));
        QVERIFY(!record.value(QStringLiteral("deleted")).toBool());
        // I campi ADIF viaggiano tutti, senza che il server debba conoscerli.
        const QVariantMap fields = record.value(QStringLiteral("fields")).toMap();
        QCOMPARE(fields.value(QStringLiteral("CALL")).toString(), QStringLiteral("EA5XYZ"));
        QCOMPARE(fields.value(QStringLiteral("SUBMODE")).toString(), QStringLiteral("FT2"));
    }

    void whatArrivesFromTheCloudIsNotQueuedAgain()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        const QVariantMap remote{
            {QStringLiteral("uuid"), QStringLiteral("11111111-1111-1111-1111-111111111111")},
            {QStringLiteral("revision"), 4},
            {QStringLiteral("deleted"), false},
            {QStringLiteral("fields"), QVariantMap{{QStringLiteral("CALL"), QStringLiteral("JA1QRP")},
                                                   {QStringLiteral("QSO_DATE"), QStringLiteral("20260918")},
                                                   {QStringLiteral("TIME_ON"), QStringLiteral("120000")},
                                                   {QStringLiteral("BAND"), QStringLiteral("15m")},
                                                   {QStringLiteral("MODE"), QStringLiteral("CW")}}},
        };
        QVERIFY(db.applyRemote(remote) != LogDatabase::RemoteResult::Failed);

        const qint64 id = db.idForUuid(QStringLiteral("11111111-1111-1111-1111-111111111111"));
        QVERIFY(id > 0);
        const auto meta = db.meta(id);
        QCOMPARE(meta->revision, 4);
        QVERIFY(!meta->dirty);
        QVERIFY(db.dirtyQsos().isEmpty());
    }

    void aLocalChangeIsNotOverwrittenByAnOlderRemote()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        const auto inserted = db.insertQso(qso(QStringLiteral("W1AW"), QStringLiteral("100000"),
                                               QStringLiteral("Locale")),
                                           QStringLiteral("udp"));
        const QString uuid = db.meta(inserted.id)->uuid;

        // Il server manda la stessa revisione con un altro contenuto: qui c'e'
        // una modifica ancora da spedire, e non si tocca.
        const QVariantMap remote{
            {QStringLiteral("uuid"), uuid},
            {QStringLiteral("revision"), db.meta(inserted.id)->revision},
            {QStringLiteral("fields"), QVariantMap{{QStringLiteral("CALL"), QStringLiteral("W1AW")},
                                                   {QStringLiteral("QSO_DATE"), QStringLiteral("20260918")},
                                                   {QStringLiteral("TIME_ON"), QStringLiteral("100000")},
                                                   {QStringLiteral("BAND"), QStringLiteral("20m")},
                                                   {QStringLiteral("MODE"), QStringLiteral("MFSK")},
                                                   {QStringLiteral("NAME"), QStringLiteral("Remoto")}}},
        };
        QCOMPARE(db.applyRemote(remote), LogDatabase::RemoteResult::Skipped);
        QCOMPARE(db.record(inserted.id)->value(QStringLiteral("NAME")), QStringLiteral("Locale"));
        QCOMPARE(db.dirtyQsos().size(), 1);
    }

    void aNewerRemoteWinsAndTheOldVersionStaysInHistory()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        const auto inserted = db.insertQso(qso(QStringLiteral("OH2BH"), QStringLiteral("110000"),
                                               QStringLiteral("Locale")),
                                           QStringLiteral("udp"));
        const QString uuid = db.meta(inserted.id)->uuid;
        db.markSynced(inserted.id, 1);

        const QVariantMap remote{
            {QStringLiteral("uuid"), uuid},
            {QStringLiteral("revision"), 7},
            {QStringLiteral("fields"), QVariantMap{{QStringLiteral("CALL"), QStringLiteral("OH2BH")},
                                                   {QStringLiteral("QSO_DATE"), QStringLiteral("20260918")},
                                                   {QStringLiteral("TIME_ON"), QStringLiteral("110000")},
                                                   {QStringLiteral("BAND"), QStringLiteral("20m")},
                                                   {QStringLiteral("MODE"), QStringLiteral("MFSK")},
                                                   {QStringLiteral("NAME"), QStringLiteral("Remoto")}}},
        };
        QCOMPARE(db.applyRemote(remote), LogDatabase::RemoteResult::Updated);
        QCOMPARE(db.record(inserted.id)->value(QStringLiteral("NAME")), QStringLiteral("Remoto"));
        QCOMPARE(db.meta(inserted.id)->revision, 7);
        QVERIFY(!db.meta(inserted.id)->dirty);
        // La versione di prima non e' sparita.
        QVERIFY(!db.history(inserted.id).isEmpty());
    }

    void aRemoteDeleteRemovesTheQsoHere()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        const auto inserted = db.insertQso(qso(QStringLiteral("SP9XYZ")), QStringLiteral("udp"));
        const QString uuid = db.meta(inserted.id)->uuid;
        db.markSynced(inserted.id, 1);

        QCOMPARE(db.applyRemote(QVariantMap{{QStringLiteral("uuid"), uuid},
                                            {QStringLiteral("revision"), 2},
                                            {QStringLiteral("deleted"), true},
                                            {QStringLiteral("fields"), QVariantMap{}}}),
                 LogDatabase::RemoteResult::Deleted);
        QVERIFY(db.meta(inserted.id)->deleted);
        QCOMPARE(db.qsoCount(), 0);
        // Una cancellazione di un QSO che qui non c'e' mai stato non fa niente.
        QCOMPARE(db.applyRemote(QVariantMap{{QStringLiteral("uuid"), QStringLiteral("mai-visto")},
                                            {QStringLiteral("revision"), 1},
                                            {QStringLiteral("deleted"), true},
                                            {QStringLiteral("fields"), QVariantMap{}}}),
                 LogDatabase::RemoteResult::Skipped);
    }

    void theSameRevisionComingBackChangesNothing()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        const auto inserted = db.insertQso(qso(QStringLiteral("F5DEF"), QStringLiteral("090000")),
                                           QStringLiteral("udp"));
        const QString uuid = db.meta(inserted.id)->uuid;
        db.markSynced(inserted.id, 1);
        const int historyBefore = static_cast<int>(db.history(inserted.id).size());

        // Il giro dopo una spinta il server ci rimanda i nostri: stessa
        // revisione, quindi non e' una modifica e non deve diventarlo.
        const QVariantMap mine{
            {QStringLiteral("uuid"), uuid},
            {QStringLiteral("revision"), 1},
            {QStringLiteral("fields"), QVariantMap{{QStringLiteral("CALL"), QStringLiteral("F5DEF")},
                                                   {QStringLiteral("QSO_DATE"), QStringLiteral("20260918")},
                                                   {QStringLiteral("TIME_ON"), QStringLiteral("090000")},
                                                   {QStringLiteral("BAND"), QStringLiteral("20m")},
                                                   {QStringLiteral("MODE"), QStringLiteral("MFSK")}}},
        };
        QCOMPARE(db.applyRemote(mine), LogDatabase::RemoteResult::Skipped);
        QCOMPARE(db.meta(inserted.id)->revision, 1);
        QCOMPARE(static_cast<int>(db.history(inserted.id).size()), historyBefore);
        QVERIFY(db.dirtyQsos().isEmpty());
    }

    void theServerUuidWins()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        const auto mine = db.insertQso(qso(QStringLiteral("LZ1AB")), QStringLiteral("udp"));

        // Il server dice: quel collegamento da me sta sotto un altro uuid.
        QVERIFY(db.adoptUuid(mine.id, QStringLiteral("22222222-2222-2222-2222-222222222222")));
        QCOMPARE(db.meta(mine.id)->uuid, QStringLiteral("22222222-2222-2222-2222-222222222222"));
        QVERIFY(!db.meta(mine.id)->dirty);
    }

    void theSyncStateIsRemembered()
    {
        LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        db.setSyncState(QStringLiteral("IU8LMC"), {{QStringLiteral("cursor"), QStringLiteral("42")},
                                                   {QStringLiteral("lastPull"), QStringLiteral("2026-09-18 17:00")}});
        QVariantMap state = db.syncState(QStringLiteral("IU8LMC"));
        QCOMPARE(state.value(QStringLiteral("cursor")).toString(), QStringLiteral("42"));

        // Un aggiornamento parziale non cancella quello che c'era.
        db.setSyncState(QStringLiteral("IU8LMC"), {{QStringLiteral("lastPush"), QStringLiteral("2026-09-18 17:05")}});
        state = db.syncState(QStringLiteral("IU8LMC"));
        QCOMPARE(state.value(QStringLiteral("cursor")).toString(), QStringLiteral("42"));
        QCOMPARE(state.value(QStringLiteral("lastPush")).toString(), QStringLiteral("2026-09-18 17:05"));
    }

    // ── Il motivo di un errore ───────────────────────────────────────────────

    void aSentenceFromTheServerIsShownAsItIs()
    {
        QCOMPARE(cloudsync::detailOf(QJsonValue(QStringLiteral("nominativo o password non validi"))),
                 QStringLiteral("nominativo o password non validi"));
    }

    void aListOfValidationErrorsBecomesWords()
    {
        // E' la forma che FastAPI manda quando la richiesta non passa la
        // validazione: un utente con la password corta si vedeva solo
        // "status code 422".
        const auto body = QJsonDocument::fromJson(R"([
            {"type": "string_too_short", "loc": ["body", "password"],
             "msg": "String should have at least 8 characters"}
        ])").array();

        const QString message = cloudsync::detailOf(QJsonValue(body));
        QVERIFY(message.contains(QStringLiteral("password")));
        QVERIFY(message.contains(QStringLiteral("8 characters")));
    }

    void twoThingsWrongAreSaidBoth()
    {
        const auto body = QJsonDocument::fromJson(R"([
            {"loc": ["body", "callsign"], "msg": "troppo corto"},
            {"loc": ["body", "password"], "msg": "troppo corta"}
        ])").array();

        const QString message = cloudsync::detailOf(QJsonValue(body));
        QVERIFY(message.contains(QStringLiteral("callsign")));
        QVERIFY(message.contains(QStringLiteral("password")));
        QVERIFY(message.contains(QStringLiteral(";")));
    }

    void nothingUsefulIsNotAMessage()
    {
        // Senza motivo si lascia parlare l'errore di rete: meglio quello che
        // una riga vuota.
        QVERIFY(cloudsync::detailOf(QJsonValue()).isEmpty());
        QVERIFY(cloudsync::detailOf(QJsonValue(QJsonArray())).isEmpty());
        QVERIFY(cloudsync::detailOf(QJsonValue(QJsonObject())).isEmpty());
    }

    // Un server che non conosce la richiesta risponde 404, e "Not Found" non
    // spiega niente a chi legge: il messaggio deve dire che e' il server a
    // essere vecchio, non il programma a essere rotto.
    void anOldServerSaysSo()
    {
        FakeServer old("HTTP/1.1 404 Not Found\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: 24\r\n"
                       "Connection: close\r\n\r\n"
                       "{\"detail\":\"Not Found\"}");
        CloudSync sync;
        sync.setServer(old.url());
        sync.setToken(QStringLiteral("un-token"));

        QSignalSpy failures(&sync, &CloudSync::failed);
        sync.purge(QStringLiteral("DELETE"));
        QVERIFY(failures.wait(5000));

        const auto error = failures.first().first().value<CloudError>();
        QVERIFY(!error.ok);
        QVERIFY(!error.unauthorized);
        QVERIFY(!error.retryLater);         // riprovare non serve: manca proprio
        QVERIFY(!error.message.contains(QStringLiteral("Not Found")));
        QVERIFY(error.message.contains(QStringLiteral("purge")));
        QVERIFY(error.message.contains(QStringLiteral("updated")));
    }
};

QTEST_GUILESS_MAIN(TestCloud)
#include "tst_cloud.moc"
