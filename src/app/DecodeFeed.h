// DecoDXLog — le due finestre di decodifica di Decodium, dentro il log.
//
// Decodium 4 mostra quello che sente in due liste: «Full Spectrum» (tutta la
// banda, ogni decodifica) e «Signal RX» (solo il QSO in corso: le trasmissioni
// proprie, i messaggi diretti a noi, quelli del corrispondente). Qui le si
// ricostruisce dal protocollo UDP, che porta ogni decodifica e lo stato
// (corrispondente, frequenze, trasmissione), con in piu' quello che solo il log
// sa: chi e' nuovo per l'entita', per la banda, per il nominativo.
//
// Le righe si mettono in testa, la piu' recente per prima, e si tengono poche:
// e' una finestra da tenere aperta accanto al log, non un archivio.
#pragma once

#include "core/WsjtxProtocol.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <functional>

namespace decolog::core {
class UdpReceiver;
} // namespace decolog::core

namespace decolog::app {

struct DecodeRow {
    quint64 serial{0};
    QString clientId;
    quint32 schema{3};
    QString time;           // HHmmss del periodo
    int slot{0};            // a ogni periodo nuovo cambia: serve a rigare
    int snr{0};
    double dt{0.0};
    int df{0};
    QString mode;           // il segno del protocollo: "~" FT8, "+" FT4…
    QString message;
    QString from;
    QString to;
    QString grid;
    bool cq{false};
    bool forMe{false};      // nomina uno dei nostri nominativi
    bool withDx{false};     // nomina il corrispondente in corso
    bool ownTx{false};      // la nostra trasmissione
    bool lowConfidence{false};
    int status{0};          // SpotStatus, come per gli spot del cluster
    QString statusLabel;
    QString entity;
    int azimuth{-1};
    int distanceKm{-1};
    core::wsjtx::Decode decode;   // com'e' arrivata: serve per rispondere
};

class DecodeListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        SerialRole = Qt::UserRole + 1, TimeRole, SlotRole, SnrRole, DtRole, DfRole, ModeRole, MessageRole,
        FromRole, ToRole, GridRole, CqRole, ForMeRole, WithDxRole, OwnTxRole, LowConfidenceRole, StatusRole,
        StatusLabelRole, EntityRole, AzimuthRole, DistanceRole,
    };

    explicit DecodeListModel(int limit, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_rows.size()); }
    const QList<DecodeRow>& rows() const { return m_rows; }
    const DecodeRow* find(quint64 serial) const;
    bool contains(const QString& time, int df, const QString& message) const;

    void prepend(const DecodeRow& row);
    void replaceAll(const QList<DecodeRow>& rows);
    void restatus(const std::function<void(DecodeRow&)>& fill);
    void clear();

signals:
    void countChanged();

private:
    QList<DecodeRow> m_rows;    // dalla piu' recente
    int m_limit;
};

class DecodeFeed : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* fullSpectrum READ fullSpectrum CONSTANT)
    Q_PROPERTY(QObject* signalRx READ signalRx CONSTANT)
    Q_PROPERTY(bool online READ online NOTIFY stateChanged)
    Q_PROPERTY(QString program READ program NOTIFY stateChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY stateChanged)
    Q_PROPERTY(QString band READ band NOTIFY stateChanged)
    Q_PROPERTY(double dialMhz READ dialMhz NOTIFY stateChanged)
    Q_PROPERTY(int rxDf READ rxDf NOTIFY stateChanged)
    Q_PROPERTY(int txDf READ txDf NOTIFY stateChanged)
    Q_PROPERTY(int period READ period NOTIFY stateChanged)
    Q_PROPERTY(QString dxCall READ dxCall NOTIFY stateChanged)
    Q_PROPERTY(QString myCall READ myCall NOTIFY stateChanged)
    Q_PROPERTY(QString txMessage READ txMessage NOTIFY stateChanged)
    Q_PROPERTY(bool transmitting READ transmitting NOTIFY stateChanged)
    Q_PROPERTY(bool txEnabled READ txEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool decoding READ decoding NOTIFY stateChanged)

public:
    struct Classification {
        int status{0};
        QString entity;
        int azimuth{-1};
        double distanceKm{-1.0};
    };

    struct Context {
        // I nominativi della stazione (profilo attivo e quello che Decodium dice).
        std::function<QStringList()> myCalls;
        std::function<Classification(const QString& call, const QString& band, const QString& mode)> classify;
        std::function<QString(const core::wsjtx::Status&)> bandOf;
        std::function<QString(int status)> statusLabel;
        // Un nominativo scelto: la scheda e il riquadro del QSO.
        std::function<void(const QString& call)> lookup;
        std::function<void(const QVariantMap& fields)> prepareQso;
        // Il file in cui Decodium tiene le decodifiche gia' fatte (db.sqlite):
        // all'apertura il pannello riparte da li', senza aspettare il prossimo
        // periodo. Vuoto: non c'e'.
        std::function<QString()> historyPath;
    };

    static constexpr int kFullLimit = 300;
    static constexpr int kSignalLimit = 150;
    // Decodium manda un battito ogni quindici secondi.
    static constexpr int kOnlineSeconds = 40;

    DecodeFeed(core::UdpReceiver* udp, Context context, QObject* parent = nullptr);

    QObject* fullSpectrum() { return &m_full; }
    QObject* signalRx() { return &m_signal; }
    bool online() const { return m_online; }
    QString program() const { return m_program; }
    QString mode() const { return m_status.submode.isEmpty() ? m_status.mode : m_status.submode; }
    QString band() const { return m_band; }
    double dialMhz() const { return static_cast<double>(m_status.dialFrequencyHz) / 1e6; }
    int rxDf() const { return static_cast<int>(m_status.rxDf); }
    int txDf() const { return static_cast<int>(m_status.txDf); }
    int period() const { return static_cast<int>(m_status.trPeriod); }
    QString dxCall() const { return m_status.dxCall; }
    QString myCall() const { return m_status.deCall; }
    QString txMessage() const { return m_status.txMessage; }
    bool transmitting() const { return m_status.transmitting; }
    bool txEnabled() const { return m_status.txEnabled; }
    bool decoding() const { return m_status.decoding; }

    Q_INVOKABLE void clearFullSpectrum() { m_full.clear(); }
    Q_INVOKABLE void clearSignalRx() { m_signal.clear(); }
    // Chiede a Decodium di mandare di nuovo le decodifiche che ha a schermo
    // (lo fa solo per la destinazione principale dei suoi rapporti UDP).
    Q_INVOKABLE bool replay();
    // Rilegge dal disco le decodifiche recenti di Decodium e le mette in
    // Full Spectrum e Signal RX, senza doppioni. Torna quante ne ha aggiunte.
    // Serve all'apertura del pannello, e dopo, se il pannello e' rimasto vuoto
    // perche' DecoDXLog era chiuso mentre Decodium decodificava.
    Q_INVOKABLE int loadHistory();
    static constexpr int kHistoryMinutes = 60;
    // Risponde a una riga come col doppio clic in Decodium: puo' far partire
    // la chiamata. `which`: 0 Full Spectrum, 1 Signal RX.
    Q_INVOKABLE bool reply(int which, qint64 serial);
    // Mette il nominativo della riga nella scheda e nel riquadro del QSO.
    Q_INVOKABLE void pick(int which, qint64 serial);
    Q_INVOKABLE QString lineText(int which, qint64 serial) const;

    // I nuovi stati dopo che il log e' cambiato.
    void restatus();

    // Per le schermate di prova: un QSO in corso con un po' di traffico attorno.
    Q_INVOKABLE void demo();

    // Per i test e la schermata di prova.
    void handleDecode(const QString& clientId, const core::wsjtx::Decode& decode, quint32 schema = 3);
    void handleStatus(const QString& clientId, const core::wsjtx::Status& status);

signals:
    void stateChanged();

private:
    void fill(DecodeRow& row) const;
    bool belongsToSignal(const DecodeRow& row) const;
    void rebuildSignal();
    void refreshOnline();
    const DecodeRow* rowOf(int which, qint64 serial) const;

    core::UdpReceiver* m_udp;
    Context m_ctx;
    bool m_historyTried{false};
    DecodeListModel m_full;
    DecodeListModel m_signal;
    core::wsjtx::Status m_status;
    QString m_band;
    QString m_program;
    QString m_lastClient;
    QString m_lastSlotTime;
    int m_slot{0};
    quint64 m_serial{0};
    bool m_online{false};
    bool m_transmitting{false};
    QDateTime m_lastHeard;
    QTimer m_onlineTimer;
};

} // namespace decolog::app
