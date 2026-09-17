// DecoLog — la lista degli spot per il QML.
//
// Tiene tutti gli spot degli ultimi sessanta minuti, raggruppati per nominativo,
// banda e modo (dieci skimmer che sentono lo stesso DX sono una riga con "10"), e
// mostra quelli che passano il filtro, dal piu' recente. Gli arrivi aggiornano la
// lista riga per riga: un flusso RBN non fa saltare la vista a ogni spot.
#pragma once

#include "core/Spots.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QTimer>
#include <functional>
#include <memory>

namespace decolog::app {

class SpotModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)

public:
    enum Roles {
        KeyRole = Qt::UserRole + 1, CallRole, FreqRole, BandRole, ModeRole, TimeRole, AgeRole, SpotterRole,
        SpottersRole, CountRole, CommentRole, SourceRole, SourceNameRole, EntityRole, DxccRole, ContinentRole,
        CqZoneRole, StatusRole, StatusLabelRole, DistanceRole, AzimuthRole, SnrRole, RefsRole, GridRole,
        LotwRole, SkimmerRole, FreshRole,
    };

    explicit SpotModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_rows.size()); }
    int totalCount() const { return static_cast<int>(m_all.size()); }

    // Aggiunge uno spot o aggiorna la riga dello stesso DX. Restituisce la riga
    // aggiornata (per gli avvisi) e se e' una novita' (prima volta in questa ora).
    const core::EnrichedSpot& add(const core::EnrichedSpot& spot, bool* isNew);
    void setFilter(const core::SpotFilter& filter);
    const core::SpotFilter& filter() const { return m_filter; }
    // Ricalcola lo stato di tutte le righe (il log e' cambiato).
    void restatus(const std::function<int(const core::EnrichedSpot&)>& status);
    void clear();

    Q_INVOKABLE QVariantMap get(int row) const;
    const core::EnrichedSpot* find(const QString& key) const;
    QList<core::EnrichedSpot> visible() const;

    static QString statusLabel(int status);

signals:
    void countChanged();

private:
    using Entry = std::shared_ptr<core::EnrichedSpot>;
    void rebuildRows();
    void expire();
    bool visibleEntry(const Entry& e) const;

    QList<Entry> m_all;                 // dal piu' recente
    QHash<QString, Entry> m_byKey;
    QList<Entry> m_rows;                // quelli mostrati
    core::SpotFilter m_filter;
    QTimer m_expiry;
};

} // namespace decolog::app
