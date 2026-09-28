#include "core/Voacap.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QTimer>

#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace decolog::core::voacap {

namespace {

// Un numero in un campo Fortran Fw.d: allineato a destra, mai piu' largo del
// campo (Fortran scriverebbe asterischi, e VOACAP leggerebbe un'altra cosa).
QString fixed(double value, int width, int decimals)
{
    QString s = QString::number(value, 'f', decimals);
    if (decimals == 0)
        s += QLatin1Char('.');
    if (s.size() > width)
        s = s.left(width);
    return s.rightJustified(width, QLatin1Char(' '));
}

QString integer(int value, int width)
{
    return QString::number(value).rightJustified(width, QLatin1Char(' '));
}

// Un testo in un campo An: solo ASCII (VOACAP non sa altro), tagliato o
// allungato alla misura.
QString text(const QString& value, int width)
{
    QString s;
    for (const QChar c : value) {
        if (s.size() >= width)
            break;
        s += c.unicode() >= 32 && c.unicode() < 127 ? c : QLatin1Char('?');
    }
    return s.leftJustified(width, QLatin1Char(' '));
}

QString card(const char* name)
{
    return QString::fromLatin1(name).leftJustified(10, QLatin1Char(' '));
}

// Il programma accanto ai suoi dati: voacapl.exe su Windows, voacapl altrove.
QString programName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("/voacapl.exe");
#else
    return QStringLiteral("/voacapl");
#endif
}

} // namespace

QString Request::key() const
{
    QStringList parts{QString::number(fromLat, 'f', 2), QString::number(fromLon, 'f', 2),
                      QString::number(toLat, 'f', 2),   QString::number(toLon, 'f', 2),
                      QString::number(year),            QString::number(month),
                      QString::number(qRound(ssn)),     QString::number(powerWatts, 'f', 0),
                      QString::number(txGainDbi, 'f', 1), QString::number(rxGainDbi, 'f', 1),
                      QString::number(noise),           QString::number(requiredSnr, 'f', 1),
                      QString::number(minAngle, 'f', 1), longPath ? QStringLiteral("L") : QStringLiteral("S")};
    for (double f : mhz)
        parts << QString::number(f, 'f', 2);
    return parts.join(QLatin1Char('|'));
}

double requiredSnrFor(const QString& mode)
{
    // soglia nella banda del modo + 10·log10(banda) + 3 dB di margine per i
    // modi digitali (alla soglia si decodifica una volta su due).
    static const QHash<QString, double> snr{
        {QStringLiteral("FT8"), -21.0 + 34.0 + 3.0},
        {QStringLiteral("FT4"), -17.5 + 34.0 + 3.0},
        // FT2 di Decodium: come FT4, finche' non se ne misura la soglia.
        {QStringLiteral("FT2"), -17.5 + 34.0 + 3.0},
        {QStringLiteral("CW"), -3.0 + 27.0},
        {QStringLiteral("RTTY"), 8.0 + 24.0},
        {QStringLiteral("SSB"), 6.0 + 33.8},
    };
    return snr.value(mode.trimmed().toUpper(), snr.value(QStringLiteral("FT8")));
}

int qualityOf(const Cell& cell)
{
    if (cell.rel >= 0.8)
        return 3;
    if (cell.rel >= 0.5)
        return 2;
    if (cell.rel >= 0.2)
        return 1;
    return 0;
}

QString deck(const Request& r)
{
    QStringList lines;
    // Una pagina sola: niente intestazioni in mezzo alle ore.
    lines << card("LINEMAX") + integer(999, 5) + QStringLiteral("       number of lines-per-page");
    lines << card("COEFFS") + QStringLiteral("CCIR");
    lines << card("TIME") + integer(1, 5) + integer(24, 5) + integer(1, 5) + integer(1, 5);
    lines << card("MONTH") + integer(r.year, 5) + fixed(std::clamp(r.month, 1, 12), 5, 2);
    lines << card("SUNSPOT") + fixed(std::clamp(std::round(r.ssn), 0.0, 300.0), 5, 0);
    lines << card("LABEL") + text(r.fromLabel, 20) + text(r.toLabel, 20);
    auto lat = [](double v) { return v < 0 ? QLatin1Char('S') : QLatin1Char('N'); };
    auto lon = [](double v) { return v < 0 ? QLatin1Char('W') : QLatin1Char('E'); };
    lines << card("CIRCUIT") + fixed(std::fabs(r.fromLat), 5, 2) + lat(r.fromLat)
                 + fixed(std::fabs(r.fromLon), 9, 2) + lon(r.fromLon) + fixed(std::fabs(r.toLat), 9, 2)
                 + lat(r.toLat) + fixed(std::fabs(r.toLon), 9, 2) + lon(r.toLon)
                 + (r.longPath ? QStringLiteral("  L ") : QStringLiteral("  S ")) + integer(0, 5);
    // Potenza (non usata qui: sta sull'antenna), rumore, angolo minimo,
    // affidabilita' richiesta, SNR richiesto, tolleranze del multipath.
    lines << card("SYSTEM") + fixed(1.0, 5, 2) + fixed(r.noise, 5, 0) + fixed(r.minAngle, 5, 2) + fixed(90, 5, 0)
                 + fixed(r.requiredSnr, 5, 2) + fixed(3.0, 5, 2) + fixed(0.1, 5, 2);
    lines << card("FPROB") + fixed(1, 5, 2) + fixed(1, 5, 2) + fixed(1, 5, 2) + fixed(0, 5, 2);
    // L'isotropa di serie: per lei VOACAP prende il guadagno dal campo della
    // frequenza di progetto (antcalc.for: "set isotrope gain").
    auto antenna = [](int number, int role, double gainDbi, double kw) {
        return card("ANTENNA") + integer(number, 5) + integer(role, 5) + integer(2, 5) + integer(30, 5)
               + fixed(gainDbi, 10, 3) + QLatin1Char('[') + text(QStringLiteral("default/isotrope"), 21)
               + QLatin1Char(']') + fixed(0, 5, 1) + fixed(kw, 10, 4);
    };
    lines << antenna(1, 1, r.txGainDbi, std::max(0.0001, r.powerWatts / 1000.0));
    lines << antenna(2, 2, r.rxGainDbi, 0.0);
    QString freqs = card("FREQUENCY");
    for (int i = 0; i < 11; ++i)
        freqs += fixed(i < r.mhz.size() ? std::clamp(r.mhz.at(i), 2.0, 30.0) : 0.0, 5, 2);
    lines << freqs;
    lines << card("METHOD") + integer(30, 5) + integer(0, 5);
    lines << QStringLiteral("EXECUTE") << QStringLiteral("QUIT");
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

Result parse(const QString& output)
{
    // Ogni ora e' un blocco: la riga "FREQ" con l'ora, la MUF e le frequenze,
    // poi una riga per grandezza con l'etichetta in fondo. Le colonne sono
    // fisse: 6 caratteri per l'ora, poi 12 campi da 5 (il primo e' alla MUF),
    // e i numeri possono toccarsi ("-5.6-13.2").
    Result r;
    int current = -1;
    QList<double> freqs;
    auto field = [](const QString& line, int i) { return line.mid(6 + 5 * i, 5).trimmed(); };
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line.remove(QLatin1Char('\r'));
        if (line.size() < 67)
            continue;
        const QString label = line.mid(66).trimmed();
        if (label == QLatin1String("FREQ")) {
            bool ok = false;
            const double hour = line.left(6).trimmed().toDouble(&ok);
            if (!ok)
                continue;
            freqs.clear();
            for (int i = 1; i < 12; ++i) {
                const double f = field(line, i).toDouble();
                if (f > 0)
                    freqs << f;
            }
            if (r.mhz.isEmpty())
                r.mhz = freqs;
            HourResult h;
            h.hourUtc = int(std::lround(hour)) % 24;
            h.muf = field(line, 0).toDouble();
            h.cells.resize(freqs.size());
            current = int(r.hours.size());
            r.hours << h;
            continue;
        }
        if (current < 0)
            continue;
        HourResult& h = r.hours[current];
        for (int i = 0; i < h.cells.size(); ++i) {
            const QString v = field(line, i + 1);
            Cell& c = h.cells[i];
            if (label == QLatin1String("REL"))
                c.rel = v.toDouble();
            else if (label == QLatin1String("SNR"))
                c.snr = v.toDouble();
            else if (label == QLatin1String("S DBW"))
                c.signalDbw = v.toDouble();
            else if (label == QLatin1String("MUFday"))
                c.mufDays = v.toDouble();
            else if (label == QLatin1String("MODE"))
                c.mode = v;
        }
    }
    if (r.hours.size() != 24) {
        r.error = r.hours.isEmpty() ? QStringLiteral("no forecast in the VOACAP output")
                                    : QStringLiteral("VOACAP gave %1 hours instead of 24").arg(r.hours.size());
        return r;
    }
    std::sort(r.hours.begin(), r.hours.end(),
              [](const HourResult& a, const HourResult& b) { return a.hourUtc < b.hourUtc; });
    r.valid = true;
    return r;
}

// ── Il motore ────────────────────────────────────────────────────────────────

Engine::Engine(QObject* parent)
    : QObject(parent)
    , m_bundled(bundledDir())
{
}

Engine::~Engine()
{
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

QString Engine::bundledDir()
{
    const QString env = qEnvironmentVariable("DECODXLOG_VOACAP_DIR");
    if (!env.isEmpty())
        return QDir::fromNativeSeparators(env);
    return QCoreApplication::applicationDirPath() + QStringLiteral("/voacap");
}

bool Engine::available() const
{
    return QFileInfo::exists(m_bundled + programName())
           && QFileInfo::exists(m_bundled + QStringLiteral("/itshfbc/coeffs/coeff01w.bin"));
}

void Engine::setWorkDir(const QString& dir)
{
    m_work = QDir::fromNativeSeparators(dir);
    m_prepared = false;
}

bool Engine::prepare(QString* error)
{
    if (m_prepared)
        return true;
    if (m_work.isEmpty()) {
        *error = QStringLiteral("no working folder for VOACAP");
        return false;
    }
    // VOACAP scrive nella sua cartella (le antenne, l'uscita, un file nel
    // database): i dati si copiano dove si puo' scrivere, e di nuovo quando il
    // programma cambia con un aggiornamento.
    const QString root = m_work + QStringLiteral("/itshfbc");
    const QFileInfo exe(m_bundled + programName());
    const QByteArray stamp = QByteArray::number(exe.size()) + '-'
                             + QByteArray::number(exe.lastModified().toSecsSinceEpoch());
    QFile stampFile(root + QStringLiteral("/decodxlog.stamp"));
    const bool fresh = stampFile.open(QIODevice::ReadOnly) && stampFile.readAll() == stamp;
    stampFile.close();
    if (!fresh) {
        const QString from = m_bundled + QStringLiteral("/itshfbc");
        QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString file = it.next();
            const QString to = root + file.mid(from.size());
            QDir().mkpath(QFileInfo(to).absolutePath());
            QFile::remove(to);
            if (!QFile::copy(file, to)) {
                *error = QStringLiteral("cannot copy %1 to %2").arg(file, to);
                return false;
            }
            QFile(to).setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        }
        if (!stampFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            *error = stampFile.errorString();
            return false;
        }
        stampFile.write(stamp);
        stampFile.close();
    }
    QDir().mkpath(root + QStringLiteral("/run"));
    m_prepared = true;
    return true;
}

void Engine::run(const Request& request)
{
    if (m_process) {
        m_next = request;
        return;
    }
    start(request);
}

void Engine::start(const Request& request)
{
    m_current = request;
    auto fail = [this](const QString& message) {
        Result r;
        r.error = message;
        // Mai dentro run(): chi l'ha chiamato sta ancora facendo i suoi conti.
        QMetaObject::invokeMethod(this, [this, r] { done(r); }, Qt::QueuedConnection);
    };
    if (!available()) {
        fail(QStringLiteral("VOACAP is not installed with this copy of DecoDXLog"));
        return;
    }
    QString error;
    if (!prepare(&error)) {
        fail(error);
        return;
    }
    const QString root = m_work + QStringLiteral("/itshfbc");
    QFile input(root + QStringLiteral("/run/voacapx.dat"));
    if (!input.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(QStringLiteral("cannot write the VOACAP input in %1").arg(root));
        return;
    }
    input.write(deck(request).toLatin1());
    input.close();
    QFile::remove(root + QStringLiteral("/run/voacapx.out"));

    // Il Fortran legge gli argomenti nella codepage di Windows e le cartelle
    // fino a 128 caratteri: il nome corto 8.3 evita accenti, spazi e lunghezza.
    QString rootArg = QDir::toNativeSeparators(root);
#ifdef Q_OS_WIN
    wchar_t shortPath[MAX_PATH];
    const DWORD n = GetShortPathNameW(reinterpret_cast<const wchar_t*>(rootArg.utf16()), shortPath, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
        rootArg = QString::fromWCharArray(shortPath, int(n));
#endif
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setWorkingDirectory(root + QStringLiteral("/run"));
    connect(m_process, &QProcess::finished, this, [this, root](int code, QProcess::ExitStatus status) {
        QFile out(root + QStringLiteral("/run/voacapx.out"));
        Result r;
        if (status != QProcess::NormalExit || !out.open(QIODevice::ReadOnly)) {
            const QString said = QString::fromLocal8Bit(m_process->readAll()).simplified();
            r.error = QStringLiteral("VOACAP stopped (%1)%2").arg(code).arg(said.isEmpty() ? QString() : QStringLiteral(": ") + said.left(200));
        } else {
            r = parse(QString::fromLatin1(out.readAll()));
        }
        done(r);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        Result r;
        r.error = QStringLiteral("VOACAP did not start: %1").arg(m_process->errorString());
        done(r);
    });
    if (!m_timeout) {
        m_timeout = new QTimer(this);
        m_timeout->setSingleShot(true);
        connect(m_timeout, &QTimer::timeout, this, [this] {
            if (!m_process)
                return;
            m_process->disconnect(this);
            m_process->kill();
            Result r;
            r.error = QStringLiteral("VOACAP did not answer in 30 seconds");
            done(r);
        });
    }
    m_timeout->start(30000);
    m_process->start(m_bundled + programName(),
                     {QStringLiteral("-s"), rootArg, QStringLiteral("voacapx.dat"), QStringLiteral("voacapx.out")});
}

void Engine::done(const Result& result)
{
    if (m_timeout)
        m_timeout->stop();
    if (m_process) {
        m_process->disconnect(this);
        m_process->deleteLater();
        m_process = nullptr;
    }
    emit finished(m_current.key(), result);
    if (m_next) {
        const Request next = *m_next;
        m_next.reset();
        start(next);
    }
}

} // namespace decolog::core::voacap
