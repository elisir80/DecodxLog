// DecoLog — la tabella del log, per il TableView QML.
//
// Tiene in memoria solo le colonne mostrate, dalla piu' recente. Un log di
// centomila QSO occupa qualche decina di megabyte e si scorre senza query a
// ogni riga; quando serviranno log piu' grandi si passera' a una finestra
// caricata a pagine, senza cambiare l'interfaccia verso il QML.
#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QString>
#include <QVector>

class QSqlQuery;
namespace decolog::core { class LogDatabase; }

namespace decolog::app {

class QsoTableModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum Column { Utc, Call, Band, Mode, Freq, RstSent, RstRcvd, Grid, Name, Country, Source, ColumnCount };
    enum Roles { IdRole = Qt::UserRole + 1, ColumnKeyRole, IsNewRole };

    explicit QsoTableModel(decolog::core::LogDatabase* db, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_rows.size()); }
    QString filterText() const { return m_filter; }
    void setFilterText(const QString& text);

    Q_INVOKABLE void reload();
    Q_INVOKABLE qint64 idAt(int row) const;
    Q_INVOKABLE QString callAt(int row) const;
    Q_INVOKABLE int columnWidthHint(int column) const;
    Q_INVOKABLE QString columnKey(int column) const;

    // Aggiunge in testa una riga appena scritta, senza ricaricare tutto.
    void prependQso(qint64 id);

signals:
    void countChanged();
    void filterTextChanged();

private:
    struct Row {
        qint64  id{0};
        QString values[ColumnCount];
        bool    fresh{false};
    };

    QString selectSql(const QString& where) const;
    Row rowFromQuery(const QSqlQuery& q) const;

    decolog::core::LogDatabase* m_db;
    QVector<Row> m_rows;
    QString m_filter;
};

} // namespace decolog::app
