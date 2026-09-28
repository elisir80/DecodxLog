// decodxlog_benchlog — quanto regge DecoDXLog su un log grande.
//
//   decodxlog_benchlog --qsos 100000 --out big.sqlite     crea un log finto e lo misura
//   decodxlog_benchlog --db big.sqlite                     misura un log che c'e' gia'
//
// Il log finto somiglia a uno vero: prefissi pesati come si lavorano da qui
// (tanta Europa, un po' di tutto il resto), vent'anni di date, le bande e i modi
// nelle proporzioni di un log FT8 e CW, conferme LoTW, cartolina ed eQSL, un po'
// di IOTA e POTA. Si importa con lo stesso importAdif del programma: anche
// l'importazione e' una misura. Poi si misurano le cose che il programma fa
// all'avvio, a ogni QSO, e quando si apre una finestra.
#include "app/QsoTableModel.h"
#include "core/Awards.h"
#include "core/LogBackup.h"
#include "core/LogDatabase.h"
#include "core/Spots.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QTextStream>
#include <QTimeZone>

#include <functional>
#include <iterator>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

using namespace decolog::core;

namespace {

QTextStream& out()
{
    static QTextStream s(stdout);
    return s;
}

double memoryMb()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.WorkingSetSize / 1048576.0;
#endif
    return 0;
}

// Una misura: il tempo, e la memoria dopo.
template <typename F>
auto measure(const char* what, F&& work)
{
    QElapsedTimer t;
    t.start();
    auto result = work();
    const qint64 ms = t.elapsed();
    out() << QStringLiteral("%1 %2 ms   mem %3 MB")
                 .arg(QLatin1String(what), -44)
                 .arg(ms, 8)
                 .arg(memoryMb(), 7, 'f', 0)
          << Qt::endl;
    return result;
}

struct Entity {
    const char* prefix;
    int dxcc;
    const char* cont;
    int cqz;
    int ituz;
    int weight;
};

// Quello che si lavora da IU8: piu' Europa, poi il mondo.
const Entity kEntities[] = {
    {"DL", 230, "EU", 14, 28, 60}, {"I", 248, "EU", 15, 28, 50}, {"F", 227, "EU", 14, 27, 35},
    {"EA", 281, "EU", 14, 37, 35}, {"G", 223, "EU", 14, 27, 30}, {"SP", 269, "EU", 15, 28, 25},
    {"OK", 503, "EU", 15, 28, 20}, {"OH", 224, "EU", 15, 18, 15}, {"SM", 284, "EU", 14, 18, 15},
    {"PA", 263, "EU", 14, 27, 20}, {"ON", 209, "EU", 14, 27, 15}, {"HA", 239, "EU", 15, 28, 15},
    {"YO", 275, "EU", 20, 28, 12}, {"UR", 288, "EU", 16, 29, 20}, {"UA", 54, "EU", 16, 29, 30},
    {"LY", 146, "EU", 15, 29, 8},  {"YL", 145, "EU", 15, 29, 6},  {"S5", 499, "EU", 15, 28, 8},
    {"9A", 497, "EU", 15, 28, 10}, {"LZ", 212, "EU", 20, 28, 10}, {"SV", 236, "EU", 20, 28, 8},
    {"CT", 272, "EU", 14, 37, 8},  {"OE", 206, "EU", 15, 28, 10}, {"HB", 287, "EU", 14, 28, 8},
    {"EI", 245, "EU", 14, 27, 5},  {"OZ", 221, "EU", 14, 18, 8},  {"LA", 266, "EU", 14, 18, 6},
    {"IS0", 225, "EU", 15, 28, 3}, {"TK", 214, "EU", 15, 28, 2},  {"EA8", 29, "AF", 33, 36, 4},
    {"K", 291, "NA", 5, 8, 40},    {"W", 291, "NA", 4, 7, 30},    {"VE", 1, "NA", 4, 9, 10},
    {"XE", 50, "NA", 6, 10, 3},    {"KP4", 202, "NA", 8, 11, 2},  {"PY", 108, "SA", 11, 15, 10},
    {"LU", 100, "SA", 13, 14, 6},  {"CE", 112, "SA", 12, 14, 3},  {"HK", 116, "SA", 9, 12, 2},
    {"JA", 339, "AS", 25, 45, 20}, {"BY", 318, "AS", 24, 44, 5},  {"HL", 137, "AS", 25, 44, 4},
    {"VU", 324, "AS", 22, 41, 4},  {"4X", 336, "AS", 20, 39, 3},  {"UA9", 15, "AS", 17, 30, 6},
    {"VK", 150, "OC", 30, 59, 6},  {"ZL", 170, "OC", 32, 60, 3},  {"YB", 327, "OC", 28, 51, 3},
    {"ZS", 462, "AF", 38, 57, 4},  {"CN", 446, "AF", 33, 37, 3},  {"5Z", 430, "AF", 37, 48, 1},
    {"SU", 478, "AF", 34, 38, 2},  {"3B8", 165, "AF", 39, 53, 1}, {"VP8", 141, "SA", 13, 16, 1},
};

const char* const kStates[] = {"CA", "TX", "NY", "FL", "OH", "MA", "WA", "IL", "PA", "MI", "GA", "CO",
                               "AZ", "NC", "VA", "MN", "WI", "OR", "NJ", "MD", "TN", "IN", "MO", "CT"};

struct Weighted {
    const char* value;
    int weight;
};
const Weighted kBands[] = {{"20m", 30}, {"40m", 18}, {"15m", 12}, {"10m", 10}, {"80m", 8}, {"17m", 6},
                           {"30m", 6},  {"12m", 4},  {"6m", 3},   {"160m", 2}, {"60m", 1}};
const Weighted kModes[] = {{"FT8", 45}, {"CW", 20}, {"SSB", 20}, {"FT4", 7}, {"FT2", 5}, {"RTTY", 3}};

template <typename T, size_t N>
const T& pick(const T (&list)[N], int (*weightOf)(const T&), QRandomGenerator& rng)
{
    int total = 0;
    for (const T& e : list)
        total += weightOf(e);
    int r = static_cast<int>(rng.bounded(total));
    for (const T& e : list) {
        r -= weightOf(e);
        if (r < 0)
            return e;
    }
    return list[0];
}

QString field(const char* name, const QString& value)
{
    return QStringLiteral("<%1:%2>%3 ").arg(QLatin1String(name)).arg(value.toUtf8().size()).arg(value);
}

QString bandFreq(const QString& band)
{
    static const QHash<QString, QString> f{
        {QStringLiteral("160m"), QStringLiteral("1.840")}, {QStringLiteral("80m"), QStringLiteral("3.573")},
        {QStringLiteral("60m"), QStringLiteral("5.357")},  {QStringLiteral("40m"), QStringLiteral("7.074")},
        {QStringLiteral("30m"), QStringLiteral("10.136")}, {QStringLiteral("20m"), QStringLiteral("14.074")},
        {QStringLiteral("17m"), QStringLiteral("18.100")}, {QStringLiteral("15m"), QStringLiteral("21.074")},
        {QStringLiteral("12m"), QStringLiteral("24.915")}, {QStringLiteral("10m"), QStringLiteral("28.074")},
        {QStringLiteral("6m"), QStringLiteral("50.313")}};
    return f.value(band);
}

// Un pezzo di log finto in ADIF: `count` QSO dal numero `first`.
QByteArray syntheticAdif(int first, int count, QRandomGenerator& rng)
{
    QString text = QStringLiteral("DecoDXLog benchmark\n<ADIF_VER:5>3.1.4\n<EOH>\n");
    text.reserve(count * 420);
    const QDateTime start(QDate(2006, 1, 1), QTime(0, 0), QTimeZone::UTC);
    const qint64 span = start.secsTo(QDateTime(QDate(2026, 9, 1), QTime(0, 0), QTimeZone::UTC));
    for (int i = 0; i < count; ++i) {
        const Entity& e = pick(kEntities, +[](const Entity& x) { return x.weight; }, rng);
        const int serial = first + i;
        // Il suffisso viene dal numero del QSO: nominativi ripetuti quanto basta
        // (un log vero lavora le stesse stazioni piu' volte), mai uguali per caso
        // nello stesso minuto.
        QString suffix;
        int n = static_cast<int>(rng.bounded(26 * 26 * 26));
        for (int k = 0; k < 1 + static_cast<int>(rng.bounded(3)); ++k) {
            suffix += QChar(QLatin1Char('A' + n % 26));
            n /= 26;
        }
        const QString call = QLatin1String(e.prefix) + QString::number(rng.bounded(10)) + suffix;
        const QDateTime when = start.addSecs(static_cast<qint64>(rng.bounded(static_cast<quint32>(span / 60))) * 60
                                             + serial % 60);
        const QString band = QLatin1String(pick(kBands, +[](const Weighted& x) { return x.weight; }, rng).value);
        const QString mode = QLatin1String(pick(kModes, +[](const Weighted& x) { return x.weight; }, rng).value);
        const bool digital = mode.startsWith(QLatin1String("FT")) || mode == QLatin1String("RTTY");
        QString r = field("CALL", call) + field("QSO_DATE", when.toString(QStringLiteral("yyyyMMdd")))
                    + field("TIME_ON", when.toString(QStringLiteral("HHmmss"))) + field("BAND", band)
                    + field("FREQ", bandFreq(band));
        if (mode == QLatin1String("FT2") || mode == QLatin1String("FT4"))
            r += field("MODE", QStringLiteral("MFSK")) + field("SUBMODE", mode);
        else
            r += field("MODE", mode);
        r += field("RST_SENT", digital ? QString::number(-20 + int(rng.bounded(30))) : QStringLiteral("599"));
        r += field("RST_RCVD", digital ? QString::number(-20 + int(rng.bounded(30))) : QStringLiteral("599"));
        r += field("DXCC", QString::number(e.dxcc)) + field("CONT", QLatin1String(e.cont))
             + field("CQZ", QString::number(e.cqz)) + field("ITUZ", QString::number(e.ituz));
        const QString grid = QStringLiteral("%1%2%3%4")
                                 .arg(QChar(QLatin1Char('A' + int(rng.bounded(18)))))
                                 .arg(QChar(QLatin1Char('A' + int(rng.bounded(18)))))
                                 .arg(rng.bounded(10))
                                 .arg(rng.bounded(10));
        r += field("GRIDSQUARE", grid);
        if (e.dxcc == 291)
            r += field("STATE", QLatin1String(kStates[rng.bounded(int(std::size(kStates)))]));
        if (rng.bounded(100) < 30)
            r += field("NAME", QStringLiteral("OP") + QString::number(rng.bounded(5000)));
        if (rng.bounded(100) < 35)
            r += field("LOTW_QSL_RCVD", QStringLiteral("Y"));
        if (rng.bounded(100) < 10)
            r += field("QSL_RCVD", QStringLiteral("Y"));
        if (rng.bounded(100) < 15)
            r += field("EQSL_QSL_RCVD", QStringLiteral("Y"));
        if (rng.bounded(100) < 2)
            r += field("IOTA", QStringLiteral("EU-%1").arg(1 + rng.bounded(190), 3, 10, QLatin1Char('0')));
        if (rng.bounded(100) < 3)
            r += field("POTA_REF", QStringLiteral("%1-%2").arg(QLatin1String(e.prefix)).arg(rng.bounded(9999), 4, 10,
                                                                                               QLatin1Char('0')));
        if (rng.bounded(100) < 5)
            r += field("COMMENT", QStringLiteral("tnx qso 73"));
        r += field("STATION_CALLSIGN", QStringLiteral("IU8LMC")) + field("MY_GRIDSQUARE", QStringLiteral("JN70"));
        text += r + QStringLiteral("<EOR>\n");
    }
    return text.toUtf8();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Measures DecoDXLog on a large log."));
    p.addHelpOption();
    p.addOption({QStringLiteral("qsos"), QStringLiteral("Make a synthetic log with this many QSOs."), QStringLiteral("n")});
    p.addOption({QStringLiteral("out"), QStringLiteral("Where to write the synthetic log."), QStringLiteral("file")});
    p.addOption({QStringLiteral("db"), QStringLiteral("Measure this existing log."), QStringLiteral("file")});
    p.addOption({QStringLiteral("seed"), QStringLiteral("Random seed."), QStringLiteral("n"), QStringLiteral("73")});
    p.process(app);

    QString path = p.value(QStringLiteral("db"));
    if (p.isSet(QStringLiteral("qsos"))) {
        path = p.value(QStringLiteral("out"));
        if (path.isEmpty())
            return 1;
        QFile::remove(path);
        QFile::remove(path + QStringLiteral("-wal"));
        QFile::remove(path + QStringLiteral("-shm"));
        const int total = p.value(QStringLiteral("qsos")).toInt();
        QRandomGenerator rng(p.value(QStringLiteral("seed")).toUInt());
        LogDatabase db;
        if (!db.open(path))
            return 2;
        out() << "== import of " << total << " synthetic QSOs ==" << Qt::endl;
        // A pezzi da 100.000: un ADIF da un milione in memoria sono 400 MB solo
        // di testo, e un'importazione vera di quelle dimensioni arriva a pezzi
        // anche lei.
        const int chunk = 100000;
        int inserted = 0, duplicates = 0, invalid = 0;
        QElapsedTimer all;
        all.start();
        for (int first = 0; first < total; first += chunk) {
            const int count = qMin(chunk, total - first);
            const QByteArray adif = syntheticAdif(first, count, rng);
            const ImportResult r = measure(qPrintable(QStringLiteral("importAdif %1 QSO (%2 MB of ADIF)")
                                                          .arg(count)
                                                          .arg(adif.size() / 1048576)),
                                           [&] { return db.importAdif(adif, QStringLiteral("import"), 0); });
            inserted += r.inserted;
            duplicates += r.duplicates;
            invalid += r.invalid;
        }
        out() << QStringLiteral("import total: %1 s, %2 new, %3 duplicates, %4 rejected")
                     .arg(all.elapsed() / 1000.0, 0, 'f', 1)
                     .arg(inserted)
                     .arg(duplicates)
                     .arg(invalid)
              << Qt::endl;
        db.close();
    }
    if (path.isEmpty())
        p.showHelp(1);

    out() << "== " << path << " (" << QFileInfo(path).size() / 1048576 << " MB) ==" << Qt::endl;
    LogDatabase db;
    if (!measure("open", [&] { return db.open(path); }))
        return 3;
    const int qsos = measure("qsoCount", [&] { return db.qsoCount(); });
    out() << "QSOs: " << qsos << Qt::endl;
    measure("dirtyCount", [&] { return db.dirtyCount(); });
    measure("conflictCount", [&] { return db.conflictCount(); });
    measure("idsWithoutDxcc", [&] { return db.idsWithoutDxcc().size(); });
    measure("qslSummary", [&] { return db.qslSummary().size(); });
    measure("ft2Award", [&] { return db.ft2Award().qsos; });

    // La tabella del log: tutto quello che fa all'avvio e quando si filtra.
    QElapsedTimer created;
    created.start();
    decolog::app::QsoTableModel model(&db);
    out() << QStringLiteral("%1 %2 ms").arg(QLatin1String("table created"), -44).arg(created.elapsed(), 8) << Qt::endl;
    measure("table (first load, ids only)", [&] { return model.rowCount(); });
    measure("table colours (on another thread)", [&] {
        QElapsedTimer t;
        t.start();
        while (model.data(model.index(0, 0), decolog::app::QsoTableModel::CategoryRole).toString().isEmpty()
               && t.elapsed() < 600000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        return 0;
    });
    measure("table first screen (40 rows x all columns)", [&] {
        int n = 0;
        for (int r = 0; r < 40; ++r)
            for (int c = 0; c < model.columnCount(); ++c)
                n += model.data(model.index(r, c)).toString().size();
        return n;
    });
    measure("table jump to the middle", [&] { return model.callAt(model.rowCount() / 2).size(); });
    // Su un log grande ordini e filtri si preparano su un altro filo: il
    // primo numero e' quanto resta ferma la finestra, il secondo quando la
    // tabella e' pronta.
    auto ready = [&model] {
        QElapsedTimer t;
        t.start();
        while (model.busy() && t.elapsed() < 600000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        return model.rowCount();
    };
    measure("table reload (all rows)", [&] { model.reload(); return model.rowCount(); });
    measure("table sort by call (window blocked)", [&] { model.sortBy(QStringLiteral("call")); return 0; });
    measure("table sort by call (ready)", ready);
    out() << "   first rows by call: " << model.callAt(0) << " " << model.callAt(1) << " ... last: "
          << model.callAt(model.rowCount() - 1) << " (" << model.rowCount() << " rows)" << Qt::endl;
    measure("table sort back by time", [&] { model.setSort(QStringLiteral("utc"), false); return ready(); });
    measure("table search 'DL1' (window blocked)", [&] { model.setFilterText(QStringLiteral("DL1")); return 0; });
    measure("table search 'DL1' (ready)", ready);
    out() << "   first row found: " << model.callAt(0) << " (" << model.rowCount() << " rows)" << Qt::endl;
    measure("table search cleared", [&] { model.setFilterText(QString()); return ready(); });
    measure("table bands/modes in log", [&] { return model.bandsInLog().size() + model.modesInLog().size(); });

    // Il cluster: l'indice di tutto il log, per colorare gli spot.
    const QList<QJsonArray> rows = measure("workedRows", [&] { return db.workedRows(true, true, false); });
    Q_UNUSED(rows);
    LogIndex index;
    measure("LogIndex rebuild", [&] { index.rebuild(db, true, true, false); return index.qsoCount(); });

    // I diplomi e le statistiche.
    const AwardCalculator calc;
    measure("awards (all, one pass)", [&] { return calc.compute(db, AwardFilter{}).size(); });
    measure("statsSummary", [&] { return db.statsSummary().size(); });
    measure("countByYear", [&] { return db.countByYear().size(); });
    measure("countByBand", [&] { return db.countByBand(StatsFilter{}).size(); });
    measure("countByEntity top 15", [&] { return db.countByEntity(StatsFilter{}, 15).size(); });
    measure("countByCall top 15", [&] { return db.countByCall(StatsFilter{}, 15).size(); });
    measure("bandByMode", [&] { return db.bandByMode().size(); });
    measure("bandByHour", [&] { return db.bandByHour().size(); });
    measure("awardProgress", [&] { return db.awardProgress().size(); });
    measure("yearsInLog", [&] { return db.yearsInLog().size(); });
    measure("workedGrids", [&] { return db.workedGrids().size(); });
    // La scheda del nominativo per l'entita' piu' comune (la Germania): nel
    // programma gira su un altro filo, qui si misura quanto ci mette.
    measure("entity counts for DL (call info, background)", [&] {
        return db.dxccWorked(230).count + int(db.bandModeSlotsForDxcc(230).size());
    });
    measure("duplicateGroups (2 minutes)", [&] { return db.duplicateGroups(120).size(); });

    // A ogni QSO: il "gia' lavorato" mentre si scrive, poi il salvataggio.
    measure("workedBefore x20", [&] {
        int n = 0;
        for (int i = 0; i < 20; ++i)
            n += db.workedBefore(QStringLiteral("DL%1ABC").arg(i % 10)).count;
        return n;
    });
    measure("bandModeSlotsForCall x20", [&] {
        qsizetype n = 0;
        for (int i = 0; i < 20; ++i)
            n += db.bandModeSlotsForCall(QStringLiteral("K%1XX").arg(i % 10)).size();
        return n;
    });
    qint64 lastId = 0;
    measure("insertQso x20 (dedup check + write)", [&] {
        int ok = 0;
        for (int i = 0; i < 20; ++i) {
            const AdifRecord r{{"CALL", QStringLiteral("ZZ9BENCH%1").arg(i)}, {"QSO_DATE", "20260928"},
                               {"TIME_ON", QStringLiteral("12%1").arg(i, 2, 10, QLatin1Char('0'))},
                               {"BAND", "20m"}, {"MODE", "FT8"}};
            const InsertResult res = db.insertQso(r, QStringLiteral("udp_decodium"));
            if (res.status == InsertResult::Status::Inserted) {
                ++ok;
                lastId = res.id;
            }
        }
        return ok;
    });
    measure("table insert of the new QSO", [&] { model.insertQso(lastId); return model.rowCount(); });
    measure("table insert of a QSO written later with an old time", [&] {
        const InsertResult old = db.insertQso({{"CALL", "ZZ9OLD"}, {"QSO_DATE", "20150615"}, {"TIME_ON", "1200"},
                                               {"BAND", "20m"}, {"MODE", "CW"}, {"DXCC", "230"}, {"CONT", "EU"},
                                               {"CQZ", "14"}, {"ITUZ", "28"}, {"GRIDSQUARE", "JO62"}},
                                              QStringLiteral("manual"), {}, true);
        model.insertQso(old.id);
        return model.rowCount();
    });

    measure("bulkEdit MY_RIG on 1000 QSO", [&] {
        QList<qint64> ids;
        for (int i = 0; i < 1000 && i < model.rowCount(); ++i)
            ids << model.idAt(i);
        return db.bulkEdit(ids, QStringLiteral("MY_RIG"), QStringLiteral("IC-7300"), false).changed;
    });

    // Uscire e mettere al sicuro.
    measure("exportAdif (all)", [&] { return db.exportAdif().size(); });
    const QString backup = path + QStringLiteral(".bench-backup");
    measure("backupTo (VACUUM INTO)", [&] { return db.backupTo(backup); });
    db.close();
    measure("inspect backup (quick_check)", [&] { return logbackup::inspect(backup).qsos; });
    QFile::remove(backup);
    return 0;
}
