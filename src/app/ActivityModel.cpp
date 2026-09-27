#include "app/ActivityModel.h"

namespace decolog::app {

ActivityModel::ActivityModel(int limit, QObject* parent)
    : QAbstractListModel(parent)
    , m_limit(limit)
{
}

int ActivityModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant ActivityModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row& r = m_rows.at(index.row());
    switch (role) {
    case TimeRole: return r.time;
    case CategoryRole: return r.category;
    case Qt::DisplayRole:
    case MessageRole: return r.message;
    case LevelRole: return r.level;
    }
    return {};
}

QHash<int, QByteArray> ActivityModel::roleNames() const
{
    return {{TimeRole, "time"}, {CategoryRole, "category"}, {MessageRole, "message"}, {LevelRole, "level"}};
}

void ActivityModel::add(const QString& time, const QString& category, const QString& message, const QString& level)
{
    beginInsertRows({}, 0, 0);
    m_rows.prepend(Row{time, category, message, level});
    endInsertRows();
    if (m_rows.size() > m_limit) {
        const int first = m_limit;
        const int last = static_cast<int>(m_rows.size()) - 1;
        beginRemoveRows({}, first, last);
        m_rows.resize(m_limit);
        endRemoveRows();
    }
    emit countChanged();
}

void ActivityModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

} // namespace decolog::app
