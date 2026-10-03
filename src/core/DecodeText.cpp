#include "core/DecodeText.h"

#include <QRegularExpression>
#include <QStringList>

namespace decolog::core::decodetext {

namespace {

QStringList tokens(const QString& message)
{
    QStringList out;
    for (QString t : message.toUpper().split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        // I nominativi con hash: <K1ABC>. I segnaposto <...> non sono nominativi.
        if (t.startsWith(QLatin1Char('<')) && t.endsWith(QLatin1Char('>')))
            t = t.mid(1, t.size() - 2);
        out << t;
    }
    return out;
}

bool isGrid(const QString& t)
{
    static const QRegularExpression re(QStringLiteral("^[A-R]{2}[0-9]{2}([A-X]{2})?$"));
    // RR73 ha la forma di un locatore ma e' una chiusura.
    return t != QLatin1String("RR73") && re.match(t).hasMatch();
}

} // namespace

bool looksLikeCall(const QString& token)
{
    const QString t = token.toUpper();
    if (t.size() < 3 || t.size() > 13)
        return false;
    static const QRegularExpression re(QStringLiteral("^([A-Z0-9]{1,4}/)?[A-Z0-9]{1,3}[0-9][A-Z0-9]{0,3}[A-Z](/[A-Z0-9]{1,4})?$"));
    if (!re.match(t).hasMatch())
        return false;
    // Un rapporto o un locatore non sono nominativi: R-05, 73, JN70.
    return !isGrid(t) && t != QLatin1String("RR73");
}

QString baseCall(const QString& call)
{
    const QStringList parts = call.toUpper().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() <= 1)
        return call.toUpper();
    // Il pezzo che somiglia di piu' a un nominativo intero: il piu' lungo con
    // una cifra ("EA8/IU8LMC/P" -> IU8LMC).
    QString best;
    for (const QString& p : parts) {
        if (p.size() > best.size() && looksLikeCall(p))
            best = p;
    }
    return best.isEmpty() ? parts.first() : best;
}

Parts parse(const QString& message)
{
    Parts out;
    const QStringList t = tokens(message);
    if (t.isEmpty())
        return out;
    // L'ultimo pezzo che e' un locatore.
    for (int i = t.size() - 1; i >= 1; --i) {
        if (isGrid(t.at(i))) {
            out.grid = t.at(i);
            break;
        }
    }
    const QString first = t.first();
    if (first == QLatin1String("CQ") || first == QLatin1String("QRZ") || first == QLatin1String("DE")) {
        out.cq = true;
        for (int i = 1; i < t.size() && i <= 3; ++i) {
            if (looksLikeCall(t.at(i))) {
                out.from = t.at(i);
                break;
            }
            // "CQ DX", "CQ POTA", "CQ NA": lettere senza cifre, fino a quattro.
            if (i == 1 && t.at(i).size() <= 4)
                out.modifier = t.at(i);
        }
        return out;
    }
    if (t.size() >= 2 && looksLikeCall(t.at(1))) {
        out.from = t.at(1);
        if (looksLikeCall(first) || first.contains(QLatin1Char('/')))
            out.to = first;
    }
    return out;
}

bool mentions(const QString& message, const QString& call)
{
    const QString c = call.trimmed().toUpper();
    if (c.isEmpty())
        return false;
    const QString base = baseCall(c);
    for (const QString& t : tokens(message)) {
        if (t == c || (!base.isEmpty() && baseCall(t) == base && looksLikeCall(t)))
            return true;
    }
    return false;
}

} // namespace decolog::core::decodetext
