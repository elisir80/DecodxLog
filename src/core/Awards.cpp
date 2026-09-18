#include "core/Awards.h"

#include "core/LogDatabase.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QTimeZone>
#include <algorithm>

namespace decolog::core {

namespace awards {

namespace {

const QSet<QString> kIgnoredSuffixes{
    QStringLiteral("P"), QStringLiteral("M"), QStringLiteral("MM"), QStringLiteral("AM"),
    QStringLiteral("QRP"), QStringLiteral("QRPP"), QStringLiteral("A"), QStringLiteral("B"),
    QStringLiteral("LH"), QStringLiteral("J"), QStringLiteral("R"), QStringLiteral("T")};

// Il prefisso di un nominativo senza barre: tutto fino all'ultima cifra seguita
// solo da lettere (WB2ABC → WB2, 2E0ABC → 2E0, 4X1AB → 4X1).
QString basePrefix(const QString& call)
{
    for (qsizetype i = call.size() - 1; i >= 0; --i) {
        if (call.at(i).isDigit()) {
            bool lettersAfter = true;
            for (qsizetype j = i + 1; j < call.size(); ++j) {
                if (!call.at(j).isLetter()) {
                    lettersAfter = false;
                    break;
                }
            }
            if (lettersAfter && i + 1 < call.size())
                return call.left(i + 1);
        }
    }
    // Nessuna cifra: si aggiunge lo zero (regola WPX), sulle prime due lettere.
    return call.left(2) + QLatin1Char('0');
}

} // namespace

QString wpxPrefix(const QString& callsign)
{
    const QString call = callsign.trimmed().toUpper();
    if (call.isEmpty())
        return {};

    QStringList parts;
    for (const QString& p : call.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (!kIgnoredSuffixes.contains(p))
            parts << p;
    }
    if (parts.isEmpty())
        return {};

    // Una sola cifra dopo la barra cambia l'area: W1AW/4 → W4.
    QString areaDigit;
    if (parts.size() >= 2 && parts.last().size() == 1 && parts.last().at(0).isDigit()) {
        areaDigit = parts.takeLast();
    }

    QString prefix;
    if (parts.size() >= 2) {
        // EA8/OH2XX o OH2XX/EA8: il prefisso e' la parte piu' corta. Se non ha
        // cifre (LX/DL1ABC) si aggiunge lo zero.
        const QString shortPart = parts.at(0).size() <= parts.at(1).size() ? parts.at(0) : parts.at(1);
        prefix = shortPart;
        if (!std::any_of(prefix.begin(), prefix.end(), [](QChar c) { return c.isDigit(); }))
            prefix += QLatin1Char('0');
    } else {
        prefix = basePrefix(parts.first());
    }

    if (!areaDigit.isEmpty()) {
        // Si sostituisce l'ultima cifra del prefisso.
        for (qsizetype i = prefix.size() - 1; i >= 0; --i) {
            if (prefix.at(i).isDigit()) {
                prefix = prefix.left(i) + areaDigit;
                break;
            }
        }
    }
    return prefix;
}

const QMap<QString, QString>& usStates()
{
    static const QMap<QString, QString> states{
        {"AL", "Alabama"}, {"AK", "Alaska"}, {"AZ", "Arizona"}, {"AR", "Arkansas"}, {"CA", "California"},
        {"CO", "Colorado"}, {"CT", "Connecticut"}, {"DE", "Delaware"}, {"FL", "Florida"}, {"GA", "Georgia"},
        {"HI", "Hawaii"}, {"ID", "Idaho"}, {"IL", "Illinois"}, {"IN", "Indiana"}, {"IA", "Iowa"},
        {"KS", "Kansas"}, {"KY", "Kentucky"}, {"LA", "Louisiana"}, {"ME", "Maine"}, {"MD", "Maryland"},
        {"MA", "Massachusetts"}, {"MI", "Michigan"}, {"MN", "Minnesota"}, {"MS", "Mississippi"},
        {"MO", "Missouri"}, {"MT", "Montana"}, {"NE", "Nebraska"}, {"NV", "Nevada"}, {"NH", "New Hampshire"},
        {"NJ", "New Jersey"}, {"NM", "New Mexico"}, {"NY", "New York"}, {"NC", "North Carolina"},
        {"ND", "North Dakota"}, {"OH", "Ohio"}, {"OK", "Oklahoma"}, {"OR", "Oregon"}, {"PA", "Pennsylvania"},
        {"RI", "Rhode Island"}, {"SC", "South Carolina"}, {"SD", "South Dakota"}, {"TN", "Tennessee"},
        {"TX", "Texas"}, {"UT", "Utah"}, {"VT", "Vermont"}, {"VA", "Virginia"}, {"WA", "Washington"},
        {"WV", "West Virginia"}, {"WI", "Wisconsin"}, {"WY", "Wyoming"}};
    return states;
}

const QMap<QString, QString>& continents()
{
    static const QMap<QString, QString> list{
        {"EU", QStringLiteral("Europa")},        {"NA", QStringLiteral("Nord America")},
        {"SA", QStringLiteral("Sud America")},   {"AS", QStringLiteral("Asia")},
        {"AF", QStringLiteral("Africa")},        {"OC", QStringLiteral("Oceania")},
        {"AN", QStringLiteral("Antartide")}};
    return list;
}

const QMap<QString, QString>& japanPrefectures()
{
    // I numeri sono quelli di ADIF (Primary Administrative Subdivision, Japan).
    static const QMap<QString, QString> list{
        {"01", "Hokkaido"},  {"02", "Aomori"},    {"03", "Iwate"},     {"04", "Akita"},
        {"05", "Yamagata"},  {"06", "Miyagi"},    {"07", "Fukushima"}, {"08", "Niigata"},
        {"09", "Nagano"},    {"10", "Tokyo"},     {"11", "Kanagawa"},  {"12", "Chiba"},
        {"13", "Saitama"},   {"14", "Ibaraki"},   {"15", "Tochigi"},   {"16", "Gunma"},
        {"17", "Yamanashi"}, {"18", "Shizuoka"},  {"19", "Gifu"},      {"20", "Aichi"},
        {"21", "Mie"},       {"22", "Kyoto"},     {"23", "Shiga"},     {"24", "Nara"},
        {"25", "Osaka"},     {"26", "Wakayama"},  {"27", "Hyogo"},     {"28", "Toyama"},
        {"29", "Fukui"},     {"30", "Ishikawa"},  {"31", "Okayama"},   {"32", "Shimane"},
        {"33", "Yamaguchi"}, {"34", "Tottori"},   {"35", "Hiroshima"}, {"36", "Kagawa"},
        {"37", "Tokushima"}, {"38", "Ehime"},     {"39", "Kochi"},     {"40", "Fukuoka"},
        {"41", "Saga"},      {"42", "Nagasaki"},  {"43", "Kumamoto"},  {"44", "Oita"},
        {"45", "Miyazaki"},  {"46", "Kagoshima"}, {"47", "Okinawa"}};
    return list;
}

QString japanPrefecture(const QString& state)
{
    // Nei log si trova "12", "JA12", "12 Chiba": conta il numero.
    const QString text = state.trimmed().toUpper();
    QString digits;
    for (const QChar c : text) {
        if (c.isDigit())
            digits += c;
        else if (!digits.isEmpty())
            break;
    }
    if (digits.isEmpty())
        return {};
    const int number = digits.toInt();
    if (number < 1 || number > 47)
        return {};
    const QString key = QStringLiteral("%1").arg(number, 2, 10, QLatin1Char('0'));
    return japanPrefectures().contains(key) ? key : QString();
}

QString japanDistrict(const QString& callsign)
{
    // Il distretto e' la cifra del nominativo giapponese: JA1AA → 1, 7K4XYZ → 4.
    // Si guarda la parte principale, saltando i suffissi con la barra.
    const QString call = callsign.trimmed().toUpper();
    QString base = call;
    for (const QString& part : call.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (part.size() >= 3) {
            base = part;
            break;
        }
    }
    for (qsizetype i = 0; i < base.size(); ++i) {
        if (!base.at(i).isDigit())
            continue;
        // La cifra del distretto e' quella che separa prefisso e suffisso:
        // dopo di lei ci sono solo lettere.
        bool lettersAfter = i + 1 < base.size();
        for (qsizetype j = i + 1; j < base.size(); ++j) {
            if (!base.at(j).isLetter()) {
                lettersAfter = false;
                break;
            }
        }
        if (lettersAfter)
            return base.mid(i, 1);
    }
    return {};
}

QString japanJarlCode(const QString& county)
{
    QString digits;
    for (const QChar c : county) {
        if (c.isDigit())
            digits += c;
    }
    if (digits.size() < 4 || digits.size() > 6)
        return {};
    const int prefecture = digits.left(2).toInt();
    if (prefecture < 1 || prefecture > 47)
        return {};
    return digits;
}

bool isJapanGun(const QString& jarlCode)
{
    return jarlCode.size() == 5;
}

} // namespace awards

int AwardResult::confirmed() const
{
    return static_cast<int>(std::count_if(items.cbegin(), items.cend(), [](const AwardItem& i) { return i.confirmed(); }));
}

QList<BandTotal> AwardResult::bandTotals(const QStringList& bands) const
{
    QList<BandTotal> out;
    for (const QString& band : bands) {
        BandTotal t;
        t.band = band;
        for (const AwardItem& i : items) {
            if (i.bandsWorked.contains(band))
                ++t.worked;
            if (i.bandsConfirmed.contains(band))
                ++t.confirmed;
        }
        out << t;
    }
    return out;
}

AwardCalculator::AwardCalculator(DxccName dxccName)
    : m_dxccName(std::move(dxccName))
{
}

QStringList AwardCalculator::awardIds()
{
    return {QStringLiteral("dxcc"), QStringLiteral("ft2"), QStringLiteral("wac"), QStringLiteral("waac"),
            QStringLiteral("waz"), QStringLiteral("was"), QStringLiteral("waja"), QStringLiteral("ajd"),
            QStringLiteral("jcc"), QStringLiteral("jcg"), QStringLiteral("wpx"),
            QStringLiteral("grids"), QStringLiteral("iota"), QStringLiteral("pota"), QStringLiteral("sota"),
            QStringLiteral("wwff")};
}

namespace {

bool modeMatches(const QString& group, const QString& mode, const QString& submode)
{
    if (group.isEmpty())
        return true;
    static const QSet<QString> phone{QStringLiteral("SSB"), QStringLiteral("AM"), QStringLiteral("FM"),
                                     QStringLiteral("DIGITALVOICE"), QStringLiteral("DSTAR")};
    if (group == QLatin1String("FT2"))
        return submode == QLatin1String("FT2");
    if (group == QLatin1String("FT8"))
        return mode == QLatin1String("FT8");
    if (group == QLatin1String("CW"))
        return mode == QLatin1String("CW");
    if (group == QLatin1String("PHONE"))
        return phone.contains(mode);
    if (group == QLatin1String("DIGITAL"))
        return mode != QLatin1String("CW") && !phone.contains(mode);
    return true;
}

// Chiave di ordinamento: numeri come numeri, il resto come testo.
bool keyLess(const AwardItem& a, const AwardItem& b)
{
    bool okA = false, okB = false;
    const int na = a.key.toInt(&okA);
    const int nb = b.key.toInt(&okB);
    if (okA && okB)
        return na < nb;
    return a.key < b.key;
}

} // namespace

QList<AwardResult> AwardCalculator::compute(const LogDatabase& db, const AwardFilter& filter) const
{
    struct Builder {
        AwardResult result;
        QHash<QString, AwardItem> items;
    };
    QHash<QString, Builder> builders;
    auto define = [&builders](const char* id, const QString& title, int target, int total) {
        Builder b;
        b.result.id = QLatin1String(id);
        b.result.title = title;
        b.result.target = target;
        b.result.total = total;
        builders.insert(b.result.id, b);
    };
    define("dxcc", QStringLiteral("DXCC"), 100, 340);
    define("ft2", QStringLiteral("FT2 Award"), 100, 0);
    // I sei continenti dell'IARU. L'Antartide non fa numero per il WAC, ma chi
    // ce l'ha vuole vederla: entra nell'elenco e non nel traguardo.
    define("wac", QStringLiteral("WAC"), 6, 6);
    // Worked All Africa: le entita' DXCC del continente africano. Quante siano
    // lo dice il cty.csv, quindi traguardo e totale si mettono a posto dopo.
    define("waac", QStringLiteral("WAAC"), 0, 0);
    define("waz", QStringLiteral("WAZ"), 40, 40);
    define("was", QStringLiteral("WAS"), 50, 50);
    // Le 47 prefetture giapponesi (WAJA) e i 10 distretti (AJD): il Giappone
    // mette la prefettura in STATE e il distretto nella cifra del nominativo.
    define("waja", QStringLiteral("WAJA"), 47, 47);
    define("ajd", QStringLiteral("AJD"), 10, 10);
    // JCC e JCG: le citta' e i distretti del JARL, col numero che sta in CNTY.
    // Il primo traguardo e' cento dell'uno e cento dell'altro; quante siano in
    // tutto lo decide il JARL e cambia, quindi il totale resta senza numero.
    define("jcc", QStringLiteral("JCC"), 100, 0);
    define("jcg", QStringLiteral("JCG"), 100, 0);
    define("wpx", QStringLiteral("WPX"), 300, 0);
    define("grids", QCoreApplication::translate("Awards", "Grids"), 100, 0);
    define("iota", QStringLiteral("IOTA"), 100, 0);
    define("pota", QStringLiteral("POTA"), 0, 0);
    define("sota", QStringLiteral("SOTA"), 0, 0);
    define("wwff", QStringLiteral("WWFF"), 44, 0);

    QSqlQuery q(db.connection());
    q.setForwardOnly(true);
    q.exec(QStringLiteral(
        "SELECT id, call, band, mode, IFNULL(submode, ''), dxcc, cqz, state, gridsquare, iota, pota_ref, sota_ref, "
        "wwff_ref, qso_datetime_on, IFNULL(station_profile_id, 0), IFNULL(tags, ''), "
        "IFNULL(cont, ''), IFNULL(cnty, ''), "
        "(SELECT rcvd FROM qsl_status s WHERE s.qso_id = qso.id AND s.service = 'lotw'), "
        "(SELECT rcvd FROM qsl_status s WHERE s.qso_id = qso.id AND s.service = 'card'), "
        "(SELECT rcvd FROM qsl_status s WHERE s.qso_id = qso.id AND s.service = 'eqsl') "
        "FROM qso WHERE deleted = 0 ORDER BY qso_datetime_on"));

    const QSet<int> usaEntities{291, 6, 110};   // USA, Alaska, Hawaii
    static const QRegularExpression iotaRef(QStringLiteral("^(AF|AN|AS|EU|NA|OC|SA)-\\d{3}$"));

    while (q.next()) {
        const QString band = q.value(2).toString();
        const QString mode = q.value(3).toString();
        const QString submode = q.value(4).toString();
        if (!filter.band.isEmpty() && band != filter.band)
            continue;
        if (!modeMatches(filter.modeGroup, mode, submode))
            continue;
        if (filter.stationProfileId > 0 && q.value(14).toLongLong() != filter.stationProfileId)
            continue;
        if (!filter.tag.isEmpty()) {
            const QStringList tags = LogDatabase::splitTags(q.value(15).toString());
            if (!std::any_of(tags.cbegin(), tags.cend(), [&filter](const QString& t) {
                    return t.compare(filter.tag, Qt::CaseInsensitive) == 0;
                }))
                continue;
        }

        const qint64 id = q.value(0).toLongLong();
        const QString call = q.value(1).toString();
        const int dxcc = q.value(5).toInt();
        const QDateTime on = QDateTime::fromString(q.value(13).toString(), Qt::ISODate).toUTC();
        const bool confirmed = (filter.confirmLotw && q.value(18).toString() == QLatin1String("Y"))
                            || (filter.confirmCard && q.value(19).toString() == QLatin1String("Y"))
                            || (filter.confirmEqsl && q.value(20).toString() == QLatin1String("Y"));

        auto add = [&](const char* award, const QString& key, const QString& name) {
            if (key.isEmpty())
                return;
            Builder& b = builders[QLatin1String(award)];
            AwardItem& item = b.items[key];
            if (item.qsoCount == 0) {
                item.key = key;
                item.name = name;
                item.first = on;
                item.firstQsoId = id;
                item.firstCall = call;
            }
            item.last = on;
            ++item.qsoCount;
            item.bandsWorked.insert(band);
            if (confirmed)
                item.bandsConfirmed.insert(band);
        };

        if (dxcc > 0) {
            const QString name = m_dxccName ? m_dxccName(dxcc) : QString();
            add("dxcc", QString::number(dxcc), name);
            if (submode == QLatin1String("FT2"))
                add("ft2", QString::number(dxcc), name);
        }
        const QString continent = q.value(16).toString().trimmed().toUpper();
        if (awards::continents().contains(continent))
            add("wac", continent, awards::continents().value(continent));
        // Worked All Africa: un'entita' africana per volta, col suo nome.
        if (continent == QLatin1String("AF") && dxcc > 0) {
            add("waac", QString::number(dxcc),
                m_dxccName ? m_dxccName(dxcc) : QString());
        }
        const int cqz = q.value(6).toInt();
        if (cqz >= 1 && cqz <= 40)
            add("waz", QString::number(cqz), QString());
        const QString state = q.value(7).toString().trimmed().toUpper();
        if (usaEntities.contains(dxcc) && awards::usStates().contains(state))
            add("was", state, awards::usStates().value(state));
        // Giappone: la prefettura sta in STATE (ADIF la scrive col numero,
        // "12" o "JA12"), il distretto e' la cifra del nominativo.
        if (dxcc == 339) {
            const QString prefecture = awards::japanPrefecture(state);
            if (!prefecture.isEmpty())
                add("waja", prefecture, awards::japanPrefectures().value(prefecture));
            const QString district = awards::japanDistrict(call);
            if (!district.isEmpty())
                add("ajd", district, QString());
            // La citta' o il distretto: il numero JARL sta nel campo CNTY, e le
            // prime due cifre dicono la prefettura, che qui fa da nome.
            const QString jarl = awards::japanJarlCode(q.value(17).toString());
            if (!jarl.isEmpty()) {
                const QString prefecture = awards::japanPrefectures().value(jarl.left(2));
                add(awards::isJapanGun(jarl) ? "jcg" : "jcc", jarl, prefecture);
            }
        }
        add("wpx", awards::wpxPrefix(call), QString());
        const QString grid = q.value(8).toString().trimmed().toUpper();
        if (grid.size() >= 4)
            add("grids", grid.left(4), QString());
        const QString iota = q.value(9).toString().trimmed().toUpper();
        if (iotaRef.match(iota).hasMatch())
            add("iota", iota, QString());
        add("pota", q.value(10).toString().trimmed().toUpper(), QString());
        add("sota", q.value(11).toString().trimmed().toUpper(), QString());
        add("wwff", q.value(12).toString().trimmed().toUpper(), QString());
    }

    QList<AwardResult> out;
    for (const QString& id : awardIds()) {
        Builder& b = builders[id];
        b.result.items = b.items.values();
        std::sort(b.result.items.begin(), b.result.items.end(), keyLess);
        out << b.result;
    }
    return out;
}

} // namespace decolog::core
