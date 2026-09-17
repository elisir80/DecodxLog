// DecoLog — il punto d'incontro fra log, rete e interfaccia.
//
// Riceve i QSO dal protocollo UDP, li scrive nel database, aggiorna la tabella e
// racconta all'operatore cosa e' successo: un QSO scartato come duplicato va
// detto, non taciuto.
#pragma once

#include "app/ClusterController.h"
#include "app/QslController.h"
#include "app/QsoTableModel.h"
#include "app/StationProfileModel.h"
#include "core/Awards.h"
#include "core/Callbook.h"
#include "core/Countries.h"
#include "core/DecoLinkServer.h"
#include "core/CredentialStore.h"
#include "core/LogDatabase.h"
#include "core/Lotw.h"
#include "core/UdpReceiver.h"

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace decolog::app {

class DecoLogController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(bool databaseOpen READ databaseOpen CONSTANT)
    Q_PROPERTY(QObject* qsoModel READ qsoModel CONSTANT)
    Q_PROPERTY(QObject* stationProfiles READ stationProfiles CONSTANT)
    Q_PROPERTY(QObject* credentials READ credentials CONSTANT)
    Q_PROPERTY(QObject* cluster READ cluster CONSTANT)
    Q_PROPERTY(QObject* qsl READ qsl CONSTANT)

    // ── Collegamento con Decodium ──────────────────────────────────────────
    Q_PROPERTY(int udpPort READ udpPort WRITE setUdpPort NOTIFY udpChanged)
    Q_PROPERTY(QString multicastGroup READ multicastGroup WRITE setMulticastGroup NOTIFY udpChanged)
    Q_PROPERTY(bool preferLoggedAdif READ preferLoggedAdif WRITE setPreferLoggedAdif NOTIFY udpChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY udpChanged)
    Q_PROPERTY(QString udpError READ udpError NOTIFY udpChanged)
    Q_PROPERTY(int dedupDigitalMinutes READ dedupDigitalMinutes WRITE setDedupDigitalMinutes NOTIFY udpChanged)
    Q_PROPERTY(int dedupManualMinutes READ dedupManualMinutes WRITE setDedupManualMinutes NOTIFY udpChanged)
    Q_PROPERTY(bool followDxCall READ followDxCall WRITE setFollowDxCall NOTIFY udpChanged)

    Q_PROPERTY(bool clientConnected READ clientConnected NOTIFY clientChanged)
    Q_PROPERTY(QString clientName READ clientName NOTIFY clientChanged)
    Q_PROPERTY(QString clientVersion READ clientVersion NOTIFY clientChanged)
    Q_PROPERTY(QString dialFrequency READ dialFrequency NOTIFY clientChanged)
    Q_PROPERTY(QString dialBand READ dialBand NOTIFY clientChanged)
    Q_PROPERTY(QString currentMode READ currentMode NOTIFY clientChanged)
    Q_PROPERTY(QString dxCall READ dxCall NOTIFY clientChanged)
    Q_PROPERTY(QString deCall READ deCall NOTIFY clientChanged)
    Q_PROPERTY(bool transmitting READ transmitting NOTIFY clientChanged)

    // ── Log ────────────────────────────────────────────────────────────────
    Q_PROPERTY(int qsoCount READ qsoCount NOTIFY logChanged)
    Q_PROPERTY(int dirtyCount READ dirtyCount NOTIFY logChanged)
    Q_PROPERTY(int conflictCount READ conflictCount NOTIFY logChanged)
    Q_PROPERTY(QVariantMap ft2Award READ ft2Award NOTIFY logChanged)
    Q_PROPERTY(QVariantList bandStats READ bandStats NOTIFY logChanged)
    Q_PROPERTY(QVariantList modeStats READ modeStats NOTIFY logChanged)
    Q_PROPERTY(QVariantList qslSummary READ qslSummary NOTIFY logChanged)
    Q_PROPERTY(QVariantList gridPoints READ gridPoints NOTIFY logChanged)
    Q_PROPERTY(QVariantList incoming READ incoming NOTIFY incomingChanged)
    Q_PROPERTY(QVariantList activity READ activity NOTIFY activityChanged)

    Q_PROPERTY(QString lookupCall READ lookupCall WRITE setLookupCall NOTIFY lookupChanged)
    Q_PROPERTY(QVariantMap callInfo READ callInfo NOTIFY lookupChanged)
    Q_PROPERTY(QString myGrid READ myGrid NOTIFY stationChanged)
    Q_PROPERTY(QVariantMap myPosition READ myPosition NOTIFY stationChanged)

    Q_PROPERTY(QStringList bands READ bands CONSTANT)
    // "auto", "it", "en": si applica al riavvio.
    Q_PROPERTY(QString uiLanguage READ uiLanguage WRITE setUiLanguage NOTIFY uiLanguageChanged)

    // ── Entita' DXCC (cty.csv di AD1C) ─────────────────────────────────────
    Q_PROPERTY(QString countriesVersion READ countriesVersion NOTIFY countriesChanged)
    Q_PROPERTY(int countriesEntities READ countriesEntities NOTIFY countriesChanged)
    Q_PROPERTY(QString countriesSource READ countriesSource NOTIFY countriesChanged)
    Q_PROPERTY(int missingDxccCount READ missingDxccCount NOTIFY logChanged)

    // ── DecoLink (canale locale con Decodium) ──────────────────────────────
    Q_PROPERTY(bool decoLinkEnabled READ decoLinkEnabled WRITE setDecoLinkEnabled NOTIFY decoLinkChanged)
    Q_PROPERTY(int decoLinkPort READ decoLinkPort WRITE setDecoLinkPort NOTIFY decoLinkChanged)
    Q_PROPERTY(bool decoLinkListening READ decoLinkListening NOTIFY decoLinkChanged)
    Q_PROPERTY(QString decoLinkError READ decoLinkError NOTIFY decoLinkChanged)
    Q_PROPERTY(QVariantList decoLinkClients READ decoLinkClients NOTIFY decoLinkChanged)

    // ── Award ──────────────────────────────────────────────────────────────
    Q_PROPERTY(QVariantList awardSummary READ awardSummary NOTIFY awardsChanged)
    Q_PROPERTY(QStringList awardBands READ awardBands NOTIFY awardsChanged)
    Q_PROPERTY(QString awardBand READ awardBand WRITE setAwardBand NOTIFY awardsChanged)
    Q_PROPERTY(QString awardModeGroup READ awardModeGroup WRITE setAwardModeGroup NOTIFY awardsChanged)
    Q_PROPERTY(bool awardConfirmLotw READ awardConfirmLotw WRITE setAwardConfirmLotw NOTIFY awardsChanged)
    Q_PROPERTY(bool awardConfirmCard READ awardConfirmCard WRITE setAwardConfirmCard NOTIFY awardsChanged)
    Q_PROPERTY(bool awardConfirmEqsl READ awardConfirmEqsl WRITE setAwardConfirmEqsl NOTIFY awardsChanged)
    Q_PROPERTY(int awardProfile READ awardProfile WRITE setAwardProfile NOTIFY awardsChanged)
    Q_PROPERTY(QString awardTag READ awardTag WRITE setAwardTag NOTIFY awardsChanged)

    // ── Callbook (QRZ.com / HamQTH) ────────────────────────────────────────
    Q_PROPERTY(QString callbookProvider READ callbookProvider WRITE setCallbookProvider NOTIFY callbookChanged)
    Q_PROPERTY(bool callbookAutofill READ callbookAutofill WRITE setCallbookAutofill NOTIFY callbookChanged)
    Q_PROPERTY(QString callbookStatus READ callbookStatus NOTIFY callbookChanged)
    Q_PROPERTY(bool callbookBusy READ callbookBusy NOTIFY callbookChanged)

    // ── LoTW (conferme) ────────────────────────────────────────────────────
    Q_PROPERTY(bool lotwBusy READ lotwBusy NOTIFY lotwChanged)
    Q_PROPERTY(QString lotwStatus READ lotwStatus NOTIFY lotwChanged)
    Q_PROPERTY(QString lotwLastSync READ lotwLastSync NOTIFY lotwChanged)
    Q_PROPERTY(QString lotwCursor READ lotwCursor NOTIFY lotwChanged)
    Q_PROPERTY(int lotwAutoHours READ lotwAutoHours WRITE setLotwAutoHours NOTIFY lotwChanged)

    // ── Backup ─────────────────────────────────────────────────────────────
    Q_PROPERTY(bool backupEnabled READ backupEnabled WRITE setBackupEnabled NOTIFY backupChanged)
    Q_PROPERTY(QString backupDir READ backupDir WRITE setBackupDir NOTIFY backupChanged)
    Q_PROPERTY(QString backupTime READ backupTime WRITE setBackupTime NOTIFY backupChanged)
    Q_PROPERTY(int backupKeep READ backupKeep WRITE setBackupKeep NOTIFY backupChanged)
    Q_PROPERTY(QString lastBackup READ lastBackup NOTIFY backupChanged)
    Q_PROPERTY(QString lastBackupInfo READ lastBackupInfo NOTIFY backupChanged)

    // ── Cloud (Fase 3: per ora solo le preferenze) ─────────────────────────
    Q_PROPERTY(QString cloudServer READ cloudServer WRITE setCloudServer NOTIFY cloudChanged)
    Q_PROPERTY(QString autoSync READ autoSync WRITE setAutoSync NOTIFY cloudChanged)
    Q_PROPERTY(QString conflictPolicy READ conflictPolicy WRITE setConflictPolicy NOTIFY cloudChanged)

public:
    explicit DecoLogController(QObject* parent = nullptr);
    ~DecoLogController() override;

    bool openDatabase(const QString& path);
    void startListening();
    // Porta da riga di comando: vale per questa sessione, non si salva.
    void overrideUdpPort(int port) { m_udpPort = port; }

    QString version() const;
    QString databasePath() const { return m_db.path(); }
    bool databaseOpen() const { return m_db.isOpen(); }
    QObject* qsoModel() const { return m_model; }
    QObject* stationProfiles() const { return m_profiles; }
    QObject* credentials() const { return m_credentials; }
    QObject* cluster() const { return m_cluster; }
    QObject* qsl() const { return m_qsl; }
    // Dopo openDatabase e startDecoLink: le fonti del cluster si collegano.
    void startCluster();

    int udpPort() const { return m_udpPort; }
    void setUdpPort(int port);
    QString multicastGroup() const { return m_multicast; }
    void setMulticastGroup(const QString& group);
    bool preferLoggedAdif() const { return m_udp.prefersLoggedAdif(); }
    void setPreferLoggedAdif(bool prefer);
    bool listening() const { return m_udp.isListening(); }
    QString udpError() const { return m_udp.lastError(); }
    int dedupDigitalMinutes() const { return m_db.dedupWindowSeconds(false) / 60; }
    void setDedupDigitalMinutes(int minutes);
    int dedupManualMinutes() const { return m_db.dedupWindowSeconds(true) / 60; }
    void setDedupManualMinutes(int minutes);
    bool followDxCall() const { return m_followDx; }
    void setFollowDxCall(bool follow);

    bool clientConnected() const;
    QString clientName() const { return m_clientName; }
    QString clientVersion() const { return m_clientVersion; }
    QString dialFrequency() const;
    QString dialBand() const;
    QString currentMode() const { return m_status.submode.isEmpty() ? m_status.mode : m_status.submode; }
    QString dxCall() const { return m_status.dxCall; }
    QString deCall() const { return m_status.deCall; }
    bool transmitting() const { return m_status.transmitting; }

    int qsoCount() const { return m_db.qsoCount(); }
    int dirtyCount() const { return m_db.dirtyCount(); }
    int conflictCount() const { return m_db.conflictCount(); }
    QVariantMap ft2Award() const;
    QVariantList bandStats() const;
    QVariantList modeStats() const;
    QVariantList qslSummary() const;
    QVariantList gridPoints() const;
    QVariantList incoming() const { return m_incoming; }
    QVariantList activity() const { return m_activity; }

    QString lookupCall() const { return m_lookupCall; }
    void setLookupCall(const QString& call);
    QVariantMap callInfo() const { return m_callInfo; }
    QString myGrid() const;
    QVariantMap myPosition() const;

    QStringList bands() const;
    QString uiLanguage() const;
    void setUiLanguage(const QString& language);

    QString countriesVersion() const { return m_countries.version(); }
    int countriesEntities() const { return m_countries.entityCount(); }
    QString countriesSource() const { return m_countriesSource; }
    int missingDxccCount() const { return static_cast<int>(m_db.idsWithoutDxcc().size()); }

    bool decoLinkEnabled() const { return m_decoLinkEnabled; }
    void setDecoLinkEnabled(bool enabled);
    int decoLinkPort() const { return m_decoLinkPort; }
    void setDecoLinkPort(int port);
    bool decoLinkListening() const { return m_decoLink.isListening(); }
    QString decoLinkError() const { return m_decoLink.lastError(); }
    QVariantList decoLinkClients() const;
    void startDecoLink();

    QVariantList awardSummary() const;
    QStringList awardBands() const;
    QString awardBand() const { return m_awardFilter.band; }
    void setAwardBand(const QString& band);
    QString awardModeGroup() const { return m_awardFilter.modeGroup; }
    void setAwardModeGroup(const QString& group);
    bool awardConfirmLotw() const { return m_awardFilter.confirmLotw; }
    void setAwardConfirmLotw(bool on);
    bool awardConfirmCard() const { return m_awardFilter.confirmCard; }
    void setAwardConfirmCard(bool on);
    bool awardConfirmEqsl() const { return m_awardFilter.confirmEqsl; }
    void setAwardConfirmEqsl(bool on);
    int awardProfile() const { return static_cast<int>(m_awardFilter.stationProfileId); }
    void setAwardProfile(int profileId);
    QString awardTag() const { return m_awardFilter.tag; }
    void setAwardTag(const QString& tag);
    // Gli elementi di un award per la tabella. `view`: "all", "unconfirmed" o
    // "missing" (quelli mai lavorati, per gli award con un elenco chiuso).
    Q_INVOKABLE QVariantList awardItems(const QString& awardId, const QString& search, const QString& view) const;
    // Per banda (le colonne della tabella): [{band, worked, confirmed}].
    Q_INVOKABLE QVariantList awardBandTotals(const QString& awardId) const;
    // DXCC, FT2, WAZ e WAS hanno un elenco completo: si puo' dire cosa manca.
    Q_INVOKABLE bool awardHasMissing(const QString& awardId) const;
    // I locatori dell'award "grids" per la mappa: [{grid, confirmed}].
    Q_INVOKABLE QVariantList awardGrids() const;

    QString callbookProvider() const { return core::CallbookClient::providerId(m_callbook.provider()); }
    void setCallbookProvider(const QString& id);
    bool callbookAutofill() const { return m_callbookAutofill; }
    void setCallbookAutofill(bool autofill);
    QString callbookStatus() const { return m_callbookStatus; }
    bool callbookBusy() const { return !m_callbookPending.isEmpty(); }

    bool lotwBusy() const { return m_lotw.busy() || m_lotwStarting; }
    QString lotwStatus() const { return m_lotwStatus; }
    QString lotwLastSync() const;
    QString lotwCursor() const { return m_db.setting(QStringLiteral("lotw.last_qsl")); }
    int lotwAutoHours() const { return m_lotwAutoHours; }
    void setLotwAutoHours(int hours);
    // Scarica le conferme nuove (o tutte, `full`) e le segna sui QSO.
    Q_INVOKABLE void syncLotw(bool full = false);
    Q_INVOKABLE void cancelLotw() { m_lotw.cancel(); }

    bool backupEnabled() const { return m_backupEnabled; }
    void setBackupEnabled(bool enabled);
    QString backupDir() const { return m_backupDir; }
    void setBackupDir(const QString& dir);
    QString backupTime() const { return m_backupTime; }
    void setBackupTime(const QString& hhmm);
    int backupKeep() const { return m_backupKeep; }
    void setBackupKeep(int keep);
    QString lastBackup() const;
    QString lastBackupInfo() const;

    QString cloudServer() const { return m_cloudServer; }
    void setCloudServer(const QString& url);
    QString autoSync() const { return m_autoSync; }
    void setAutoSync(const QString& mode);
    QString conflictPolicy() const { return m_conflictPolicy; }
    void setConflictPolicy(const QString& policy);

    // Campi: call, date (yyyy-MM-dd), time (HH:mm), band, freq, mode, submode,
    // rst_sent, rst_rcvd, name, qth, gridsquare, tx_pwr, pota_ref, sota_ref,
    // iota, wwff_ref, comment. Restituisce un messaggio d'errore, o stringa vuota
    // se il QSO e' stato scritto.
    Q_INVOKABLE QString logManualQso(const QVariantMap& fields);
    Q_INVOKABLE QVariantMap utcNow() const;

    // Scheda di un QSO: campi ADIF, dati di sync, QSL, storico, effetto sugli award.
    Q_INVOKABLE QVariantMap qsoDetail(qint64 id) const;
    // `fields` e' la mappa ADIF completa (come in qsoDetail().fields).
    Q_INVOKABLE QString saveQso(qint64 id, const QVariantMap& fields, qint64 stationProfileId);
    Q_INVOKABLE bool deleteQso(qint64 id);
    Q_INVOKABLE QString restoreRevision(qint64 id, qint64 historyId);

    // Etichette: aggiunge o toglie `tag` ai QSO indicati. Restituisce quanti sono cambiati.
    Q_INVOKABLE int tagQsos(const QVariantList& ids, const QString& tag, bool add);
    // Le entita' presenti nel log per il filtro: [{dxcc, name, count}].
    Q_INVOKABLE QVariantList dxccInLog() const;
    Q_INVOKABLE QString dxccName(int dxcc) const { return m_countries.nameFor(dxcc); }

    Q_INVOKABLE void importAdif(const QUrl& file);
    Q_INVOKABLE void exportAdif(const QUrl& file);
    Q_INVOKABLE void exportQsos(const QVariantList& ids, const QUrl& file);
    Q_INVOKABLE QString bandForFrequency(const QString& mhz) const;
    Q_INVOKABLE void backupNow();
    // Completa DXCC, paese, zone e continente dei QSO che non li hanno. Ogni QSO
    // modificato diventa una nuova revisione. Restituisce quanti ne ha completati.
    Q_INVOKABLE int fillMissingDxcc();
    // Carica un cty.csv scelto dall'operatore e lo copia nella cartella dei dati.
    Q_INVOKABLE QString installCountries(const QUrl& file);
    Q_INVOKABLE void clearActivity();
    Q_INVOKABLE void openDatabaseFolder() const;
    Q_INVOKABLE QString localPath(const QUrl& url) const { return url.toLocalFile(); }

signals:
    void udpChanged();
    void clientChanged();
    void logChanged();
    void incomingChanged();
    void activityChanged();
    void lookupChanged();
    void stationChanged();
    void backupChanged();
    void cloudChanged();
    void countriesChanged();
    void callbookChanged();
    void awardsChanged();
    void decoLinkChanged();
    void uiLanguageChanged();
    void lotwChanged();

private:
    void onQsoReceived(const core::AdifRecord& record, const QString& source, const QString& sourceApp);
    void addActivity(const QString& category, const QString& text, const QString& level = QStringLiteral("info"));
    void refreshCallInfo();
    void maybeCreateProfileFromDecodium();
    void checkBackupSchedule();
    // Aggiunge al record i dati del profilo che il record non ha gia'.
    void applyProfile(core::AdifRecord& record, qint64 profileId) const;
    // Aggiunge DXCC, COUNTRY, CQZ, ITUZ e CONT se mancano. true se ha aggiunto qualcosa.
    bool applyEntity(core::AdifRecord& record) const;
    void loadCountries();
    void onLotwReport(const core::lotw::Report& report);
    void checkLotwSchedule();
    QSet<QString> confirmedAwardKeys(const QString& awardId) const;
    void requestCallbook();
    const QList<core::AwardResult>& awardResults() const;
    // Gli stessi award senza i filtri di banda, modo, profilo ed etichetta (solo le
    // conferme scelte): quello che vede Decodium e che decide "nuovo confermato".
    const QList<core::AwardResult>& globalAwardResults() const;
    void awardFilterChanged();
    QJsonObject decoLinkAward() const;
    QJsonArray decoLinkQuery(const QJsonObject& query) const;
    void decoLinkQso(const core::AdifRecord& record, const QString& status, qint64 id,
                     const QString& source, const QString& app, const QString& message = {});

    core::LogDatabase m_db;
    core::Countries   m_countries;
    core::CredentialStore* m_credentials{nullptr};
    core::CallbookClient m_callbook;
    core::AwardFilter m_awardFilter;
    core::DecoLinkServer m_decoLink;
    core::LotwClient  m_lotw;
    bool      m_lotwStarting{false};
    bool      m_lotwAuto{false};
    QString   m_lotwStatus;
    int       m_lotwAutoHours{12};
    QTimer    m_lotwTimer;
    bool      m_decoLinkEnabled{true};
    int       m_decoLinkPort{core::DecoLinkServer::kDefaultPort};
    QTimer    m_decoLinkAwardDebounce;
    mutable QList<core::AwardResult> m_awardCache;
    mutable bool m_awardsDirty{true};
    mutable QList<core::AwardResult> m_globalAwardCache;
    mutable bool m_globalAwardsDirty{true};
    bool      m_callbookAutofill{true};
    QString   m_callbookStatus;
    QString   m_callbookPending;
    QTimer    m_callbookDebounce;
    QHash<QString, QVariantMap> m_callbookResults;   // per nominativo, sessione corrente
    QHash<QString, QString> m_callbookErrors;
    QString           m_countriesSource;
    core::UdpReceiver m_udp;
    QsoTableModel*    m_model{nullptr};
    StationProfileModel* m_profiles{nullptr};
    ClusterController*   m_cluster{nullptr};
    QslController*       m_qsl{nullptr};

    int       m_udpPort{2237};
    QString   m_multicast;
    bool      m_followDx{true};
    QString   m_clientName;
    QString   m_clientVersion;
    QDateTime m_clientLastSeen;
    core::wsjtx::Status m_status;
    QTimer    m_clientWatch;

    QVariantList m_incoming;
    QVariantList m_activity;
    QString      m_lookupCall;
    QVariantMap  m_callInfo;

    bool    m_backupEnabled{true};
    QString m_backupDir;
    QString m_backupTime{QStringLiteral("02:00")};
    int     m_backupKeep{14};
    QTimer  m_backupTimer;

    QString m_cloudServer;
    QString m_autoSync;
    QString m_conflictPolicy;
};

} // namespace decolog::app
