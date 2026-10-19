#include "app/StationProfileModel.h"
#include "core/LogDatabase.h"

#include <QTest>
#include <QSqlQuery>

using namespace decolog;

class TestStationProfile : public QObject {
    Q_OBJECT
private slots:
    void reportsConflictingAwardAggregationWithoutChangingProfiles()
    {
        core::LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        core::StationProfile home;
        home.name = QStringLiteral("Home");
        home.stationCallsign = QStringLiteral("IU8LMC");
        home.operatorCall = QStringLiteral("IU8LMC");
        home.myDxcc = 225;
        home.awardAccount = QStringLiteral("DXCC-main");
        home.operatingLocation = QStringLiteral("home");
        home.validFrom = QStringLiteral("2026-01-01");
        home.validTo = QStringLiteral("2026-12-31");
        core::StationProfile portable = home;
        portable.id = 0;
        portable.uuid.clear();
        portable.name = QStringLiteral("Portable");
        portable.stationCallsign = QStringLiteral("IU8LMC/P");
        portable.myDxcc = 100;
        portable.operatorCall = QStringLiteral("OTHER");
        portable.operatingLocation = QStringLiteral("portable");
        const qint64 first = db.saveStationProfile(home);
        const qint64 second = db.saveStationProfile(portable);
        QVERIFY(first > 0 && second > 0);
        app::StationProfileModel model(&db);
        const QVariantList conflicts = model.awardAccountConflicts();
        QCOMPARE(conflicts.size(), 1);
        QVERIFY(conflicts.first().toMap().value(QStringLiteral("requiresReview")).toBool());
        QVERIFY(conflicts.first().toMap().value(QStringLiteral("reason")).toString().contains(QStringLiteral("DXCC")));
        QVERIFY(conflicts.first().toMap().value(QStringLiteral("reason")).toString().contains(QStringLiteral("operators")));
        QCOMPARE(model.count(), 2);
        const QVariantList groups = model.awardAccountGroups();
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups.first().toMap().value(QStringLiteral("profiles")).toStringList().size(), 2);
        QVERIFY(!groups.first().toMap().value(QStringLiteral("compatible")).toBool());
    }

    void normalizesSharedOperatorLocationAndDxccEntities()
    {
        core::LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        core::StationProfile first;
        first.name = QStringLiteral("Home");
        first.stationCallsign = QStringLiteral("IU8LMC");
        first.operatorCall = QStringLiteral("iu8lmc");
        first.operatingLocation = QStringLiteral("home");
        first.myGridsquare = QStringLiteral("JN70");
        first.myDxcc = 225;
        core::StationProfile second = first;
        second.name = QStringLiteral("Portable");
        second.stationCallsign = QStringLiteral("IU8LMC/P");
        second.myRig = QStringLiteral("portable rig");
        const qint64 firstId = db.saveStationProfile(first);
        const qint64 secondId = db.saveStationProfile(second);
        QVERIFY(firstId > 0 && secondId > 0);
        const auto profiles = db.stationProfiles();
        QCOMPARE(profiles.size(), 2);
        QCOMPARE(profiles.at(0).operatorProfileId, profiles.at(1).operatorProfileId);
        QCOMPARE(profiles.at(0).operatingLocationId, profiles.at(1).operatingLocationId);
        QCOMPARE(profiles.at(0).dxccEntityId, 225);
        QCOMPARE(profiles.at(1).dxccEntityId, 225);

        QSqlQuery query(db.connection());
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM operator_profile")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM operating_location")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM dxcc_entity WHERE id = 225")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
    }

    void groupsCompatibleProfilesForSharedAwardAccount()
    {
        core::LogDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        core::StationProfile first;
        first.name = QStringLiteral("Home");
        first.stationCallsign = QStringLiteral("IU8LMC");
        first.awardAccount = QStringLiteral("main");
        first.operatorCall = QStringLiteral("IU8LMC");
        first.operatingLocation = QStringLiteral("home");
        first.validFrom = QStringLiteral("2026-01-01");
        first.validTo = QStringLiteral("2026-01-31");
        auto second = first;
        second.name = QStringLiteral("Portable");
        second.stationCallsign = QStringLiteral("IU8LMC/P");
        second.validFrom = QStringLiteral("2026-02-01");
        second.validTo = QStringLiteral("2026-02-28");
        QVERIFY(db.saveStationProfile(first) > 0);
        QVERIFY(db.saveStationProfile(second) > 0);
        app::StationProfileModel model(&db);
        const QVariantList groups = model.awardAccountGroups();
        QCOMPARE(groups.size(), 1);
        QVERIFY(groups.first().toMap().value(QStringLiteral("compatible")).toBool());
        QCOMPARE(groups.first().toMap().value(QStringLiteral("conflicts")).toStringList().size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestStationProfile)
#include "tst_stationprofile.moc"
