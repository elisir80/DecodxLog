#include "core/WsjtxProtocol.h"

#include <QColor>
#include <QDataStream>
#include <QIODevice>
#include <QStringList>

#include <limits>

namespace decolog::core::wsjtx {

namespace {

// Lo schema stabilisce la versione di QDataStream: cambia la codifica di
// QDateTime, quindi va impostata prima di leggere qualunque campo data.
void applySchema(QDataStream& s, quint32 schema)
{
    if (schema <= 1)
        s.setVersion(QDataStream::Qt_5_0);
    else if (schema == 2)
        s.setVersion(QDataStream::Qt_5_2);
    else
        s.setVersion(QDataStream::Qt_5_4);
}

// "utf8" del protocollo: QByteArray serializzato (lunghezza + byte).
QString readUtf8(QDataStream& s)
{
    QByteArray raw;
    s >> raw;
    return QString::fromUtf8(raw);
}

void writeUtf8(QDataStream& s, const QString& value)
{
    s << value.toUtf8();
}

bool ok(const QDataStream& s) { return s.status() == QDataStream::Ok; }

QDataStream& begin(QDataStream& s, Type type, const QString& clientId, quint32 schema)
{
    applySchema(s, schema);
    s << kMagic << schema << static_cast<quint32>(type);
    writeUtf8(s, clientId);
    return s;
}

} // namespace

std::optional<Message> parse(const QByteArray& datagram)
{
    QDataStream in(datagram);
    in.setByteOrder(QDataStream::BigEndian);

    quint32 magic = 0;
    quint32 schema = 0;
    in >> magic >> schema;
    if (!ok(in) || magic != kMagic || schema == 0)
        return std::nullopt;
    applySchema(in, schema);

    quint32 type = 0;
    in >> type;
    Message msg;
    msg.schema = schema;
    msg.clientId = readUtf8(in);
    if (!ok(in))
        return std::nullopt;

    switch (static_cast<Type>(type)) {
    case Type::Heartbeat: {
        Heartbeat hb;
        in >> hb.maxSchema;
        if (!ok(in)) {
            // Prima dello schema 3 il campo non esisteva.
            msg.payload = Heartbeat{};
            return msg;
        }
        hb.version = readUtf8(in);
        hb.revision = readUtf8(in);
        msg.payload = hb;
        return msg;
    }
    case Type::Status: {
        Status st;
        in >> st.dialFrequencyHz;
        st.mode = readUtf8(in);
        st.dxCall = readUtf8(in);
        st.report = readUtf8(in);
        st.txMode = readUtf8(in);
        in >> st.txEnabled >> st.transmitting >> st.decoding;
        if (!ok(in))
            return std::nullopt;
        quint32 rxDf = 0, txDf = 0;
        in >> rxDf >> txDf;
        st.deCall = readUtf8(in);
        st.deGrid = readUtf8(in);
        st.dxGrid = readUtf8(in);
        bool watchdog = false;
        in >> watchdog;
        st.submode = readUtf8(in);
        bool fast = false;
        quint8 special = 0;
        quint32 tolerance = 0, period = 0;
        in >> fast >> special >> tolerance >> period;
        st.configurationName = readUtf8(in);
        // I campi finali sono opzionali: quel che si e' letto resta buono.
        msg.payload = st;
        return msg;
    }
    case Type::QsoLogged: {
        QsoLogged q;
        in >> q.timeOff;
        q.dxCall = readUtf8(in);
        q.dxGrid = readUtf8(in);
        in >> q.txFrequencyHz;
        q.mode = readUtf8(in);
        q.reportSent = readUtf8(in);
        q.reportReceived = readUtf8(in);
        q.txPower = readUtf8(in);
        q.comments = readUtf8(in);
        q.name = readUtf8(in);
        in >> q.timeOn;
        if (!ok(in) || q.dxCall.isEmpty())
            return std::nullopt;
        q.operatorCall = readUtf8(in);
        q.myCall = readUtf8(in);
        q.myGrid = readUtf8(in);
        q.exchangeSent = readUtf8(in);
        q.exchangeReceived = readUtf8(in);
        q.propagationMode = readUtf8(in);
        msg.payload = q;
        return msg;
    }
    case Type::LoggedAdif: {
        LoggedAdif la;
        in >> la.adif;
        if (!ok(in))
            return std::nullopt;
        msg.payload = la;
        return msg;
    }
    case Type::Close:
        msg.payload = Close{};
        return msg;
    case Type::Decode: {
        Decode d;
        in >> d.isNew >> d.time >> d.snr >> d.deltaTime >> d.deltaFrequency;
        d.mode = readUtf8(in);
        d.message = readUtf8(in);
        if (!ok(in))
            return std::nullopt;
        msg.payload = d;
        return msg;
    }
    default:
        msg.payload = Other{type};
        return msg;
    }
}

QByteArray buildHeartbeat(const QString& clientId, const Heartbeat& hb, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::Heartbeat, clientId, schema);
    s << hb.maxSchema;
    writeUtf8(s, hb.version);
    writeUtf8(s, hb.revision);
    return buffer;
}

QByteArray buildStatus(const QString& clientId, const Status& st, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::Status, clientId, schema);
    s << st.dialFrequencyHz;
    writeUtf8(s, st.mode);
    writeUtf8(s, st.dxCall);
    writeUtf8(s, st.report);
    writeUtf8(s, st.txMode);
    s << st.txEnabled << st.transmitting << st.decoding << quint32{0} << quint32{0};
    writeUtf8(s, st.deCall);
    writeUtf8(s, st.deGrid);
    writeUtf8(s, st.dxGrid);
    s << false;
    writeUtf8(s, st.submode);
    s << false << quint8{0} << quint32{0} << quint32{0};
    writeUtf8(s, st.configurationName);
    writeUtf8(s, QString());
    return buffer;
}

QByteArray buildDecode(const QString& clientId, const Decode& d, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::Decode, clientId, schema);
    s << d.isNew << d.time << d.snr << d.deltaTime << d.deltaFrequency;
    writeUtf8(s, d.mode);
    writeUtf8(s, d.message);
    s << false << false;   // low confidence, off air
    return buffer;
}

QByteArray buildQsoLogged(const QString& clientId, const QsoLogged& q, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::QsoLogged, clientId, schema);
    s << q.timeOff;
    writeUtf8(s, q.dxCall);
    writeUtf8(s, q.dxGrid);
    s << q.txFrequencyHz;
    writeUtf8(s, q.mode);
    writeUtf8(s, q.reportSent);
    writeUtf8(s, q.reportReceived);
    writeUtf8(s, q.txPower);
    writeUtf8(s, q.comments);
    writeUtf8(s, q.name);
    s << q.timeOn;
    writeUtf8(s, q.operatorCall);
    writeUtf8(s, q.myCall);
    writeUtf8(s, q.myGrid);
    writeUtf8(s, q.exchangeSent);
    writeUtf8(s, q.exchangeReceived);
    writeUtf8(s, q.propagationMode);
    return buffer;
}

QByteArray buildLoggedAdif(const QString& clientId, const QByteArray& adif, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::LoggedAdif, clientId, schema);
    s << adif;
    return buffer;
}

// ── Il monitor del traffico ─────────────────────────────────────────────────

QString typeName(quint32 type)
{
    switch (static_cast<Type>(type)) {
    case Type::Heartbeat: return QStringLiteral("Heartbeat");
    case Type::Status: return QStringLiteral("Status");
    case Type::Decode: return QStringLiteral("Decode");
    case Type::Clear: return QStringLiteral("Clear");
    case Type::Reply: return QStringLiteral("Reply");
    case Type::QsoLogged: return QStringLiteral("QSOLogged");
    case Type::Close: return QStringLiteral("Close");
    case Type::Replay: return QStringLiteral("Replay");
    case Type::HaltTx: return QStringLiteral("HaltTx");
    case Type::FreeText: return QStringLiteral("FreeText");
    case Type::WsprDecode: return QStringLiteral("WSPRDecode");
    case Type::Location: return QStringLiteral("Location");
    case Type::LoggedAdif: return QStringLiteral("LoggedADIF");
    case Type::HighlightCallsign: return QStringLiteral("HighlightCallsign");
    case Type::SwitchConfiguration: return QStringLiteral("SwitchConfiguration");
    case Type::Configure: return QStringLiteral("Configure");
    case Type::AnnotationInfo: return QStringLiteral("AnnotationInfo");
    case Type::SetupTx: return QStringLiteral("SetupTx");
    case Type::EnqueueDecode: return QStringLiteral("EnqueueDecode");
    }
    return QStringLiteral("Type %1").arg(type);
}

namespace {

QString mhz(quint64 hz)
{
    return hz > 0 ? QString::number(static_cast<double>(hz) / 1e6, 'f', 6) : QStringLiteral("—");
}

QString decodeLine(const QTime& time, qint32 snr, double dt, quint32 df, const QString& mode, const QString& message)
{
    return QStringLiteral("%1 %2 %3 %4 %5 %6")
        .arg(time.isValid() ? time.toString(QStringLiteral("HHmmss")) : QStringLiteral("------"))
        .arg(snr, 3)
        .arg(dt, 4, 'f', 1)
        .arg(df, 4)
        .arg(mode, message);
}

} // namespace

Description describe(const QByteArray& datagram)
{
    Description d;
    QDataStream in(datagram);
    in.setByteOrder(QDataStream::BigEndian);
    quint32 magic = 0;
    in >> magic >> d.schema;
    if (!ok(in) || magic != kMagic || d.schema == 0) {
        d.typeName = QStringLiteral("?");
        d.summary = QStringLiteral("%1 bytes, not WSJT-X").arg(datagram.size());
        return d;
    }
    applySchema(in, d.schema);
    in >> d.type;
    d.clientId = readUtf8(in);
    if (!ok(in))
        return d;
    d.valid = true;
    d.typeName = typeName(d.type);

    // Quello che parse() sa gia' leggere si prende da li'.
    if (const auto msg = parse(datagram)) {
        if (const auto* hb = std::get_if<Heartbeat>(&msg->payload)) {
            d.summary = QStringLiteral("schema %1 · %2 %3").arg(hb->maxSchema).arg(hb->version, hb->revision).trimmed();
            return d;
        }
        if (const auto* st = std::get_if<Status>(&msg->payload)) {
            QStringList parts{mhz(st->dialFrequencyHz) + QStringLiteral(" MHz"), st->mode};
            if (!st->submode.isEmpty())
                parts << st->submode;
            if (!st->dxCall.isEmpty())
                parts << QStringLiteral("DX %1").arg(st->dxCall) + (st->report.isEmpty() ? QString() : QLatin1Char(' ') + st->report);
            parts << (st->transmitting ? QStringLiteral("TX ON AIR") : st->txEnabled ? QStringLiteral("TX enabled")
                                                                                      : QStringLiteral("RX"));
            if (st->decoding)
                parts << QStringLiteral("decoding");
            if (!st->deCall.isEmpty())
                parts << QStringLiteral("de %1 %2").arg(st->deCall, st->deGrid).trimmed();
            d.summary = parts.join(QStringLiteral(" · "));
            return d;
        }
        if (const auto* dec = std::get_if<Decode>(&msg->payload)) {
            d.summary = decodeLine(dec->time, dec->snr, dec->deltaTime, dec->deltaFrequency, dec->mode, dec->message)
                        + (dec->isNew ? QString() : QStringLiteral(" (replay)"));
            return d;
        }
        if (const auto* q = std::get_if<QsoLogged>(&msg->payload)) {
            d.summary = QStringLiteral("%1 %2 · %3 MHz %4 · %5/%6 · %7")
                            .arg(q->dxCall, q->dxGrid, mhz(q->txFrequencyHz), q->mode, q->reportSent,
                                 q->reportReceived, q->timeOn.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
            return d;
        }
        if (const auto* la = std::get_if<LoggedAdif>(&msg->payload)) {
            const qsizetype at = la->adif.toUpper().indexOf("<CALL:");
            QString call;
            if (at >= 0) {
                const qsizetype gt = la->adif.indexOf('>', at);
                const int len = la->adif.mid(at + 6, gt - at - 6).toInt();
                call = QString::fromUtf8(la->adif.mid(gt + 1, len));
            }
            d.summary = QStringLiteral("%1 bytes of ADIF%2").arg(la->adif.size())
                            .arg(call.isEmpty() ? QString() : QStringLiteral(" · CALL ") + call);
            return d;
        }
        if (std::holds_alternative<Close>(msg->payload)) {
            d.summary = QStringLiteral("the program is closing");
            return d;
        }
    }

    // I messaggi verso il programma.
    switch (static_cast<Type>(d.type)) {
    case Type::Reply: {
        QTime time;
        qint32 snr = 0;
        double dt = 0;
        quint32 df = 0;
        in >> time >> snr >> dt >> df;
        const QString mode = readUtf8(in);
        const QString message = readUtf8(in);
        bool lowConfidence = false;
        quint8 modifiers = 0;
        in >> lowConfidence >> modifiers;
        d.summary = decodeLine(time, snr, dt, df, mode, message)
                    + (modifiers ? QStringLiteral(" · modifiers %1").arg(modifiers) : QString());
        break;
    }
    case Type::HaltTx: {
        bool autoOnly = false;
        in >> autoOnly;
        d.summary = autoOnly ? QStringLiteral("auto TX off") : QStringLiteral("halt TX now");
        break;
    }
    case Type::FreeText: {
        const QString text = readUtf8(in);
        bool send = false;
        in >> send;
        d.summary = QStringLiteral("\"%1\"%2").arg(text, send ? QStringLiteral(" · send") : QString());
        break;
    }
    case Type::Clear: {
        quint8 window = 0;
        in >> window;
        d.summary = !ok(in) ? QStringLiteral("all") : window == 0 ? QStringLiteral("band activity")
                  : window == 1 ? QStringLiteral("RX frequency") : QStringLiteral("both windows");
        break;
    }
    case Type::Replay:
        d.summary = QStringLiteral("send the decodes again");
        break;
    case Type::Location:
        d.summary = readUtf8(in);
        break;
    case Type::HighlightCallsign: {
        const QString call = readUtf8(in);
        QColor bg, fg;
        bool last = false;
        in >> bg >> fg >> last;
        d.summary = call + (bg.isValid() ? QStringLiteral(" · ") + bg.name() : QStringLiteral(" · off"));
        break;
    }
    case Type::SwitchConfiguration:
        d.summary = readUtf8(in);
        break;
    case Type::Configure: {
        const QString mode = readUtf8(in);
        quint32 tolerance = 0;
        in >> tolerance;
        const QString submode = readUtf8(in);
        bool fast = false;
        quint32 period = 0, rxDf = 0;
        in >> fast >> period >> rxDf;
        const QString dxCall = readUtf8(in);
        const QString dxGrid = readUtf8(in);
        QStringList parts;
        if (!mode.isEmpty())
            parts << mode + (submode.isEmpty() ? QString() : QLatin1Char(' ') + submode);
        if (rxDf != std::numeric_limits<quint32>::max())
            parts << QStringLiteral("RX %1 Hz").arg(rxDf);
        if (!dxCall.isEmpty())
            parts << QStringLiteral("DX %1 %2").arg(dxCall, dxGrid).trimmed();
        d.summary = parts.isEmpty() ? QStringLiteral("no change") : parts.join(QStringLiteral(" · "));
        break;
    }
    case Type::AnnotationInfo: {
        const QString call = readUtf8(in);
        bool provided = false;
        quint32 order = 0;
        in >> provided >> order;
        d.summary = provided ? QStringLiteral("%1 · order %2").arg(call).arg(order) : call;
        break;
    }
    case Type::SetupTx: {
        qint32 index = 0;
        in >> index;
        const QString message = readUtf8(in);
        d.summary = QStringLiteral("TX%1 \"%2\"").arg(index).arg(message);
        break;
    }
    case Type::EnqueueDecode: {
        bool isNew = false;
        QTime time;
        qint32 snr = 0;
        double dt = 0;
        quint32 df = 0;
        in >> isNew >> time >> snr >> dt >> df;
        const QString mode = readUtf8(in);
        const QString message = readUtf8(in);
        d.summary = decodeLine(time, snr, dt, df, mode, message);
        break;
    }
    case Type::WsprDecode: {
        bool isNew = false;
        QTime time;
        qint32 snr = 0;
        double dt = 0;
        quint64 freq = 0;
        qint32 drift = 0;
        in >> isNew >> time >> snr >> dt >> freq >> drift;
        const QString call = readUtf8(in);
        const QString grid = readUtf8(in);
        d.summary = QStringLiteral("%1 %2 dB %3 MHz %4 %5")
                        .arg(time.toString(QStringLiteral("HHmm"))).arg(snr).arg(mhz(freq), call, grid);
        break;
    }
    default:
        d.summary = QStringLiteral("%1 bytes").arg(datagram.size());
        break;
    }
    return d;
}

QByteArray buildReply(const QString& clientId, const Decode& dec, quint8 modifiers, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::Reply, clientId, schema);
    s << dec.time << dec.snr << dec.deltaTime << dec.deltaFrequency;
    writeUtf8(s, dec.mode);
    writeUtf8(s, dec.message);
    s << false << modifiers;
    return buffer;
}

QByteArray buildHaltTx(const QString& clientId, bool autoTxOnly, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::HaltTx, clientId, schema);
    s << autoTxOnly;
    return buffer;
}

QByteArray buildFreeText(const QString& clientId, const QString& text, bool send, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::FreeText, clientId, schema);
    writeUtf8(s, text);
    s << send;
    return buffer;
}

QByteArray buildReplay(const QString& clientId, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::Replay, clientId, schema);
    return buffer;
}

QByteArray buildClear(const QString& clientId, quint8 window, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::Clear, clientId, schema);
    s << window;
    return buffer;
}

QByteArray buildLocation(const QString& clientId, const QString& grid, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::Location, clientId, schema);
    writeUtf8(s, grid);
    return buffer;
}

QByteArray buildHighlightCallsign(const QString& clientId, const QString& call, const QString& background,
                                  const QString& foreground, bool highlightLast, quint32 schema)
{
    QByteArray buffer;
    QDataStream s(&buffer, QIODevice::WriteOnly);
    begin(s, Type::HighlightCallsign, clientId, schema);
    writeUtf8(s, call);
    s << QColor(background) << QColor(foreground) << highlightLast;
    return buffer;
}

} // namespace decolog::core::wsjtx
