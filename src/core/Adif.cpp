#include "core/Adif.h"

#include <QStringDecoder>

namespace decolog::core {

AdifRecord::AdifRecord(std::initializer_list<AdifField> fields)
{
    for (const auto& f : fields)
        set(f.name, f.value);
}

QString AdifRecord::value(const QString& name) const
{
    const QString key = name.toUpper();
    for (const auto& f : m_fields) {
        if (f.name == key)
            return f.value;
    }
    return {};
}

bool AdifRecord::contains(const QString& name) const
{
    const QString key = name.toUpper();
    for (const auto& f : m_fields) {
        if (f.name == key)
            return true;
    }
    return false;
}

void AdifRecord::set(const QString& name, const QString& value)
{
    const QString key = name.toUpper();
    if (value.isEmpty()) {
        remove(key);
        return;
    }
    for (auto& f : m_fields) {
        if (f.name == key) {
            f.value = value;
            return;
        }
    }
    m_fields.append({key, value});
}

void AdifRecord::remove(const QString& name)
{
    const QString key = name.toUpper();
    m_fields.removeIf([&key](const AdifField& f) { return f.name == key; });
}

namespace adif {

namespace {

QString decode(const QByteArray& data)
{
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString text = utf8.decode(data);
    if (!utf8.hasError())
        return text;
    return QString::fromLatin1(data);
}

// Quanti caratteri occupano i primi `bytes` byte UTF-8 a partire da `from`.
// -1 se il conteggio cade a meta' di un carattere.
qsizetype charsForUtf8Bytes(const QString& text, qsizetype from, qsizetype bytes)
{
    qsizetype used = 0;
    qsizetype i = from;
    while (used < bytes && i < text.size()) {
        const QChar c = text.at(i);
        if (c.isHighSurrogate() && i + 1 < text.size()) {
            used += 4;
            i += 2;
        } else {
            const auto u = c.unicode();
            used += u < 0x80 ? 1 : (u < 0x800 ? 2 : 3);
            ++i;
        }
    }
    return used == bytes ? i - from : -1;
}

// Il valore finisce bene se dopo c'e' solo spazio e poi un nuovo tag, o niente.
bool endsCleanly(const QString& text, qsizetype pos)
{
    while (pos < text.size() && text.at(pos) != QLatin1Char('<')) {
        if (!text.at(pos).isSpace())
            return false;
        ++pos;
    }
    return true;
}

bool isNonAscii(const QString& s, qsizetype from, qsizetype len)
{
    const qsizetype end = qMin(from + len, s.size());
    for (qsizetype i = from; i < end; ++i) {
        if (s.at(i).unicode() >= 0x80)
            return true;
    }
    return false;
}

} // namespace

AdifDocument parse(const QByteArray& data)
{
    const QString text = decode(data);
    AdifDocument doc;
    AdifRecord current;

    // Tutto quello che precede il primo '<' e' testo libero d'intestazione. Se il
    // file comincia subito con un tag non c'e' intestazione.
    bool inHeader = !text.trimmed().startsWith(QLatin1Char('<'));

    qsizetype pos = 0;
    while (true) {
        const qsizetype open = text.indexOf(QLatin1Char('<'), pos);
        if (open < 0)
            break;
        const qsizetype close = text.indexOf(QLatin1Char('>'), open + 1);
        if (close < 0)
            break;

        const QString tag = text.mid(open + 1, close - open - 1);
        pos = close + 1;

        const QStringList parts = tag.split(QLatin1Char(':'));
        const QString name = parts.at(0).trimmed().toUpper();
        if (name.isEmpty())
            continue;

        if (parts.size() == 1) {
            if (name == QLatin1String("EOH")) {
                doc.header = current;
                current = {};
                inHeader = false;
            } else if (name == QLatin1String("EOR")) {
                if (!current.isEmpty())
                    doc.records.append(current);
                current = {};
            }
            continue;
        }

        bool ok = false;
        const qsizetype length = parts.at(1).trimmed().toLongLong(&ok);
        if (!ok || length < 0)
            continue;

        qsizetype take = qMin(length, text.size() - pos);
        // Lunghezza in caratteri o in byte? Con solo ASCII coincidono. Altrimenti
        // si sceglie l'interpretazione dopo la quale il file prosegue con un tag.
        // Se vanno bene tutte e due (fra valore e tag c'e' spazio) e differiscono
        // solo per spazi in coda, vince la piu' corta: un nome non finisce con
        // uno spazio.
        if (isNonAscii(text, pos, take)) {
            const qsizetype byBytes = charsForUtf8Bytes(text, pos, length);
            const bool charsClean = endsCleanly(text, pos + take);
            const bool bytesClean = byBytes >= 0 && endsCleanly(text, pos + byBytes);
            if (bytesClean && (!charsClean || (byBytes < take && text.mid(pos + byBytes, take - byBytes).trimmed().isEmpty())))
                take = byBytes;
        }

        const QString value = text.mid(pos, take);
        pos += take;

        if (!value.isEmpty())
            current.set(name, value);
    }

    // Un record senza <EOR> finale (capita nei messaggi UDP troncati) vale lo
    // stesso: meglio un QSO in piu' da controllare che uno perso.
    if (!current.isEmpty()) {
        if (inHeader)
            doc.header = current;
        else
            doc.records.append(current);
    }
    return doc;
}

QString writeRecord(const AdifRecord& record)
{
    QString out;
    for (const auto& f : record.fields()) {
        if (!out.isEmpty())
            out += QLatin1Char(' ');
        out += QStringLiteral("<%1:%2>%3").arg(f.name.toLower()).arg(f.value.size()).arg(f.value);
    }
    out += QStringLiteral(" <eor>");
    return out;
}

QByteArray writeDocument(const AdifDocument& document)
{
    QString out = QStringLiteral("DecoLog ADIF export\n");
    for (const auto& f : document.header.fields())
        out += QStringLiteral("<%1:%2>%3\n").arg(f.name.toLower()).arg(f.value.size()).arg(f.value);
    out += QStringLiteral("<eoh>\n\n");
    for (const auto& r : document.records)
        out += writeRecord(r) + QLatin1Char('\n');
    return out.toUtf8();
}

void normalizeMode(AdifRecord& record)
{
    const QString mode = record.value(QStringLiteral("MODE")).trimmed().toUpper();
    static const QStringList mfskSubmodes{
        QStringLiteral("FT2"), QStringLiteral("FT4"), QStringLiteral("FST4"),
        QStringLiteral("FST4W"), QStringLiteral("Q65")};
    if (mfskSubmodes.contains(mode)) {
        record.set(QStringLiteral("MODE"), QStringLiteral("MFSK"));
        if (record.value(QStringLiteral("SUBMODE")).isEmpty())
            record.set(QStringLiteral("SUBMODE"), mode);
    }
}

} // namespace adif
} // namespace decolog::core
