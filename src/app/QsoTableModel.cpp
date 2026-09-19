#include "app/QsoTableModel.h"

#include "core/Bands.h"
#include "core/LogDatabase.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QVariantMap>

namespace decolog::app {

using core::LogDatabase;

namespace {

// Una lettera per servizio, nell'ordine delle colonne L Q C E: 'c' confermato,
// 's' inviato o in coda, '-' niente.
QString qslCodes(const QString& summary)
{
    QString codes = QStringLiteral("----");
    static const QStringList order{QStringLiteral("lotw"), QStringLiteral("qrz"),
                                   QStringLiteral("clublog"), QStringLiteral("eqsl")};
    for (const QString& item : summary.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QStringList parts = item.split(QLatin1Char(':'));
        if (parts.size() != 3)
            continue;
        const qsizetype i = order.indexOf(parts.at(0));
        if (i < 0)
            continue;
        if (parts.at(2) == QLatin1String("Y"))
            codes[i] = QLatin1Char('c');
        else if (parts.at(1) != QLatin1String("N"))
            codes[i] = QLatin1Char('s');
    }
    return codes;
}

} // namespace

QsoTableModel::QsoTableModel(LogDatabase* db, QObject* parent)
    : QAbstractTableModel(parent)
    , m_db(db)
{
    reload();
}

int QsoTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int QsoTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant QsoTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row& row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return row.values[index.column()];
    case IdRole:
        return row.id;
    case ColumnKeyRole:
        return columnKey(index.column());
    case IsNewRole:
        return row.fresh;
    case ModeRole:
        return row.values[Mode];
    default:
        return {};
    }
}

QVariant QsoTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};
    return columnTitle(section);
}

QHash<int, QByteArray> QsoTableModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {IdRole, "qsoId"},
        {ColumnKeyRole, "columnKey"},
        {IsNewRole, "isNew"},
        {ModeRole, "modeName"},
    };
}

QString QsoTableModel::columnKey(int column) const
{
    static const QStringList keys{
        QStringLiteral("utc"), QStringLiteral("call"), QStringLiteral("band"), QStringLiteral("freq"),
        QStringLiteral("mode"), QStringLiteral("rst_sent"), QStringLiteral("rst_rcvd"),
        QStringLiteral("grid"), QStringLiteral("name"), QStringLiteral("qth"), QStringLiteral("country"),
        QStringLiteral("state"), QStringLiteral("county"), QStringLiteral("cqz"), QStringLiteral("ituz"),
        QStringLiteral("iota"), QStringLiteral("dxcc"), QStringLiteral("qsl"),
        QStringLiteral("source"), QStringLiteral("tags")};
    return keys.value(column);
}

QString QsoTableModel::columnTitle(int column) const
{
    switch (column) {
    case Utc:     return tr("UTC");
    case Call:    return tr("Call");
    case Band:    return tr("Band");
    case Freq:    return tr("Freq");
    case Mode:    return tr("Mode");
    case RstSent: return tr("S");
    case RstRcvd: return tr("R");
    case Grid:    return tr("Grid");
    case Name:    return tr("Name");
    case Qth:     return tr("City / QTH");
    case Country: return tr("Country");
    case State:   return tr("State");
    case County:  return tr("County");
    case Cqz:     return tr("CQ");
    case Ituz:    return tr("ITU");
    case Iota:    return tr("IOTA");
    case Dxcc:    return tr("DXCC");
    case Qsl:     return tr("QSL");
    case Source:  return tr("Src");
    case Tags:    return tr("Tags");
    default:      return {};
    }
}

int QsoTableModel::columnWidthHint(int column) const
{
    switch (column) {
    case Utc:     return 132;
    case Call:    return 118;
    case Band:    return 56;
    case Freq:    return 96;
    case Mode:    return 60;
    case RstSent:
    case RstRcvd: return 46;
    case Grid:    return 74;
    case Name:    return 140;
    case Qth:     return 140;
    case Country: return 130;
    case State:   return 52;
    case County:  return 110;
    case Cqz:
    case Ituz:    return 44;
    case Iota:    return 64;
    case Dxcc:    return 56;
    case Qsl:     return 84;
    case Source:  return 52;
    case Tags:    return 130;
    default:      return 80;
    }
}

QString QsoTableModel::selectSql(const QString& where) const
{
    return QStringLiteral(
               "SELECT id, qso_datetime_on, call, band, mode, submode, freq, rst_sent, rst_rcvd, "
               "gridsquare, name, dxcc, source, "
               "(SELECT group_concat(service || ':' || sent || ':' || rcvd) FROM qsl_status s WHERE s.qso_id = qso.id), "
               "IFNULL(tags, ''), "
               "IFNULL(qth, ''), IFNULL(country, ''), IFNULL(state, ''), IFNULL(cnty, ''), "
               "cqz, ituz, IFNULL(iota, '') "
               "FROM qso WHERE deleted = 0 %1 ORDER BY qso_datetime_on DESC, id DESC")
        .arg(where);
}

QsoTableModel::Row QsoTableModel::rowFromQuery(const QSqlQuery& q) const
{
    Row r;
    r.id = q.value(0).toLongLong();
    r.sortKey = q.value(1).toString();
    const QDateTime on = QDateTime::fromString(r.sortKey, Qt::ISODate);
    r.values[Utc] = on.toUTC().toString(QStringLiteral("yy-MM-dd HH:mm"));
    r.values[Call] = q.value(2).toString();
    r.values[Band] = q.value(3).toString();
    const QString submode = q.value(5).toString();
    // FT2 si legge FT2, non MFSK: il modo ADIF resta nel database, qui si mostra
    // quello che l'operatore riconosce.
    r.values[Mode] = submode.isEmpty() || q.value(4).toString() == QLatin1String("SSB") ? q.value(4).toString() : submode;
    r.values[Freq] = q.value(6).isNull() ? QString() : QString::number(q.value(6).toDouble(), 'f', 6);
    r.values[RstSent] = q.value(7).toString();
    r.values[RstRcvd] = q.value(8).toString();
    r.values[Grid] = q.value(9).toString();
    r.values[Name] = q.value(10).toString();
    r.values[Dxcc] = q.value(11).isNull() ? QString() : q.value(11).toString();
    const QString source = q.value(12).toString();
    if (source.startsWith(QLatin1String("udp")))  r.values[Source] = QStringLiteral("udp");
    else if (source == QLatin1String("manual"))   r.values[Source] = QStringLiteral("man");
    else if (source == QLatin1String("import"))   r.values[Source] = QStringLiteral("imp");
    else if (source == QLatin1String("cloud"))    r.values[Source] = QStringLiteral("cld");
    else r.values[Source] = source.left(3);
    r.values[Qsl] = qslCodes(q.value(13).toString());
    r.values[Tags] = q.value(14).toString().replace(QLatin1Char(','), QStringLiteral(", "));
    r.values[Qth] = q.value(15).toString();
    r.values[Country] = q.value(16).toString();
    r.values[State] = q.value(17).toString();
    r.values[County] = q.value(18).toString();
    r.values[Cqz] = q.value(19).isNull() || q.value(19).toInt() <= 0 ? QString() : q.value(19).toString();
    r.values[Ituz] = q.value(20).isNull() || q.value(20).toInt() <= 0 ? QString() : q.value(20).toString();
    r.values[Iota] = q.value(21).toString();
    return r;
}

bool QsoTableModel::filtered() const
{
    return !m_filter.trimmed().isEmpty() || !m_bands.isEmpty() || !m_modes.isEmpty() || !m_month.isEmpty()
        || m_dxcc > 0 || !m_qsl.isEmpty() || m_profile > 0 || !m_tag.isEmpty() || !m_dateFrom.isEmpty()
        || !m_dateTo.isEmpty();
}

void QsoTableModel::refreshTotal()
{
    m_total = m_db && m_db->isOpen() ? m_db->qsoCount() : 0;
}

void QsoTableModel::reload()
{
    beginResetModel();
    m_rows.clear();
    if (m_db && m_db->isOpen()) {
        QStringList where;
        QVariantList binds;
        const QString f = m_filter.trimmed().toUpper();
        if (!f.isEmpty()) {
            // Ricerca libera: nominativo, locatore o nome.
            where << QStringLiteral("(call LIKE ? OR UPPER(gridsquare) LIKE ? OR UPPER(name) LIKE ?)");
            const QString like = QLatin1Char('%') + f + QLatin1Char('%');
            binds << like << like << like;
        }
        if (!m_bands.isEmpty()) {
            QStringList marks;
            for (const QString& b : m_bands) {
                marks << QStringLiteral("?");
                binds << b.toLower();
            }
            where << QStringLiteral("band IN (%1)").arg(marks.join(QLatin1Char(',')));
        }
        if (!m_modes.isEmpty()) {
            QStringList marks;
            for (const QString& m : m_modes) {
                marks << QStringLiteral("?");
                binds << m.toUpper();
            }
            where << QStringLiteral("(CASE WHEN IFNULL(submode, '') = '' OR mode = 'SSB' THEN mode ELSE submode END) IN (%1)")
                         .arg(marks.join(QLatin1Char(',')));
        }
        if (!m_month.isEmpty()) {
            where << QStringLiteral("SUBSTR(qso_datetime_on, 1, 7) = ?");
            binds << m_month;
        }
        if (m_dxcc > 0) {
            where << QStringLiteral("dxcc = ?");
            binds << m_dxcc;
        }
        if (!m_qsl.isEmpty()) {
            const QString confirmedBy = QStringLiteral(
                "EXISTS (SELECT 1 FROM qsl_status s WHERE s.qso_id = qso.id AND s.rcvd = 'Y' AND s.service %1)");
            if (m_qsl == QLatin1String("confirmed")) {
                where << confirmedBy.arg(QStringLiteral("IN ('lotw', 'card', 'eqsl', 'qrz')"));
            } else if (m_qsl == QLatin1String("unconfirmed")) {
                where << QStringLiteral("NOT ") + confirmedBy.arg(QStringLiteral("IN ('lotw', 'card', 'eqsl', 'qrz')"));
            } else {
                where << confirmedBy.arg(QStringLiteral("= ?"));
                binds << m_qsl;
            }
        }
        if (m_profile > 0) {
            where << QStringLiteral("station_profile_id = ?");
            binds << m_profile;
        }
        if (!m_tag.isEmpty()) {
            // Un'etichetta intera, non un pezzo: "pota" non trova "potato".
            where << QStringLiteral("instr(',' || LOWER(IFNULL(tags, '')) || ',', ?) > 0");
            binds << QLatin1Char(',') + m_tag.toLower() + QLatin1Char(',');
        }
        // Le date nel database sono ISO-8601 a lunghezza fissa: il confronto fra
        // stringhe basta, e "fino al" comprende tutto quel giorno.
        if (!m_dateFrom.isEmpty()) {
            where << QStringLiteral("qso_datetime_on >= ?");
            binds << m_dateFrom;
        }
        if (!m_dateTo.isEmpty()) {
            const QDate to = QDate::fromString(m_dateTo, QStringLiteral("yyyy-MM-dd"));
            where << QStringLiteral("qso_datetime_on < ?");
            binds << (to.isValid() ? to.addDays(1).toString(QStringLiteral("yyyy-MM-dd")) : m_dateTo);
        }

        QSqlQuery q(m_db->connection());
        q.prepare(selectSql(where.isEmpty() ? QString()
                                            : QStringLiteral("AND ") + where.join(QStringLiteral(" AND "))));
        for (const auto& b : binds)
            q.addBindValue(b);
        q.setForwardOnly(true);
        if (q.exec()) {
            while (q.next())
                m_rows.append(rowFromQuery(q));
        }
    }
    refreshTotal();
    endResetModel();
    emit countChanged();
}

void QsoTableModel::refreshQso(qint64 id)
{
    if (!m_db || !m_db->isOpen())
        return;
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id != id)
            continue;
        QSqlQuery q(m_db->connection());
        q.prepare(selectSql(QStringLiteral("AND id = ?")));
        q.addBindValue(id);
        if (!q.exec() || !q.next())
            return;
        const bool wasFresh = m_rows.at(i).fresh;
        m_rows[i] = rowFromQuery(q);
        m_rows[i].fresh = wasFresh;
        emit dataChanged(index(static_cast<int>(i), 0), index(static_cast<int>(i), ColumnCount - 1));
        return;
    }
}

void QsoTableModel::insertQso(qint64 id)
{
    if (!m_db || !m_db->isOpen())
        return;
    if (filtered()) {
        reload();
        return;
    }
    QSqlQuery q(m_db->connection());
    q.prepare(selectSql(QStringLiteral("AND id = ?")));
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return;
    Row r = rowFromQuery(q);
    r.fresh = true;

    // Di solito il QSO appena fatto e' il piu' recente; uno scritto a mano con
    // l'ora di prima no.
    qsizetype pos = 0;
    while (pos < m_rows.size() && m_rows.at(pos).sortKey > r.sortKey)
        ++pos;

    // Evidenziata solo l'ultima arrivata: e' quella che l'operatore cerca con
    // lo sguardo, le altre tornano righe normali.
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].fresh) {
            m_rows[i].fresh = false;
            emit dataChanged(index(static_cast<int>(i), 0), index(static_cast<int>(i), ColumnCount - 1), {IsNewRole});
        }
    }

    beginInsertRows({}, static_cast<int>(pos), static_cast<int>(pos));
    m_rows.insert(pos, r);
    endInsertRows();
    refreshTotal();
    emit countChanged();
}

void QsoTableModel::clearFilters()
{
    m_filter.clear();
    m_bands.clear();
    m_modes.clear();
    m_month.clear();
    m_dxcc = 0;
    m_qsl.clear();
    m_profile = 0;
    m_tag.clear();
    m_dateFrom.clear();
    m_dateTo.clear();
    emit filtersChanged();
    reload();
}

QVariantMap QsoTableModel::filterState() const
{
    return {
        {QStringLiteral("text"), m_filter},
        {QStringLiteral("bands"), m_bands},
        {QStringLiteral("modes"), m_modes},
        {QStringLiteral("month"), m_month},
        {QStringLiteral("dxcc"), m_dxcc},
        {QStringLiteral("qsl"), m_qsl},
        {QStringLiteral("profile"), m_profile},
        {QStringLiteral("tag"), m_tag},
        {QStringLiteral("dateFrom"), m_dateFrom},
        {QStringLiteral("dateTo"), m_dateTo},
    };
}

void QsoTableModel::applyFilterState(const QVariantMap& state)
{
    m_filter = state.value(QStringLiteral("text")).toString();
    m_bands = state.value(QStringLiteral("bands")).toStringList();
    m_modes = state.value(QStringLiteral("modes")).toStringList();
    m_month = state.value(QStringLiteral("month")).toString();
    m_dxcc = state.value(QStringLiteral("dxcc")).toInt();
    m_qsl = state.value(QStringLiteral("qsl")).toString();
    m_profile = state.value(QStringLiteral("profile")).toInt();
    m_tag = state.value(QStringLiteral("tag")).toString();
    m_dateFrom = state.value(QStringLiteral("dateFrom")).toString();
    m_dateTo = state.value(QStringLiteral("dateTo")).toString();
    emit filtersChanged();
    reload();
}

qint64 QsoTableModel::idAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).id : 0;
}

QString QsoTableModel::callAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).values[Call] : QString();
}

QString QsoTableModel::valueAt(int row, int column) const
{
    if (row < 0 || row >= m_rows.size() || column < 0 || column >= ColumnCount)
        return {};
    return m_rows.at(row).values[column];
}

int QsoTableModel::rowForId(qint64 id) const
{
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id == id)
            return static_cast<int>(i);
    }
    return -1;
}

QStringList QsoTableModel::bandsInLog() const
{
    QStringList out;
    if (!m_db || !m_db->isOpen())
        return out;
    for (const auto& row : m_db->countByBand())
        out << row.key;
    return out;
}

QStringList QsoTableModel::modesInLog() const
{
    QStringList out;
    if (!m_db || !m_db->isOpen())
        return out;
    for (const auto& row : m_db->countByMode())
        out << row.key;
    return out;
}

QVariantList QsoTableModel::tagsInLog() const
{
    QVariantList out;
    if (!m_db || !m_db->isOpen())
        return out;
    for (const auto& row : m_db->tagCounts())
        out << QVariantMap{{QStringLiteral("key"), row.key}, {QStringLiteral("count"), row.count}};
    return out;
}

QVariantList QsoTableModel::shownIds() const
{
    QVariantList out;
    out.reserve(m_rows.size());
    for (const Row& r : m_rows)
        out << r.id;
    return out;
}

void QsoTableModel::setDxccFilter(int dxcc)
{
    if (dxcc == m_dxcc)
        return;
    m_dxcc = qMax(0, dxcc);
    emit filtersChanged();
    reload();
}

void QsoTableModel::setQslFilter(const QString& qsl)
{
    if (qsl == m_qsl)
        return;
    m_qsl = qsl;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setProfileFilter(int profileId)
{
    if (profileId == m_profile)
        return;
    m_profile = qMax(0, profileId);
    emit filtersChanged();
    reload();
}

void QsoTableModel::setTagFilter(const QString& tag)
{
    const QString t = tag.simplified();
    if (t == m_tag)
        return;
    m_tag = t;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setDateFrom(const QString& date)
{
    const QString d = QDate::fromString(date.trimmed(), QStringLiteral("yyyy-MM-dd")).isValid() ? date.trimmed() : QString();
    if (d == m_dateFrom)
        return;
    m_dateFrom = d;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setDateTo(const QString& date)
{
    const QString d = QDate::fromString(date.trimmed(), QStringLiteral("yyyy-MM-dd")).isValid() ? date.trimmed() : QString();
    if (d == m_dateTo)
        return;
    m_dateTo = d;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setFilterText(const QString& text)
{
    if (text == m_filter)
        return;
    m_filter = text;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setBandFilter(const QStringList& bands)
{
    if (bands == m_bands)
        return;
    m_bands = bands;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setModeFilter(const QStringList& modes)
{
    if (modes == m_modes)
        return;
    m_modes = modes;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setMonthFilter(const QString& month)
{
    if (month == m_month)
        return;
    m_month = month;
    emit filtersChanged();
    reload();
}

} // namespace decolog::app
