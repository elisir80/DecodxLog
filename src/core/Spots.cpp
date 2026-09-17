#include "core/Spots.h"

#include "core/Bands.h"
#include "core/LogDatabase.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QTimeZone>
#include <algorithm>
#include <array>
#include <cmath>

namespace decolog::core {

QString EnrichedSpot::key() const
{
    return spot.dxCall + QLatin1Char('|') + spot.band + QLatin1Char('|') + spots::modeKey(spot.mode);
}

namespace spots {

namespace {

struct DialFrequency {
    double khz;
    const char* mode;
};

// Frequenze di chiamata dei modi digitali (dial, kHz). Il segnale sta da 0 a 3 kHz
// sopra; FT2 sono le frequenze di Decodium.
constexpr std::array kDials{
    DialFrequency{1840, "FT8"},    DialFrequency{1843, "FT2"},    DialFrequency{3568, "FT2"},
    DialFrequency{3573, "FT8"},    DialFrequency{3575, "FT4"},    DialFrequency{5357, "FT8"},
    DialFrequency{5360, "FT2"},    DialFrequency{7047.5, "FT4"},  DialFrequency{7062, "FT2"},
    DialFrequency{7074, "FT8"},    DialFrequency{10136, "FT8"},   DialFrequency{10140, "FT4"},
    DialFrequency{10144, "FT2"},   DialFrequency{14074, "FT8"},   DialFrequency{14080, "FT4"},
    DialFrequency{14084, "FT2"},   DialFrequency{18100, "FT8"},   DialFrequency{18104, "FT4"},
    DialFrequency{18108, "FT2"},   DialFrequency{21074, "FT8"},   DialFrequency{21140, "FT4"},
    DialFrequency{21144, "FT2"},   DialFrequency{24915, "FT8"},   DialFrequency{24919, "FT4"},
    DialFrequency{24923, "FT2"},   DialFrequency{28074, "FT8"},   DialFrequency{28180, "FT4"},
    DialFrequency{28184, "FT2"},   DialFrequency{50313, "FT8"},   DialFrequency{50316, "FT2"},
    DialFrequency{50318, "FT4"},   DialFrequency{50323, "FT8"},   DialFrequency{70154, "FT8"},
    DialFrequency{70157, "FT2"},   DialFrequency{144170, "FT4"},  DialFrequency{144174, "FT8"},
    DialFrequency{144177, "FT2"},
};

struct Segment {
    double low;
    double high;
    const char* mode;
};

// Piano di banda IARU Regione 1, semplificato: dove non c'e' un modo chiaro (60m,
// VHF alte) non si indovina.
constexpr std::array kSegments{
    Segment{1810, 1838, "CW"},     Segment{1838, 1843, "DIGI"},   Segment{1843, 2000, "SSB"},
    Segment{3500, 3570, "CW"},     Segment{3570, 3600, "DIGI"},   Segment{3600, 4000, "SSB"},
    Segment{7000, 7040, "CW"},     Segment{7040, 7060, "DIGI"},   Segment{7060, 7300, "SSB"},
    Segment{10100, 10130, "CW"},   Segment{10130, 10150, "DIGI"},
    Segment{14000, 14070, "CW"},   Segment{14070, 14100, "DIGI"}, Segment{14100, 14350, "SSB"},
    Segment{18068, 18095, "CW"},   Segment{18095, 18111, "DIGI"}, Segment{18111, 18168, "SSB"},
    Segment{21000, 21070, "CW"},   Segment{21070, 21151, "DIGI"}, Segment{21151, 21450, "SSB"},
    Segment{24890, 24915, "CW"},   Segment{24915, 24931, "DIGI"}, Segment{24931, 24990, "SSB"},
    Segment{28000, 28070, "CW"},   Segment{28070, 28190, "DIGI"}, Segment{28300, 29700, "SSB"},
    Segment{50000, 50100, "CW"},   Segment{50100, 50300, "SSB"},
};

QDateTime timeFromHhmm(const QString& hhmm, const QDateTime& now)
{
    const QString digits = QString(hhmm).remove(QLatin1Char(':'));
    const QTime t = QTime::fromString(digits.left(4), QStringLiteral("HHmm"));
    if (!t.isValid())
        return now;
    QDateTime at(now.toUTC().date(), t, QTimeZone::UTC);
    // Uno spot delle 23:58 letto alle 00:01 e' di ieri.
    if (at > now.addSecs(30 * 60))
        at = at.addDays(-1);
    return at;
}

bool plausibleCall(const QString& call)
{
    static const QRegularExpression re(QStringLiteral("^[A-Z0-9/]{3,15}$"));
    if (!re.match(call).hasMatch())
        return false;
    const bool digit = std::any_of(call.cbegin(), call.cend(), [](QChar c) { return c.isDigit(); });
    const bool letter = std::any_of(call.cbegin(), call.cend(), [](QChar c) { return c.isLetter(); });
    return digit && letter;
}

void fillBandAndMode(Spot& s)
{
    s.band = bands::fromMhz(s.freqKhz / 1000.0);
    if (s.mode.isEmpty())
        s.mode = modeFor(s.freqKhz, s.comment);
    else
        s.mode = s.mode.toUpper();
    extractReferences(s);
}

} // namespace

QString modeFor(double freqKhz, const QString& comment)
{
    const QString c = comment.toUpper();
    // Le espressioni si compilano una volta: l'RBN manda centinaia di spot al minuto.
    static const QList<QPair<QRegularExpression, QString>> keywords = [] {
        const std::array<std::pair<const char*, const char*>, 15> list{{
            {"\\bFT-?8\\b", "FT8"},   {"\\bFT-?4\\b", "FT4"},     {"\\bFT-?2\\b", "FT2"},
            {"\\bJS8", "JS8"},         {"\\bQ65", "Q65"},           {"\\bJT65", "JT65"},
            {"\\bJT9\\b", "JT9"},     {"\\bMSK144\\b", "MSK144"}, {"\\bFST4W?\\b", "FST4"},
            {"\\bRTTY\\b", "RTTY"},   {"\\bPSK(31|63|125)?\\b", "PSK"},
            {"\\bSSTV\\b", "SSTV"},   {"\\bCW\\b", "CW"},
            {"\\b(SSB|USB|LSB)\\b", "SSB"}, {"\\bFM\\b", "FM"},
        }};
        QList<QPair<QRegularExpression, QString>> out;
        for (const auto& [pattern, mode] : list)
            out.append({QRegularExpression(QLatin1String(pattern)), QLatin1String(mode)});
        return out;
    }();
    for (const auto& [re, mode] : keywords) {
        if (re.match(c).hasMatch())
            return mode;
    }

    // Frequenza di chiamata digitale: quella con il dial piu' alto sotto lo spot.
    const DialFrequency* best = nullptr;
    for (const auto& d : kDials) {
        const double offset = freqKhz - d.khz;
        if (offset >= -0.2 && offset <= 3.2 && (!best || d.khz > best->khz))
            best = &d;
    }
    if (best)
        return QLatin1String(best->mode);
    for (const auto& s : kSegments) {
        if (freqKhz >= s.low && freqKhz < s.high)
            return QLatin1String(s.mode);
    }
    return {};
}

Tuning tuningFor(double freqKhz, const QString& mode)
{
    Tuning t;
    t.dialKhz = freqKhz;
    const QString m = mode.toUpper();
    const DialFrequency* best = nullptr;
    for (const auto& d : kDials) {
        const double offset = freqKhz - d.khz;
        if (m == QLatin1String(d.mode) && offset >= -0.2 && offset <= 3.2 && (!best || d.khz > best->khz))
            best = &d;
    }
    if (best) {
        t.dialKhz = best->khz;
        t.audioHz = qMax(0, static_cast<int>(std::lround((freqKhz - best->khz) * 1000.0)));
    }
    return t;
}

QString modeKey(const QString& mode)
{
    const QString m = mode.trimmed().toUpper();
    static const QSet<QString> phone{QStringLiteral("SSB"), QStringLiteral("USB"), QStringLiteral("LSB"),
                                     QStringLiteral("AM"), QStringLiteral("FM"), QStringLiteral("PHONE"),
                                     QStringLiteral("DIGITALVOICE")};
    if (phone.contains(m))
        return QStringLiteral("PHONE");
    if (m == QLatin1String("DIGI") || m == QLatin1String("DATA"))
        return {};
    return m;
}

void extractReferences(Spot& s)
{
    const QString c = s.comment.toUpper();
    static const QRegularExpression pota(QStringLiteral("\\b([A-Z0-9]{1,4}-\\d{4,5})\\b"));
    static const QRegularExpression sota(QStringLiteral("\\b([A-Z0-9]{1,3}/[A-Z]{2}-\\d{3})\\b"));
    static const QRegularExpression wwff(QStringLiteral("\\b([A-Z0-9]{1,4}FF-\\d{4})\\b"));
    static const QRegularExpression iota(QStringLiteral("\\b((?:AF|AN|AS|EU|NA|OC|SA)-\\d{3})\\b"));
    static const QRegularExpression grid(QStringLiteral("\\b([A-R]{2}\\d{2}(?:[A-X]{2})?)\\b"));

    if (s.wwffRef.isEmpty()) {
        if (const auto m = wwff.match(c); m.hasMatch())
            s.wwffRef = m.captured(1);
    }
    if (s.sotaRef.isEmpty()) {
        if (const auto m = sota.match(c); m.hasMatch())
            s.sotaRef = m.captured(1);
    }
    if (s.iotaRef.isEmpty()) {
        if (const auto m = iota.match(c); m.hasMatch())
            s.iotaRef = m.captured(1);
    }
    if (s.potaRef.isEmpty()) {
        // "IT-1234" o "US-12345", ma non un riferimento IOTA o WWFF gia' trovato.
        auto it = pota.globalMatch(c);
        while (it.hasNext()) {
            const QString ref = it.next().captured(1);
            if (ref != s.iotaRef && !ref.contains(QLatin1String("FF-")) && (c.contains(QLatin1String("POTA")) || ref.size() >= 7)) {
                s.potaRef = ref;
                break;
            }
        }
    }
    if (s.dxGrid.isEmpty()) {
        // Un solo locatore nel commento: quello del DX. Con due ("JN71<>FN42") non si
        // sa quale sia, meglio nessuno.
        QStringList found;
        auto it = grid.globalMatch(c);
        while (it.hasNext())
            found << it.next().captured(1);
        if (found.size() == 1)
            s.dxGrid = found.first();
    }
}

std::optional<Spot> parseDxLine(const QString& line, const QDateTime& now)
{
    static const QRegularExpression re(
        QStringLiteral("^DX de\\s+([^\\s:]+):?\\s+(\\d+(?:\\.\\d+)?)\\s+(\\S+)\\s+(.*?)\\s*(\\d{4})Z(?:\\s+(.*))?$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(line.trimmed());
    if (!m.hasMatch())
        return std::nullopt;

    Spot s;
    s.spotter = m.captured(1).toUpper();
    s.freqKhz = m.captured(2).toDouble();
    s.dxCall = m.captured(3).toUpper();
    s.comment = m.captured(4).simplified();
    s.time = timeFromHhmm(m.captured(5), now);
    s.source = QStringLiteral("cluster");
    if (s.freqKhz <= 0 || !plausibleCall(s.dxCall))
        return std::nullopt;

    static const QRegularExpression snr(QStringLiteral("(-?\\d{1,2})\\s*dB\\b"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression wpm(QStringLiteral("(\\d{1,2})\\s*WPM\\b"), QRegularExpression::CaseInsensitiveOption);
    if (const auto sm = snr.match(s.comment); sm.hasMatch()) {
        s.hasSnr = true;
        s.snr = sm.captured(1).toInt();
    }
    if (const auto wm = wpm.match(s.comment); wm.hasMatch())
        s.wpm = wm.captured(1).toInt();
    fillBandAndMode(s);
    return s;
}

std::optional<Spot> parseShowDxLine(const QString& line, const QDateTime& now)
{
    static const QRegularExpression re(
        QStringLiteral("^\\s*(\\d+(?:\\.\\d+)?)\\s+(\\S+)\\s+(\\d{1,2}-[A-Za-z]{3}-\\d{4})\\s+(\\d{4})Z\\s*(.*?)\\s*<([^>]+)>\\s*$"));
    const auto m = re.match(line);
    if (!m.hasMatch())
        return std::nullopt;
    Spot s;
    s.freqKhz = m.captured(1).toDouble();
    s.dxCall = m.captured(2).toUpper();
    const QDate date = QLocale(QLocale::English).toDate(m.captured(3), QStringLiteral("d-MMM-yyyy"));
    const QTime time = QTime::fromString(m.captured(4), QStringLiteral("HHmm"));
    s.time = date.isValid() && time.isValid() ? QDateTime(date, time, QTimeZone::UTC) : now;
    s.comment = m.captured(5).simplified();
    s.spotter = m.captured(6).toUpper();
    s.source = QStringLiteral("cluster");
    if (s.freqKhz <= 0 || !plausibleCall(s.dxCall))
        return std::nullopt;
    fillBandAndMode(s);
    return s;
}

std::optional<Spot> parseHamAlertJson(const QByteArray& line, const QDateTime& now)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(line.trimmed(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;
    const QJsonObject o = doc.object();
    auto text = [&o](const char* key) {
        const QJsonValue v = o.value(QLatin1String(key));
        return v.isDouble() ? QString::number(v.toDouble(), 'g', 12) : v.toString().trimmed();
    };

    Spot s;
    s.dxCall = (text("fullCallsign").isEmpty() ? text("callsign") : text("fullCallsign")).toUpper();
    const double f = text("frequency").toDouble();
    s.freqKhz = f > 0 && f < 1000 ? f * 1000.0 : f;   // HamAlert la scrive in MHz
    if (!plausibleCall(s.dxCall) || s.freqKhz <= 0)
        return std::nullopt;
    s.spotter = text("spotter").toUpper();
    s.comment = text("comment");
    s.mode = text("mode").toUpper();
    if (s.mode == QLatin1String("DIGI") || s.mode.isEmpty())
        s.mode = modeFor(s.freqKhz, s.comment + QLatin1Char(' ') + text("modeDetail"));
    s.time = text("time").isEmpty() ? now : timeFromHhmm(text("time"), now);
    s.source = QStringLiteral("hamalert");
    const QString origin = text("source");
    s.sourceName = origin.isEmpty() ? QStringLiteral("HamAlert") : QStringLiteral("HamAlert · %1").arg(origin);
    s.sotaRef = text("summitRef").toUpper();
    s.wwffRef = text("wwffRef").toUpper();
    s.potaRef = text("potaRef").toUpper();
    s.iotaRef = text("iotaGroupRef").toUpper();
    s.dxName = text("summitName").isEmpty() ? text("potaName") : text("summitName");
    if (o.contains(QLatin1String("snr"))) {
        s.hasSnr = true;
        s.snr = text("snr").toInt();
    }
    s.wpm = text("speed").toInt();
    fillBandAndMode(s);
    return s;
}

QList<Spot> parsePotaJson(const QByteArray& json)
{
    QList<Spot> out;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    for (const QJsonValue& v : doc.array()) {
        const QJsonObject o = v.toObject();
        Spot s;
        s.dxCall = o.value(QLatin1String("activator")).toString().trimmed().toUpper();
        s.freqKhz = o.value(QLatin1String("frequency")).toVariant().toDouble();
        if (!plausibleCall(s.dxCall) || s.freqKhz <= 0)
            continue;
        s.mode = o.value(QLatin1String("mode")).toString().trimmed().toUpper();
        s.potaRef = o.value(QLatin1String("reference")).toString().trimmed().toUpper();
        s.dxName = o.value(QLatin1String("name")).toString().trimmed();
        s.spotter = o.value(QLatin1String("spotter")).toString().trimmed().toUpper();
        s.comment = o.value(QLatin1String("comments")).toString().simplified();
        const QString grid = o.value(QLatin1String("grid6")).toString();
        s.dxGrid = grid.isEmpty() ? o.value(QLatin1String("grid4")).toString() : grid;
        QDateTime at = QDateTime::fromString(o.value(QLatin1String("spotTime")).toString(), Qt::ISODate);
        at.setTimeZone(QTimeZone::UTC);
        s.time = at.isValid() ? at : QDateTime::currentDateTimeUtc();
        s.source = QStringLiteral("pota");
        s.sourceName = QStringLiteral("POTA");
        if (s.mode.isEmpty())
            s.mode = modeFor(s.freqKhz, s.comment);
        fillBandAndMode(s);
        out << s;
    }
    return out;
}

} // namespace spots

// ── LogIndex ──────────────────────────────────────────────────────────────────

void LogIndex::clear()
{
    m_calls.clear();
    m_callBand.clear();
    m_callBandMode.clear();
    m_dxcc.clear();
    m_dxccBand.clear();
    m_dxccMode.clear();
    m_dxccSlot.clear();
    m_dxccConfirmed.clear();
    m_qsos = 0;
}

void LogIndex::rebuild(const LogDatabase& db, bool confirmLotw, bool confirmCard, bool confirmEqsl)
{
    clear();
    QSqlQuery q(db.connection());
    q.setForwardOnly(true);
    q.prepare(QStringLiteral(
        "SELECT call, band, CASE WHEN IFNULL(submode, '') = '' OR mode = 'SSB' THEN mode ELSE submode END, "
        "IFNULL(dxcc, 0), "
        "EXISTS (SELECT 1 FROM qsl_status s WHERE s.qso_id = qso.id AND s.rcvd = 'Y' AND ("
        "  (s.service = 'lotw' AND ?) OR (s.service = 'card' AND ?) OR (s.service = 'eqsl' AND ?))) "
        "FROM qso WHERE deleted = 0"));
    q.addBindValue(confirmLotw ? 1 : 0);
    q.addBindValue(confirmCard ? 1 : 0);
    q.addBindValue(confirmEqsl ? 1 : 0);
    if (!q.exec())
        return;
    const QLatin1Char sep('|');
    while (q.next()) {
        const QString call = q.value(0).toString();
        const QString band = q.value(1).toString();
        const QString mode = spots::modeKey(q.value(2).toString());
        const int dxcc = q.value(3).toInt();
        m_calls.insert(call);
        m_callBand.insert(call + sep + band);
        m_callBandMode.insert(call + sep + band + sep + mode);
        if (dxcc > 0) {
            const QString d = QString::number(dxcc);
            m_dxcc.insert(dxcc);
            m_dxccBand.insert(d + sep + band);
            m_dxccMode.insert(d + sep + mode);
            m_dxccSlot.insert(d + sep + band + sep + mode);
            if (q.value(4).toBool())
                m_dxccConfirmed.insert(dxcc);
        }
        ++m_qsos;
    }
}

int LogIndex::status(const Spot& spot, int dxcc) const
{
    int st = 0;
    const QLatin1Char sep('|');
    const QString call = spot.dxCall.toUpper();
    const QString mode = spots::modeKey(spot.mode);
    if (!m_calls.contains(call))
        st |= StatusNewCall;
    else if (m_callBand.contains(call + sep + spot.band)
             && (mode.isEmpty() || m_callBandMode.contains(call + sep + spot.band + sep + mode)))
        st |= StatusWorkedBand;

    if (dxcc > 0) {
        const QString d = QString::number(dxcc);
        if (!m_dxcc.contains(dxcc)) {
            st |= StatusNewDxcc;
        } else {
            if (!spot.band.isEmpty() && !m_dxccBand.contains(d + sep + spot.band))
                st |= StatusNewBand;
            if (!mode.isEmpty() && !m_dxccMode.contains(d + sep + mode))
                st |= StatusNewMode;
            if (!mode.isEmpty() && !spot.band.isEmpty() && !m_dxccSlot.contains(d + sep + spot.band + sep + mode))
                st |= StatusNewSlot;
            if (!m_dxccConfirmed.contains(dxcc))
                st |= StatusUnconfirmed;
        }
    }
    return st;
}

// ── SpotFilter ────────────────────────────────────────────────────────────────

namespace {

// Il modo come lo sceglie l'operatore nel filtro.
QString filterMode(const QString& mode)
{
    const QString key = spots::modeKey(mode);
    if (key == QLatin1String("PHONE"))
        return QStringLiteral("SSB");
    static const QSet<QString> own{QStringLiteral("FT8"), QStringLiteral("FT4"), QStringLiteral("FT2"),
                                   QStringLiteral("CW"), QStringLiteral("RTTY")};
    if (own.contains(key))
        return key;
    return mode.isEmpty() ? QString() : QStringLiteral("DIGI");
}

QStringList toList(const QSet<QString>& set)
{
    QStringList l(set.cbegin(), set.cend());
    l.sort();
    return l;
}

QSet<QString> toSet(const QVariant& v)
{
    const QStringList l = v.toStringList();
    return QSet<QString>(l.cbegin(), l.cend());
}

} // namespace

bool SpotFilter::isEmpty() const
{
    return bands.isEmpty() && modes.isEmpty() && dxContinents.isEmpty() && spotterContinents.isEmpty()
        && sources.isEmpty() && dxcc.isEmpty() && anyStatus == 0 && !hideWorkedBand && !onlyActivations
        && !onlyLotw && skimmers && minSnr <= -99 && calls.trimmed().isEmpty() && text.trimmed().isEmpty();
}

bool SpotFilter::matches(const EnrichedSpot& e, const QDateTime& now) const
{
    const Spot& s = e.spot;
    if (maxAgeMinutes > 0 && s.time.isValid() && s.time.secsTo(now) > maxAgeMinutes * 60)
        return false;
    if (!bands.isEmpty() && !bands.contains(s.band))
        return false;
    if (!modes.isEmpty() && !modes.contains(filterMode(s.mode)))
        return false;
    if (!dxContinents.isEmpty() && !dxContinents.contains(e.continent))
        return false;
    if (!spotterContinents.isEmpty() && !spotterContinents.contains(e.spotterContinent))
        return false;
    if (!sources.isEmpty() && !sources.contains(s.source))
        return false;
    if (!dxcc.isEmpty() && !dxcc.contains(e.dxcc))
        return false;
    if (anyStatus != 0 && (e.status & anyStatus) == 0)
        return false;
    if (hideWorkedBand && (e.status & StatusWorkedBand))
        return false;
    if (onlyActivations && s.potaRef.isEmpty() && s.sotaRef.isEmpty() && s.wwffRef.isEmpty() && s.iotaRef.isEmpty())
        return false;
    if (onlyLotw && !(e.status & StatusLotwUser))
        return false;
    if (!skimmers && s.isSkimmer())
        return false;
    if (s.hasSnr && s.snr < minSnr)
        return false;

    if (!calls.trimmed().isEmpty()) {
        bool any = false;
        for (const QString& p : calls.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts)) {
            const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(p.toUpper()));
            if (re.match(s.dxCall).hasMatch()) {
                any = true;
                break;
            }
        }
        if (!any)
            return false;
    }
    const QString needle = text.trimmed().toUpper();
    if (!needle.isEmpty()) {
        const QString hay = QStringList{s.dxCall, e.entity, s.comment, s.potaRef, s.sotaRef, s.wwffRef, s.iotaRef, s.dxName}
                                .join(QLatin1Char(' '))
                                .toUpper();
        if (!hay.contains(needle))
            return false;
    }
    return true;
}

QVariantMap SpotFilter::toMap() const
{
    QVariantList dx;
    for (int d : dxcc)
        dx << d;
    return {
        {QStringLiteral("bands"), toList(bands)},
        {QStringLiteral("modes"), toList(modes)},
        {QStringLiteral("dxContinents"), toList(dxContinents)},
        {QStringLiteral("spotterContinents"), toList(spotterContinents)},
        {QStringLiteral("sources"), toList(sources)},
        {QStringLiteral("dxcc"), dx},
        {QStringLiteral("anyStatus"), anyStatus},
        {QStringLiteral("hideWorkedBand"), hideWorkedBand},
        {QStringLiteral("onlyActivations"), onlyActivations},
        {QStringLiteral("onlyLotw"), onlyLotw},
        {QStringLiteral("skimmers"), skimmers},
        {QStringLiteral("minSnr"), minSnr},
        {QStringLiteral("maxAgeMinutes"), maxAgeMinutes},
        {QStringLiteral("calls"), calls},
        {QStringLiteral("text"), text},
    };
}

SpotFilter SpotFilter::fromMap(const QVariantMap& m)
{
    SpotFilter f;
    f.bands = toSet(m.value(QStringLiteral("bands")));
    f.modes = toSet(m.value(QStringLiteral("modes")));
    f.dxContinents = toSet(m.value(QStringLiteral("dxContinents")));
    f.spotterContinents = toSet(m.value(QStringLiteral("spotterContinents")));
    f.sources = toSet(m.value(QStringLiteral("sources")));
    for (const QVariant& v : m.value(QStringLiteral("dxcc")).toList())
        f.dxcc.insert(v.toInt());
    f.anyStatus = m.value(QStringLiteral("anyStatus"), 0).toInt();
    f.hideWorkedBand = m.value(QStringLiteral("hideWorkedBand"), false).toBool();
    f.onlyActivations = m.value(QStringLiteral("onlyActivations"), false).toBool();
    f.onlyLotw = m.value(QStringLiteral("onlyLotw"), false).toBool();
    f.skimmers = m.value(QStringLiteral("skimmers"), true).toBool();
    f.minSnr = m.value(QStringLiteral("minSnr"), -99).toInt();
    f.maxAgeMinutes = m.value(QStringLiteral("maxAgeMinutes"), 30).toInt();
    f.calls = m.value(QStringLiteral("calls")).toString();
    f.text = m.value(QStringLiteral("text")).toString();
    return f;
}

// ── LotwUsers ─────────────────────────────────────────────────────────────────

int LotwUsers::load(const QByteArray& csv)
{
    m_lastUpload.clear();
    qsizetype start = 0;
    while (start < csv.size()) {
        qsizetype end = csv.indexOf('\n', start);
        if (end < 0)
            end = csv.size();
        const QByteArray line = csv.mid(start, end - start).trimmed();
        start = end + 1;
        const qsizetype comma = line.indexOf(',');
        if (comma <= 0)
            continue;
        const QDate date = QDate::fromString(QString::fromLatin1(line.mid(comma + 1, 10)), QStringLiteral("yyyy-MM-dd"));
        if (date.isValid())
            m_lastUpload.insert(QString::fromLatin1(line.left(comma)).toUpper(), date);
    }
    return size();
}

bool LotwUsers::isActive(const QString& call, int days, const QDate& today) const
{
    QString c = call.trimmed().toUpper();
    auto it = m_lastUpload.constFind(c);
    if (it == m_lastUpload.constEnd() && c.contains(QLatin1Char('/'))) {
        // IU8LMC/P carica come IU8LMC: la parte piu' lunga del nominativo.
        const QStringList parts = c.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        c = *std::max_element(parts.cbegin(), parts.cend(), [](const QString& a, const QString& b) { return a.size() < b.size(); });
        it = m_lastUpload.constFind(c);
    }
    return it != m_lastUpload.constEnd() && it->daysTo(today) <= days;
}

} // namespace decolog::core
