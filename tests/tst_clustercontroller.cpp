// Cluster nell'applicazione: spot arricchiti dal log e dal cty.csv, filtri, regole
// d'avviso e messaggi a Decodium (spot e tune) su un DecoLink vero.
#include "app/ClusterController.h"
#include "core/DecoLinkServer.h"
#include "core/LogDatabase.h"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTest>

using namespace decolog::core;
using decolog::app::ClusterController;
using decolog::app::SpotModel;

namespace {

class Client {
public:
    QTcpSocket socket;
    QList<QJsonObject> received;

    bool connectTo(quint16 port)
    {
        socket.connectToHost(QHostAddress::LocalHost, port);
        return socket.waitForConnected(3000);
    }
    void send(const QJsonObject& o) { socket.write(QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n'); }
    std::optional<QJsonObject> waitFor(const QString& type, int timeoutMs = 3000)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < timeoutMs) {
            for (qsizetype i = m_consumed; i < received.size(); ++i) {
                if (received.at(i).value("type").toString() == type) {
                    m_consumed = i + 1;
                    return received.at(i);
                }
            }
            if (!socket.waitForReadyRead(50))
                QCoreApplication::processEvents();
            m_buffer += socket.readAll();
            qsizetype nl;
            while ((nl = m_buffer.indexOf('\n')) >= 0) {
                received << QJsonDocument::fromJson(m_buffer.left(nl)).object();
                m_buffer.remove(0, nl + 1);
            }
        }
        return std::nullopt;
    }

private:
    QByteArray m_buffer;
    qsizetype m_consumed{0};
};

} // namespace

class TestClusterController : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Impostazioni e cache in una cartella di prova, non in quelle di DecoDXLog.
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName("DecoDXLogTest");
        QCoreApplication::setApplicationName("tst_clustercontroller");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings().clear();
    }

    void spotsAlertsAndDecodium()
    {
        LogDatabase db;
        QVERIFY(db.open(":memory:"));
        db.insertQso({{"CALL", "JA1XX"}, {"QSO_DATE", "20260101"}, {"TIME_ON", "1200"}, {"BAND", "20m"},
                      {"MODE", "FT8"}, {"DXCC", "339"}}, "import");

        Countries countries;
        QFile cty(":/decolog/cty.csv");
        QVERIFY(cty.open(QIODevice::ReadOnly));
        QVERIFY(countries.load(cty.readAll()));

        DecoLinkServer link;
        link.workedRows = [] { return QList<QJsonArray>{}; };
        link.awardState = [] { return QJsonObject{}; };
        link.resolveQuery = [](const QJsonObject&) { return QJsonArray{}; };
        QVERIFY(link.start(0));

        QStringList activity;
        QString looked;
        ClusterController::Context ctx;
        ctx.db = &db;
        ctx.countries = &countries;
        ctx.decoLink = &link;
        ctx.stationCall = [] { return QStringLiteral("IU8LMC"); };
        ctx.stationGrid = [] { return QStringLiteral("JN71DC"); };
        ctx.activity = [&activity](const QString& cat, const QString& text, const QString&) { activity << cat + ": " + text; };
        ctx.lookup = [&looked](const QString& call) { looked = call; };
        ClusterController cluster(std::move(ctx));
        cluster.setMuted(true);

        Client decodium;
        QVERIFY(decodium.connectTo(link.port()));
        decodium.send({{"type", "hello"}, {"app", "Decodium"}, {"version", "test"}, {"protocol", 1}});
        QVERIFY(decodium.waitFor("hello"));

        QSignalSpy alerts(&cluster, &ClusterController::alertRaised);
        auto* model = qobject_cast<SpotModel*>(cluster.spots());
        const QString hhmm = QDateTime::currentDateTimeUtc().toString("HHmm");

        // Nuovo DXCC: regola predefinita, avviso e spot a Decodium con alert.
        cluster.injectLine(QString("DX de EA5WU-#:  14025.1  3Y0J  CW 18 dB 24 WPM CQ  %1Z").arg(hhmm));
        QCOMPARE(model->count(), 1);
        QCOMPARE(alerts.size(), 1);
        const auto spot = decodium.waitFor("spot");
        QVERIFY(spot);
        QCOMPARE(spot->value("call").toString(), QString("3Y0J"));
        QVERIFY(spot->value("alert").toBool());
        QVERIFY(spot->value("statusBits").toInt() & StatusNewDxcc);
        QVERIFY(activity.last().startsWith("CLUSTER:"));

        // Lo stesso DX da un altro skimmer: una riga con due spotter, nessun nuovo avviso.
        cluster.injectLine(QString("DX de DL1ABC-#:  14025.1  3Y0J  CW 22 dB 25 WPM CQ  %1Z").arg(hhmm));
        QCOMPARE(model->count(), 1);
        QCOMPARE(model->get(0).value("spotCount").toInt(), 2);
        QCOMPARE(alerts.size(), 1);

        // Gia' lavorato sulla banda: nessun avviso; il filtro "hide worked" lo nasconde.
        cluster.injectLine(QString("DX de IK8XXX:  14075.0  JA1XX  FT8 -12 dB  %1Z").arg(hhmm));
        QCOMPARE(model->count(), 2);
        QCOMPARE(model->get(0).value("statusLabel").toString(), QString("WORKED"));
        QCOMPARE(alerts.size(), 1);
        QVariantMap f = cluster.filter();
        f["hideWorkedBand"] = true;
        cluster.setFilter(f);
        QCOMPARE(model->count(), 1);

        // Doppio clic: tune con dial e audio per FT8.
        cluster.injectLine(QString("DX de SP9ABC:  18101.5  VK2DEF  FT8 -15 dB  %1Z").arg(hhmm));
        const QString key = model->get(0).value("spotKey").toString();
        QCOMPARE(model->get(0).value("call").toString(), QString("VK2DEF"));
        QVERIFY(model->get(0).value("distance").toInt() > 12000);
        cluster.tune(key);
        QCOMPARE(looked, QString("VK2DEF"));
        const auto tune = decodium.waitFor("tune");
        QVERIFY(tune);
        QCOMPARE(tune->value("dialKhz").toDouble(), 18100.0);
        QCOMPARE(tune->value("audioHz").toInt(), 1500);
        QCOMPARE(tune->value("mode").toString(), QString("FT8"));

        // Un QSO nuovo nel log cambia lo stato degli spot gia' in lista.
        cluster.setFilter({});
        const QString dxcc = model->get(model->count() - 1).value("dxcc").toString();
        db.insertQso({{"CALL", "3Y0J"}, {"QSO_DATE", QDateTime::currentDateTimeUtc().toString("yyyyMMdd")}, {"TIME_ON", hhmm},
                      {"BAND", "20m"}, {"MODE", "CW"}, {"DXCC", dxcc}}, "manual");
        cluster.logChanged();
        QTRY_COMPARE_WITH_TIMEOUT(model->get(model->count() - 1).value("statusLabel").toString(), QString("WORKED"), 4000);
    }
};

QTEST_GUILESS_MAIN(TestClusterController)
#include "tst_clustercontroller.moc"
