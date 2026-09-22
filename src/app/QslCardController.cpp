#include "app/QslCardController.h"

#include "core/CredentialStore.h"
#include "core/LogDatabase.h"
#include "core/QslCards.h"

#include <QDateTime>

#include <algorithm>
#include <QDir>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QSettings>
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
    loadCard();
    loadMail();

    connect(&m_mail, &core::MailSender::sent, this, [this](qint64 id, const QString& to) {
        ++m_mailSent;
        // Una QSL partita e' una QSL mandata: la coda lo registra, con la via
        // elettronica, cosi' non si rimanda la stessa cartolina due volte.
        touch(id, QStringLiteral("Y"), QString(), QStringLiteral("E"));
        note(tr("QSL sent to %1").arg(to), QStringLiteral("success"));
        emit changed();
        emit mailChanged();
    });
    connect(&m_mail, &core::MailSender::failed, this,
            [this](qint64, const QString& to, const QString& error) {
                ++m_mailFailed;
                note(to.isEmpty() ? error : tr("QSL not sent to %1: %2").arg(to, error),
                     QStringLiteral("error"));
                emit mailChanged();
            });
    connect(&m_mail, &core::MailSender::finished, this, [this] {
        if (m_mailWaiting > 0)
            return;   // altre stanno ancora aspettando l'email dal callbook
        m_mailStatus = m_mailFailed == 0
                           ? tr("%n QSL sent by email", nullptr, m_mailSent)
                           : tr("%1 sent, %2 not").arg(m_mailSent).arg(m_mailFailed);
        emit mailChanged();
    });
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


// -- La cartolina ------------------------------------------------------------

namespace {

// Dove sta scritto com'e' fatta la cartolina: una riga sola di JSON nelle
// impostazioni, che cosi' segue il resto nella copia di sicurezza e nel Cloud.
const char* kCardKey = "cards/card";

} // namespace

void QslCardController::loadCard()
{
    const QString json = QSettings().value(QLatin1String(kCardKey)).toString();
    if (!json.isEmpty())
        m_card = qsldesign::fromJson(QJsonDocument::fromJson(json.toUtf8()).object());
    measureTemplate();
}

void QslCardController::saveCard()
{
    QSettings().setValue(QLatin1String(kCardKey),
                         QString::fromUtf8(QJsonDocument(qsldesign::toJson(m_card))
                                               .toJson(QJsonDocument::Compact)));
    emit cardChanged();
}

void QslCardController::measureTemplate()
{
    m_cardSize = QSize();
    if (m_card.templatePath.isEmpty())
        return;
    // Solo la misura: leggere tutta l'immagine per sapere quanto e' larga
    // sarebbe uno spreco, e la cartolina puo' essere una scansione grossa.
    QImageReader reader(m_card.templatePath);
    if (reader.canRead())
        m_cardSize = reader.size();
}

QVariantList QslCardController::cardFields() const
{
    QVariantList out;
    for (const qsldesign::Field& f : m_card.fields) {
        out << QVariantMap{
            {QStringLiteral("key"), f.key},
            {QStringLiteral("text"), f.text},
            {QStringLiteral("x"), f.x},
            {QStringLiteral("y"), f.y},
            {QStringLiteral("size"), f.size},
            {QStringLiteral("bold"), f.bold},
            {QStringLiteral("color"), f.color},
            {QStringLiteral("align"), f.align},
        };
    }
    return out;
}

QVariantList QslCardController::cardKeys() const
{
    // L'etichetta e' quella che si legge nel menu: tradotta, la chiave no.
    auto label = [](const QString& key) -> QString {
        if (key == QLatin1String("call"))       return tr("Callsign");
        if (key == QLatin1String("date"))       return tr("Date");
        if (key == QLatin1String("day"))        return tr("Day");
        if (key == QLatin1String("month"))      return tr("Month (number)");
        if (key == QLatin1String("monthName"))  return tr("Month (name)");
        if (key == QLatin1String("year"))       return tr("Year");
        if (key == QLatin1String("time"))       return tr("UTC");
        if (key == QLatin1String("freq"))       return tr("MHz");
        if (key == QLatin1String("band"))       return tr("Band");
        if (key == QLatin1String("mode"))       return tr("Mode");
        if (key == QLatin1String("rst"))        return tr("RST");
        if (key == QLatin1String("name"))       return tr("Name");
        if (key == QLatin1String("qth"))        return tr("QTH");
        if (key == QLatin1String("country"))    return tr("Country");
        if (key == QLatin1String("grid"))       return tr("Grid");
        if (key == QLatin1String("via"))        return tr("Via");
        if (key == QLatin1String("myCall"))     return tr("My callsign");
        if (key == QLatin1String("myGrid"))     return tr("My grid");
        if (key == QLatin1String("myName"))     return tr("My name");
        if (key == QLatin1String("myQth"))      return tr("My QTH");
        if (key == QLatin1String("text"))       return tr("Free text");
        return key;
    };
    QVariantList out;
    for (const QString& key : qsldesign::keys())
        out << QVariantMap{{QStringLiteral("key"), key}, {QStringLiteral("label"), label(key)}};
    return out;
}

void QslCardController::setCardTemplate(const QUrl& file)
{
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (path == m_card.templatePath)
        return;
    m_card.templatePath = path;
    measureTemplate();
    if (!path.isEmpty() && !m_cardSize.isValid()) {
        // Un file che non e' un'immagine non diventa una cartolina: meglio
        // dirlo subito che stampare cinquanta fogli bianchi.
        m_status = tr("This file is not an image DecoDXLog can read.");
        note(m_status, QStringLiteral("warning"));
        m_card.templatePath.clear();
        emit changed();
    } else if (!path.isEmpty()) {
        note(tr("QSL card model: %1 (%2 x %3)")
                 .arg(QDir::toNativeSeparators(path))
                 .arg(m_cardSize.width()).arg(m_cardSize.height()),
             QStringLiteral("info"));
    }
    saveCard();
}

void QslCardController::addCardField(const QString& key)
{
    if (!qsldesign::keys().contains(key))
        return;
    qsldesign::Field field;
    field.key = key;
    // In mezzo, che e' dove si vede: poi lo si trascina dove serve.
    field.x = 0.5;
    field.y = 0.5;
    if (key == QLatin1String("text"))
        field.text = tr("text");
    m_card.fields << field;
    saveCard();
}

namespace {

// I riquadri della QSL classica: la fascia "Confirming QSO/SWL to" e la tabella
// DAY MONTH YEAR UTC MHZ MODE RST sotto. Le misure non sono a occhio: sono
// prese dalla cartolina di IU8LMC (1920 x 1080) trovando le righe e le colonne
// della tabella, e poi ridotte a frazione, che vale per qualunque misura.
//
// Su una cartolina fatta in un altro modo finiscono nel posto sbagliato: allora
// si trascinano, che e' il mestiere di questo pannello.
struct Slot {
    const char* key;
    double x;
    double y;
};

const Slot kStandardSlots[] = {
    {"call",  0.8237, 0.4907},   // dopo "Confirming QSO/SWL to:"
    {"day",   0.0721, 0.7065},
    {"month", 0.2156, 0.7065},
    {"year",  0.3471, 0.7065},
    {"time",  0.4708, 0.7065},
    {"freq",  0.6081, 0.7065},
    {"mode",  0.7539, 0.7065},
    {"rst",   0.9128, 0.7065},
};

// Il corpo: sulla cartolina di prova le scritte della tabella sono alte una
// cinquantina di pixel su 1080.
constexpr int kStandardSize = 46;

} // namespace

void QslCardController::addStandardCardFields()
{
    for (const Slot& slot : kStandardSlots) {
        const QString key = QString::fromLatin1(slot.key);
        auto found = std::find_if(m_card.fields.begin(), m_card.fields.end(),
                                  [&key](const qsldesign::Field& f) { return f.key == key; });
        qsldesign::Field& field = found != m_card.fields.end()
                                      ? *found
                                      : (m_card.fields.append(qsldesign::Field{}), m_card.fields.last());
        field.key = key;
        field.x = slot.x;
        field.y = slot.y;
        field.size = kStandardSize;
        field.bold = true;
        // In mezzo al riquadro: e' li' che sta bene un dato dentro una casella.
        field.align = QStringLiteral("center");
    }
    saveCard();
    note(tr("The usual fields are on the card: drag any that do not fall in the right box."),
         QStringLiteral("info"));
}

void QslCardController::moveCardField(int index, double x, double y)
{
    if (index < 0 || index >= m_card.fields.size())
        return;
    m_card.fields[index].x = qBound(0.0, x, 1.0);
    m_card.fields[index].y = qBound(0.0, y, 1.0);
    saveCard();
}

void QslCardController::updateCardField(int index, const QVariantMap& props)
{
    if (index < 0 || index >= m_card.fields.size())
        return;
    qsldesign::Field& f = m_card.fields[index];
    if (props.contains(QStringLiteral("size")))
        f.size = qBound(5, props.value(QStringLiteral("size")).toInt(), 400);
    if (props.contains(QStringLiteral("bold")))
        f.bold = props.value(QStringLiteral("bold")).toBool();
    if (props.contains(QStringLiteral("color")))
        f.color = props.value(QStringLiteral("color")).toString();
    if (props.contains(QStringLiteral("align")))
        f.align = props.value(QStringLiteral("align")).toString();
    if (props.contains(QStringLiteral("text")))
        f.text = props.value(QStringLiteral("text")).toString();
    saveCard();
}

void QslCardController::removeCardField(int index)
{
    if (index < 0 || index >= m_card.fields.size())
        return;
    m_card.fields.removeAt(index);
    saveCard();
}

QVariantMap QslCardController::stationInfo() const
{
    return m_ctx.station ? m_ctx.station() : QVariantMap{};
}

QVariantMap QslCardController::sampleQso() const
{
    if (m_ctx.db && m_ctx.db->isOpen()) {
        const QList<QVariantMap> queued = m_ctx.db->cardRows(QStringLiteral("queue"), 1);
        if (!queued.isEmpty())
            return queued.first();
    }
    // Niente in coda: una cartolina finta, ma con dati che sembrano veri, se no
    // non si capisce dove si stanno mettendo i campi.
    return QVariantMap{
        {QStringLiteral("call"), QStringLiteral("DL9ZZT")},
        {QStringLiteral("date"), QStringLiteral("2026-03-21")},
        {QStringLiteral("time"), QStringLiteral("1432")},
        {QStringLiteral("band"), QStringLiteral("20m")},
        {QStringLiteral("mode"), QStringLiteral("FT8")},
        {QStringLiteral("freq"), QStringLiteral("14.074")},
        {QStringLiteral("rst"), QStringLiteral("599")},
        {QStringLiteral("name"), QStringLiteral("Klaus")},
        {QStringLiteral("country"), QStringLiteral("Germany")},
    };
}

QList<QVariantMap> QslCardController::chosenQsos(const QVariantList& ids) const
{
    QList<QVariantMap> out;
    if (!m_ctx.db || !m_ctx.db->isOpen())
        return out;
    if (ids.isEmpty()) {
        // Senza scelta si stampa la coda: e' quello che si vuole quasi sempre.
        for (const QVariantMap& row : m_ctx.db->cardRows(QStringLiteral("queue")))
            out << row;
        return out;
    }
    const QList<qint64> wanted = toIds(ids);
    for (const QVariantMap& row : m_ctx.db->cardRows(QStringLiteral("all"))) {
        if (wanted.contains(row.value(QStringLiteral("id")).toLongLong()))
            out << row;
    }
    return out;
}

QString QslCardController::writeCardsPdf(const QUrl& file, const QVariantList& ids, int perPage)
{
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    const QList<QVariantMap> qsos = chosenQsos(ids);
    if (qsos.isEmpty()) {
        m_status = tr("No QSO to make a card for.");
        note(m_status, QStringLiteral("warning"));
        emit changed();
        return {};
    }
    QString error;
    if (!qsldesign::writePdf(path, m_card, qsos, stationInfo(), perPage, &error)) {
        m_status = error;
        note(error, QStringLiteral("error"));
        emit changed();
        return {};
    }
    m_lastFile = path;
    m_status = tr("%n card(s) written", nullptr, static_cast<int>(qsos.size()));
    note(tr("QSL cards: %1 (%2)").arg(m_status, QDir::toNativeSeparators(path)), QStringLiteral("success"));
    emit changed();
    return path;
}

QString QslCardController::writeCardsPng(const QUrl& folder, const QVariantList& ids)
{
    const QString dir = folder.isLocalFile() ? folder.toLocalFile() : folder.toString();
    const QList<QVariantMap> qsos = chosenQsos(ids);
    if (qsos.isEmpty()) {
        m_status = tr("No QSO to make a card for.");
        note(m_status, QStringLiteral("warning"));
        emit changed();
        return {};
    }
    QStringList written;
    QString error;
    if (!qsldesign::writePng(dir, m_card, qsos, stationInfo(), &written, &error)) {
        m_status = error;
        note(error, QStringLiteral("error"));
        emit changed();
        return {};
    }
    m_lastFile = written.isEmpty() ? dir : written.last();
    m_status = tr("%n card(s) written", nullptr, static_cast<int>(written.size()));
    note(tr("QSL cards: %1 (%2)").arg(m_status, QDir::toNativeSeparators(dir)), QStringLiteral("success"));
    emit changed();
    return dir;
}

// ── Mandare le cartoline per email ──────────────────────────────────────────

namespace {

// Quello che si scrive nell'email quando non lo si e' scritto nelle
// impostazioni. I segnaposto sono gli stessi che si vedono nel riquadro.
const char* kDefaultSubject = "QSL {MYCALL} -> {CALL}";
const char* kDefaultBody =
    "Dear {NAME},\n"
    "\n"
    "thank you for the QSO on {DATE} at {TIME} UTC, {BAND} {MODE}, RST {RST}.\n"
    "My QSL card is attached. A QSL from you would be very welcome.\n"
    "\n"
    "73 de {MYCALL}\n";

QString fill(const QString& text, const QVariantMap& qso, const QVariantMap& station)
{
    QString out = text;
    auto put = [&out](const char* key, const QString& value) {
        out.replace(QLatin1String(key), value);
    };
    put("{CALL}", qso.value(QStringLiteral("call")).toString());
    put("{NAME}", qso.value(QStringLiteral("name")).toString().isEmpty()
                      ? qso.value(QStringLiteral("call")).toString()
                      : qso.value(QStringLiteral("name")).toString());
    put("{DATE}", qso.value(QStringLiteral("date")).toString());
    const QString time = qso.value(QStringLiteral("time")).toString();
    put("{TIME}", time.size() == 4 ? time.left(2) + QLatin1Char(':') + time.mid(2) : time);
    put("{BAND}", qso.value(QStringLiteral("band")).toString());
    put("{MODE}", qso.value(QStringLiteral("mode")).toString());
    put("{RST}", qso.value(QStringLiteral("rst")).toString());
    put("{MYCALL}", station.value(QStringLiteral("call")).toString());
    put("{MYNAME}", station.value(QStringLiteral("name")).toString());
    return out;
}

} // namespace

void QslCardController::loadMail()
{
    QSettings s;
    core::MailAccount account;
    account.host = s.value(QStringLiteral("mail/host"), QStringLiteral("smtp.gmail.com")).toString();
    account.port = s.value(QStringLiteral("mail/port"), 587).toInt();
    account.fromName = s.value(QStringLiteral("mail/fromName")).toString();
    // L'indirizzo e' quello del portachiavi: e' lo stesso con cui ci si
    // autentica, e tenerne due sarebbe un modo per sbagliarli.
    if (m_ctx.credentials)
        account.user = m_ctx.credentials->account(QStringLiteral("mail"));
    m_mail.setAccount(account);
}

QVariantMap QslCardController::mail() const
{
    const core::MailAccount a = m_mail.account();
    QSettings s;
    return QVariantMap{
        {QStringLiteral("host"), a.host},
        {QStringLiteral("port"), a.port},
        {QStringLiteral("address"), a.user},
        {QStringLiteral("fromName"), a.fromName},
        {QStringLiteral("subject"), s.value(QStringLiteral("mail/subject"),
                                            QLatin1String(kDefaultSubject)).toString()},
        {QStringLiteral("body"), s.value(QStringLiteral("mail/body"),
                                         QLatin1String(kDefaultBody)).toString()},
        {QStringLiteral("route"), mailRoute()},
        {QStringLiteral("mailboxReady"), !a.host.trimmed().isEmpty() && !a.user.trimmed().isEmpty()},
        {QStringLiteral("cloudReady"), m_ctx.cloudAccess && !m_ctx.cloudAccess().second.isEmpty()},
        {QStringLiteral("ready"), mailRoute() == QLatin1String("cloud")
                                     ? (m_ctx.cloudAccess && !m_ctx.cloudAccess().second.isEmpty())
                                     : (!a.host.trimmed().isEmpty() && !a.user.trimmed().isEmpty())},
    };
}

bool QslCardController::mailBusy() const
{
    return m_mailWaiting > 0 || m_mail.busy();
}

void QslCardController::setMail(const QVariantMap& settings)
{
    QSettings s;
    if (settings.contains(QStringLiteral("host")))
        s.setValue(QStringLiteral("mail/host"), settings.value(QStringLiteral("host")).toString().trimmed());
    if (settings.contains(QStringLiteral("port")))
        s.setValue(QStringLiteral("mail/port"), qBound(1, settings.value(QStringLiteral("port")).toInt(), 65535));
    if (settings.contains(QStringLiteral("fromName")))
        s.setValue(QStringLiteral("mail/fromName"), settings.value(QStringLiteral("fromName")).toString());
    if (settings.contains(QStringLiteral("subject")))
        s.setValue(QStringLiteral("mail/subject"), settings.value(QStringLiteral("subject")).toString());
    if (settings.contains(QStringLiteral("body")))
        s.setValue(QStringLiteral("mail/body"), settings.value(QStringLiteral("body")).toString());
    loadMail();
    emit mailChanged();
}

QString QslCardController::mailBodyFor(const QVariantMap& qso) const
{
    return fill(QSettings().value(QStringLiteral("mail/body"), QLatin1String(kDefaultBody)).toString(),
                qso, stationInfo());
}

QByteArray QslCardController::drawCard(const QVariantMap& qso) const
{
    // La cartolina si disegna in memoria: non serve lasciare file in giro per
    // mandarne uno.
    const QImage background(m_card.templatePath);
    const QSize size = background.isNull() ? QSize(1748, 1240) : background.size();
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        core::qsldesign::paint(painter, QRectF(QPointF(0, 0), QSizeF(size)), background,
                               m_card, qso, stationInfo());
    }
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return png;
}

QString QslCardController::mailRoute() const
{
    return QSettings().value(QStringLiteral("mail/route"), QStringLiteral("cloud")).toString();
}

void QslCardController::setMailRoute(const QString& route)
{
    QSettings().setValue(QStringLiteral("mail/route"),
                         route == QLatin1String("mailbox") ? route : QStringLiteral("cloud"));
    emit mailChanged();
}

// Il Cloud fa da ponte: la cartolina va li', e la casella ce l'ha il server.
// Cosi' sul computer dell'operatore non c'e' nessuna password di posta.
void QslCardController::sendThroughCloud(const QVariantMap& qso, const QString& email,
                                         const QByteArray& png)
{
    const auto access = m_ctx.cloudAccess ? m_ctx.cloudAccess() : QPair<QString, QString>{};
    const qint64 id = qso.value(QStringLiteral("id")).toLongLong();
    const QString call = qso.value(QStringLiteral("call")).toString().toUpper();
    if (access.first.isEmpty() || access.second.isEmpty()) {
        ++m_mailFailed;
        note(tr("%1: the Cloud is not linked, so there is nowhere to send from").arg(call),
             QStringLiteral("error"));
        emit mailChanged();
        return;
    }

    if (!m_network)
        m_network = new QNetworkAccessManager(this);

    QUrl url(access.first);
    url.setPath(QStringLiteral("/v1/qsl/mail"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + access.second.toUtf8());

    const QJsonObject body{
        {QStringLiteral("to"), email},
        {QStringLiteral("subject"), fill(QSettings().value(QStringLiteral("mail/subject"),
                                                           QLatin1String(kDefaultSubject)).toString(),
                                         qso, stationInfo())},
        {QStringLiteral("body"), mailBodyFor(qso)},
        {QStringLiteral("replyTo"), m_ctx.replyTo ? m_ctx.replyTo() : QString()},
        {QStringLiteral("attachmentName"),
         QStringLiteral("%1-%2.png").arg(call, qso.value(QStringLiteral("date")).toString()
                                                   .remove(QLatin1Char('-')))},
        {QStringLiteral("attachment"), QString::fromLatin1(png.toBase64())},
    };

    ++m_mailWaiting;
    QNetworkReply* reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, id, call, email] {
        reply->deleteLater();
        // Fermato a meta': i conti sono gia' azzerati, non si tocca piu' niente.
        if (reply->error() == QNetworkReply::OperationCanceledError)
            return;
        --m_mailWaiting;
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
        if (code == 200) {
            ++m_mailSent;
            touch(id, QStringLiteral("Y"), QString(), QStringLiteral("E"));
            note(tr("QSL sent to %1").arg(email), QStringLiteral("success"));
            emit changed();
        } else {
            ++m_mailFailed;
            const QString detail = answer.value(QStringLiteral("detail")).toString();
            note(tr("QSL not sent to %1: %2")
                     .arg(email, detail.isEmpty() ? reply->errorString() : detail),
                 QStringLiteral("error"));
        }
        if (m_mailWaiting == 0) {
            m_mailStatus = m_mailFailed == 0
                               ? tr("%n QSL sent by email", nullptr, m_mailSent)
                               : tr("%1 sent, %2 not").arg(m_mailSent).arg(m_mailFailed);
        }
        emit mailChanged();
    });
}

void QslCardController::queueCard(const QVariantMap& qso, const QString& email)
{
    const QByteArray png = drawCard(qso);
    if (mailRoute() == QLatin1String("cloud")) {
        sendThroughCloud(qso, email, png);
        return;
    }

    const QString call = qso.value(QStringLiteral("call")).toString().toUpper();
    core::MailMessage message;
    message.tag = qso.value(QStringLiteral("id")).toLongLong();
    message.to = email;
    message.subject = fill(QSettings().value(QStringLiteral("mail/subject"),
                                             QLatin1String(kDefaultSubject)).toString(),
                           qso, stationInfo());
    message.body = mailBodyFor(qso);
    message.attachmentName = QStringLiteral("%1-%2.png")
                                 .arg(call, qso.value(QStringLiteral("date")).toString()
                                                .remove(QLatin1Char('-')));
    message.attachment = png;
    m_mail.send(message);
}

void QslCardController::sendCardsByEmail(const QVariantList& ids)
{
    const QList<QVariantMap> qsos = chosenQsos(ids);
    if (qsos.isEmpty()) {
        m_mailStatus = tr("No QSO to send a card to.");
        note(m_mailStatus, QStringLiteral("warning"));
        emit mailChanged();
        return;
    }
    if (!mail().value(QStringLiteral("ready")).toBool()) {
        m_mailStatus = mailRoute() == QLatin1String("cloud")
                           ? tr("Link the Cloud first, or send from your own mailbox: "
                                "Setup -> QSL services.")
                           : tr("Set up the outgoing mailbox first: Setup -> QSL services.");
        note(m_mailStatus, QStringLiteral("warning"));
        emit mailChanged();
        return;
    }
    if (m_card.fields.isEmpty()) {
        m_mailStatus = tr("The card has no fields yet: nothing to send.");
        note(m_mailStatus, QStringLiteral("warning"));
        emit mailChanged();
        return;
    }
    if (!m_ctx.emailFor) {
        m_mailStatus = tr("Sending by email is not available.");
        emit mailChanged();
        return;
    }

    m_mailSent = 0;
    m_mailFailed = 0;
    m_mailWaiting = static_cast<int>(qsos.size());
    m_mailStatus = tr("Looking up %n address(es)…", nullptr, static_cast<int>(qsos.size()));
    emit mailChanged();

    // Dal Cloud la password non serve: ce l'ha il server. Si parte e basta.
    if (mailRoute() == QLatin1String("cloud")) {
        lookupAndSend(qsos);
        return;
    }
    // Dalla propria casella invece serve, e sta nel portachiavi: si chiede una
    // volta sola, e solo adesso che serve davvero.
    if (!m_ctx.credentials) {
        m_mailWaiting = 0;
        m_mailStatus = tr("Sending by email is not available.");
        emit mailChanged();
        return;
    }
    m_ctx.credentials->readSecret(QStringLiteral("mail"), [this, qsos](const QString& secret,
                                                                      const QString& error) {
        if (secret.isEmpty()) {
            m_mailWaiting = 0;
            m_mailStatus = error.isEmpty()
                               ? tr("No password for the outgoing mailbox.")
                               : error;
            note(m_mailStatus, QStringLiteral("error"));
            emit mailChanged();
            return;
        }
        core::MailAccount account = m_mail.account();
        account.password = secret;
        m_mail.setAccount(account);
        lookupAndSend(qsos);
    });
}

void QslCardController::lookupAndSend(const QList<QVariantMap>& qsos)
{
    for (const QVariantMap& qso : qsos) {
        const QString call = qso.value(QStringLiteral("call")).toString();
        m_ctx.emailFor(call, [this, qso, call](const QString& email, const QString& why) {
            --m_mailWaiting;
            if (email.isEmpty()) {
                ++m_mailFailed;
                note(tr("%1: no email in the callbook (%2)").arg(call, why),
                     QStringLiteral("warning"));
                emit mailChanged();
            } else {
                queueCard(qso, email);
            }
            if (m_mailWaiting == 0 && !m_mail.busy()) {
                m_mailStatus = tr("%1 sent, %2 not").arg(m_mailSent).arg(m_mailFailed);
                emit mailChanged();
            }
        });
    }
}

void QslCardController::cancelMail()
{
    // Quelle gia' consegnate al Cloud sono partite e non tornano indietro; le
    // altre si fermano qui, prima di uscire.
    if (m_network) {
        const QList<QNetworkReply*> flying = m_network->findChildren<QNetworkReply*>();
        for (QNetworkReply* reply : flying)
            reply->abort();
    }
    m_mailWaiting = 0;
    m_mail.cancel();
    m_mailStatus = tr("Sending stopped.");
    emit mailChanged();
}

void QslCardController::refresh()
{
    if (m_ctx.logChanged)
        m_ctx.logChanged();
    emit changed();
}

} // namespace decolog::app
