#include "core/SuperCheck.h"

#include <QRegularExpression>
#include <algorithm>

namespace decolog::core {

namespace {

bool plausibleCall(const QString& c)
{
    static const QRegularExpression re(QStringLiteral("^[A-Z0-9/]{3,15}$"));
    return re.match(c).hasMatch() && std::any_of(c.cbegin(), c.cend(), [](QChar ch) { return ch.isDigit(); });
}

} // namespace

int SuperCheck::load(const QByteArray& scp)
{
    int added = 0;
    for (const QByteArray& raw : scp.split('\n')) {
        const QString line = QString::fromLatin1(raw).trimmed().toUpper();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (plausibleCall(line) && !m_set.contains(line)) {
            m_set.insert(line);
            ++added;
        }
    }
    m_dirty = true;
    rebuild();
    return added;
}

void SuperCheck::addCalls(const QStringList& calls)
{
    for (const QString& c : calls) {
        const QString call = c.trimmed().toUpper();
        if (plausibleCall(call))
            m_set.insert(call);
    }
    m_dirty = true;
    rebuild();
}

void SuperCheck::clear()
{
    m_set.clear();
    m_calls.clear();
}

void SuperCheck::rebuild()
{
    if (!m_dirty)
        return;
    m_calls = QStringList(m_set.cbegin(), m_set.cend());
    m_calls.sort();
    m_dirty = false;
}

QStringList SuperCheck::partial(const QString& fragment, int limit) const
{
    const QString f = fragment.trimmed().toUpper();
    QStringList out;
    if (f.size() < 2)
        return out;
    const bool wild = f.contains(QLatin1Char('?'));
    QRegularExpression re;
    if (wild)
        re.setPattern(QRegularExpression::escape(f).replace(QStringLiteral("\\?"), QStringLiteral(".")));
    for (const QString& c : m_calls) {
        if (wild ? re.match(c).hasMatch() : c.contains(f))
            out << c;
    }
    // Prima quelli che cominciano cosi', poi i piu' corti: e' li' che sta di
    // solito il nominativo giusto.
    std::stable_sort(out.begin(), out.end(), [&f](const QString& a, const QString& b) {
        const bool pa = a.startsWith(f), pb = b.startsWith(f);
        if (pa != pb)
            return pa;
        return a.size() < b.size();
    });
    if (limit > 0 && out.size() > limit)
        out = out.mid(0, limit);
    return out;
}

bool SuperCheck::oneEditApart(const QString& a, const QString& b)
{
    const qsizetype la = a.size(), lb = b.size();
    if (std::abs(la - lb) > 1 || a == b)
        return false;
    if (la == lb) {
        int diff = 0;
        for (qsizetype i = 0; i < la; ++i)
            if (a.at(i) != b.at(i) && ++diff > 1)
                return false;
        return diff == 1;
    }
    const QString& s = la < lb ? a : b;     // il piu' corto
    const QString& l = la < lb ? b : a;
    qsizetype i = 0, j = 0;
    bool skipped = false;
    while (i < s.size() && j < l.size()) {
        if (s.at(i) == l.at(j)) {
            ++i;
            ++j;
        } else {
            if (skipped)
                return false;
            skipped = true;
            ++j;
        }
    }
    return true;
}

QStringList SuperCheck::nPlusOne(const QString& call, int limit) const
{
    const QString c = call.trimmed().toUpper();
    QStringList out;
    if (c.size() < 3)
        return out;
    for (const QString& x : m_calls) {
        if (oneEditApart(c, x)) {
            out << x;
            if (limit > 0 && out.size() >= limit)
                break;
        }
    }
    return out;
}

} // namespace decolog::core
