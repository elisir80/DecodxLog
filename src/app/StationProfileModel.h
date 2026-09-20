// DecoDXLog — i profili stazione per il QML: elenco, profilo attivo, salvataggio.
//
// Il profilo attivo e' quello con cui si scrivono i QSO nuovi (a mano o da
// Decodium, quando il nominativo di stazione non indica un altro profilo). Si
// ricorda per postazione, in QSettings, perche' lo stesso log puo' essere aperto
// da casa e da un portatile.
#pragma once

#include <QAbstractListModel>
#include <QVariantMap>

#include "core/LogDatabase.h"

namespace decolog::app {

class StationProfileModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(qint64 activeProfileId READ activeProfileId WRITE setActiveProfileId NOTIFY activeChanged)
    Q_PROPERTY(QVariantMap activeProfile READ activeProfile NOTIFY activeChanged)
    Q_PROPERTY(QStringList names READ names NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1, NameRole, StationCallsignRole, OperatorRole, GridRole,
        CqZoneRole, ItuZoneRole, DxccRole, RigRole, AntennaRole, PowerRole, LotwLocationRole,
        IsDefaultRole, DeletedRole, DirtyRole, RevisionRole, QsoCountRole, SubtitleRole, UuidRole
    };

    explicit StationProfileModel(decolog::core::LogDatabase* db, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_profiles.size()); }
    qint64 activeProfileId() const { return m_activeId; }
    void setActiveProfileId(qint64 id);
    QVariantMap activeProfile() const;
    QStringList names() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QVariantMap byId(qint64 id) const;
    Q_INVOKABLE int rowForId(qint64 id) const;
    // Restituisce l'id salvato, 0 se non e' andata (nome o nominativo mancanti).
    Q_INVOKABLE qint64 save(const QVariantMap& profile);
    Q_INVOKABLE bool remove(qint64 id);

signals:
    void countChanged();
    void activeChanged();
    void profilesChanged();

private:
    void pickActive();

    decolog::core::LogDatabase* m_db;
    QList<decolog::core::StationProfile> m_profiles;
    qint64 m_activeId{0};
};

} // namespace decolog::app
