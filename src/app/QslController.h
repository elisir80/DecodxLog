// DecoLog — l'invio delle QSL: coda, stato per servizio, risultati.
//
// LoTW parte in blocco (un file ADIF firmato da TQSL), QRZ Logbook ed eQSL un QSO
// alla volta. Quello che il servizio accetta diventa "inviato" nella tabella
// qsl_status; un duplicato conta come inviato, perche' ritentarlo non serve; un
// rifiuto resta scritto sul QSO con il motivo, e non blocca gli altri.
#pragma once

#include "core/QslUpload.h"

#include <QDir>
#include <QHash>
#include <QObject>
#include <QQueue>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

namespace decolog::core {
class CredentialStore;
class LogDatabase;
}

namespace decolog::app {

class QslController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList services READ services NOTIFY changed)
    Q_PROPERTY(QString tqslPath READ tqslPath WRITE setTqslPath NOTIFY changed)
    Q_PROPERTY(QStringList tqslLocations READ tqslLocations NOTIFY changed)
    Q_PROPERTY(QString tqslLocation READ tqslLocation WRITE setTqslLocation NOTIFY changed)
    Q_PROPERTY(bool tqslReady READ tqslReady NOTIFY changed)
    Q_PROPERTY(QString clubLogApiKey READ clubLogApiKey WRITE setClubLogApiKey NOTIFY changed)
    Q_PROPERTY(QString tqslStatus READ tqslStatus NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString busyService READ busyService NOTIFY changed)

public:
    struct Context {
        core::LogDatabase* db{nullptr};
        core::CredentialStore* credentials{nullptr};
        std::function<QString()> stationLocation;   // la station location del profilo attivo
        std::function<QString()> stationCallsign;   // il nominativo del profilo attivo
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
        std::function<void()> logChanged;
    };

    explicit QslController(Context context, QObject* parent = nullptr);

    // I servizi con cui si puo' caricare: id, label, in coda, inviati, confermati,
    // errori, configurato, automatico, ultimo esito.
    QVariantList services() const;
    QString tqslPath() const { return m_tqslPath; }
    void setTqslPath(const QString& path);
    QStringList tqslLocations() const { return core::qsl::tqslStationLocations(); }
    QString tqslLocation() const { return m_tqslLocation; }
    void setTqslLocation(const QString& location);
    bool tqslReady() const;
    QString clubLogApiKey() const { return m_clubLogApiKey; }
    void setClubLogApiKey(const QString& key);
    QString tqslStatus() const;
    // Dove TQSL tiene certificati e station location: serve per capirci qualcosa
    // quando qualcosa non torna.
    Q_INVOKABLE QString tqslDataDirectory() const { return QDir::toNativeSeparators(core::qsl::tqslDataDirectory()); }
    bool busy() const { return !m_busyService.isEmpty(); }
    QString busyService() const { return m_busyService; }

    // Manda i QSO in attesa; `limit` 0 = tutti.
    Q_INVOKABLE void uploadPending(const QString& service, int limit = 0);
    // Un QSO solo (dalla sua scheda).
    Q_INVOKABLE void uploadQso(qint64 id, const QString& service);
    // Un gruppo di QSO scelti nel log. Torna quanti ne parte davvero: quelli
    // gia' inviati a quel servizio non si rimandano.
    Q_INVOKABLE int uploadQsos(const QVariantList& ids, const QString& service);
    Q_INVOKABLE void setAutoUpload(const QString& service, bool automatic);
    Q_INVOKABLE bool autoUpload(const QString& service) const;
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void refresh() { emit changed(); }

    // Un QSO appena scritto nel log: se il servizio e' automatico, parte da solo.
    void qsoLogged(qint64 id);

signals:
    void changed();

private:
    void startNext();
    void uploadNextWeb();
    void finishBatch(const core::QslUploadResult& result);
    void markSent(qint64 id, const QString& service, const core::QslUploadResult& result);
    void note(const QString& text, const QString& level);
    void readSecret(const QString& service, std::function<void(const QString&, const QString&)> done);

    Context m_ctx;
    core::TqslUploader m_tqsl;
    core::WebQslUploader m_web;
    QString m_tqslPath;
    QString m_clubLogApiKey;
    QString m_tqslLocation;
    QString m_busyService;
    QList<qint64> m_batch;          // i QSO dell'invio in corso
    bool m_batchMode{false};        // LoTW e Club Log: tutto il blocco in una volta
    QQueue<qint64> m_pending;       // per QRZ ed eQSL, uno alla volta
    QString m_secret;               // credenziale del servizio in corso, solo durante l'invio
    QString m_account;
    QHash<QString, QString> m_lastResult;
    QHash<QString, bool> m_auto;
    QString m_adifFile;             // file temporaneo di TQSL
    int m_accepted{0};
    int m_duplicates{0};
    int m_rejected{0};
    QTimer m_autoDelay;
    QList<qint64> m_autoQueue;
};

} // namespace decolog::app
