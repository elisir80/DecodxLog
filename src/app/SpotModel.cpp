#include "app/SpotModel.h"

#include <QCoreApplication>

namespace decolog::app {

using namespace decolog::core;

namespace {

constexpr int kKeepMinutes = 60;
constexpr int kMaxSpots = 3000;

QString refsOf(const Spot& s)
{
    QStringList refs;
    if (!s.potaRef.isEmpty()) refs << QStringLiteral("POTA ") + s.potaRef;
    if (!s.sotaRef.isEmpty()) refs << QStringLiteral("SOTA ") + s.sotaRef;
    if (!s.wwffRef.isEmpty()) refs << QStringLiteral("WWFF ") + s.wwffRef;
    if (!s.iotaRef.isEmpty()) refs << QStringLiteral("IOTA ") + s.iotaRef;
    return refs.join(QStringLiteral(" · "));
}

} // namespace

SpotModel::SpotModel(QObject* parent)
    : QAbstractListModel(parent)
{
    // Ogni minuto: via gli spot vecchi, e l'eta' mostrata cambia.
    m_expiry.setInterval(60'000);
    connect(&m_expiry, &QTimer::timeout, this, &SpotModel::expire);
    m_expiry.start();
}

QString SpotModel::statusLabel(int status)
{
    if (status & StatusNewDxcc) return QStringLiteral("NEW DXCC");
    if (status & StatusNewBand) return QStringLiteral("NEW BAND");
    if (status & StatusNewMode) return QStringLiteral("NEW MODE");
    if (status & StatusNewSlot) return QStringLiteral("NEW SLOT");
    if (status & StatusWorkedBand) return QStringLiteral("WORKED");
    if (status & StatusNewCall) return QStringLiteral("NEW CALL");
    return {};
}

int SpotModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant SpotModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const EnrichedSpot& e = *m_rows.at(index.row());
    const Spot& s = e.spot;
    switch (role) {
    case KeyRole:         return e.key();
    case Qt::DisplayRole:
    case CallRole:        return s.dxCall;
    case FreqRole:        return QString::number(s.freqKhz, 'f', 1);
    case BandRole:        return s.band;
    case ModeRole:        return s.mode;
    case TimeRole:        return s.time.toString(QStringLiteral("HH:mm"));
    case AgeRole:         return static_cast<int>(s.time.secsTo(QDateTime::currentDateTimeUtc()) / 60);
    case SpotterRole:     return s.spotter;
    case SpottersRole:    return e.spotters.join(QStringLiteral(", "));
    case CountRole:       return e.count;
    case CommentRole:     return s.comment;
    case SourceRole:      return s.source;
    case SourceNameRole:  return s.sourceName;
    case EntityRole:      return e.entity;
    case DxccRole:        return e.dxcc;
    case ContinentRole:   return e.continent;
    case CqZoneRole:      return e.cqZone;
    case StatusRole:      return e.status;
    case StatusLabelRole: return statusLabel(e.status);
    case DistanceRole:    return e.distanceKm >= 0 ? QVariant(qRound(e.distanceKm)) : QVariant();
    case AzimuthRole:     return e.azimuth >= 0 ? QVariant(e.azimuth) : QVariant();
    case SnrRole:         return s.hasSnr ? QVariant(s.snr) : QVariant();
    case RefsRole:        return refsOf(s);
    case GridRole:        return s.dxGrid;
    case LotwRole:        return bool(e.status & StatusLotwUser);
    case SkimmerRole:     return s.isSkimmer();
    case FreshRole:       return e.firstSeen.secsTo(QDateTime::currentDateTimeUtc()) < 90;
    default:              return {};
    }
}

QHash<int, QByteArray> SpotModel::roleNames() const
{
    return {
        {KeyRole, "spotKey"}, {CallRole, "call"}, {FreqRole, "freq"}, {BandRole, "band"}, {ModeRole, "mode"},
        {TimeRole, "time"}, {AgeRole, "age"}, {SpotterRole, "spotter"}, {SpottersRole, "spotters"},
        {CountRole, "spotCount"}, {CommentRole, "comment"}, {SourceRole, "source"}, {SourceNameRole, "sourceName"},
        {EntityRole, "entity"}, {DxccRole, "dxcc"}, {ContinentRole, "continent"}, {CqZoneRole, "cqZone"},
        {StatusRole, "status"}, {StatusLabelRole, "statusLabel"}, {DistanceRole, "distance"},
        {AzimuthRole, "azimuth"}, {SnrRole, "snr"}, {RefsRole, "refs"}, {GridRole, "grid"},
        {LotwRole, "lotw"}, {SkimmerRole, "skimmer"}, {FreshRole, "fresh"},
    };
}

bool SpotModel::visibleEntry(const Entry& e) const
{
    return m_filter.matches(*e, QDateTime::currentDateTimeUtc());
}

const EnrichedSpot& SpotModel::add(const EnrichedSpot& incoming, bool* isNew)
{
    const QString key = incoming.key();
    Entry entry = m_byKey.value(key);
    const bool fresh = !entry;
    if (isNew)
        *isNew = fresh;

    if (entry) {
        // Stesso DX: si toglie dalla posizione vecchia e si rimette in cima.
        const qsizetype row = m_rows.indexOf(entry);
        if (row >= 0) {
            beginRemoveRows({}, static_cast<int>(row), static_cast<int>(row));
            m_rows.removeAt(row);
            endRemoveRows();
        }
        m_all.removeOne(entry);
        const QStringList spotters = entry->spotters;
        const int count = entry->count;
        const QDateTime firstSeen = entry->firstSeen;
        Spot previous = entry->spot;
        *entry = incoming;
        entry->firstSeen = firstSeen;
        entry->spotters = spotters;
        entry->count = count;
        if (!entry->spotters.contains(incoming.spot.spotter)) {
            entry->spotters.prepend(incoming.spot.spotter);
            ++entry->count;
        }
        // Un commento vuoto (skimmer) non cancella quello utile di prima.
        if (entry->spot.comment.isEmpty())
            entry->spot.comment = previous.comment;
        if (entry->spot.potaRef.isEmpty()) entry->spot.potaRef = previous.potaRef;
        if (entry->spot.sotaRef.isEmpty()) entry->spot.sotaRef = previous.sotaRef;
        if (entry->spot.wwffRef.isEmpty()) entry->spot.wwffRef = previous.wwffRef;
        if (entry->spot.iotaRef.isEmpty()) entry->spot.iotaRef = previous.iotaRef;
        if (entry->spot.dxGrid.isEmpty()) entry->spot.dxGrid = previous.dxGrid;
        if (entry->spot.time < previous.time)
            entry->spot.time = previous.time;
    } else {
        entry = std::make_shared<EnrichedSpot>(incoming);
        entry->firstSeen = QDateTime::currentDateTimeUtc();
        entry->spotters = QStringList{incoming.spot.spotter};
        entry->count = 1;
        m_byKey.insert(key, entry);
    }
    m_all.prepend(entry);

    if (visibleEntry(entry)) {
        beginInsertRows({}, 0, 0);
        m_rows.prepend(entry);
        endInsertRows();
    }
    while (m_all.size() > kMaxSpots) {
        const Entry last = m_all.takeLast();
        m_byKey.remove(last->key());
        const qsizetype row = m_rows.indexOf(last);
        if (row >= 0) {
            beginRemoveRows({}, static_cast<int>(row), static_cast<int>(row));
            m_rows.removeAt(row);
            endRemoveRows();
        }
    }
    emit countChanged();
    return *entry;
}

void SpotModel::rebuildRows()
{
    beginResetModel();
    m_rows.clear();
    for (const Entry& e : m_all) {
        if (visibleEntry(e))
            m_rows << e;
    }
    endResetModel();
    emit countChanged();
}

void SpotModel::setFilter(const SpotFilter& filter)
{
    m_filter = filter;
    rebuildRows();
}

void SpotModel::restatus(const std::function<int(const EnrichedSpot&)>& status)
{
    for (const Entry& e : m_all)
        e->status = status(*e);
    rebuildRows();
}

void SpotModel::expire()
{
    const QDateTime limit = QDateTime::currentDateTimeUtc().addSecs(-kKeepMinutes * 60);
    bool removed = false;
    while (!m_all.isEmpty() && m_all.last()->spot.time < limit) {
        m_byKey.remove(m_all.last()->key());
        m_all.removeLast();
        removed = true;
    }
    // Anche senza rimozioni il filtro d'eta' puo' nascondere righe.
    Q_UNUSED(removed);
    rebuildRows();
}

void SpotModel::clear()
{
    beginResetModel();
    m_all.clear();
    m_byKey.clear();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

QVariantMap SpotModel::get(int row) const
{
    QVariantMap out;
    if (row < 0 || row >= m_rows.size())
        return out;
    const QHash<int, QByteArray> names = roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        out.insert(QString::fromLatin1(it.value()), data(index(row), it.key()));
    return out;
}

const EnrichedSpot* SpotModel::find(const QString& key) const
{
    const Entry e = m_byKey.value(key);
    return e ? e.get() : nullptr;
}

QList<EnrichedSpot> SpotModel::visible() const
{
    QList<EnrichedSpot> out;
    for (const Entry& e : m_rows)
        out << *e;
    return out;
}

} // namespace decolog::app
