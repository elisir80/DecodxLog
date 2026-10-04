#include "app/DecodeFeed.h"

#include "core/DecodeText.h"
#include "core/UdpReceiver.h"

#include <QDateTime>
#include <QFile>
#include <QTimeZone>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariantMap>

#include <algorithm>

namespace decolog::app {

using namespace decolog::core;

// ── Il modello ───────────────────────────────────────────────────────────────

DecodeListModel::DecodeListModel(int limit, QObject* parent)
    : QAbstractListModel(parent)
    , m_limit(limit)
{
}

int DecodeListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant DecodeListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const DecodeRow& r = m_rows.at(index.row());
    switch (role) {
    case SerialRole: return static_cast<qint64>(r.serial);
    case TimeRole: return r.time;
    case SlotRole: return r.slot;
    case SnrRole: return r.snr;
    case DtRole: return r.dt;
    case DfRole: return r.df;
    case ModeRole: return r.mode;
    case Qt::DisplayRole:
    case MessageRole: return r.message;
    case FromRole: return r.from;
    case ToRole: return r.to;
    case GridRole: return r.grid;
    case CqRole: return r.cq;
    case ForMeRole: return r.forMe;
    case WithDxRole: return r.withDx;
    case OwnTxRole: return r.ownTx;
    case LowConfidenceRole: return r.lowConfidence;
    case StatusRole: return r.status;
    case StatusLabelRole: return r.statusLabel;
    case EntityRole: return r.entity;
    case AzimuthRole: return r.azimuth;
    case DistanceRole: return r.distanceKm;
    }
    return {};
}

QHash<int, QByteArray> DecodeListModel::roleNames() const
{
    return {{SerialRole, "serial"},     {TimeRole, "time"},       {SlotRole, "slot"},
            {SnrRole, "snr"},           {DtRole, "dt"},           {DfRole, "df"},
            {ModeRole, "mode"},         {MessageRole, "message"}, {FromRole, "from"},
            {ToRole, "to"},             {GridRole, "grid"},       {CqRole, "cq"},
            {ForMeRole, "forMe"},       {WithDxRole, "withDx"},   {OwnTxRole, "ownTx"},
            {LowConfidenceRole, "lowConfidence"}, {StatusRole, "status"},
            {StatusLabelRole, "statusLabel"},     {EntityRole, "entity"},
            {AzimuthRole, "azimuth"},   {DistanceRole, "distance"}};
}

const DecodeRow* DecodeListModel::find(quint64 serial) const
{
    for (const DecodeRow& r : m_rows) {
        if (r.serial == serial)
            return &r;
    }
    return nullptr;
}

bool DecodeListModel::contains(const QString& time, int df, const QString& message) const
{
    for (const DecodeRow& r : m_rows) {
        if (r.df == df && r.time == time && r.message == message)
            return true;
    }
    return false;
}

void DecodeListModel::prepend(const DecodeRow& row)
{
    beginInsertRows({}, 0, 0);
    m_rows.prepend(row);
    endInsertRows();
    if (m_rows.size() > m_limit) {
        const int first = m_limit;
        const int last = static_cast<int>(m_rows.size()) - 1;
        beginRemoveRows({}, first, last);
        m_rows.resize(m_limit);
        endRemoveRows();
    }
    emit countChanged();
}

void DecodeListModel::replaceAll(const QList<DecodeRow>& rows)
{
    beginResetModel();
    m_rows = rows.size() > m_limit ? rows.mid(0, m_limit) : rows;
    endResetModel();
    emit countChanged();
}

void DecodeListModel::restatus(const std::function<void(DecodeRow&)>& fill)
{
    if (m_rows.isEmpty())
        return;
    for (DecodeRow& r : m_rows)
        fill(r);
    emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1));
}

void DecodeListModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}

// ── Il flusso ────────────────────────────────────────────────────────────────

DecodeFeed::DecodeFeed(UdpReceiver* udp, Context context, QObject* parent)
    : QObject(parent)
    , m_udp(udp)
    , m_ctx(std::move(context))
    , m_full(kFullLimit)
    , m_signal(kSignalLimit)
{
    if (m_udp) {
        connect(m_udp, &UdpReceiver::decodeReceived, this,
                [this](const QString& id, const wsjtx::Decode& d) { handleDecode(id, d); });
        connect(m_udp, &UdpReceiver::statusReceived, this,
                [this](const QString& id, const wsjtx::Status& st) { handleStatus(id, st); });
        connect(m_udp, &UdpReceiver::clientSeen, this, [this](const UdpClientInfo& c) {
            m_lastHeard = QDateTime::currentDateTimeUtc();
            if (m_program != c.id) {
                m_program = c.id;
                m_lastClient = c.id;
            }
            refreshOnline();
        });
        connect(m_udp, &UdpReceiver::clientClosed, this, [this](const QString&) {
            m_lastHeard = {};
            refreshOnline();
        });
    }
    m_onlineTimer.setInterval(5000);
    connect(&m_onlineTimer, &QTimer::timeout, this, &DecodeFeed::refreshOnline);
    m_onlineTimer.start();
}

void DecodeFeed::refreshOnline()
{
    const bool on = m_lastHeard.isValid()
                    && m_lastHeard.secsTo(QDateTime::currentDateTimeUtc()) <= kOnlineSeconds;
    if (on == m_online)
        return;
    m_online = on;
    emit stateChanged();
}

void DecodeFeed::fill(DecodeRow& row) const
{
    const decodetext::Parts parts = decodetext::parse(row.message);
    row.from = parts.from;
    row.to = parts.to;
    row.grid = parts.grid;
    row.cq = parts.cq;

    const QStringList mine = m_ctx.myCalls ? m_ctx.myCalls() : QStringList();
    row.forMe = false;
    bool senderIsMe = false;
    for (const QString& call : mine) {
        if (call.isEmpty())
            continue;
        if (decodetext::mentions(row.message, call))
            row.forMe = true;
        if (!row.from.isEmpty() && decodetext::baseCall(row.from) == decodetext::baseCall(call))
            senderIsMe = true;
    }
    // Una riga nostra non e' "per noi": e' la nostra.
    if (row.ownTx || senderIsMe)
        row.forMe = false;
    row.withDx = !row.ownTx && !m_status.dxCall.isEmpty() && decodetext::mentions(row.message, m_status.dxCall);

    row.status = 0;
    row.statusLabel.clear();
    row.entity.clear();
    row.azimuth = -1;
    row.distanceKm = -1;
    if (row.ownTx || senderIsMe || row.from.isEmpty() || !m_ctx.classify)
        return;
    const Classification c = m_ctx.classify(row.from, m_band, mode());
    row.status = c.status;
    row.entity = c.entity;
    row.azimuth = c.azimuth;
    row.distanceKm = c.distanceKm >= 0 ? qRound(c.distanceKm) : -1;
    row.statusLabel = m_ctx.statusLabel ? m_ctx.statusLabel(c.status) : QString();
}

bool DecodeFeed::belongsToSignal(const DecodeRow& row) const
{
    // Come in Decodium: le trasmissioni proprie, quello che ci nomina, quello
    // che nomina il corrispondente. Niente fascia di frequenza attorno
    // all'ascolto: il traffico accanto resta nell'altra lista.
    return row.ownTx || row.forMe || row.withDx;
}

void DecodeFeed::handleDecode(const QString& clientId, const wsjtx::Decode& d, quint32 schema)
{
    m_lastHeard = QDateTime::currentDateTimeUtc();
    m_lastClient = clientId;
    m_program = clientId;
    if (!m_online) {
        m_online = true;
        emit stateChanged();
    }

    DecodeRow row;
    row.clientId = clientId;
    row.schema = schema;
    row.time = d.time.isValid() ? d.time.toString(QStringLiteral("HHmmss")) : QString();
    row.snr = d.snr;
    row.dt = d.deltaTime;
    row.df = static_cast<int>(d.deltaFrequency);
    row.mode = d.mode;
    row.message = d.message.trimmed();
    row.lowConfidence = d.lowConfidence;
    row.decode = d;
    if (row.message.isEmpty())
        return;
    // Una ripetizione (Replay) di quello che c'e' gia' non si rimette.
    if (!d.isNew && m_full.contains(row.time, row.df, row.message))
        return;
    if (row.time != m_lastSlotTime) {
        m_lastSlotTime = row.time;
        ++m_slot;
    }
    row.slot = m_slot;
    row.serial = ++m_serial;
    fill(row);
    m_full.prepend(row);
    if (belongsToSignal(row))
        m_signal.prepend(row);
}

void DecodeFeed::handleStatus(const QString& clientId, const wsjtx::Status& st)
{
    m_lastHeard = QDateTime::currentDateTimeUtc();
    m_lastClient = clientId;
    m_program = clientId;

    const wsjtx::Status before = m_status;
    const QString oldBand = m_band;
    m_status = st;
    m_band = m_ctx.bandOf ? m_ctx.bandOf(st) : QString();

    bool changed = !m_online;
    m_online = true;
    changed = changed || before.dialFrequencyHz != st.dialFrequencyHz || before.mode != st.mode
              || before.submode != st.submode || before.dxCall != st.dxCall || before.deCall != st.deCall
              || before.rxDf != st.rxDf || before.txDf != st.txDf || before.txEnabled != st.txEnabled
              || before.transmitting != st.transmitting || before.decoding != st.decoding
              || before.txMessage != st.txMessage || before.trPeriod != st.trPeriod || oldBand != m_band;

    // Comincia una trasmissione: e' una riga di Signal RX, come in Decodium.
    if (st.transmitting && !before.transmitting && !st.txMessage.trimmed().isEmpty()) {
        DecodeRow row;
        row.clientId = clientId;
        row.ownTx = true;
        row.time = QDateTime::currentDateTimeUtc().toString(QStringLiteral("HHmmss"));
        row.slot = m_slot;
        row.df = static_cast<int>(st.txDf);
        row.mode = QStringLiteral("~");
        row.message = st.txMessage.trimmed();
        row.serial = ++m_serial;
        fill(row);
        m_signal.prepend(row);
    }

    // La prima volta che si sa dov'e' la radio: le decodifiche che Decodium ha
    // gia' fatto entrano nel pannello, invece di aspettare il periodo dopo.
    if (!m_historyTried && st.dialFrequencyHz > 0) {
        m_historyTried = true;
        loadHistory();
    }

    // Un corrispondente nuovo: le sue righe di prima entrano in Signal RX, come
    // quando in Decodium si fa doppio clic su un CQ vecchio.
    if (before.dxCall != st.dxCall) {
        m_full.restatus([this](DecodeRow& r) {
            r.withDx = !r.ownTx && !m_status.dxCall.isEmpty() && decodetext::mentions(r.message, m_status.dxCall);
        });
        if (!st.dxCall.isEmpty())
            rebuildSignal();
    }
    // La banda o il modo sono cambiati: gli stati calcolati sulla banda di
    // prima non valgono piu'.
    if (!oldBand.isEmpty() && oldBand != m_band)
        restatus();

    if (changed)
        emit stateChanged();
}

void DecodeFeed::rebuildSignal()
{
    QList<DecodeRow> merged;
    for (const DecodeRow& r : m_signal.rows()) {
        if (r.ownTx)
            merged.append(r);
    }
    for (const DecodeRow& r : m_full.rows()) {
        if (belongsToSignal(r))
            merged.append(r);
    }
    std::stable_sort(merged.begin(), merged.end(),
                     [](const DecodeRow& a, const DecodeRow& b) { return a.serial > b.serial; });
    m_signal.replaceAll(merged);
}

void DecodeFeed::demo()
{
    const QString id = QStringLiteral("Decodium");
    wsjtx::Status st;
    st.dialFrequencyHz = 14074000;
    st.mode = QStringLiteral("FT8");
    st.txMode = QStringLiteral("FT8");
    st.deCall = QStringLiteral("IU8LMC");
    st.deGrid = QStringLiteral("JN70");
    st.dxCall = QStringLiteral("9A3XY");
    st.dxGrid = QStringLiteral("JN75");
    st.rxDf = 1503;
    st.txDf = 1503;
    st.trPeriod = 15;
    st.decoding = true;
    st.txEnabled = true;
    handleStatus(id, st);

    struct Line {
        int slot;          // periodi fa
        int snr;
        double dt;
        int df;
        const char* text;
    };
    static const Line lines[] = {
        {4, -18, 0.1, 612, "CQ 3Y0J JD15"},
        {4, -9, 0.3, 1034, "CQ DX JA1ZZZ PM95"},
        {4, -4, -0.1, 1503, "CQ 9A3XY JN75"},
        {4, -12, 0.2, 1840, "EA8XX K1ABC RR73"},
        {3, -15, 0.2, 2210, "CQ POTA W1AW FN31"},
        {3, -6, 0.1, 1503, "IU8LMC 9A3XY JN75"},
        {3, -20, 0.4, 905, "CQ DL1ABC JO31"},
        {2, -11, 0.1, 1503, "IU8LMC 9A3XY R-11"},
        {2, -3, 0.0, 1190, "VK9XX JA1ZZZ -08"},
        {2, -17, 0.3, 2480, "CQ 4U1UN FN30"},
        {1, -9, 0.1, 1503, "IU8LMC 9A3XY 73"},
        {1, -5, 0.2, 760, "CQ IK0TEST JN61"},
    };
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const qint64 grid = (now.toSecsSinceEpoch() / 15) * 15;
    for (const Line& l : lines) {
        wsjtx::Decode d;
        d.time = QDateTime::fromSecsSinceEpoch(grid - 15 * l.slot, QTimeZone::UTC).time();
        d.snr = l.snr;
        d.deltaTime = l.dt;
        d.deltaFrequency = static_cast<quint32>(l.df);
        d.mode = QStringLiteral("~");
        d.message = QString::fromLatin1(l.text);
        handleDecode(id, d);
        // La nostra risposta, fra una riga del corrispondente e l'altra.
        if (l.slot == 3 && l.df == 1503) {
            wsjtx::Status tx = m_status;
            tx.transmitting = true;
            tx.txMessage = QStringLiteral("9A3XY IU8LMC -07");
            handleStatus(id, tx);
            tx.transmitting = false;
            handleStatus(id, tx);
        }
    }
    wsjtx::Status now2 = m_status;
    now2.transmitting = true;
    now2.txMessage = QStringLiteral("9A3XY IU8LMC RR73");
    handleStatus(id, now2);
    now2.transmitting = false;
    handleStatus(id, now2);
}

void DecodeFeed::restatus()
{
    const auto refill = [this](DecodeRow& r) { fill(r); };
    m_full.restatus(refill);
    m_signal.restatus(refill);
}

const DecodeRow* DecodeFeed::rowOf(int which, qint64 serial) const
{
    return (which == 1 ? m_signal : m_full).find(static_cast<quint64>(serial));
}

QString DecodeFeed::lineText(int which, qint64 serial) const
{
    const DecodeRow* r = rowOf(which, serial);
    if (!r)
        return {};
    return QStringLiteral("%1 %2 %3 %4 %5 %6")
        .arg(r->time, 6)
        .arg(r->snr, 3)
        .arg(r->dt, 4, 'f', 1)
        .arg(r->df, 4)
        .arg(r->mode, r->message);
}

int DecodeFeed::loadHistory()
{
    const QString path = m_ctx.historyPath ? m_ctx.historyPath() : QString();
    if (path.isEmpty() || !QFile::exists(path) || m_status.dialFrequencyHz == 0)
        return 0;

    struct Stored {
        qint64 when;
        int snr;
        double dt;
        int df;
        QString message;
    };
    QList<Stored> stored;
    const QString connection = QStringLiteral("decodium-history-") + QUuid::createUuid().toString(QUuid::Id128);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        // Il file e' di Decodium, aperto e in scrittura: si legge soltanto, e
        // se in quel momento e' occupato si rinuncia in fretta.
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=250"));
        if (db.open()) {
            QSqlQuery q(db);
            // La stessa banda e lo stesso modo di adesso: Decodium salva la
            // frequenza assoluta, la distanza dall'ascolto si ricava dal quadrante.
            q.prepare(QStringLiteral(
                "SELECT ts_utc, snr_db, dt_s, freq_hz, message FROM decodes "
                "WHERE ts_utc >= ? AND mode = ? COLLATE NOCASE AND freq_hz BETWEEN ? AND ? "
                "ORDER BY id DESC LIMIT ?"));
            const qint64 dial = static_cast<qint64>(m_status.dialFrequencyHz);
            q.addBindValue(QDateTime::currentMSecsSinceEpoch() - qint64(kHistoryMinutes) * 60 * 1000);
            q.addBindValue(m_status.mode);
            q.addBindValue(dial);
            q.addBindValue(dial + 4000);
            q.addBindValue(kFullLimit);
            if (q.exec()) {
                while (q.next()) {
                    stored.append(Stored{q.value(0).toLongLong(), q.value(1).toInt(), q.value(2).toDouble(),
                                         static_cast<int>(q.value(3).toLongLong() - dial), q.value(4).toString()});
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);

    // Dalla piu' vecchia: ogni riga si mette in testa, e la piu' recente resta sopra.
    int added = 0;
    for (auto it = stored.crbegin(); it != stored.crend(); ++it) {
        QString message = it->message.trimmed();
        // Decodium marca con «?» le decodifiche poco sicure: il segno non fa parte del messaggio.
        const bool low = message.endsWith(QLatin1Char('?'));
        if (low)
            message = message.chopped(1).trimmed();
        if (message.isEmpty())
            continue;
        wsjtx::Decode d;
        d.isNew = false;      // e' storia: niente doppioni con quello che arriva in diretta
        d.time = QDateTime::fromMSecsSinceEpoch(it->when, QTimeZone::UTC).time();
        d.snr = it->snr;
        d.deltaTime = it->dt;
        d.deltaFrequency = static_cast<quint32>(std::max(0, it->df));
        d.mode = QStringLiteral("~");
        d.message = message;
        d.lowConfidence = low;
        const int before = m_full.count();
        handleDecode(m_lastClient.isEmpty() ? QStringLiteral("Decodium") : m_lastClient, d);
        if (m_full.count() > before || m_full.count() == kFullLimit)
            ++added;
    }
    return added;
}

bool DecodeFeed::replay()
{
    if (!m_udp)
        return false;
    const QString id = m_lastClient;
    return !id.isEmpty() && m_udp->sendToClient(id, wsjtx::buildReplay(id));
}

bool DecodeFeed::reply(int which, qint64 serial)
{
    const DecodeRow* r = rowOf(which, serial);
    if (!r || r->ownTx || !m_udp)
        return false;
    return m_udp->sendToClient(r->clientId, wsjtx::buildReply(r->clientId, r->decode));
}

void DecodeFeed::pick(int which, qint64 serial)
{
    const DecodeRow* r = rowOf(which, serial);
    if (!r || r->from.isEmpty())
        return;
    if (m_ctx.lookup)
        m_ctx.lookup(r->from);
    if (m_ctx.prepareQso) {
        m_ctx.prepareQso(QVariantMap{
            {QStringLiteral("call"), r->from},
            {QStringLiteral("mhz"), dialMhz()},
            {QStringLiteral("mode"), mode()},
            {QStringLiteral("grid"), r->grid},
        });
    }
}

} // namespace decolog::app
