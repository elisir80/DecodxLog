// DecoDXLog — il registro attivita' come modello a righe.
//
// Prima era una lista intera passata al QML: a ogni riga nuova la lista veniva
// ricopiata e la vista rifatta da capo, trecento righe alla volta. Con il
// cluster che segnala spot di continuo, era un colpo al filo dell'interfaccia
// per ogni avviso. Qui una riga nuova e' una riga inserita in cima, e basta.
#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

namespace decolog::app {

class ActivityModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles { TimeRole = Qt::UserRole + 1, CategoryRole, MessageRole, LevelRole };

    explicit ActivityModel(int limit, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_rows.size()); }
    void add(const QString& time, const QString& category, const QString& message, const QString& level);
    Q_INVOKABLE void clear();

signals:
    void countChanged();

private:
    struct Row {
        QString time;
        QString category;
        QString message;
        QString level;
    };
    QList<Row> m_rows;      // dalla piu' recente
    int m_limit;
};

} // namespace decolog::app
