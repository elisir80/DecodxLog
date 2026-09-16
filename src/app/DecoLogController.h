// DecoLog — il punto d'incontro fra log, rete e interfaccia.
//
// Riceve i QSO dal protocollo UDP, li scrive nel database, aggiorna la tabella e
// racconta all'operatore cosa e' successo: un QSO scartato come duplicato va
// detto, non taciuto.
#pragma once

#include "app/QsoTableModel.h"
#include "core/LogDatabase.h"
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

    Q_PROPERTY(int udpPort READ udpPort WRITE setUdpPort NOTIFY udpChanged)
    Q_PROPERTY(QString multicastGroup READ multicastGroup WRITE setMulticastGroup NOTIFY udpChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY udpChanged)
    Q_PROPERTY(QString udpError READ udpError NOTIFY udpChanged)

    Q_PROPERTY(bool clientConnected READ clientConnected NOTIFY clientChanged)
    Q_PROPERTY(QString clientName READ clientName NOTIFY clientChanged)
    Q_PROPERTY(QString clientVersion READ clientVersion NOTIFY clientChanged)
    Q_PROPERTY(QString dialFrequency READ dialFrequency NOTIFY clientChanged)
    Q_PROPERTY(QString currentMode READ currentMode NOTIFY clientChanged)
    Q_PROPERTY(QString dxCall READ dxCall NOTIFY clientChanged)
    Q_PROPERTY(QString deCall READ deCall NOTIFY clientChanged)
    Q_PROPERTY(bool transmitting READ transmitting NOTIFY clientChanged)

    Q_PROPERTY(int qsoCount READ qsoCount NOTIFY logChanged)
    Q_PROPERTY(int dirtyCount READ dirtyCount NOTIFY logChanged)
    Q_PROPERTY(QVariantList incoming READ incoming NOTIFY incomingChanged)
    Q_PROPERTY(QVariantList activity READ activity NOTIFY activityChanged)

    Q_PROPERTY(QString lookupCall READ lookupCall WRITE setLookupCall NOTIFY lookupChanged)
    Q_PROPERTY(QVariantMap workedBefore READ workedBefore NOTIFY lookupChanged)

    Q_PROPERTY(QStringList bands READ bands CONSTANT)

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

    int udpPort() const { return m_udpPort; }
    void setUdpPort(int port);
    QString multicastGroup() const { return m_multicast; }
    void setMulticastGroup(const QString& group);
    bool listening() const { return m_udp.isListening(); }
    QString udpError() const { return m_udp.lastError(); }

    bool clientConnected() const;
    QString clientName() const { return m_clientName; }
    QString clientVersion() const { return m_clientVersion; }
    QString dialFrequency() const;
    QString currentMode() const { return m_status.submode.isEmpty() ? m_status.mode : m_status.submode; }
    QString dxCall() const { return m_status.dxCall; }
    QString deCall() const { return m_status.deCall; }
    bool transmitting() const { return m_status.transmitting; }

    int qsoCount() const { return m_db.qsoCount(); }
    int dirtyCount() const { return m_db.dirtyCount(); }
    QVariantList incoming() const { return m_incoming; }
    QVariantList activity() const { return m_activity; }

    QString lookupCall() const { return m_lookupCall; }
    void setLookupCall(const QString& call);
    QVariantMap workedBefore() const { return m_workedBefore; }

    QStringList bands() const;

    // Campi: call, date (yyyy-MM-dd), time (HH:mm), band, freq, mode, submode,
    // rst_sent, rst_rcvd, name, qth, gridsquare, comment. Restituisce un
    // messaggio d'errore, o stringa vuota se il QSO e' stato scritto.
    Q_INVOKABLE QString logManualQso(const QVariantMap& fields);
    Q_INVOKABLE void importAdif(const QUrl& file);
    Q_INVOKABLE void exportAdif(const QUrl& file);
    Q_INVOKABLE QString bandForFrequency(const QString& mhz) const;

signals:
    void udpChanged();
    void clientChanged();
    void logChanged();
    void incomingChanged();
    void activityChanged();
    void lookupChanged();

private:
    void onQsoReceived(const core::AdifRecord& record, const QString& source, const QString& sourceApp);
    void addActivity(const QString& text, const QString& level = QStringLiteral("info"));
    void refreshWorkedBefore();

    core::LogDatabase m_db;
    core::UdpReceiver m_udp;
    QsoTableModel*    m_model{nullptr};

    int       m_udpPort{2237};
    QString   m_multicast;
    QString   m_clientName;
    QString   m_clientVersion;
    QDateTime m_clientLastSeen;
    core::wsjtx::Status m_status;
    QTimer    m_clientWatch;

    QVariantList m_incoming;
    QVariantList m_activity;
    QString      m_lookupCall;
    QVariantMap  m_workedBefore;
};

} // namespace decolog::app
