#include "core/DecodiumLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTimeZone>

namespace decolog::core::decodiumlog {

namespace {

QDateTime adifDateTime(const QString& date, const QString& time)
{
    const QDate d = QDate::fromString(date.trimmed().left(8), QStringLiteral("yyyyMMdd"));
    QString t = time.trimmed();
    if (t.size() == 4)
        t += QStringLiteral("00");
    const QTime h = QTime::fromString(t.left(6), QStringLiteral("HHmmss"));
    if (!d.isValid() || !h.isValid())
        return {};
    return QDateTime(d, h, QTimeZone::UTC);
}

} // namespace

QStringList candidates()
{
    QStringList paths;
    auto add = [&paths](const QString& path) {
        if (path.isEmpty())
            return;
        const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        if (!paths.contains(clean, Qt::CaseInsensitive) && QFileInfo(clean).isFile())
            paths << clean;
    };
    // Il log che Decodium sta usando adesso: si puo' averlo spostato o averne
    // piu' d'uno.
    const QSettings decodium(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"),
                             QStringLiteral("Decodium3"));
    add(decodium.value(QStringLiteral("Logbooks/ActivePath")).toString().trimmed());
    // Dove Decodium lo crea da solo: la cartella di prima, poi quella di Qt.
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    add(data + QStringLiteral("/Decodium/decodium_log.adi"));
    add(data + QStringLiteral("/Decodium/Decodium/decodium_log.adi"));
    return paths;
}

QDateTime loggedAt(const AdifRecord& record)
{
    const QString dateOn = record.value(QStringLiteral("QSO_DATE"));
    const QDateTime on = adifDateTime(dateOn, record.value(QStringLiteral("TIME_ON")));
    const QString timeOff = record.value(QStringLiteral("TIME_OFF"));
    if (timeOff.trimmed().isEmpty())
        return on;
    QString dateOff = record.value(QStringLiteral("QSO_DATE_OFF"));
    QDateTime off = adifDateTime(dateOff.trimmed().isEmpty() ? dateOn : dateOff, timeOff);
    // Senza la data di fine, una fine "prima" dell'inizio e' dopo mezzanotte.
    if (off.isValid() && on.isValid() && dateOff.trimmed().isEmpty() && off < on)
        off = off.addDays(1);
    return off.isValid() ? off : on;
}

Tail recent(const QString& path, const QDateTime& from, const QDateTime& to, qint64 firstChunk)
{
    Tail tail;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        tail.error = file.errorString();
        return tail;
    }
    tail.size = file.size();

    QList<AdifRecord> records;
    // Senza un "da quando" serve tutto il file.
    qint64 chunk = from.isValid() ? qMax<qint64>(4096, firstChunk) : tail.size;
    while (true) {
        const qint64 start = qMax<qint64>(0, tail.size - chunk);
        if (!file.seek(start)) {
            tail.error = file.errorString();
            return tail;
        }
        QByteArray data = file.read(tail.size - start);
        const QByteArray lower = data.toLower();
        const qsizetype last = lower.lastIndexOf("<eor>");
        // Il pezzo comincia a meta' di un record: si parte dopo il primo <eor>.
        const qsizetype first = start > 0 ? lower.indexOf("<eor>") : -1;
        if (last < 0 || (start > 0 && first == last)) {
            if (start == 0)
                break;   // nessun record intero nel file
            chunk *= 4;
            continue;
        }
        data.truncate(last + 5);
        if (start > 0)
            data.remove(0, first + 5);
        records = adif::parse(data).records;
        if (start == 0)
            break;
        // Basta se il QSO piu' vecchio letto e' gia' prima di "da quando".
        QDateTime oldest;
        for (const AdifRecord& r : std::as_const(records)) {
            const QDateTime at = loggedAt(r);
            if (at.isValid() && (!oldest.isValid() || at < oldest))
                oldest = at;
        }
        if (oldest.isValid() && oldest <= from)
            break;
        chunk *= 4;
    }

    for (const AdifRecord& r : std::as_const(records)) {
        const QDateTime at = loggedAt(r);
        if (!at.isValid() || (from.isValid() && at <= from) || (to.isValid() && at > to))
            continue;
        tail.records << r;
    }
    tail.ok = true;
    return tail;
}

} // namespace decolog::core::decodiumlog
