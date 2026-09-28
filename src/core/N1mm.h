// DecoDXLog — i QSO di N1MM Logger+ (e di chi parla come lui).
//
// N1MM Logger+ manda ogni contatto in UDP, in XML, di solito sulla porta 12060:
// <contactinfo> quando lo registra, <contactreplace> quando lo corregge,
// <contactdelete> quando lo cancella. Ogni contatto ha un ID (un GUID) che
// resta lo stesso nelle tre: con quello si ritrova il QSO da correggere o
// cancellare. Anche Decodium sa mandare i suoi QSO in questo formato.
#pragma once

#include "core/Adif.h"

#include <QByteArray>
#include <QObject>
#include <QString>

class QUdpSocket;

namespace decolog::core {

namespace n1mm {

enum class Kind { None, Contact, Replace, Delete };

struct Packet {
    Kind kind{Kind::None};
    QString id;           // il GUID di N1MM
    AdifRecord record;    // per Contact e Replace
};

// Un pacchetto XML di N1MM, gia' tradotto in ADIF. Kind::None se non e' un
// contatto (N1MM manda anche RadioInfo, spot, punteggi).
Packet parse(const QByteArray& xml);

} // namespace n1mm

class N1mmReceiver : public QObject {
    Q_OBJECT

public:
    explicit N1mmReceiver(QObject* parent = nullptr);

    // Porta 0 = spento. Si ascolta su tutti gli indirizzi: N1MM in rete manda
    // dalle altre postazioni.
    bool start(quint16 port);
    void stop();
    bool isListening() const;
    QString lastError() const { return m_lastError; }

    // Per i test: come se fosse arrivato dalla rete.
    void handleDatagram(const QByteArray& data);

signals:
    void contactReceived(const decolog::core::AdifRecord& record, const QString& id);
    void contactReplaced(const decolog::core::AdifRecord& record, const QString& id);
    void contactDeleted(const QString& id, const decolog::core::AdifRecord& record);
    void listeningChanged();

private:
    QUdpSocket* m_socket{nullptr};
    QString m_lastError;
};

} // namespace decolog::core
