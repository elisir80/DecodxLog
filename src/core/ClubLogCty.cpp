#include "core/ClubLogCty.h"

#include <QFile>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <zlib.h>

namespace decolog::core {

namespace {

QDateTime parseDate(const QString& text)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return {};
    QDateTime d = QDateTime::fromString(t, Qt::ISODate);
    return d.isValid() ? d.toUTC() : QDateTime();
}

// I suffissi che non cambiano l'entita'.
bool neutralSuffix(const QString& part)
{
    static const QStringList suffixes{QStringLiteral("P"),   QStringLiteral("M"),   QStringLiteral("QRP"),
                                      QStringLiteral("A"),   QStringLiteral("B"),   QStringLiteral("LH"),
                                      QStringLiteral("J"),   QStringLiteral("R"),   QStringLiteral("T"),
                                      QStringLiteral("QRPP"), QStringLiteral("LGT"), QStringLiteral("SOTA")};
    return suffixes.contains(part);
}

} // namespace

bool ClubLogCty::Rule::covers(const QDateTime& when) const
{
    if (!when.isValid())
        return !end.isValid();   // senza data: quello che vale oggi
    if (start.isValid() && when < start)
        return false;
    if (end.isValid() && when > end)
        return false;
    return true;
}

QByteArray ClubLogCty::gunzip(const QByteArray& gz, QString* error)
{
    if (gz.isEmpty())
        return {};
    z_stream s{};
    // 16 + MAX_WBITS: intestazione gzip.
    if (inflateInit2(&s, 16 + MAX_WBITS) != Z_OK) {
        if (error)
            *error = QStringLiteral("zlib");
        return {};
    }
    QByteArray out;
    char buffer[65536];
    s.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(gz.constData()));
    s.avail_in = static_cast<uInt>(gz.size());
    int status = Z_OK;
    do {
        s.next_out = reinterpret_cast<Bytef*>(buffer);
        s.avail_out = sizeof(buffer);
        status = inflate(&s, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) {
            inflateEnd(&s);
            if (error)
                *error = QStringLiteral("not a gzip file");
            return {};
        }
        out.append(buffer, static_cast<qsizetype>(sizeof(buffer) - s.avail_out));
    } while (status != Z_STREAM_END && s.avail_in > 0);
    inflateEnd(&s);
    return out;
}

bool ClubLogCty::loadFile(const QString& path, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    QByteArray data = f.readAll();
    // gzip comincia con 1f 8b.
    if (data.size() > 2 && uchar(data.at(0)) == 0x1f && uchar(data.at(1)) == 0x8b)
        data = gunzip(data, error);
    return !data.isEmpty() && load(data, error);
}

bool ClubLogCty::load(const QByteArray& xml, QString* error)
{
    m_entities.clear();
    m_exceptions.clear();
    m_prefixes.clear();
    m_invalid.clear();
    m_zones.clear();
    m_longestPrefix = 0;
    m_date = {};

    QXmlStreamReader reader(xml);
    // Un elemento con i suoi figli semplici: <call>, <adif>, <start>...
    auto readFields = [&reader]() {
        QHash<QString, QString> f;
        const QString self = reader.name().toString();
        while (reader.readNextStartElement())
            f.insert(reader.name().toString(), reader.readElementText(QXmlStreamReader::SkipChildElements));
        Q_UNUSED(self);
        return f;
    };
    auto ruleFrom = [](const QHash<QString, QString>& f) {
        Rule r;
        r.adif = f.value(QStringLiteral("adif")).toInt();
        r.entity = f.value(QStringLiteral("entity"));
        r.cqz = f.value(QStringLiteral("cqz")).toInt();
        if (r.cqz == 0)
            r.cqz = f.value(QStringLiteral("zone")).toInt();
        r.cont = f.value(QStringLiteral("cont"));
        r.start = parseDate(f.value(QStringLiteral("start")));
        r.end = parseDate(f.value(QStringLiteral("end")));
        return r;
    };

    if (!reader.readNextStartElement() || reader.name() != QLatin1String("clublog")) {
        if (error)
            *error = QStringLiteral("not a Club Log cty.xml");
        return false;
    }
    m_date = parseDate(reader.attributes().value(QStringLiteral("date")).toString());
    while (reader.readNextStartElement()) {
        const QString section = reader.name().toString();
        if (section == QLatin1String("entities")) {
            while (reader.readNextStartElement()) {
                const auto f = readFields();
                CtyEntity e;
                e.adif = f.value(QStringLiteral("adif")).toInt();
                e.name = f.value(QStringLiteral("name"));
                e.prefix = f.value(QStringLiteral("prefix"));
                e.deleted = f.value(QStringLiteral("deleted")).compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0;
                e.cqz = f.value(QStringLiteral("cqz")).toInt();
                e.cont = f.value(QStringLiteral("cont"));
                e.start = parseDate(f.value(QStringLiteral("start")));
                e.end = parseDate(f.value(QStringLiteral("end")));
                if (e.adif > 0)
                    m_entities.insert(e.adif, e);
            }
        } else if (section == QLatin1String("exceptions") || section == QLatin1String("prefixes")
                   || section == QLatin1String("invalid_operations") || section == QLatin1String("zone_exceptions")) {
            auto& target = section == QLatin1String("exceptions")         ? m_exceptions
                         : section == QLatin1String("prefixes")           ? m_prefixes
                         : section == QLatin1String("invalid_operations") ? m_invalid
                                                                          : m_zones;
            while (reader.readNextStartElement()) {
                const auto f = readFields();
                const QString call = f.value(QStringLiteral("call")).trimmed().toUpper();
                if (call.isEmpty())
                    continue;
                target[call] << ruleFrom(f);
                if (&target == &m_prefixes)
                    m_longestPrefix = qMax(m_longestPrefix, static_cast<int>(call.size()));
            }
        } else {
            reader.skipCurrentElement();
        }
    }
    if (reader.hasError() && m_entities.isEmpty()) {
        if (error)
            *error = reader.errorString();
        return false;
    }
    if (m_entities.isEmpty()) {
        if (error)
            *error = QStringLiteral("no entities in the file");
        return false;
    }
    return true;
}

const CtyEntity* ClubLogCty::entity(int adif) const
{
    const auto it = m_entities.constFind(adif);
    return it == m_entities.constEnd() ? nullptr : &it.value();
}

const ClubLogCty::Rule* ClubLogCty::matchRule(const QList<Rule>& rules, const QDateTime& when) const
{
    for (const Rule& r : rules) {
        if (r.covers(when))
            return &r;
    }
    return nullptr;
}

CtyMatch ClubLogCty::fromRule(const Rule& r) const
{
    CtyMatch m;
    m.found = r.adif > 0;
    m.adif = r.adif;
    m.cqz = r.cqz;
    m.cont = r.cont;
    m.name = r.entity;
    if (const CtyEntity* e = entity(r.adif)) {
        if (m.name.isEmpty())
            m.name = e->name;
        if (m.cqz == 0)
            m.cqz = e->cqz;
        if (m.cont.isEmpty())
            m.cont = e->cont;
        m.deleted = e->deleted;
    }
    return m;
}

CtyMatch ClubLogCty::lookup(const QString& callsign, const QDateTime& when) const
{
    CtyMatch none;
    const QString call = callsign.trimmed().toUpper();
    if (call.isEmpty() || m_entities.isEmpty())
        return none;

    // Le operazioni che non valgono e le eccezioni: il nominativo esatto.
    if (const Rule* r = matchRule(m_invalid.value(call), when)) {
        Q_UNUSED(r);
        none.invalid = true;
        return none;
    }
    CtyMatch match;
    if (const Rule* r = matchRule(m_exceptions.value(call), when)) {
        match = fromRule(*r);
    } else {
        // Il pezzo del nominativo che dice l'entita': "EA8/OH2XX" → EA8,
        // "OH2XX/EA8" → EA8, "W1AW/4" e "IU8LMC/P" → il nominativo stesso.
        QStringList parts = call.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        for (const QString& p : std::as_const(parts)) {
            if (p == QLatin1String("MM") || p == QLatin1String("AM")) {
                none.noEntity = true;
                return none;
            }
        }
        parts.erase(std::remove_if(parts.begin(), parts.end(),
                                   [](const QString& p) {
                                       static const QRegularExpression digit(QStringLiteral("^\\d$"));
                                       return neutralSuffix(p) || digit.match(p).hasMatch();
                                   }),
                    parts.end());
        QString key = call;
        if (parts.size() == 1) {
            key = parts.first();
        } else if (parts.size() >= 2) {
            // Il prefisso e' il pezzo piu' corto.
            key = parts.first().size() <= parts.last().size() ? parts.first() : parts.last();
        }
        for (int len = qMin(static_cast<int>(key.size()), m_longestPrefix); len > 0; --len) {
            const auto it = m_prefixes.constFind(key.left(len));
            if (it == m_prefixes.constEnd())
                continue;
            if (const Rule* r = matchRule(it.value(), when)) {
                match = fromRule(*r);
                break;
            }
        }
    }
    if (!match.found)
        return none;
    // Le zone CQ che cambiano per un nominativo (grandi paesi, isole).
    if (const Rule* z = matchRule(m_zones.value(call), when))
        match.cqz = z->cqz;
    return match;
}

} // namespace decolog::core
