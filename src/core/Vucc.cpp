#include "core/Vucc.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

namespace decolog::core::vucc {

namespace {
QString norm(const QString& value) { return value.trimmed().toUpper(); }
}

bool validGrid4(const QString& grid)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-R]{2}[0-9]{2}$"));
    return pattern.match(norm(grid)).hasMatch();
}

bool Result::isValid(QString* error) const
{
    if (rules.target < 1 || rules.target > 10000) {
        if (error) *error = QStringLiteral("invalid VUCC target");
        return false;
    }
    if (rules.band.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("VUCC band is required");
        return false;
    }
    return true;
}

Result select(const QList<Entry>& entries, const Rules& rules)
{
    Result result;
    result.rules = rules;
    QString error;
    if (!result.isValid(&error)) {
        result.exclusions << error;
        return result;
    }
    QSet<QString> grids;
    QList<Entry> sorted = entries;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Entry& a, const Entry& b) {
        return a.qsoId < b.qsoId;
    });
    for (const Entry& entry : sorted) {
        const QString grid = norm(entry.grid).left(4);
        if (entry.qsoId <= 0 || !validGrid4(grid)) {
            result.exclusions << QStringLiteral("QSO %1: invalid GRID4").arg(entry.qsoId);
            continue;
        }
        if (norm(entry.band) != norm(rules.band)) {
            result.exclusions << QStringLiteral("QSO %1: band mismatch").arg(entry.qsoId);
            continue;
        }
        if (!rules.account.trimmed().isEmpty() && norm(entry.account) != norm(rules.account)) {
            result.exclusions << QStringLiteral("QSO %1: account mismatch").arg(entry.qsoId);
            continue;
        }
        if (!rules.site.trimmed().isEmpty() && norm(entry.site) != norm(rules.site)) {
            result.exclusions << QStringLiteral("QSO %1: operating site mismatch").arg(entry.qsoId);
            continue;
        }
        if (!entry.confirmed || entry.evidence.trimmed().isEmpty()) {
            result.exclusions << QStringLiteral("QSO %1: confirmation evidence required").arg(entry.qsoId);
            continue;
        }
        if (grids.contains(grid)) {
            result.exclusions << QStringLiteral("QSO %1: GRID4 already credited").arg(entry.qsoId);
            continue;
        }
        Entry credited = entry;
        credited.grid = grid;
        result.eligible << credited;
        grids.insert(grid);
    }
    return result;
}

QList<Entry> loadEntries(const LogDatabase& db, QString* error)
{
    QList<Entry> entries;
    QSqlQuery query(db.connection());
    query.setForwardOnly(true);
    if (!query.exec(QStringLiteral(
            "SELECT q.id, q.gridsquare, q.band, IFNULL(p.award_account, ''), "
            "IFNULL(p.operating_location, ''), "
            "EXISTS(SELECT 1 FROM qsl_status s WHERE s.qso_id = q.id AND s.rcvd = 'Y'), "
            "(SELECT group_concat(s.service, ',') FROM qsl_status s WHERE s.qso_id = q.id AND s.rcvd = 'Y') "
            "FROM qso q LEFT JOIN station_profile p ON p.id = q.station_profile_id "
            "WHERE q.deleted = 0 AND IFNULL(q.gridsquare, '') <> '' ORDER BY q.id"))) {
        if (error) *error = query.lastError().text();
        return {};
    }
    while (query.next()) {
        Entry entry;
        entry.qsoId = query.value(0).toLongLong();
        entry.grid = query.value(1).toString();
        entry.band = query.value(2).toString();
        entry.account = query.value(3).toString();
        entry.site = query.value(4).toString();
        entry.confirmed = query.value(5).toBool();
        entry.evidence = query.value(6).toString();
        entries << entry;
    }
    return entries;
}

QJsonDocument Result::toJson() const
{
    QJsonArray rows;
    for (const Entry& entry : eligible) {
        rows.append(QJsonObject{{QStringLiteral("qsoId"), static_cast<double>(entry.qsoId)},
                                {QStringLiteral("grid4"), entry.grid},
                                {QStringLiteral("band"), entry.band},
                                {QStringLiteral("account"), entry.account},
                                {QStringLiteral("site"), entry.site},
                                {QStringLiteral("evidence"), entry.evidence}});
    }
    QJsonArray exclusions;
    for (const auto& value : this->exclusions) exclusions.append(value);
    return QJsonDocument(QJsonObject{{QStringLiteral("format"), QStringLiteral("decodxlog-vucc-1")},
                                     {QStringLiteral("officialRulesVerified"), officialRulesVerified},
                                     {QStringLiteral("account"), rules.account},
                                     {QStringLiteral("site"), rules.site},
                                     {QStringLiteral("band"), rules.band},
                                     {QStringLiteral("target"), rules.target},
                                     {QStringLiteral("eligible"), rows},
                                     {QStringLiteral("exclusions"), exclusions}});
}

} // namespace decolog::core::vucc
