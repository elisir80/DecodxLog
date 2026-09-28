#include "core/N1mm.h"

#include "core/Bands.h"

#include <QDateTime>
#include <QHash>
#include <QNetworkDatagram>
#include <QTimeZone>
#include <QUdpSocket>
#include <QXmlStreamReader>

namespace decolog::core {

namespace n1mm {

namespace {

// Le frequenze di N1MM sono in decine di hertz: 1402512 = 14025.12 kHz.
QString mhzFrom(const QString& tensOfHz)
{
    bool ok = false;
    const double value = tensOfHz.trimmed().toDouble(&ok);
    if (!ok || value <= 0)
        return {};
    return QString::number(value * 10.0 / 1e6, 'f', 6);
}

void setIf(AdifRecord& r, const char* field, const QString& value)
{
    const QString v = value.trimmed();
    if (!v.isEmpty())
        r.set(QLatin1String(field), v);
}

// Un numero che N1MM manda come 0 quando non c'e'.
void setNumberIf(AdifRecord& r, const char* field, const QString& value)
{
    const QString v = value.trimmed();
    if (!v.isEmpty() && v != QLatin1String("0"))
        r.set(QLatin1String(field), v);
}

} // namespace

Packet parse(const QByteArray& xml)
{
    Packet packet;
    QXmlStreamReader reader(xml);
    QHash<QString, QString> f;
    QString root;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement())
            continue;
        const QString name = reader.name().toString().toLower();
        if (root.isEmpty()) {
            root = name;
            continue;
        }
        // Solo i campi di primo livello: i contatti non ne hanno altri.
        f.insert(name, reader.readElementText(QXmlStreamReader::SkipChildElements));
    }
    if (reader.hasError() && f.isEmpty())
        return packet;

    if (root == QLatin1String("contactinfo"))
        packet.kind = Kind::Contact;
    else if (root == QLatin1String("contactreplace"))
        packet.kind = Kind::Replace;
    else if (root == QLatin1String("contactdelete"))
        packet.kind = Kind::Delete;
    else
        return packet;
    packet.id = f.value(QStringLiteral("id")).trimmed();

    AdifRecord& r = packet.record;
    r.set(QStringLiteral("CALL"), f.value(QStringLiteral("call")).trimmed().toUpper());
    // "2026-09-28 16:43:38", in UTC.
    const QDateTime at = QDateTime::fromString(f.value(QStringLiteral("timestamp")).trimmed(),
                                               QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (at.isValid()) {
        const QDateTime utc(at.date(), at.time(), QTimeZone::UTC);
        r.set(QStringLiteral("QSO_DATE"), utc.toString(QStringLiteral("yyyyMMdd")));
        r.set(QStringLiteral("TIME_ON"), utc.toString(QStringLiteral("HHmmss")));
    }
    // La frequenza di trasmissione e' quella del QSO; se quella di ricezione e'
    // diversa, era in split.
    const QString tx = mhzFrom(f.value(QStringLiteral("txfreq")));
    const QString rx = mhzFrom(f.value(QStringLiteral("rxfreq")));
    const QString freq = !tx.isEmpty() ? tx : rx;
    if (!freq.isEmpty()) {
        r.set(QStringLiteral("FREQ"), freq);
        r.set(QStringLiteral("BAND"), bands::fromMhz(freq.toDouble()));
    }
    if (!rx.isEmpty() && rx != freq)
        r.set(QStringLiteral("FREQ_RX"), rx);

    // I modi: USB/LSB sono SSB con il loro sottomodo; i digitali nuovi MFSK.
    const QString mode = f.value(QStringLiteral("mode")).trimmed().toUpper();
    if (mode == QLatin1String("USB") || mode == QLatin1String("LSB")) {
        r.set(QStringLiteral("MODE"), QStringLiteral("SSB"));
        r.set(QStringLiteral("SUBMODE"), mode);
    } else if (mode == QLatin1String("PSK31") || mode == QLatin1String("PSK63")) {
        r.set(QStringLiteral("MODE"), QStringLiteral("PSK"));
        r.set(QStringLiteral("SUBMODE"), mode);
    } else if (!mode.isEmpty()) {
        r.set(QStringLiteral("MODE"), mode);
    }
    adif::normalizeMode(r);

    setIf(r, "RST_SENT", f.value(QStringLiteral("snt")));
    setIf(r, "RST_RCVD", f.value(QStringLiteral("rcv")));
    setNumberIf(r, "STX", f.value(QStringLiteral("sntnr")));
    setNumberIf(r, "SRX", f.value(QStringLiteral("rcvnr")));
    setIf(r, "SRX_STRING", f.value(QStringLiteral("exchange1")));
    setIf(r, "ARRL_SECT", f.value(QStringLiteral("section")));
    setIf(r, "GRIDSQUARE", f.value(QStringLiteral("gridsquare")));
    setIf(r, "NAME", f.value(QStringLiteral("name")));
    setIf(r, "QTH", f.value(QStringLiteral("qth")));
    setIf(r, "COMMENT", f.value(QStringLiteral("comment")));
    setIf(r, "PRECEDENCE", f.value(QStringLiteral("prec")));
    setNumberIf(r, "CHECK", f.value(QStringLiteral("ck")));
    setIf(r, "OPERATOR", f.value(QStringLiteral("operator")).toUpper());
    setIf(r, "STATION_CALLSIGN", f.value(QStringLiteral("mycall")).toUpper());
    setIf(r, "CONTEST_ID", f.value(QStringLiteral("contestname")));
    // La zona e' quella della gara: ITU nella IARU, CQ nelle altre.
    const QString contest = f.value(QStringLiteral("contestname")).toUpper();
    setNumberIf(r, contest.contains(QLatin1String("IARU")) ? "ITUZ" : "CQZ", f.value(QStringLiteral("zone")));
    setIf(r, "APP_N1MM_ID", packet.id);
    return packet;
}

} // namespace n1mm

N1mmReceiver::N1mmReceiver(QObject* parent)
    : QObject(parent)
{
}

bool N1mmReceiver::start(quint16 port)
{
    stop();
    if (port == 0)
        return false;
    m_socket = new QUdpSocket(this);
    // Altri programmi (N1MM stesso, i suoi add-on) ascoltano spesso la stessa
    // porta: si condivide.
    if (!m_socket->bind(QHostAddress::Any, port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        m_lastError = m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        emit listeningChanged();
        return false;
    }
    m_lastError.clear();
    connect(m_socket, &QUdpSocket::readyRead, this, [this] {
        while (m_socket && m_socket->hasPendingDatagrams())
            handleDatagram(m_socket->receiveDatagram().data());
    });
    emit listeningChanged();
    return true;
}

void N1mmReceiver::stop()
{
    if (!m_socket)
        return;
    delete m_socket;
    m_socket = nullptr;
    emit listeningChanged();
}

bool N1mmReceiver::isListening() const
{
    return m_socket && m_socket->state() == QAbstractSocket::BoundState;
}

void N1mmReceiver::handleDatagram(const QByteArray& data)
{
    const n1mm::Packet p = n1mm::parse(data);
    switch (p.kind) {
    case n1mm::Kind::Contact:
        if (!p.record.value(QStringLiteral("CALL")).isEmpty())
            emit contactReceived(p.record, p.id);
        break;
    case n1mm::Kind::Replace:
        if (!p.record.value(QStringLiteral("CALL")).isEmpty())
            emit contactReplaced(p.record, p.id);
        break;
    case n1mm::Kind::Delete:
        if (!p.id.isEmpty())
            emit contactDeleted(p.id, p.record);
        break;
    case n1mm::Kind::None:
        break;
    }
}

} // namespace decolog::core
