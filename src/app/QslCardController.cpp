#include "app/QslCardController.h"

#include "core/LogDatabase.h"
#include "core/QslCards.h"

#include <QDateTime>
#include <QUrl>

namespace decolog::app {

using namespace decolog::core;

namespace {

QString today()
{
    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd"));
}

QList<qint64> toIds(const QVariantList& list)
{
    QList<qint64> ids;
    ids.reserve(list.size());
    for (const QVariant& v : list) {
        const qint64 id = v.toLongLong();
        if (id > 0)
            ids << id;
    }
    return ids;
}

} // namespace

QslCardController::QslCardController(Context context, QObject* parent)
    : QObject(parent)
    , m_ctx(std::move(context))
{
}

QVariantList QslCardController::rows(const QString& state, int limit) const
{
    QVariantList out;
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return out;
    for (const QVariantMap& row : m_ctx.db->cardRows(state, limit))
        out << row;
    return out;
}

QVariantMap QslCardController::counts() const
{
    QVariantMap out{{QStringLiteral("queue"), 0}, {QStringLiteral("sent"), 0},
                    {QStringLiteral("received"), 0}, {QStringLiteral("unanswered"), 0}};
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return out;
    out[QStringLiteral("queue")] = static_cast<int>(m_ctx.db->cardRows(QStringLiteral("queue")).size());
    out[QStringLiteral("sent")] = static_cast<int>(m_ctx.db->cardRows(QStringLiteral("sent")).size());
    const QList<QVariantMap> received = m_ctx.db->cardRows(QStringLiteral("received"));
    out[QStringLiteral("received")] = static_cast<int>(received.size());
    int unanswered = 0;
    for (const QVariantMap& row : received) {
        if (row.value(QStringLiteral("sent")).toString() != QLatin1String("Y"))
            ++unanswered;
    }
    out[QStringLiteral("unanswered")] = unanswered;
    return out;
}

QVariantList QslCardController::sheets() const
{
    QVariantList out;
    for (const qslcard::Sheet& s : qslcard::sheets()) {
        out << QVariantMap{{QStringLiteral("id"), s.id},
                           {QStringLiteral("label"), s.label},
                           {QStringLiteral("perPage"), s.columns * s.rows}};
    }
    return out;
}

void QslCardController::note(const QString& text, const QString& level)
{
    if (m_ctx.activity)
        m_ctx.activity(QStringLiteral("QSL"), text, level);
}

void QslCardController::touch(qint64 id, const QString& sent, const QString& rcvd, const QString& via)
{
    if (!m_ctx.db)
        return;
    QslState card;
    card.service = QStringLiteral("card");
    // Quello che c'e' gia' resta: una data di invio non si cancella perche' e'
    // arrivata la risposta.
    for (const QslState& existing : m_ctx.db->qslStatus(id)) {
        if (existing.service == QLatin1String("card")) {
            card = existing;
            break;
        }
    }
    if (!sent.isEmpty()) {
        if (sent == QLatin1String("Y") && card.sent != QLatin1String("Y"))
            card.sentDate = today();
        card.sent = sent;
    }
    if (!rcvd.isEmpty()) {
        if (rcvd == QLatin1String("Y") && card.rcvd != QLatin1String("Y"))
            card.rcvdDate = today();
        else if (rcvd == QLatin1String("N"))
            card.rcvdDate.clear();
        card.rcvd = rcvd;
    }
    if (!via.isEmpty())
        card.via = via;
    m_ctx.db->setCardState(id, card);
}

void QslCardController::enqueue(const QVariantList& ids, const QString& via)
{
    const QList<qint64> list = toIds(ids);
    for (qint64 id : list)
        touch(id, QStringLiteral("Q"), QString(), via);
    if (!list.isEmpty()) {
        m_status = tr("%n QSO in the paper queue", nullptr, static_cast<int>(list.size()));
        note(m_status, QStringLiteral("info"));
    }
    refresh();
}

int QslCardController::enqueueUnanswered(const QString& via)
{
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return 0;
    int count = 0;
    for (const QVariantMap& row : m_ctx.db->cardRows(QStringLiteral("received"))) {
        if (row.value(QStringLiteral("sent")).toString() == QLatin1String("Y"))
            continue;
        touch(row.value(QStringLiteral("id")).toLongLong(), QStringLiteral("Q"), QString(), via);
        ++count;
    }
    m_status = count > 0 ? tr("%n QSL to answer put in the queue", nullptr, count)
                         : tr("no QSL waiting for an answer");
    note(m_status, QStringLiteral("info"));
    refresh();
    return count;
}

void QslCardController::markSent(const QVariantList& ids, const QString& via)
{
    const QList<qint64> list = toIds(ids);
    for (qint64 id : list)
        touch(id, QStringLiteral("Y"), QString(), via);
    if (!list.isEmpty()) {
        m_status = tr("%n QSL marked as sent", nullptr, static_cast<int>(list.size()));
        note(m_status, QStringLiteral("success"));
    }
    refresh();
}

void QslCardController::markReceived(qint64 id, bool received)
{
    touch(id, QString(), received ? QStringLiteral("Y") : QStringLiteral("N"), QString());
    refresh();
}

void QslCardController::drop(const QVariantList& ids)
{
    const QList<qint64> list = toIds(ids);
    for (qint64 id : list)
        touch(id, QStringLiteral("N"), QString(), QString());
    if (!list.isEmpty())
        m_status = tr("%n QSO taken out of the queue", nullptr, static_cast<int>(list.size()));
    refresh();
}

QString QslCardController::writeLabels(const QUrl& file, const QString& sheetId, int perLabel, bool guides)
{
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return {};
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    const QList<QVariantMap> queued = m_ctx.db->cardRows(QStringLiteral("queue"));
    const QList<qslcard::Label> labels = qslcard::group(queued, perLabel);
    const QVariantMap station = m_ctx.station ? m_ctx.station() : QVariantMap{};

    QString error;
    if (!qslcard::writePdf(path, labels, qslcard::sheetById(sheetId), station, guides, &error)) {
        m_status = error;
        note(error, QStringLiteral("error"));
        emit changed();
        return {};
    }
    m_lastFile = path;
    m_status = tr("%n label(s) written", nullptr, static_cast<int>(labels.size()));
    note(tr("QSL labels: %1 (%2)").arg(m_status, path), QStringLiteral("success"));
    emit changed();
    return path;
}

void QslCardController::refresh()
{
    if (m_ctx.logChanged)
        m_ctx.logChanged();
    emit changed();
}

} // namespace decolog::app
