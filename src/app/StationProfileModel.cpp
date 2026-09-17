#include "app/StationProfileModel.h"

#include "core/LogDatabase.h"

#include <QSettings>

namespace decolog::app {

using core::StationProfile;

namespace {

QVariantMap toMap(const StationProfile& p)
{
    return {
        {QStringLiteral("id"), p.id},
        {QStringLiteral("uuid"), p.uuid},
        {QStringLiteral("name"), p.name},
        {QStringLiteral("stationCallsign"), p.stationCallsign},
        {QStringLiteral("operatorCall"), p.operatorCall},
        {QStringLiteral("myGridsquare"), p.myGridsquare},
        {QStringLiteral("myCqZone"), p.myCqZone},
        {QStringLiteral("myItuZone"), p.myItuZone},
        {QStringLiteral("myDxcc"), p.myDxcc},
        {QStringLiteral("myRig"), p.myRig},
        {QStringLiteral("myAntenna"), p.myAntenna},
        {QStringLiteral("defaultTxPwr"), p.defaultTxPwr},
        {QStringLiteral("lotwStationLocation"), p.lotwStationLocation},
        {QStringLiteral("isDefault"), p.isDefault},
        {QStringLiteral("deleted"), p.deleted},
        {QStringLiteral("dirty"), p.dirty},
        {QStringLiteral("revision"), p.revision},
        {QStringLiteral("qsoCount"), p.qsoCount},
    };
}

QString subtitleFor(const StationProfile& p)
{
    QStringList parts;
    if (!p.operatorCall.isEmpty() && p.operatorCall != p.stationCallsign)
        parts << QStringLiteral("op %1").arg(p.operatorCall);
    else
        parts << p.stationCallsign;
    if (p.defaultTxPwr > 0)
        parts << QStringLiteral("%1 W").arg(p.defaultTxPwr);
    if (!p.myAntenna.isEmpty())
        parts << p.myAntenna;
    else if (!p.myGridsquare.isEmpty())
        parts << p.myGridsquare;
    return parts.join(QStringLiteral(" · "));
}

} // namespace

StationProfileModel::StationProfileModel(core::LogDatabase* db, QObject* parent)
    : QAbstractListModel(parent)
    , m_db(db)
{
    m_activeId = QSettings().value(QStringLiteral("station/activeProfile"), 0).toLongLong();
    reload();
}

int StationProfileModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_profiles.size());
}

QVariant StationProfileModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_profiles.size())
        return {};
    const StationProfile& p = m_profiles.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:            return p.name;
    case IdRole:              return p.id;
    case UuidRole:            return p.uuid;
    case StationCallsignRole: return p.stationCallsign;
    case OperatorRole:        return p.operatorCall;
    case GridRole:            return p.myGridsquare;
    case CqZoneRole:          return p.myCqZone;
    case ItuZoneRole:         return p.myItuZone;
    case DxccRole:            return p.myDxcc;
    case RigRole:             return p.myRig;
    case AntennaRole:         return p.myAntenna;
    case PowerRole:           return p.defaultTxPwr;
    case LotwLocationRole:    return p.lotwStationLocation;
    case IsDefaultRole:       return p.isDefault;
    case DeletedRole:         return p.deleted;
    case DirtyRole:           return p.dirty;
    case RevisionRole:        return p.revision;
    case QsoCountRole:        return p.qsoCount;
    case SubtitleRole:        return subtitleFor(p);
    default:                  return {};
    }
}

QHash<int, QByteArray> StationProfileModel::roleNames() const
{
    return {
        {IdRole, "profileId"}, {NameRole, "name"}, {StationCallsignRole, "stationCallsign"},
        {OperatorRole, "operatorCall"}, {GridRole, "myGridsquare"}, {CqZoneRole, "myCqZone"},
        {ItuZoneRole, "myItuZone"}, {DxccRole, "myDxcc"}, {RigRole, "myRig"}, {AntennaRole, "myAntenna"},
        {PowerRole, "defaultTxPwr"}, {LotwLocationRole, "lotwStationLocation"}, {IsDefaultRole, "isDefault"},
        {DeletedRole, "deleted"}, {DirtyRole, "dirty"}, {RevisionRole, "revision"},
        {QsoCountRole, "qsoCount"}, {SubtitleRole, "subtitle"}, {UuidRole, "uuid"},
    };
}

void StationProfileModel::reload()
{
    beginResetModel();
    m_profiles = m_db && m_db->isOpen() ? m_db->stationProfiles(true) : QList<StationProfile>{};
    endResetModel();
    emit countChanged();
    emit profilesChanged();
    pickActive();
}

// Il profilo attivo salvato, se esiste ancora; altrimenti il predefinito;
// altrimenti il primo non cancellato.
void StationProfileModel::pickActive()
{
    qint64 chosen = 0;
    for (const auto& p : m_profiles) {
        if (p.id == m_activeId && !p.deleted)
            chosen = p.id;
    }
    if (chosen == 0) {
        for (const auto& p : m_profiles) {
            if (p.isDefault && !p.deleted)
                chosen = p.id;
        }
    }
    if (chosen == 0) {
        for (const auto& p : m_profiles) {
            if (!p.deleted) {
                chosen = p.id;
                break;
            }
        }
    }
    const bool changed = chosen != m_activeId;
    m_activeId = chosen;
    if (changed)
        QSettings().setValue(QStringLiteral("station/activeProfile"), chosen);
    emit activeChanged();
}

void StationProfileModel::setActiveProfileId(qint64 id)
{
    if (id == m_activeId)
        return;
    m_activeId = id;
    QSettings().setValue(QStringLiteral("station/activeProfile"), id);
    emit activeChanged();
}

QVariantMap StationProfileModel::activeProfile() const
{
    return byId(m_activeId);
}

QStringList StationProfileModel::names() const
{
    QStringList out;
    for (const auto& p : m_profiles)
        out << p.name;
    return out;
}

QVariantMap StationProfileModel::get(int row) const
{
    return row >= 0 && row < m_profiles.size() ? toMap(m_profiles.at(row)) : QVariantMap{};
}

QVariantMap StationProfileModel::byId(qint64 id) const
{
    for (const auto& p : m_profiles) {
        if (p.id == id)
            return toMap(p);
    }
    return {};
}

int StationProfileModel::rowForId(qint64 id) const
{
    for (qsizetype i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).id == id)
            return static_cast<int>(i);
    }
    return -1;
}

qint64 StationProfileModel::save(const QVariantMap& m)
{
    StationProfile p;
    p.id = m.value(QStringLiteral("id")).toLongLong();
    p.name = m.value(QStringLiteral("name")).toString().trimmed();
    p.stationCallsign = m.value(QStringLiteral("stationCallsign")).toString().trimmed().toUpper();
    if (p.name.isEmpty() || p.stationCallsign.isEmpty())
        return 0;
    p.operatorCall = m.value(QStringLiteral("operatorCall")).toString();
    p.myGridsquare = m.value(QStringLiteral("myGridsquare")).toString().trimmed();
    p.myCqZone = m.value(QStringLiteral("myCqZone")).toInt();
    p.myItuZone = m.value(QStringLiteral("myItuZone")).toInt();
    p.myDxcc = m.value(QStringLiteral("myDxcc")).toInt();
    p.myRig = m.value(QStringLiteral("myRig")).toString();
    p.myAntenna = m.value(QStringLiteral("myAntenna")).toString();
    p.defaultTxPwr = m.value(QStringLiteral("defaultTxPwr")).toString().replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
    p.lotwStationLocation = m.value(QStringLiteral("lotwStationLocation")).toString();
    p.isDefault = m.value(QStringLiteral("isDefault")).toBool();

    const qint64 id = m_db->saveStationProfile(p);
    if (id > 0) {
        reload();
        if (m_activeId == 0)
            setActiveProfileId(id);
    }
    return id;
}

bool StationProfileModel::remove(qint64 id)
{
    const bool ok = m_db->deleteStationProfile(id);
    if (ok)
        reload();
    return ok;
}

} // namespace decolog::app
