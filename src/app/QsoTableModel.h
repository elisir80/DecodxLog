// DecoDXLog — la tabella del log, per il TableView QML.
//
// In memoria ci sono solo gli id delle righe, nell'ordine della tabella: filtri
// e ordine li fa SQLite. I valori si leggono a pagine di duecento righe quando
// la tabella li chiede, e se ne tengono poche decine: un log da un milione di
// QSO si apre in un secondo e occupa qualche megabyte, non un gigabyte.
#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QList>
#include <QSet>
#include <QThreadPool>

#include <array>
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
    Q_PROPERTY(int columns READ columns NOTIFY layoutChanged)
    // Le colonne mostrate, nell'ordine in cui si vedono: chiavi del catalogo
    // (vedi availableColumns) o "x:CAMPO" per un campo ADIF qualsiasi.
    Q_PROPERTY(QStringList columnLayout READ columnLayout WRITE setColumnLayout NOTIFY layoutChanged)
    Q_PROPERTY(bool filtered READ filtered NOTIFY filtersChanged)
    // L'ordine delle righe: la colonna (chiave) e il verso. Di serie l'ora del
    // QSO, dal piu' recente. Un clic sull'intestazione lo cambia.
    Q_PROPERTY(QString sortKey READ sortKey NOTIFY sortChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)

public:
    // I valori che ogni riga tiene sempre, qualunque colonna si veda: servono
    // ai menu (nominativo, DXCC, etichette) anche quando la colonna e' nascosta.
    enum Column { Utc, Call, Band, Freq, Mode, RstSent, RstRcvd, Grid, Name, Comment, Qth, Country,
                  State, County, Cqz, Ituz, Iota, Dxcc, Qsl, Source, Tags, ColumnCount };
    enum Roles { IdRole = Qt::UserRole + 1, ColumnKeyRole, IsNewRole, ModeRole, CategoryRole };

    // Che cosa ha portato un QSO quando e' stato fatto, come Decodium 4 lo
    // dice dei decode: il primo con quell'entita' (in assoluto o sulla
    // banda), con quel continente, zona CQ, zona ITU, locatore, nominativo;
    // oppure un gia' lavorato. Il nome e' la chiave del colore (colorNewDxcc…).
    static QStringList categoryKeys();

    // Il risultato della conta delle categorie di tutto il log.
    struct CategoryPass {
        QHash<qint64, quint8> category;
        QSet<quint64> seen[12];
        QString lastOn;
        qint64 lastId{0};
        bool valid{false};
    };

    explicit QsoTableModel(decolog::core::LogDatabase* db, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_ids.size()); }
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
    int columns() const { return static_cast<int>(m_layout.size()); }
    QStringList columnLayout() const { return m_layout; }
    void setColumnLayout(const QStringList& keys);
    // Le colonne di sempre, nell'ordine di sempre.
    static QStringList defaultLayout();
    // Tutte le colonne che si possono mostrare: [{key, title, field}].
    Q_INVOKABLE QVariantList availableColumns() const;
    // Il titolo di una colonna dalla sua chiave, anche se non e' mostrata.
    Q_INVOKABLE QString titleOf(const QString& key) const;
    // Il valore di una riga per chiave, anche se la colonna e' nascosta.
    Q_INVOKABLE QString valueFor(int row, const QString& key) const;
    bool filtered() const;

    Q_INVOKABLE void reload();
    QString sortKey() const { return m_sortKey; }
    bool sortAscending() const { return m_sortAscending; }
    // Ordina per quella colonna; di nuovo sulla stessa, al contrario.
    Q_INVOKABLE void sortBy(const QString& key);
    // L'ordine esatto (per ripristinare quello salvato).
    Q_INVOKABLE void setSort(const QString& key, bool ascending);
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
    // Rilegge una riga che e' cambiata (una conferma, il callbook che completa
    // un QSO): la tabella non si ricostruisce per una riga sola.
    void refreshQso(qint64 id);

signals:
    void countChanged();
    void filtersChanged();
    void layoutChanged();
    void sortChanged();

private:
    // La categoria di ogni QSO, come posizione in categoryKeys(): un byte, non
    // una stringa, per un milione di righe.
    QHash<qint64, quint8> m_category;
    QString m_sortKey{QStringLiteral("utc")};
    bool m_sortAscending{false};
    bool defaultSort() const { return m_sortKey == QLatin1String("utc") && !m_sortAscending; }
    // Filtri e ordine come SQL.
    QString whereSql(QVariantList& binds) const;
    QString orderSql() const;
    QString m_categorySignature;   // il log com'era quando si sono contate
    // Quello che si e' gia' visto, per le categorie di un QSO nuovo senza
    // ricontare il log: entita', continente, zone, locatore, nominativo, da
    // soli e per banda, come impronte. Valgono per i QSO piu' recenti
    // dell'ultimo contato.
    mutable QSet<quint64> m_seen[12];
    void adoptCategories(CategoryPass&& pass);
    // Su un log in un file la conta gira su un altro filo; i QSO arrivati nel
    // frattempo si guardano quando finisce.
    QThreadPool m_categoryPool;
    int m_categoryGeneration{0};
    bool m_categoryRunning{false};
    mutable QList<qint64> m_categoryLater;
    mutable QString m_seenLastOn;
    mutable qint64 m_seenLastId{0};
    mutable bool m_seenValid{false};

    struct Row {
        qint64  id{0};
        QString sortKey;
        QString values[ColumnCount];
        // Le colonne mostrate che non sono fra quelle di sempre, nell'ordine
        // di m_extra.
        QStringList extra;
    };
    // La riga in quella posizione, letta con la sua pagina se non c'e' gia'.
    const Row* rowAt(int row) const;
    void dropPages();

    QString selectSql(const QString& where) const;
    // Le categorie di tutto il log in una volta (reload), o di un QSO solo.
    void computeCategories();
    QString categoryOf(qint64 id) const;
    Row rowFromQuery(const QSqlQuery& q) const;
    void refreshTotal();

    // La colonna mostrata in quella posizione, come valore: fra quelle di
    // sempre (core >= 0) o fra le altre (extra >= 0).
    struct Slot {
        int core{-1};
        int extra{-1};
    };
    void rebuildSlots();

    decolog::core::LogDatabase* m_db;
    // Le righe mostrate, nell'ordine della tabella.
    QVector<qint64> m_ids;
    // Evidenziata: l'ultima arrivata, quella che l'operatore cerca con lo sguardo.
    qint64 m_freshId{0};
    // Le pagine lette, e l'ordine in cui sono state usate (la prima e' la
    // prima a uscire).
    mutable QHash<int, QVector<Row>> m_pages;
    mutable QList<int> m_pageUse;
    QStringList m_layout;
    QStringList m_extra;        // le chiavi mostrate che non sono di sempre
    QVector<Slot> m_slots;
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
