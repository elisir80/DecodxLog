// DecoDXLog — il monitor del traffico con Decodium.
//
// Tutto quello che passa fra DecoDXLog e Decodium 4, nei due versi: i
// datagrammi del protocollo UDP di WSJT-X (anche quelli inoltrati ai programmi
// accanto e le loro risposte), le righe DecoLink, e gli annunci DecoPort della
// radio in rete. Ogni riga dice chi, verso dove, che tipo e cosa porta.
//
// Ed e' a due vie: da qui si parla a Decodium sullo stesso socket da cui lui
// scrive (rispondere a una riga decodificata, fermare la trasmissione, il testo
// libero, le finestre, il locatore, evidenziare un nominativo) e sulla
// connessione DecoLink. DecoPort resta solo in ascolto: la sessione e' firmata
// e un comando puo' mettere in trasmissione la radio.
//
// Registra solo mentre la finestra e' aperta: chiusa, non costa niente.
#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

class QUdpSocket;

namespace decolog::core {
class DecoLinkServer;
class DecoPortListener;
class UdpReceiver;
} // namespace decolog::core

namespace decolog::app {

class TrafficMonitor : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int total READ total NOTIFY countChanged)
    Q_PROPERTY(bool showUdp READ showUdp WRITE setShowUdp NOTIFY filterChanged)
    Q_PROPERTY(bool showDecoLink READ showDecoLink WRITE setShowDecoLink NOTIFY filterChanged)
    Q_PROPERTY(bool showDecoPort READ showDecoPort WRITE setShowDecoPort NOTIFY filterChanged)
    Q_PROPERTY(bool hideRoutine READ hideRoutine WRITE setHideRoutine NOTIFY filterChanged)
    Q_PROPERTY(QString textFilter READ textFilter WRITE setTextFilter NOTIFY filterChanged)
    Q_PROPERTY(QStringList udpClients READ udpClients NOTIFY clientsChanged)
    Q_PROPERTY(int decoLinkClients READ decoLinkClients NOTIFY clientsChanged)
    Q_PROPERTY(bool decoPortListening READ decoPortListening NOTIFY decoPortChanged)
    Q_PROPERTY(QString decoPortError READ decoPortError NOTIFY decoPortChanged)
    Q_PROPERTY(QString lastResult READ lastResult NOTIFY lastResultChanged)
    Q_PROPERTY(bool lastResultOk READ lastResultOk NOTIFY lastResultChanged)

public:
    enum Roles {
        TimeRole = Qt::UserRole + 1,
        ChannelRole,
        DirectionRole,
        PeerRole,
        TypeRole,
        ClientRole,
        SummaryRole,
        BytesRole,
        CanReplyRole,
        SerialRole,
    };
    enum Channel : quint8 { Udp = 0, DecoLink = 1, DecoPort = 2 };

    static constexpr int kLimit = 5000;

    TrafficMonitor(core::UdpReceiver* udp, core::DecoLinkServer* decoLink, QObject* parent = nullptr);
    ~TrafficMonitor() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool active() const { return m_active; }
    void setActive(bool on);
    bool paused() const { return m_paused; }
    void setPaused(bool on);
    int count() const { return static_cast<int>(m_rows.size()); }
    int total() const { return static_cast<int>(m_all.size()); }
    bool showUdp() const { return m_show[Udp]; }
    void setShowUdp(bool on) { setShow(Udp, on); }
    bool showDecoLink() const { return m_show[DecoLink]; }
    void setShowDecoLink(bool on) { setShow(DecoLink, on); }
    bool showDecoPort() const { return m_show[DecoPort]; }
    void setShowDecoPort(bool on) { setShow(DecoPort, on); }
    bool hideRoutine() const { return m_hideRoutine; }
    void setHideRoutine(bool on);
    QString textFilter() const { return m_textFilter; }
    void setTextFilter(const QString& text);
    QStringList udpClients() const;
    int decoLinkClients() const;
    bool decoPortListening() const;
    QString decoPortError() const;
    QString lastResult() const { return m_lastResult; }
    bool lastResultOk() const { return m_lastResultOk; }

    Q_INVOKABLE void clear();
    // Le righe si indicano col loro numero di serie: l'indice cambia a ogni
    // riga nuova in cima.
    // La riga per intero: i byte in esadecimale, o il JSON in chiaro.
    Q_INVOKABLE QString detail(qint64 serial) const;
    // Le righe che si vedono, come testo da copiare.
    Q_INVOKABLE QString asText() const;

    // ── Verso Decodium, protocollo UDP ──
    // Client vuoto: l'ultimo che ha scritto.
    // Come il doppio clic sulla riga in Decodium: puo' far partire la chiamata.
    Q_INVOKABLE bool reply(qint64 serial, int modifiers = 0);
    Q_INVOKABLE bool haltTx(const QString& client, bool autoOnly);
    Q_INVOKABLE bool freeText(const QString& client, const QString& text, bool send);
    Q_INVOKABLE bool replay(const QString& client);
    Q_INVOKABLE bool clearWindows(const QString& client, int window);
    Q_INVOKABLE bool location(const QString& client, const QString& grid);
    Q_INVOKABLE bool highlight(const QString& client, const QString& call, const QString& background,
                               const QString& foreground, bool last);

    // ── Verso Decodium, DecoLink ──
    // Una riga JSON a tutti i client DecoLink (deve essere un oggetto con "type").
    Q_INVOKABLE bool sendDecoLink(const QString& json);
    Q_INVOKABLE bool resendDecoLinkList();
    Q_INVOKABLE bool resendDecoLinkAward();

    // Per le prove e le schermate: qualche messaggio di Decodium finto, mandato
    // davvero alla porta UDP (cosi' il programma si vede fra i client), piu'
    // righe DecoLink e un annuncio DecoPort.
    Q_INVOKABLE void demo();

    // Dove si ascoltano gli annunci DecoPort (5560); 0, da nessuna parte: i
    // test non vogliono le radio vere della rete di casa.
    void setDecoPortPort(quint16 port) { m_decoPortPort = port; }

    // Per i test e gli strumenti: una riga come se fosse passata davvero.
    void record(Channel channel, const QString& direction, const QString& peer, const QByteArray& data);

signals:
    void activeChanged();
    void pausedChanged();
    void countChanged();
    void filterChanged();
    void clientsChanged();
    void decoPortChanged();
    void lastResultChanged();

private:
    struct Row {
        quint64 serial{0};
        QString time;
        Channel channel{Udp};
        QString direction;
        QString peer;
        QString type;
        QString client;
        QString summary;
        QByteArray data;
        bool canReply{false};
        bool routine{false};
    };

    void setShow(Channel channel, bool on);
    const Row* find(qint64 serial) const;
    bool matches(const Row& row) const;
    void rebuild();
    // Il client a cui si scrive: vuoto, l'ultimo che si e' sentito.
    QString target(const QString& client) const;
    bool sendUdp(const QString& client, const QByteArray& datagram, const QString& what);
    void setResult(bool ok, const QString& text);
    static QString channelName(Channel channel);

    core::UdpReceiver* m_udp;
    core::DecoLinkServer* m_decoLink;
    core::DecoPortListener* m_decoPort;
    QUdpSocket* m_demoSocket{nullptr};
    quint16 m_decoPortPort;
    QList<Row> m_all;       // dalla piu' vecchia
    QList<Row> m_rows;      // quelle che si vedono, dalla piu' recente
    quint64 m_serial{0};
    bool m_active{false};
    bool m_paused{false};
    bool m_show[3]{true, true, true};
    bool m_hideRoutine{false};
    QString m_textFilter;
    QStringList m_udpClients;
    QString m_lastResult;
    bool m_lastResultOk{true};
};

} // namespace decolog::app
