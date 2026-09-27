#include "app/QsoTableModel.h"
#include "../StartupTrace.h"

#include "core/Awards.h"
#include "core/Bands.h"
#include "core/Dates.h"
#include "core/LogDatabase.h"

#include <QCoreApplication>
#include <QDateTime>

#include <algorithm>
#include <QSet>
#include <QSqlQuery>
#include <QVariantMap>

namespace decolog::app {

using core::LogDatabase;

namespace {

// Le chiavi delle colonne di sempre, nell'ordine dell'enum Column.
const QStringList& coreKeys()
{
    static const QStringList keys{
        QStringLiteral("utc"), QStringLiteral("call"), QStringLiteral("band"), QStringLiteral("freq"),
        QStringLiteral("mode"), QStringLiteral("rst_sent"), QStringLiteral("rst_rcvd"),
        QStringLiteral("grid"), QStringLiteral("name"), QStringLiteral("comment"), QStringLiteral("qth"),
        QStringLiteral("country"), QStringLiteral("state"), QStringLiteral("county"), QStringLiteral("cqz"),
        QStringLiteral("ituz"), QStringLiteral("iota"), QStringLiteral("dxcc"), QStringLiteral("qsl"),
        QStringLiteral("source"), QStringLiteral("tags")};
    return keys;
}

// Le altre colonne: il campo ADIF che mostrano e come si legge dal database.
// Quello che ha una colonna sua si legge da li'; il resto sta nei campi ADIF
// in piu' (adif_extra, in JSON); gli stati QSL nella tabella qsl_status.
struct ExtraColumn {
    const char* key;
    const char* field;      // il nome ADIF, come lo mostra Logger32 e gli altri
    const char* title;      // da tradurre
    int width;
    const char* sql;        // vuoto: json_extract(adif_extra, '$.CAMPO')
};

QString qslSql(const char* what, const char* service)
{
    return QStringLiteral("(SELECT IFNULL(%1, '') FROM qsl_status s WHERE s.qso_id = qso.id AND s.service = '%2')")
        .arg(QLatin1String(what), QLatin1String(service));
}

const QList<ExtraColumn>& extraColumns()
{
    static const QList<ExtraColumn> list{
        {"qso_date", "QSO_DATE", QT_TRANSLATE_NOOP("QsoTableModel", "Date"), 92, "substr(qso_datetime_on, 1, 10)"},
        {"time_on", "TIME_ON", QT_TRANSLATE_NOOP("QsoTableModel", "Time on"), 70, "substr(qso_datetime_on, 12, 5)"},
        {"time_off", "TIME_OFF", QT_TRANSLATE_NOOP("QsoTableModel", "Time off"), 70, "CASE WHEN IFNULL(qso_datetime_off, '') <> '' THEN substr(qso_datetime_off, 12, 5) "
         "WHEN json_extract(adif_extra, '$.TIME_OFF') IS NOT NULL "
         "THEN substr(json_extract(adif_extra, '$.TIME_OFF'), 1, 2) || ':' || substr(json_extract(adif_extra, '$.TIME_OFF'), 3, 2) "
         "ELSE '' END"},
        {"pfx", "PFX", QT_TRANSLATE_NOOP("QsoTableModel", "Prefix"), 60, ""},
        {"submode", "SUBMODE", QT_TRANSLATE_NOOP("QsoTableModel", "Submode"), 70, "IFNULL(submode, '')"},
        {"band_rx", "BAND_RX", QT_TRANSLATE_NOOP("QsoTableModel", "Band RX"), 60, "IFNULL(band_rx, '')"},
        {"freq_rx", "FREQ_RX", QT_TRANSLATE_NOOP("QsoTableModel", "Freq RX"), 96,
         "CASE WHEN freq_rx IS NULL THEN '' ELSE printf('%.6f', freq_rx) END"},
        {"cont", "CONT", QT_TRANSLATE_NOOP("QsoTableModel", "Continent"), 60, "IFNULL(cont, '')"},
        {"sota_ref", "SOTA_REF", QT_TRANSLATE_NOOP("QsoTableModel", "SOTA"), 90, "IFNULL(sota_ref, '')"},
        {"pota_ref", "POTA_REF", QT_TRANSLATE_NOOP("QsoTableModel", "POTA"), 80, "IFNULL(pota_ref, '')"},
        {"wwff_ref", "WWFF_REF", QT_TRANSLATE_NOOP("QsoTableModel", "WWFF"), 90, "IFNULL(wwff_ref, '')"},
        {"sig", "SIG", QT_TRANSLATE_NOOP("QsoTableModel", "Program"), 70, "IFNULL(sig, '')"},
        {"sig_info", "SIG_INFO", QT_TRANSLATE_NOOP("QsoTableModel", "Reference"), 90, "IFNULL(sig_info, '')"},
        {"notes", "NOTES", QT_TRANSLATE_NOOP("QsoTableModel", "Notes"), 180, "IFNULL(notes, '')"},
        {"prop_mode", "PROP_MODE", QT_TRANSLATE_NOOP("QsoTableModel", "Propagation"), 80, "IFNULL(prop_mode, '')"},
        {"sat_name", "SAT_NAME", QT_TRANSLATE_NOOP("QsoTableModel", "Satellite"), 80, "IFNULL(sat_name, '')"},
        {"sat_mode", "SAT_MODE", QT_TRANSLATE_NOOP("QsoTableModel", "Sat mode"), 70, "IFNULL(sat_mode, '')"},
        {"tx_pwr", "TX_PWR", QT_TRANSLATE_NOOP("QsoTableModel", "TX power"), 70,
         "CASE WHEN tx_pwr IS NULL THEN '' ELSE CAST(tx_pwr AS TEXT) END"},
        {"rx_pwr", "RX_PWR", QT_TRANSLATE_NOOP("QsoTableModel", "RX power"), 70, ""},
        {"station_callsign", "STATION_CALLSIGN", QT_TRANSLATE_NOOP("QsoTableModel", "Station"), 100,
         "COALESCE(json_extract(adif_extra, '$.STATION_CALLSIGN'), "
         "(SELECT station_callsign FROM station_profile p WHERE p.id = qso.station_profile_id), '')"},
        {"operator", "OPERATOR", QT_TRANSLATE_NOOP("QsoTableModel", "Operator"), 90,
         "COALESCE(json_extract(adif_extra, '$.OPERATOR'), "
         "(SELECT operator FROM station_profile p WHERE p.id = qso.station_profile_id), '')"},
        {"my_gridsquare", "MY_GRIDSQUARE", QT_TRANSLATE_NOOP("QsoTableModel", "My grid"), 74,
         "COALESCE(json_extract(adif_extra, '$.MY_GRIDSQUARE'), "
         "(SELECT my_gridsquare FROM station_profile p WHERE p.id = qso.station_profile_id), '')"},
        {"contest_id", "CONTEST_ID", QT_TRANSLATE_NOOP("QsoTableModel", "Contest"), 110, ""},
        {"stx", "STX", QT_TRANSLATE_NOOP("QsoTableModel", "Nr sent"), 60, ""},
        {"srx", "SRX", QT_TRANSLATE_NOOP("QsoTableModel", "Nr rcvd"), 60, ""},
        {"stx_string", "STX_STRING", QT_TRANSLATE_NOOP("QsoTableModel", "Exch sent"), 80, ""},
        {"srx_string", "SRX_STRING", QT_TRANSLATE_NOOP("QsoTableModel", "Exch rcvd"), 80, ""},
        {"arrl_sect", "ARRL_SECT", QT_TRANSLATE_NOOP("QsoTableModel", "ARRL section"), 70, ""},
        {"ten_ten", "TEN_TEN", QT_TRANSLATE_NOOP("QsoTableModel", "Ten-Ten"), 60, ""},
        {"qsl_via", "QSL_VIA", QT_TRANSLATE_NOOP("QsoTableModel", "QSL via"), 100, ""},
        {"qslmsg", "QSLMSG", QT_TRANSLATE_NOOP("QsoTableModel", "QSL message"), 160, ""},
        {"address", "ADDRESS", QT_TRANSLATE_NOOP("QsoTableModel", "Address"), 180, ""},
        {"email", "EMAIL", QT_TRANSLATE_NOOP("QsoTableModel", "Email"), 160, ""},
        {"distance", "DISTANCE", QT_TRANSLATE_NOOP("QsoTableModel", "Distance"), 70, ""},
        {"age", "AGE", QT_TRANSLATE_NOOP("QsoTableModel", "Age"), 50, ""},
        {"rig", "RIG", QT_TRANSLATE_NOOP("QsoTableModel", "His rig"), 120, ""},
        {"sfi", "SFI", QT_TRANSLATE_NOOP("QsoTableModel", "SFI"), 50, ""},
        {"k_index", "K_INDEX", QT_TRANSLATE_NOOP("QsoTableModel", "K"), 40, ""},
        {"a_index", "A_INDEX", QT_TRANSLATE_NOOP("QsoTableModel", "A"), 40, ""},
        {"qsl_sent", "QSL_SENT", QT_TRANSLATE_NOOP("QsoTableModel", "Card sent"), 60, nullptr},
        {"qsl_rcvd", "QSL_RCVD", QT_TRANSLATE_NOOP("QsoTableModel", "Card rcvd"), 60, nullptr},
        {"qslsdate", "QSLSDATE", QT_TRANSLATE_NOOP("QsoTableModel", "Card sent on"), 90, nullptr},
        {"qslrdate", "QSLRDATE", QT_TRANSLATE_NOOP("QsoTableModel", "Card rcvd on"), 90, nullptr},
        {"lotw_qsl_sent", "LOTW_QSL_SENT", QT_TRANSLATE_NOOP("QsoTableModel", "LoTW sent"), 60, nullptr},
        {"lotw_qsl_rcvd", "LOTW_QSL_RCVD", QT_TRANSLATE_NOOP("QsoTableModel", "LoTW rcvd"), 60, nullptr},
        {"eqsl_qsl_sent", "EQSL_QSL_SENT", QT_TRANSLATE_NOOP("QsoTableModel", "eQSL sent"), 60, nullptr},
        {"eqsl_qsl_rcvd", "EQSL_QSL_RCVD", QT_TRANSLATE_NOOP("QsoTableModel", "eQSL rcvd"), 60, nullptr},
        {"clublog_status", "CLUBLOG_QSO_UPLOAD_STATUS", QT_TRANSLATE_NOOP("QsoTableModel", "Club Log"), 60, nullptr},
        {"qrz_status", "QRZCOM_QSO_UPLOAD_STATUS", QT_TRANSLATE_NOOP("QsoTableModel", "QRZ"), 60, nullptr},
    };
    return list;
}

const ExtraColumn* extraColumn(const QString& key)
{
    for (const auto& c : extraColumns()) {
        if (key == QLatin1String(c.key))
            return &c;
    }
    return nullptr;
}

// L'espressione SQL di una colonna non di sempre. "x:CAMPO" e' un campo ADIF
// qualsiasi, quelli che il catalogo non conosce (i campi APP_ di un altro
// programma, per esempio).
QString extraSql(const QString& key)
{
    auto json = [](const QString& field) {
        QString safe = field;
        safe.remove(QLatin1Char('\''));
        return QStringLiteral("IFNULL(CAST(json_extract(adif_extra, '$.%1') AS TEXT), '')").arg(safe);
    };
    if (key.startsWith(QLatin1String("x:")))
        return json(key.mid(2).toUpper());
    const ExtraColumn* c = extraColumn(key);
    if (!c)
        return QStringLiteral("''");
    if (key == QLatin1String("qsl_sent")) return qslSql("sent", "card");
    if (key == QLatin1String("qsl_rcvd")) return qslSql("rcvd", "card");
    if (key == QLatin1String("qslsdate")) return qslSql("sent_date", "card");
    if (key == QLatin1String("qslrdate")) return qslSql("rcvd_date", "card");
    if (key == QLatin1String("lotw_qsl_sent")) return qslSql("sent", "lotw");
    if (key == QLatin1String("lotw_qsl_rcvd")) return qslSql("rcvd", "lotw");
    if (key == QLatin1String("eqsl_qsl_sent")) return qslSql("sent", "eqsl");
    if (key == QLatin1String("eqsl_qsl_rcvd")) return qslSql("rcvd", "eqsl");
    if (key == QLatin1String("clublog_status")) return qslSql("sent", "clublog");
    if (key == QLatin1String("qrz_status")) return qslSql("sent", "qrz");
    if (key == QLatin1String("pfx"))
        return json(QStringLiteral("PFX"));   // se manca, lo calcola rowFromQuery
    return c->sql && *c->sql ? QString::fromLatin1(c->sql) : json(QLatin1String(c->field));
}

// Una lettera per servizio, nell'ordine delle colonne L Q C E: 'c' confermato,
// 's' inviato o in coda, '-' niente.
QString qslCodes(const QString& summary)
{
    QString codes = QStringLiteral("----");
    static const QStringList order{QStringLiteral("lotw"), QStringLiteral("qrz"),
                                   QStringLiteral("clublog"), QStringLiteral("eqsl")};
    for (const QString& item : summary.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QStringList parts = item.split(QLatin1Char(':'));
        if (parts.size() != 3)
            continue;
        const qsizetype i = order.indexOf(parts.at(0));
        if (i < 0)
            continue;
        if (parts.at(2) == QLatin1String("Y"))
            codes[i] = QLatin1Char('c');
        else if (parts.at(1) != QLatin1String("N"))
            codes[i] = QLatin1Char('s');
    }
    return codes;
}

} // namespace

QsoTableModel::QsoTableModel(LogDatabase* db, QObject* parent)
    : QAbstractTableModel(parent)
    , m_db(db)
    , m_layout(coreKeys())
{
    rebuildSlots();
    reload();
}

int QsoTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int QsoTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_layout.size());
}

QVariant QsoTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row& row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole: {
        const Slot s = m_slots.value(index.column());
        if (s.core >= 0)
            return row.values[s.core];
        return s.extra >= 0 ? row.extra.value(s.extra) : QString();
    }
    case IdRole:
        return row.id;
    case ColumnKeyRole:
        return columnKey(index.column());
    case IsNewRole:
        return row.fresh;
    case ModeRole:
        return row.values[Mode];
    case CategoryRole:
        return m_category.value(row.id);
    default:
        return {};
    }
}

QVariant QsoTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};
    return columnTitle(section);
}

QHash<int, QByteArray> QsoTableModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {IdRole, "qsoId"},
        {ColumnKeyRole, "columnKey"},
        {IsNewRole, "isNew"},
        {ModeRole, "modeName"},
        {CategoryRole, "rowCategory"},
    };
}

QString QsoTableModel::columnKey(int column) const
{
    return m_layout.value(column);
}

namespace {
QString coreTitle(int column)
{
    switch (column) {
    case QsoTableModel::Utc:     return QsoTableModel::tr("UTC");
    case QsoTableModel::Call:    return QsoTableModel::tr("Call");
    case QsoTableModel::Band:    return QsoTableModel::tr("Band");
    case QsoTableModel::Freq:    return QsoTableModel::tr("Freq");
    case QsoTableModel::Mode:    return QsoTableModel::tr("Mode");
    case QsoTableModel::RstSent: return QsoTableModel::tr("S");
    case QsoTableModel::RstRcvd: return QsoTableModel::tr("R");
    case QsoTableModel::Grid:    return QsoTableModel::tr("Grid");
    case QsoTableModel::Name:    return QsoTableModel::tr("Name");
    case QsoTableModel::Qth:     return QsoTableModel::tr("City / QTH");
    case QsoTableModel::Country: return QsoTableModel::tr("Country");
    case QsoTableModel::State:   return QsoTableModel::tr("State");
    case QsoTableModel::County:  return QsoTableModel::tr("County");
    case QsoTableModel::Cqz:     return QsoTableModel::tr("CQ");
    case QsoTableModel::Ituz:    return QsoTableModel::tr("ITU");
    case QsoTableModel::Iota:    return QsoTableModel::tr("IOTA");
    case QsoTableModel::Dxcc:    return QsoTableModel::tr("DXCC");
    case QsoTableModel::Qsl:     return QsoTableModel::tr("QSL");
    case QsoTableModel::Source:  return QsoTableModel::tr("Src");
    case QsoTableModel::Tags:    return QsoTableModel::tr("Tags");
    // Il COMMENT dell'ADIF: quello che gli altri programmi chiamano commento.
    // Le etichette sono un'altra cosa, di DecoDXLog.
    case QsoTableModel::Comment: return QsoTableModel::tr("Comment");
    default:                     return {};
    }
}

int coreWidth(int column)
{
    switch (column) {
    case QsoTableModel::Utc:     return 132;
    case QsoTableModel::Call:    return 118;
    case QsoTableModel::Band:    return 56;
    case QsoTableModel::Freq:    return 96;
    case QsoTableModel::Mode:    return 60;
    case QsoTableModel::RstSent:
    case QsoTableModel::RstRcvd: return 46;
    case QsoTableModel::Grid:    return 74;
    case QsoTableModel::Name:    return 140;
    case QsoTableModel::Comment: return 180;
    case QsoTableModel::Qth:     return 140;
    case QsoTableModel::Country: return 130;
    case QsoTableModel::State:   return 52;
    case QsoTableModel::County:  return 110;
    case QsoTableModel::Cqz:
    case QsoTableModel::Ituz:    return 44;
    case QsoTableModel::Iota:    return 64;
    case QsoTableModel::Dxcc:    return 56;
    case QsoTableModel::Qsl:     return 84;
    case QsoTableModel::Source:  return 52;
    case QsoTableModel::Tags:    return 130;
    default:                     return 80;
    }
}
} // namespace

QString QsoTableModel::titleOf(const QString& key) const
{
    const qsizetype core = coreKeys().indexOf(key);
    if (core >= 0)
        return coreTitle(static_cast<int>(core));
    if (key.startsWith(QLatin1String("x:")))
        return key.mid(2).toUpper();
    if (const ExtraColumn* c = extraColumn(key))
        return QCoreApplication::translate("QsoTableModel", c->title);
    return key;
}

QString QsoTableModel::columnTitle(int column) const
{
    return titleOf(m_layout.value(column));
}

int QsoTableModel::columnWidthHint(int column) const
{
    const QString key = m_layout.value(column);
    const qsizetype core = coreKeys().indexOf(key);
    if (core >= 0)
        return coreWidth(static_cast<int>(core));
    if (const ExtraColumn* c = extraColumn(key))
        return c->width;
    return 100;
}

QStringList QsoTableModel::defaultLayout()
{
    return coreKeys();
}

QVariantList QsoTableModel::availableColumns() const
{
    QVariantList out;
    static const QStringList coreFields{
        QStringLiteral("QSO_DATE+TIME_ON"), QStringLiteral("CALL"), QStringLiteral("BAND"), QStringLiteral("FREQ"),
        QStringLiteral("MODE"), QStringLiteral("RST_SENT"), QStringLiteral("RST_RCVD"), QStringLiteral("GRIDSQUARE"),
        QStringLiteral("NAME"), QStringLiteral("COMMENT"), QStringLiteral("QTH"), QStringLiteral("COUNTRY"),
        QStringLiteral("STATE"), QStringLiteral("CNTY"), QStringLiteral("CQZ"), QStringLiteral("ITUZ"),
        QStringLiteral("IOTA"), QStringLiteral("DXCC"), QStringLiteral("L Q C E"), QString(),
        QStringLiteral("APP_DECOLOG_TAGS")};
    for (qsizetype i = 0; i < coreKeys().size(); ++i) {
        out << QVariantMap{{QStringLiteral("key"), coreKeys().at(i)},
                           {QStringLiteral("title"), coreTitle(static_cast<int>(i))},
                           {QStringLiteral("field"), coreFields.value(i)}};
    }
    for (const auto& c : extraColumns()) {
        out << QVariantMap{{QStringLiteral("key"), QLatin1String(c.key)},
                           {QStringLiteral("title"), QCoreApplication::translate("QsoTableModel", c.title)},
                           {QStringLiteral("field"), QLatin1String(c.field)}};
    }
    // I campi ADIF qualsiasi che l'operatore ha aggiunto.
    for (const QString& key : m_layout) {
        if (key.startsWith(QLatin1String("x:")))
            out << QVariantMap{{QStringLiteral("key"), key},
                               {QStringLiteral("title"), key.mid(2).toUpper()},
                               {QStringLiteral("field"), key.mid(2).toUpper()}};
    }
    return out;
}

void QsoTableModel::setColumnLayout(const QStringList& keys)
{
    QStringList clean;
    for (const QString& raw : keys) {
        const QString key = raw.trimmed();
        const bool known = coreKeys().contains(key) || extraColumn(key)
                           || (key.startsWith(QLatin1String("x:")) && key.size() > 2);
        if (known && !clean.contains(key))
            clean << key;
    }
    // Il nominativo non si toglie: senza, una riga non dice niente.
    if (!clean.contains(QStringLiteral("call")))
        clean.prepend(QStringLiteral("call"));
    if (clean == m_layout)
        return;
    const QStringList oldExtra = m_extra;
    m_layout = clean;
    m_extra.clear();
    for (const QString& key : m_layout) {
        if (!coreKeys().contains(key))
            m_extra << key;
    }
    rebuildSlots();
    if (m_extra != oldExtra) {
        // Colonne nuove da leggere: si rilegge il log.
        reload();
    } else {
        beginResetModel();
        endResetModel();
    }
    emit layoutChanged();
}

void QsoTableModel::rebuildSlots()
{
    m_slots.clear();
    for (const QString& key : m_layout) {
        Slot s;
        s.core = static_cast<int>(coreKeys().indexOf(key));
        if (s.core < 0)
            s.extra = static_cast<int>(m_extra.indexOf(key));
        m_slots << s;
    }
}

void QsoTableModel::sortBy(const QString& key)
{
    if (key.isEmpty() || key == QLatin1String("qsl"))
        return;
    // Stessa colonna: si gira. Colonna nuova: l'ora dal piu' recente, il resto
    // dalla A alla Z (o dal piu' piccolo).
    const bool ascending = key == m_sortKey ? !m_sortAscending : key != QLatin1String("utc");
    setSort(key, ascending);
}

void QsoTableModel::setSort(const QString& key, bool ascending)
{
    const QString clean = key.isEmpty() ? QStringLiteral("utc") : key;
    if (clean == m_sortKey && ascending == m_sortAscending)
        return;
    m_sortKey = clean;
    m_sortAscending = ascending;
    beginResetModel();
    applySort();
    endResetModel();
    emit sortChanged();
}

void QsoTableModel::applySort()
{
    if (defaultSort()) {
        // L'ordine della query: dal piu' recente.
        std::stable_sort(m_rows.begin(), m_rows.end(), [](const Row& a, const Row& b) {
            return a.sortKey != b.sortKey ? a.sortKey > b.sortKey : a.id > b.id;
        });
        return;
    }
    const qsizetype core = coreKeys().indexOf(m_sortKey);
    const qsizetype extra = m_extra.indexOf(m_sortKey);
    const QStringList bandOrder = core::bands::all();
    const bool isUtc = m_sortKey == QLatin1String("utc");
    const bool isBand = m_sortKey == QLatin1String("band");
    const bool isDate = m_sortKey.contains(QLatin1String("date"), Qt::CaseInsensitive);
    auto value = [&](const Row& r) {
        return core >= 0 ? r.values[core] : extra >= 0 ? r.extra.value(extra) : QString();
    };
    // Una chiave che si confronta bene: numeri come numeri, date come date,
    // bande nell'ordine delle frequenze, il resto senza badare alle maiuscole.
    struct Key {
        bool empty{true};
        bool numeric{false};
        double number{0};
        QString text;
    };
    auto keyOf = [&](const Row& r) {
        Key k;
        if (isUtc) {
            k.empty = r.sortKey.isEmpty();
            k.text = r.sortKey;
            return k;
        }
        const QString v = value(r).trimmed();
        k.empty = v.isEmpty();
        if (k.empty)
            return k;
        if (isBand) {
            const qsizetype i = bandOrder.indexOf(v.toLower());
            k.numeric = true;
            k.number = i >= 0 ? static_cast<double>(i) : 1e9;
            return k;
        }
        if (isDate) {
            const QString iso = core::dates::read(v);
            k.text = iso.isEmpty() ? v : iso;
            return k;
        }
        bool ok = false;
        const double d = v.toDouble(&ok);
        if (ok) {
            k.numeric = true;
            k.number = d;
            return k;
        }
        k.text = v.toUpper();
        return k;
    };
    QHash<qint64, Key> keys;
    keys.reserve(m_rows.size());
    for (const Row& r : std::as_const(m_rows))
        keys.insert(r.id, keyOf(r));
    const bool asc = m_sortAscending;
    std::stable_sort(m_rows.begin(), m_rows.end(), [&](const Row& a, const Row& b) {
        const Key& ka = keys[a.id];
        const Key& kb = keys[b.id];
        // I vuoti in fondo, in tutti e due i versi.
        if (ka.empty != kb.empty)
            return !ka.empty;
        if (!ka.empty) {
            int cmp = 0;
            if (ka.numeric && kb.numeric)
                cmp = ka.number < kb.number ? -1 : ka.number > kb.number ? 1 : 0;
            else if (ka.numeric != kb.numeric)
                cmp = ka.numeric ? -1 : 1;
            else
                cmp = QString::compare(ka.text, kb.text);
            if (cmp != 0)
                return asc ? cmp < 0 : cmp > 0;
        }
        // A pari merito, il piu' recente prima.
        return a.sortKey != b.sortKey ? a.sortKey > b.sortKey : a.id > b.id;
    });
}

QString QsoTableModel::valueFor(int row, const QString& key) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    const Row& r = m_rows.at(row);
    const qsizetype core = coreKeys().indexOf(key);
    if (core >= 0)
        return r.values[core];
    const qsizetype extra = m_extra.indexOf(key);
    return extra >= 0 ? r.extra.value(extra) : QString();
}

QStringList QsoTableModel::categoryKeys()
{
    // In ordine di importanza: vince la prima che vale.
    return {QStringLiteral("colorNewDxcc"), QStringLiteral("colorNewDxccBand"),
            QStringLiteral("colorNewContinent"), QStringLiteral("colorNewContinentBand"),
            QStringLiteral("colorNewCqZone"), QStringLiteral("colorNewCqZoneBand"),
            QStringLiteral("colorNewItuZone"), QStringLiteral("colorNewItuZoneBand"),
            QStringLiteral("colorNewGrid"), QStringLiteral("colorNewGridBand"),
            QStringLiteral("colorNewCall"), QStringLiteral("colorNewCallBand"),
            QStringLiteral("colorLotwConfirmed"), QStringLiteral("colorB4")};
}

namespace {

// Una categoria per QSO, in un giro solo del log in ordine di tempo: il primo
// che porta una cosa mai vista (entita', entita' sulla banda, continente…) la
// prende. Su un log di quindicimila QSO sono qualche decina di millisecondi.
QString categoryFrom(const QSqlQuery& q)
{
    const QStringList keys = QsoTableModel::categoryKeys();
    // Le colonne 1..12 sono i "primi"; 13 la conferma LoTW; niente = gia' lavorato.
    for (int i = 0; i < 12; ++i) {
        if (!q.value(1 + i).isNull() && q.value(1 + i).toLongLong() == 1)
            return keys.at(i);
    }
    if (q.value(13).toBool())
        return keys.at(12);
    return keys.at(13);
}

} // namespace

void QsoTableModel::computeCategories()
{
    decolog::StartupSpan trace("QsoTableModel::computeCategories");
    if (!m_db || !m_db->isOpen()) {
        m_category.clear();
        m_categorySignature.clear();
        return;
    }
    // I filtri ricaricano la tabella spesso; le categorie cambiano solo se
    // cambia il log. Si contano di nuovo solo allora.
    QSqlQuery sig(m_db->connection());
    QString signature;
    if (sig.exec(QStringLiteral("SELECT COUNT(*), IFNULL(MAX(updated_at), ''), "
                                "(SELECT COUNT(*) FROM qsl_status WHERE service = 'lotw' AND rcvd = 'Y') "
                                "FROM qso WHERE deleted = 0"))
        && sig.next())
        signature = sig.value(0).toString() + QLatin1Char('|') + sig.value(1).toString() + QLatin1Char('|')
                  + sig.value(2).toString();
    if (!signature.isEmpty() && signature == m_categorySignature)
        return;
    m_categorySignature = signature;
    m_category.clear();

    QSet<qint64> lotw;
    QSqlQuery l(m_db->connection());
    l.setForwardOnly(true);
    if (l.exec(QStringLiteral("SELECT qso_id FROM qsl_status WHERE service = 'lotw' AND rcvd = 'Y'"))) {
        while (l.next())
            lotw.insert(l.value(0).toLongLong());
    }

    QSqlQuery q(m_db->connection());
    q.setForwardOnly(true);
    if (!q.exec(QStringLiteral("SELECT id, IFNULL(dxcc, 0), band, IFNULL(cont, ''), IFNULL(cqz, 0), IFNULL(ituz, 0), "
                               "UPPER(SUBSTR(IFNULL(gridsquare, ''), 1, 4)), call FROM qso WHERE deleted = 0 "
                               "ORDER BY qso_datetime_on, id")))
        return;
    const QStringList keys = categoryKeys();
    QSet<QString> seen[12];
    while (q.next()) {
        const qint64 id = q.value(0).toLongLong();
        const int dxcc = q.value(1).toInt();
        const QString band = q.value(2).toString();
        const QString cont = q.value(3).toString();
        const int cqz = q.value(4).toInt();
        const int ituz = q.value(5).toInt();
        const QString grid = q.value(6).toString();
        const QString call = q.value(7).toString();
        const QString slot = QLatin1Char('|') + band;
        // La chiave vuota vuol dire "non si sa": non conta.
        const QString parts[12] = {
            dxcc > 0 ? QString::number(dxcc) : QString(), dxcc > 0 ? QString::number(dxcc) + slot : QString(),
            cont, cont.isEmpty() ? QString() : cont + slot,
            cqz > 0 ? QString::number(cqz) : QString(), cqz > 0 ? QString::number(cqz) + slot : QString(),
            ituz > 0 ? QString::number(ituz) : QString(), ituz > 0 ? QString::number(ituz) + slot : QString(),
            grid.size() == 4 ? grid : QString(), grid.size() == 4 ? grid + slot : QString(),
            call, call + slot,
        };
        QString category;
        for (int i = 0; i < 12; ++i) {
            if (parts[i].isEmpty() || seen[i].contains(parts[i]))
                continue;
            seen[i].insert(parts[i]);
            if (category.isEmpty())
                category = keys.at(i);
        }
        if (category.isEmpty())
            category = lotw.contains(id) ? keys.at(12) : keys.at(13);
        m_category.insert(id, category);
    }
}

QString QsoTableModel::categoryOf(qint64 id) const
{
    // Un QSO solo: si contano quelli fatti prima con la stessa cosa. Con gli
    // indici su entita', locatore e nominativo e' immediato anche su un log
    // grande — il conto di tutto il log si rifa' solo al ricarico.
    if (!m_db || !m_db->isOpen())
        return {};
    QSqlQuery q(m_db->connection());
    auto before = [](const char* condition) {
        return QStringLiteral("(SELECT CASE WHEN %1 THEN (SELECT COUNT(*) FROM qso o WHERE o.deleted = 0 AND %2 "
                              "AND (o.qso_datetime_on < t.qso_datetime_on OR (o.qso_datetime_on = t.qso_datetime_on "
                              "AND o.id < t.id))) + 1 END)")
            .arg(QString::fromLatin1(condition).section(QLatin1Char('|'), 0, 0),
                 QString::fromLatin1(condition).section(QLatin1Char('|'), 1, 1));
    };
    const QStringList parts{
        before("IFNULL(t.dxcc, 0) > 0|o.dxcc = t.dxcc"),
        before("IFNULL(t.dxcc, 0) > 0|o.dxcc = t.dxcc AND o.band = t.band"),
        before("IFNULL(t.cont, '') <> ''|o.cont = t.cont"),
        before("IFNULL(t.cont, '') <> ''|o.cont = t.cont AND o.band = t.band"),
        before("IFNULL(t.cqz, 0) > 0|o.cqz = t.cqz"),
        before("IFNULL(t.cqz, 0) > 0|o.cqz = t.cqz AND o.band = t.band"),
        before("IFNULL(t.ituz, 0) > 0|o.ituz = t.ituz"),
        before("IFNULL(t.ituz, 0) > 0|o.ituz = t.ituz AND o.band = t.band"),
        before("LENGTH(IFNULL(t.gridsquare, '')) >= 4|UPPER(SUBSTR(o.gridsquare, 1, 4)) = UPPER(SUBSTR(t.gridsquare, 1, 4))"),
        before("LENGTH(IFNULL(t.gridsquare, '')) >= 4|UPPER(SUBSTR(o.gridsquare, 1, 4)) = UPPER(SUBSTR(t.gridsquare, 1, 4)) "
               "AND o.band = t.band"),
        before("1|o.call = t.call"),
        before("1|o.call = t.call AND o.band = t.band"),
    };
    q.prepare(QStringLiteral("SELECT t.id, %1, EXISTS (SELECT 1 FROM qsl_status s WHERE s.qso_id = t.id "
                             "AND s.service = 'lotw' AND s.rcvd = 'Y') FROM qso t WHERE t.id = ?")
                  .arg(parts.join(QStringLiteral(", "))));
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return {};
    return categoryFrom(q);
}

QString QsoTableModel::selectSql(const QString& where) const
{
    QString extras;
    for (const QString& key : m_extra)
        extras += QStringLiteral(", ") + extraSql(key);
    return QStringLiteral(
               "SELECT id, qso_datetime_on, call, band, mode, submode, freq, rst_sent, rst_rcvd, "
               "gridsquare, name, dxcc, source, "
               "(SELECT group_concat(service || ':' || sent || ':' || rcvd) FROM qsl_status s WHERE s.qso_id = qso.id), "
               "IFNULL(tags, ''), "
               "IFNULL(qth, ''), IFNULL(country, ''), IFNULL(state, ''), IFNULL(cnty, ''), "
               "cqz, ituz, IFNULL(iota, ''), IFNULL(comment, '')%2 "
               "FROM qso WHERE deleted = 0 %1 ORDER BY qso_datetime_on DESC, id DESC")
        .arg(where, extras);
}

QsoTableModel::Row QsoTableModel::rowFromQuery(const QSqlQuery& q) const
{
    Row r;
    r.id = q.value(0).toLongLong();
    r.sortKey = q.value(1).toString();
    const QDateTime on = QDateTime::fromString(r.sortKey, Qt::ISODate);
    // Nella forma della lingua: in italiano 25/09/26 12:18.
    r.values[Utc] = on.toUTC().toString(core::dates::shortFormat() + QStringLiteral(" HH:mm"));
    r.values[Call] = q.value(2).toString();
    r.values[Band] = q.value(3).toString();
    const QString submode = q.value(5).toString();
    // FT2 si legge FT2, non MFSK: il modo ADIF resta nel database, qui si mostra
    // quello che l'operatore riconosce.
    r.values[Mode] = submode.isEmpty() || q.value(4).toString() == QLatin1String("SSB") ? q.value(4).toString() : submode;
    r.values[Freq] = q.value(6).isNull() ? QString() : QString::number(q.value(6).toDouble(), 'f', 6);
    r.values[RstSent] = q.value(7).toString();
    r.values[RstRcvd] = q.value(8).toString();
    r.values[Grid] = q.value(9).toString();
    r.values[Name] = q.value(10).toString();
    r.values[Dxcc] = q.value(11).isNull() ? QString() : q.value(11).toString();
    const QString source = q.value(12).toString();
    if (source.startsWith(QLatin1String("udp")))  r.values[Source] = QStringLiteral("udp");
    else if (source == QLatin1String("manual"))   r.values[Source] = QStringLiteral("man");
    else if (source == QLatin1String("import"))   r.values[Source] = QStringLiteral("imp");
    else if (source == QLatin1String("cloud"))    r.values[Source] = QStringLiteral("cld");
    else r.values[Source] = source.left(3);
    r.values[Qsl] = qslCodes(q.value(13).toString());
    r.values[Tags] = q.value(14).toString().replace(QLatin1Char(','), QStringLiteral(", "));
    r.values[Qth] = q.value(15).toString();
    r.values[Country] = q.value(16).toString();
    r.values[State] = q.value(17).toString();
    r.values[County] = q.value(18).toString();
    r.values[Cqz] = q.value(19).isNull() || q.value(19).toInt() <= 0 ? QString() : q.value(19).toString();
    r.values[Ituz] = q.value(20).isNull() || q.value(20).toInt() <= 0 ? QString() : q.value(20).toString();
    r.values[Iota] = q.value(21).toString();
    r.values[Comment] = q.value(22).toString();
    for (qsizetype i = 0; i < m_extra.size(); ++i) {
        QString v = q.value(23 + static_cast<int>(i)).toString();
        // Il prefisso WPX: chi non l'ha scritto nel file lo trova lo stesso.
        if (v.isEmpty() && m_extra.at(i) == QLatin1String("pfx"))
            v = core::awards::wpxPrefix(r.values[Call]);
        // Le date (del QSO, delle cartoline, di LoTW, dei campi ADIF a scelta)
        // si leggono come le altre.
        else if (m_extra.at(i).contains(QLatin1String("date"), Qt::CaseInsensitive))
            v = core::dates::show(v);
        r.extra << v;
    }
    return r;
}

bool QsoTableModel::filtered() const
{
    return !m_filter.trimmed().isEmpty() || !m_bands.isEmpty() || !m_modes.isEmpty() || !m_month.isEmpty()
        || m_dxcc > 0 || !m_qsl.isEmpty() || m_profile > 0 || !m_tag.isEmpty() || !m_dateFrom.isEmpty()
        || !m_dateTo.isEmpty();
}

void QsoTableModel::refreshTotal()
{
    m_total = m_db && m_db->isOpen() ? m_db->qsoCount() : 0;
}

void QsoTableModel::reload()
{
    decolog::StartupSpan trace("QsoTableModel::reload");
    beginResetModel();
    m_rows.clear();
    if (m_db && m_db->isOpen()) {
        QStringList where;
        QVariantList binds;
        const QString f = m_filter.trimmed().toUpper();
        if (!f.isEmpty()) {
            // Ricerca libera: nominativo, locatore, nome o commento.
            where << QStringLiteral("(call LIKE ? OR UPPER(gridsquare) LIKE ? OR UPPER(name) LIKE ? "
                                    "OR UPPER(IFNULL(comment, '')) LIKE ?)");
            const QString like = QLatin1Char('%') + f + QLatin1Char('%');
            binds << like << like << like << like;
        }
        if (!m_bands.isEmpty()) {
            QStringList marks;
            for (const QString& b : m_bands) {
                marks << QStringLiteral("?");
                binds << b.toLower();
            }
            where << QStringLiteral("band IN (%1)").arg(marks.join(QLatin1Char(',')));
        }
        if (!m_modes.isEmpty()) {
            QStringList marks;
            for (const QString& m : m_modes) {
                marks << QStringLiteral("?");
                binds << m.toUpper();
            }
            where << QStringLiteral("(CASE WHEN IFNULL(submode, '') = '' OR mode = 'SSB' THEN mode ELSE submode END) IN (%1)")
                         .arg(marks.join(QLatin1Char(',')));
        }
        if (!m_month.isEmpty()) {
            where << QStringLiteral("SUBSTR(qso_datetime_on, 1, 7) = ?");
            binds << m_month;
        }
        if (m_dxcc > 0) {
            where << QStringLiteral("dxcc = ?");
            binds << m_dxcc;
        }
        if (!m_qsl.isEmpty()) {
            const QString confirmedBy = QStringLiteral(
                "EXISTS (SELECT 1 FROM qsl_status s WHERE s.qso_id = qso.id AND s.rcvd = 'Y' AND s.service %1)");
            if (m_qsl == QLatin1String("confirmed")) {
                where << confirmedBy.arg(QStringLiteral("IN ('lotw', 'card', 'eqsl', 'qrz')"));
            } else if (m_qsl == QLatin1String("unconfirmed")) {
                where << QStringLiteral("NOT ") + confirmedBy.arg(QStringLiteral("IN ('lotw', 'card', 'eqsl', 'qrz')"));
            } else {
                where << confirmedBy.arg(QStringLiteral("= ?"));
                binds << m_qsl;
            }
        }
        if (m_profile > 0) {
            where << QStringLiteral("station_profile_id = ?");
            binds << m_profile;
        }
        if (!m_tag.isEmpty()) {
            // Un'etichetta intera, non un pezzo: "pota" non trova "potato".
            where << QStringLiteral("instr(',' || LOWER(IFNULL(tags, '')) || ',', ?) > 0");
            binds << QLatin1Char(',') + m_tag.toLower() + QLatin1Char(',');
        }
        // Le date nel database sono ISO-8601 a lunghezza fissa: il confronto fra
        // stringhe basta, e "fino al" comprende tutto quel giorno.
        if (!m_dateFrom.isEmpty()) {
            where << QStringLiteral("qso_datetime_on >= ?");
            binds << m_dateFrom;
        }
        if (!m_dateTo.isEmpty()) {
            const QDate to = QDate::fromString(m_dateTo, QStringLiteral("yyyy-MM-dd"));
            where << QStringLiteral("qso_datetime_on < ?");
            binds << (to.isValid() ? to.addDays(1).toString(QStringLiteral("yyyy-MM-dd")) : m_dateTo);
        }

        QSqlQuery q(m_db->connection());
        q.prepare(selectSql(where.isEmpty() ? QString()
                                            : QStringLiteral("AND ") + where.join(QStringLiteral(" AND "))));
        for (const auto& b : binds)
            q.addBindValue(b);
        q.setForwardOnly(true);
        if (q.exec()) {
            while (q.next())
                m_rows.append(rowFromQuery(q));
        }
        computeCategories();
        if (!defaultSort())
            applySort();
    }
    refreshTotal();
    endResetModel();
    emit countChanged();
}

void QsoTableModel::refreshQso(qint64 id)
{
    if (!m_db || !m_db->isOpen())
        return;
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id != id)
            continue;
        QSqlQuery q(m_db->connection());
        q.prepare(selectSql(QStringLiteral("AND id = ?")));
        q.addBindValue(id);
        if (!q.exec() || !q.next())
            return;
        const bool wasFresh = m_rows.at(i).fresh;
        m_rows[i] = rowFromQuery(q);
        m_rows[i].fresh = wasFresh;
        m_category.insert(id, categoryOf(id));
        emit dataChanged(index(static_cast<int>(i), 0), index(static_cast<int>(i), columns() - 1));
        return;
    }
}

void QsoTableModel::insertQso(qint64 id)
{
    if (!m_db || !m_db->isOpen())
        return;
    // Con un filtro o un ordine scelto, il posto giusto lo trova il ricarico.
    if (filtered() || !defaultSort()) {
        reload();
        return;
    }
    QSqlQuery q(m_db->connection());
    q.prepare(selectSql(QStringLiteral("AND id = ?")));
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return;
    Row r = rowFromQuery(q);
    r.fresh = true;
    m_category.insert(id, categoryOf(id));

    // Di solito il QSO appena fatto e' il piu' recente; uno scritto a mano con
    // l'ora di prima no.
    qsizetype pos = 0;
    while (pos < m_rows.size() && m_rows.at(pos).sortKey > r.sortKey)
        ++pos;

    // Evidenziata solo l'ultima arrivata: e' quella che l'operatore cerca con
    // lo sguardo, le altre tornano righe normali.
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].fresh) {
            m_rows[i].fresh = false;
            emit dataChanged(index(static_cast<int>(i), 0), index(static_cast<int>(i), columns() - 1), {IsNewRole});
        }
    }

    beginInsertRows({}, static_cast<int>(pos), static_cast<int>(pos));
    m_rows.insert(pos, r);
    endInsertRows();
    refreshTotal();
    emit countChanged();
}

void QsoTableModel::clearFilters()
{
    m_filter.clear();
    m_bands.clear();
    m_modes.clear();
    m_month.clear();
    m_dxcc = 0;
    m_qsl.clear();
    m_profile = 0;
    m_tag.clear();
    m_dateFrom.clear();
    m_dateTo.clear();
    emit filtersChanged();
    reload();
}

QVariantMap QsoTableModel::filterState() const
{
    return {
        {QStringLiteral("text"), m_filter},
        {QStringLiteral("bands"), m_bands},
        {QStringLiteral("modes"), m_modes},
        {QStringLiteral("month"), m_month},
        {QStringLiteral("dxcc"), m_dxcc},
        {QStringLiteral("qsl"), m_qsl},
        {QStringLiteral("profile"), m_profile},
        {QStringLiteral("tag"), m_tag},
        {QStringLiteral("dateFrom"), m_dateFrom},
        {QStringLiteral("dateTo"), m_dateTo},
    };
}

void QsoTableModel::applyFilterState(const QVariantMap& state)
{
    m_filter = state.value(QStringLiteral("text")).toString();
    m_bands = state.value(QStringLiteral("bands")).toStringList();
    m_modes = state.value(QStringLiteral("modes")).toStringList();
    m_month = state.value(QStringLiteral("month")).toString();
    m_dxcc = state.value(QStringLiteral("dxcc")).toInt();
    m_qsl = state.value(QStringLiteral("qsl")).toString();
    m_profile = state.value(QStringLiteral("profile")).toInt();
    m_tag = state.value(QStringLiteral("tag")).toString();
    m_dateFrom = state.value(QStringLiteral("dateFrom")).toString();
    m_dateTo = state.value(QStringLiteral("dateTo")).toString();
    emit filtersChanged();
    reload();
}

qint64 QsoTableModel::idAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).id : 0;
}

QString QsoTableModel::callAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).values[Call] : QString();
}

QString QsoTableModel::valueAt(int row, int column) const
{
    return valueFor(row, m_layout.value(column));
}

int QsoTableModel::rowForId(qint64 id) const
{
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id == id)
            return static_cast<int>(i);
    }
    return -1;
}

QStringList QsoTableModel::bandsInLog() const
{
    QStringList out;
    if (!m_db || !m_db->isOpen())
        return out;
    for (const auto& row : m_db->countByBand())
        out << row.key;
    return out;
}

QStringList QsoTableModel::modesInLog() const
{
    QStringList out;
    if (!m_db || !m_db->isOpen())
        return out;
    for (const auto& row : m_db->countByMode())
        out << row.key;
    return out;
}

QVariantList QsoTableModel::tagsInLog() const
{
    QVariantList out;
    if (!m_db || !m_db->isOpen())
        return out;
    for (const auto& row : m_db->tagCounts())
        out << QVariantMap{{QStringLiteral("key"), row.key}, {QStringLiteral("count"), row.count}};
    return out;
}

QVariantList QsoTableModel::shownIds() const
{
    QVariantList out;
    out.reserve(m_rows.size());
    for (const Row& r : m_rows)
        out << r.id;
    return out;
}

void QsoTableModel::setDxccFilter(int dxcc)
{
    if (dxcc == m_dxcc)
        return;
    m_dxcc = qMax(0, dxcc);
    emit filtersChanged();
    reload();
}

void QsoTableModel::setQslFilter(const QString& qsl)
{
    if (qsl == m_qsl)
        return;
    m_qsl = qsl;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setProfileFilter(int profileId)
{
    if (profileId == m_profile)
        return;
    m_profile = qMax(0, profileId);
    emit filtersChanged();
    reload();
}

void QsoTableModel::setTagFilter(const QString& tag)
{
    const QString t = tag.simplified();
    if (t == m_tag)
        return;
    m_tag = t;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setDateFrom(const QString& date)
{
    const QString d = QDate::fromString(date.trimmed(), QStringLiteral("yyyy-MM-dd")).isValid() ? date.trimmed() : QString();
    if (d == m_dateFrom)
        return;
    m_dateFrom = d;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setDateTo(const QString& date)
{
    const QString d = QDate::fromString(date.trimmed(), QStringLiteral("yyyy-MM-dd")).isValid() ? date.trimmed() : QString();
    if (d == m_dateTo)
        return;
    m_dateTo = d;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setFilterText(const QString& text)
{
    if (text == m_filter)
        return;
    m_filter = text;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setBandFilter(const QStringList& bands)
{
    if (bands == m_bands)
        return;
    m_bands = bands;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setModeFilter(const QStringList& modes)
{
    if (modes == m_modes)
        return;
    m_modes = modes;
    emit filtersChanged();
    reload();
}

void QsoTableModel::setMonthFilter(const QString& month)
{
    if (month == m_month)
        return;
    m_month = month;
    emit filtersChanged();
    reload();
}

} // namespace decolog::app
