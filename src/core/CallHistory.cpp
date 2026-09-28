#include "core/CallHistory.h"

#include <QFile>

namespace decolog::core {

bool CallHistory::load(const QString& path, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    // Sono file vecchi e fatti a mano: UTF-8 se si capisce, se no Latin-1.
    const QByteArray data = f.readAll();
    QString text = QString::fromUtf8(data);
    if (text.contains(QChar::ReplacementCharacter))
        text = QString::fromLatin1(data);
    if (!loadText(text)) {
        if (error)
            *error = QStringLiteral("no !!Order!! line and no call in the file");
        return false;
    }
    m_path = path;
    return true;
}

bool CallHistory::loadText(const QString& text)
{
    clear();
    QStringList columns;
    for (QString line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QStringList parts = line.split(QLatin1Char(','));
        if (parts.first().trimmed().compare(QLatin1String("!!Order!!"), Qt::CaseInsensitive) == 0) {
            columns.clear();
            for (qsizetype i = 1; i < parts.size(); ++i)
                columns << parts.at(i).trimmed().toUpper();
            continue;
        }
        if (columns.isEmpty())
            continue;
        QHash<QString, QString> row;
        for (qsizetype i = 0; i < parts.size() && i < columns.size(); ++i) {
            const QString value = parts.at(i).trimmed();
            if (!columns.at(i).isEmpty() && !value.isEmpty())
                row.insert(columns.at(i), value);
        }
        const QString call = row.value(QStringLiteral("CALL")).toUpper();
        if (!call.isEmpty())
            m_rows.insert(call, row);
    }
    return !m_rows.isEmpty();
}

void CallHistory::clear()
{
    m_rows.clear();
    m_path.clear();
}

QHash<QString, QString> CallHistory::find(const QString& call) const
{
    return m_rows.value(call.trimmed().toUpper());
}

QString CallHistory::value(const QString& call, const QStringList& columns) const
{
    const auto row = find(call);
    for (const QString& c : columns) {
        const QString v = row.value(c.toUpper());
        if (!v.isEmpty())
            return v;
    }
    return {};
}

} // namespace decolog::core
