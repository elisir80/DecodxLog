// DecoDXLog — la sessione di attivazione o di contest.
//
// Finche' e' aperta: ogni QSO prende la referenza dell'attivatore, il locatore del
// posto, l'etichetta e il numero progressivo; un nominativo gia' lavorato sulla
// stessa banda e nello stesso modo dentro la sessione e' un duplicato, anche se il
// QSO di prima e' di tre ore fa; i conteggi dicono quanto manca perche'
// l'attivazione valga (dieci QSO per POTA, quattro per SOTA).
#pragma once

#include "core/Activation.h"

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

namespace decolog::core { class LogDatabase; }

namespace decolog::app {

class ActivationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(int qsoCount READ qsoCount NOTIFY changed)
    Q_PROPERTY(int uniqueCalls READ uniqueCalls NOTIFY changed)
    Q_PROPERTY(int requiredQsos READ requiredQsos NOTIFY changed)
    Q_PROPERTY(int nextSerial READ nextSerial NOTIFY changed)
    Q_PROPERTY(QVariantList perBand READ perBand NOTIFY changed)
    Q_PROPERTY(QString lastQso READ lastQso NOTIFY changed)
    Q_PROPERTY(QString elapsed READ elapsed NOTIFY changed)
    Q_PROPERTY(QStringList kinds READ kinds CONSTANT)

public:
    struct Context {
        core::LogDatabase* db{nullptr};
        std::function<QString()> stationCall;
        std::function<QString()> stationGrid;
        std::function<qint64()> activeProfileId;
        std::function<void(const QString& category, const QString& text, const QString& level)> activity;
        std::function<void()> logChanged;
    };

    explicit ActivationController(Context context, QObject* parent = nullptr);

    // Da chiamare quando il log e' aperto: la sessione sopravvive a una chiusura
    // del programma (sta nel database, non nelle impostazioni: e' del log).
    void load();

    bool active() const { return m_session.active; }
    QVariantMap state() const;
    QString title() const { return m_session.title(); }
    int qsoCount() const { return m_qsoCount; }
    int uniqueCalls() const { return m_uniqueCalls; }
    int requiredQsos() const { return m_session.requiredQsos(); }
    int nextSerial() const { return m_session.nextSerial; }
    QVariantList perBand() const { return m_perBand; }
    QString lastQso() const { return m_lastQso; }
    QString elapsed() const;
    QStringList kinds() const;

    // {kind, reference, name, contestId, myGrid, tag, band, mode, serialEnabled, nextSerial}
    Q_INVOKABLE QString start(const QVariantMap& session);
    Q_INVOKABLE void update(const QVariantMap& session);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void refresh();
    // Gli id dei QSO della sessione, dal primo.
    Q_INVOKABLE QVariantList qsoIds() const;
    // Nome del file suggerito per l'invio (POTA e compagnia).
    Q_INVOKABLE QString suggestedFileName() const;
    // Scrive l'ADIF della sessione. Restituisce un messaggio d'errore, o "".
    Q_INVOKABLE QString exportAdif(const QUrl& file);
    // Scrive il Cabrillo del contest. `info` sono le righe della testata:
    // {contest, callsign, categoryOperator, categoryPower, categoryMode,
    //  categoryBand, categoryAssisted, gridLocator, location, club, name,
    //  address, email, operators, claimedScore, soapbox}. Torna "" se e' andata.
    Q_INVOKABLE QString exportCabrillo(const QUrl& file, const QVariantMap& info);
    // La testata gia' riempita con quello che il programma sa.
    Q_INVOKABLE QVariantMap cabrilloDefaults() const;

    // ── Finestra contest ────────────────────────────────────────────────────
    // Gli ultimi QSO della sessione, dal piu' recente.
    Q_INVOKABLE QVariantList recentQsos(int limit = 12) const;
    // QSO all'ora sugli ultimi 10 e 60 minuti, moltiplicatori, ultimo scambio.
    Q_INVOKABLE QVariantMap rate() const;
    // Questo nominativo e' gia' stato lavorato in questa sessione, su questa
    // banda e in questo modo?
    Q_INVOKABLE bool wouldDuplicate(const QString& call, const QString& band, const QString& mode) const
    {
        return isDuplicate(call, band, mode);
    }

    // ── Usate dal controller principale ─────────────────────────────────────
    const core::Activation& session() const { return m_session; }
    // Il QSO sarebbe un duplicato di questa attivazione?
    bool isDuplicate(const QString& call, const QString& band, const QString& mode) const;
    // Aggiunge al record quello che la sessione impone.
    void applyTo(core::AdifRecord& record) const;
    // Un QSO e' stato scritto: numero progressivo avanti e conteggi aggiornati.
    void qsoLogged();

signals:
    void changed();

private:
    void save();
    void recount();

    Context m_ctx;
    core::Activation m_session;
    int m_qsoCount{0};
    int m_uniqueCalls{0};
    QVariantList m_perBand;
    QString m_lastQso;
};

} // namespace decolog::app
