// DecoLog — la tabella del log, per il TableView QML.
//
// Tiene in memoria solo le colonne mostrate, dalla piu' recente. Un log di
// centomila QSO occupa qualche decina di megabyte e si scorre senza query a
// ogni riga; quando serviranno log piu' grandi si passera' a una finestra
// caricata a pagine, senza cambiare l'interfaccia verso il QML.
#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class QSqlQuery;
namespace decolog::core { class LogDatabase; }

namespace decolog::app {

class QsoTableModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filtersChanged)
    Q_PROPERTY(QStringList bandFilter READ bandFilter WRITE setBandFilter NOTIFY filtersChanged)
    Q_PROPERTY(QStringList modeFilter READ modeFilter WRITE setModeFilter NOTIFY filtersChanged)
    // "yyyy-MM", vuoto = tutti i mesi.
    Q_PROPERTY(QString monthFilter READ monthFilter WRITE setMonthFilter NOTIFY filtersChanged)
    // Numero DXCC, 0 = tutte le entita'.
    Q_PROPERTY(int dxccFilter READ dxccFilter WRITE setDxccFilter NOTIFY filtersChanged)
    // "", "confirmed" (qualunque servizio), "lotw", "card", "eqsl", "unconfirmed".
    Q_PROPERTY(QString qslFilter READ qslFilter WRITE setQslFilter NOTIFY filtersChanged)
    // Id del profilo stazione, 0 = tutti.
    Q_PROPERTY(int profileFilter READ profileFilter WRITE setProfileFilter NOTIFY filtersChanged)
    Q_PROPERTY(QString tagFilter READ tagFilter WRITE setTagFilter NOTIFY filtersChanged)
    // "yyyy-MM-dd", estremi compresi; vuoti = senza limite.
    Q_PROPERTY(QString dateFrom READ dateFrom WRITE setDateFrom NOTIFY filtersChanged)
    Q_PROPERTY(QString dateTo READ dateTo WRITE setDateTo NOTIFY filtersChanged)
    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(bool filtered READ filtered NOTIFY filtersChanged)

public:
    enum Column { Utc, Call, Band, Freq, Mode, RstSent, RstRcvd, Grid, Name, Dxcc, Qsl, Source, Tags, ColumnCount };
    enum Roles { IdRole = Qt::UserRole + 1, ColumnKeyRole, IsNewRole, ModeRole };

    explicit QsoTableModel(decolog::core::LogDatabase* db, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_rows.size()); }
    int totalCount() const { return m_total; }
    QString filterText() const { return m_filter; }
    void setFilterText(const QString& text);
    QStringList bandFilter() const { return m_bands; }
    void setBandFilter(const QStringList& bands);
    QStringList modeFilter() const { return m_modes; }
    void setModeFilter(const QStringList& modes);
    QString monthFilter() const { return m_month; }
    void setMonthFilter(const QString& month);
    int dxccFilter() const { return m_dxcc; }
    void setDxccFilter(int dxcc);
    QString qslFilter() const { return m_qsl; }
    void setQslFilter(const QString& qsl);
    int profileFilter() const { return m_profile; }
    void setProfileFilter(int profileId);
    QString tagFilter() const { return m_tag; }
    void setTagFilter(const QString& tag);
    QString dateFrom() const { return m_dateFrom; }
    void setDateFrom(const QString& date);
    QString dateTo() const { return m_dateTo; }
    void setDateTo(const QString& date);
    int columns() const { return ColumnCount; }
    bool filtered() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE void clearFilters();
    // Filtri come mappa, per salvarli con un nome e riapplicarli.
    Q_INVOKABLE QVariantMap filterState() const;
    Q_INVOKABLE void applyFilterState(const QVariantMap& state);
    Q_INVOKABLE qint64 idAt(int row) const;
    Q_INVOKABLE QString callAt(int row) const;
    Q_INVOKABLE QString valueAt(int row, int column) const;
    Q_INVOKABLE int rowForId(qint64 id) const;
    Q_INVOKABLE int columnWidthHint(int column) const;
    Q_INVOKABLE QString columnKey(int column) const;
    Q_INVOKABLE QString columnTitle(int column) const;
    // Bande e modi presenti nel log, per il menu dei filtri.
    Q_INVOKABLE QStringList bandsInLog() const;
    Q_INVOKABLE QStringList modesInLog() const;
    // Etichette del log con il conteggio: [{key, count}].
    Q_INVOKABLE QVariantList tagsInLog() const;
    // Gli id delle righe mostrate, nell'ordine della tabella (per export ed etichette).
    Q_INVOKABLE QVariantList shownIds() const;

    // Aggiunge una riga appena scritta, senza ricaricare tutto.
    void insertQso(qint64 id);

signals:
    void countChanged();
    void filtersChanged();

private:
    struct Row {
        qint64  id{0};
        QString sortKey;
        QString values[ColumnCount];
        bool    fresh{false};
    };

    QString selectSql(const QString& where) const;
    Row rowFromQuery(const QSqlQuery& q) const;
    void refreshTotal();

    decolog::core::LogDatabase* m_db;
    QVector<Row> m_rows;
    QString m_filter;
    QStringList m_bands;
    QStringList m_modes;
    QString m_month;
    int m_dxcc{0};
    QString m_qsl;
    int m_profile{0};
    QString m_tag;
    QString m_dateFrom;
    QString m_dateTo;
    int m_total{0};
};

} // namespace decolog::app
