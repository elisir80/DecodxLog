#include "core/Dates.h"

#include <QDate>
#include <QRegularExpression>
#include <QStringList>

namespace decolog::core::dates {

namespace {

QString g_separator;   // vuoto: anno-mese-giorno

QString convert(const QString& text, bool shortYear)
{
    const QString t = text.trimmed();
    QDate date;
    QString rest;
    if (t.size() >= 10 && t.at(4) == QLatin1Char('-') && t.at(7) == QLatin1Char('-')) {
        date = QDate::fromString(t.left(10), QStringLiteral("yyyy-MM-dd"));
        rest = t.mid(10);
    } else if (t.size() == 8) {
        date = QDate::fromString(t, QStringLiteral("yyyyMMdd"));
    }
    if (!date.isValid())
        return text;
    // "T" fra data e ora e' per le macchine: a chi legge basta uno spazio.
    if (rest.startsWith(QLatin1Char('T')))
        rest[0] = QLatin1Char(' ');
    return date.toString(shortYear ? shortFormat() : format()) + rest;
}

} // namespace

void setLanguage(const QString& language)
{
    const QString l = language.section(QLatin1Char('_'), 0, 0).toLower();
    if (l == QLatin1String("it") || l == QLatin1String("fr") || l == QLatin1String("es")
        || l == QLatin1String("ca"))
        g_separator = QStringLiteral("/");
    else if (l == QLatin1String("de") || l == QLatin1String("ru") || l == QLatin1String("da")
             || l == QLatin1String("lv") || l == QLatin1String("ro"))
        g_separator = QStringLiteral(".");
    else if (l == QLatin1String("nl"))
        g_separator = QStringLiteral("-");
    else
        g_separator.clear();
}

bool dayFirst()
{
    return !g_separator.isEmpty();
}

QString format()
{
    return dayFirst() ? QStringLiteral("dd%1MM%1yyyy").arg(g_separator) : QStringLiteral("yyyy-MM-dd");
}

QString shortFormat()
{
    return dayFirst() ? QStringLiteral("dd%1MM%1yy").arg(g_separator) : QStringLiteral("yy-MM-dd");
}

QString show(const QString& text)
{
    return convert(text, false);
}

QString showShort(const QString& text)
{
    return convert(text, true);
}

QString read(const QString& text)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return {};
    static const QRegularExpression digitsOnly(QStringLiteral(R"(^\d{8}$)"));
    if (digitsOnly.match(t).hasMatch()) {
        // 8 cifre: ADIF (20260925); se non e' valida cosi', giorno-mese-anno
        // scritto senza separatori (25092026).
        QDate d = QDate::fromString(t, QStringLiteral("yyyyMMdd"));
        if (!d.isValid())
            d = QDate::fromString(t, QStringLiteral("ddMMyyyy"));
        return d.isValid() ? d.toString(Qt::ISODate) : QString();
    }
    static const QRegularExpression parts(QStringLiteral(R"(^(\d{1,4})\s*[-./\s]\s*(\d{1,2})\s*[-./\s]\s*(\d{1,4})$)"));
    const QRegularExpressionMatch m = parts.match(t);
    if (!m.hasMatch())
        return {};
    int y, mo, d;
    if (m.captured(1).size() == 4) {
        y = m.captured(1).toInt();
        mo = m.captured(2).toInt();
        d = m.captured(3).toInt();
    } else {
        // Scritta col giorno davanti: e' la forma di chi non parte dall'anno.
        d = m.captured(1).toInt();
        mo = m.captured(2).toInt();
        y = m.captured(3).toInt();
        if (m.captured(3).size() <= 2)
            y += 2000;
        else if (m.captured(3).size() == 3)
            return {};
    }
    const QDate date(y, mo, d);
    return date.isValid() ? date.toString(Qt::ISODate) : QString();
}

} // namespace decolog::core::dates
