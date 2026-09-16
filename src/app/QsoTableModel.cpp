#include "app/QsoTableModel.h"

#include "core/LogDatabase.h"

#include <QDateTime>
#include <QSqlQuery>

namespace decolog::app {

using core::LogDatabase;

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
    default:
        return {};
    }
}

QVariant QsoTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};
    switch (section) {
    case Utc:     return tr("UTC");
    case Call:    return tr("Call");
    case Band:    return tr("Band");
    case Mode:    return tr("Mode");
    case Freq:    return tr("MHz");
    case RstSent: return tr("Sent");
    case RstRcvd: return tr("Rcvd");
    case Grid:    return tr("Grid");
    case Name:    return tr("Name");
    case Country: return tr("Country");
    case Source:  return tr("Source");
    default:      return {};
    }
}

QHash<int, QByteArray> QsoTableModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {IdRole, "qsoId"},
        {ColumnKeyRole, "columnKey"},
        {IsNewRole, "isNew"},
    };
}

QString QsoTableModel::columnKey(int column) const
{
    static const QStringList keys{
        QStringLiteral("utc"), QStringLiteral("call"), QStringLiteral("band"),
        QStringLiteral("mode"), QStringLiteral("freq"), QStringLiteral("rst_sent"),
        QStringLiteral("rst_rcvd"), QStringLiteral("grid"), QStringLiteral("name"),
        QStringLiteral("country"), QStringLiteral("source")};
    return keys.value(column);
}

int QsoTableModel::columnWidthHint(int column) const
{
    switch (column) {
    case Utc:     return 164;
    case Call:    return 96;
    case Band:    return 56;
    case Mode:    return 60;
    case Freq:    return 92;
    case RstSent:
    case RstRcvd: return 50;
    case Grid:    return 62;
    case Name:    return 120;
    case Country: return 130;
    case Source:  return 96;
    default:      return 80;
    }
}

QString QsoTableModel::selectSql(const QString& where) const
{
    return QStringLiteral(
               "SELECT id, qso_datetime_on, call, band, mode, submode, freq, rst_sent, rst_rcvd, "
               "gridsquare, name, country, source FROM qso WHERE deleted = 0 %1 "
               "ORDER BY qso_datetime_on DESC, id DESC")
        .arg(where);
}

QsoTableModel::Row QsoTableModel::rowFromQuery(const QSqlQuery& q) const
{
    Row r;
    r.id = q.value(0).toLongLong();
    const QDateTime on = QDateTime::fromString(q.value(1).toString(), Qt::ISODate);
    r.values[Utc] = on.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    r.values[Call] = q.value(2).toString();
    r.values[Band] = q.value(3).toString();
    const QString submode = q.value(5).toString();
    // FT2 si legge FT2, non MFSK: il modo ADIF resta nel database, qui si mostra
    // quello che l'operatore riconosce.
    r.values[Mode] = submode.isEmpty() ? q.value(4).toString() : submode;
    r.values[Freq] = q.value(6).isNull() ? QString() : QString::number(q.value(6).toDouble(), 'f', 6);
    r.values[RstSent] = q.value(7).toString();
    r.values[RstRcvd] = q.value(8).toString();
    r.values[Grid] = q.value(9).toString();
    r.values[Name] = q.value(10).toString();
    r.values[Country] = q.value(11).toString();
    const QString source = q.value(12).toString();
    if (source == QLatin1String("udp_decodium"))   r.values[Source] = QStringLiteral("Decodium");
    else if (source == QLatin1String("udp_wsjtx")) r.values[Source] = QStringLiteral("WSJT-X");
    else if (source == QLatin1String("import"))    r.values[Source] = tr("Import");
    else if (source == QLatin1String("manual"))    r.values[Source] = tr("Manual");
    else r.values[Source] = source;
    return r;
}

void QsoTableModel::reload()
{
    beginResetModel();
    m_rows.clear();
    if (m_db && m_db->isOpen()) {
        QSqlQuery q(m_db->connection());
        const QString f = m_filter.trimmed().toUpper();
        if (f.isEmpty()) {
            q.prepare(selectSql({}));
        } else {
            // Un filtro semplice per ora: nominativo, locatore, banda o modo.
            q.prepare(selectSql(QStringLiteral(
                "AND (call LIKE ? OR gridsquare LIKE ? OR band = ? OR mode = ? OR submode = ?)")));
            const QString like = QLatin1Char('%') + f + QLatin1Char('%');
            q.addBindValue(like);
            q.addBindValue(like);
            q.addBindValue(f.toLower());
            q.addBindValue(f);
            q.addBindValue(f);
        }
        q.setForwardOnly(true);
        if (q.exec()) {
            while (q.next())
                m_rows.append(rowFromQuery(q));
        }
    }
    endResetModel();
    emit countChanged();
}

void QsoTableModel::prependQso(qint64 id)
{
    if (!m_db || !m_db->isOpen())
        return;
    if (!m_filter.trimmed().isEmpty()) {
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

    // Di solito il QSO appena fatto e' il piu' recente; un import vecchio no.
    qsizetype pos = 0;
    const QString key = r.values[Utc];
    while (pos < m_rows.size() && m_rows.at(pos).values[Utc] > key)
        ++pos;

    // Evidenziata solo l'ultima arrivata: e' quella che l'operatore cerca con
    // lo sguardo, le altre tornano righe normali.
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].fresh) {
            m_rows[i].fresh = false;
            const QModelIndex a = index(static_cast<int>(i), 0);
            emit dataChanged(a, index(static_cast<int>(i), ColumnCount - 1), {IsNewRole});
        }
    }

    beginInsertRows({}, static_cast<int>(pos), static_cast<int>(pos));
    m_rows.insert(pos, r);
    endInsertRows();
    emit countChanged();
}

qint64 QsoTableModel::idAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).id : 0;
}

QString QsoTableModel::callAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).values[Call] : QString();
}

void QsoTableModel::setFilterText(const QString& text)
{
    if (text == m_filter)
        return;
    m_filter = text;
    emit filterTextChanged();
    reload();
}

} // namespace decolog::app
