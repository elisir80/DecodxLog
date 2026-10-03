// DecoDXLog — DecoPort visto da fuori, per il monitor del traffico.
//
// DecoPort e' il protocollo con cui Decodium 4 mette in rete la radio
// (Decodium-4.0/doc/DECOPORT_PROTOCOL.md): UDP, intestazione di 28 byte "DPRT",
// la sessione sulla 5559 e l'annuncio del gateway in broadcast sulla 5560 ogni
// due secondi. DecoDXLog non e' un client DecoPort e non vuole diventarlo: qui
// ascolta soltanto gli annunci (chi c'e' in rete, che radio, su che frequenza,
// in che stato) e sa leggere qualunque pacchetto DecoPort gli passi davanti.
// Non manda niente: la sessione e' firmata, e un estraneo non deve entrarci.
#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QString>

class QUdpSocket;

namespace decolog::core {

namespace decoport {

inline constexpr quint32 kMagic = 0x44505254u;   // "DPRT"
inline constexpr int kHeaderBytes = 28;
inline constexpr quint16 kSessionPort = 5559;
inline constexpr quint16 kAnnouncePort = 5560;

struct Description {
    bool valid{false};
    quint8 type{0};
    quint32 streamId{0};
    quint32 sequence{0};
    bool authenticated{false};
    QString typeName;
    QString summary;
};

// Un pacchetto DecoPort in due righe: il tipo e cosa dice (frequenza, modo,
// PTT, radio, stato, strumenti); l'audio solo con la sua misura.
Description describe(const QByteArray& packet);

// Per i test e la prova del monitor: un pacchetto con l'intestazione giusta e
// un contesto con i campi principali (frequenza, modo, radio, stato, porta).
struct Context {
    qint64 rfFrequencyHz{0};
    quint8 mode{0};
    QString rigLabel;
    quint32 stateFlags{0};
    quint16 sessionPort{0};
};
QByteArray encodeContext(const Context& context);
QByteArray buildPacket(quint8 type, quint32 streamId, quint32 sequence, const QByteArray& payload);

} // namespace decoport

class DecoPortListener : public QObject {
    Q_OBJECT

public:
    explicit DecoPortListener(QObject* parent = nullptr);
    ~DecoPortListener() override;

    // In ascolto degli annunci, insieme agli altri programmi che li ascoltano
    // (porta condivisa).
    bool start(quint16 port = decoport::kAnnouncePort);
    void stop();
    bool listening() const;
    QString lastError() const { return m_lastError; }

signals:
    void traffic(const QString& direction, const QString& peer, const QByteArray& packet);

private:
    QUdpSocket* m_socket{nullptr};
    QString m_lastError;
};

} // namespace decolog::core
