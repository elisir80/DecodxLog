#include "app/LogLibrary.h"

#include "core/LogDatabase.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace decolog::app {

namespace {

QString toPath(const QUrl& url)
{
    return url.isLocalFile() ? QDir::toNativeSeparators(url.toLocalFile()) : url.toString();
}

// Due percorsi che puntano allo stesso file sono lo stesso log, anche se uno ha
// le barre al contrario e l'altro no.
QString canonical(const QString& path)
{
    const QFileInfo info(path);
    const QString resolved = info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
    return QDir::toNativeSeparators(resolved);
}

} // namespace

LogLibrary::LogLibrary(QObject* parent)
    : QObject(parent)
{
    load();
}

void LogLibrary::load()
{
    QSettings settings;
    m_entries.clear();
    const int count = settings.beginReadArray(QStringLiteral("logs/list"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        Entry e;
        e.name = settings.value(QStringLiteral("name")).toString();
        e.path = settings.value(QStringLiteral("path")).toString();
        e.lastUsed = settings.value(QStringLiteral("lastUsed")).toString();
        if (!e.path.isEmpty())
            m_entries << e;
    }
    settings.endArray();
}

void LogLibrary::save()
{
    QSettings settings;
    settings.beginWriteArray(QStringLiteral("logs/list"), static_cast<int>(m_entries.size()));
    for (int i = 0; i < m_entries.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("name"), m_entries.at(i).name);
        settings.setValue(QStringLiteral("path"), m_entries.at(i).path);
        settings.setValue(QStringLiteral("lastUsed"), m_entries.at(i).lastUsed);
    }
    settings.endArray();
    emit changed();
}

bool LogLibrary::askAtStart() const
{
    return QSettings().value(QStringLiteral("logs/askAtStart"), false).toBool();
}

void LogLibrary::setAskAtStart(bool ask)
{
    QSettings().setValue(QStringLiteral("logs/askAtStart"), ask);
    emit changed();
}

void LogLibrary::setCurrent(const QString& path)
{
    m_current = canonical(path);
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    for (Entry& e : m_entries) {
        if (canonical(e.path) == m_current) {
            e.lastUsed = now;
            save();
            return;
        }
    }
    // Un log aperto da riga di comando entra nell'elenco da solo: chi lo apre
    // una volta lo ritrova, invece di doverlo ritrovare a mano.
    Entry e;
    e.name = QFileInfo(m_current).completeBaseName();
    e.path = m_current;
    e.lastUsed = now;
    m_entries << e;
    save();
}

QVariantList LogLibrary::logs() const
{
    QList<Entry> sorted = m_entries;
    std::sort(sorted.begin(), sorted.end(), [](const Entry& a, const Entry& b) {
        return a.lastUsed > b.lastUsed;
    });
    QVariantList out;
    for (const Entry& e : std::as_const(sorted)) {
        const QFileInfo info(e.path);
        const bool exists = info.exists();
        // Quanti QSO ci sono dentro: aprire il log solo per contarli sarebbe
        // caro, ma la dimensione del file non dice niente a nessuno.
        int qsos = -1;
        if (exists && canonical(e.path) != m_current) {
            core::LogDatabase peek;
            if (peek.open(e.path))
                qsos = peek.qsoCount();
        }
        out << QVariantMap{
            {QStringLiteral("name"), e.name.isEmpty() ? info.completeBaseName() : e.name},
            {QStringLiteral("path"), QDir::toNativeSeparators(e.path)},
            {QStringLiteral("folder"), QDir::toNativeSeparators(info.absolutePath())},
            {QStringLiteral("current"), canonical(e.path) == m_current},
            {QStringLiteral("missing"), !exists},
            {QStringLiteral("qsos"), qsos},
            {QStringLiteral("lastUsed"), e.lastUsed},
        };
    }
    return out;
}

QString LogLibrary::defaultFolder() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir::toNativeSeparators(dir);
}

QString LogLibrary::createLog(const QString& name, const QUrl& path)
{
    const QString wanted = name.trimmed();
    if (wanted.isEmpty())
        return tr("Give the log a name.");

    QString file = toPath(path);
    if (file.isEmpty()) {
        // Il nome diventa il file: niente caratteri che Windows non accetta.
        QString safe = wanted;
        safe.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
        file = QDir(defaultFolder()).filePath(safe + QStringLiteral(".sqlite"));
    }
    if (!file.endsWith(QLatin1String(".sqlite"), Qt::CaseInsensitive))
        file += QStringLiteral(".sqlite");
    if (QFileInfo::exists(file))
        return tr("There is already a file called %1.").arg(QDir::toNativeSeparators(file));

    QDir().mkpath(QFileInfo(file).absolutePath());
    {
        // Si crea davvero adesso, con le sue tabelle: un log che non si apre e'
        // meglio scoprirlo qui che al riavvio, con la finestra gia' chiusa.
        core::LogDatabase db;
        if (!db.open(file)) {
            const QString why = db.lastError();
            QFile::remove(file);
            return why.isEmpty() ? tr("The log could not be created.") : why;
        }
    }

    Entry e;
    e.name = wanted;
    e.path = QDir::toNativeSeparators(file);
    e.lastUsed = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_entries << e;
    save();
    return {};
}

QString LogLibrary::addExisting(const QUrl& path)
{
    const QString file = toPath(path);
    if (file.isEmpty())
        return tr("No file chosen.");
    if (!QFileInfo::exists(file))
        return tr("%1 is not there.").arg(QDir::toNativeSeparators(file));
    {
        core::LogDatabase db;
        if (!db.open(file))
            return tr("%1 is not a DecoDXLog log.").arg(QDir::toNativeSeparators(file));
    }
    const QString resolved = canonical(file);
    for (const Entry& e : std::as_const(m_entries)) {
        if (canonical(e.path) == resolved)
            return {};   // c'era gia': non e' un errore
    }
    Entry e;
    e.name = QFileInfo(file).completeBaseName();
    e.path = QDir::toNativeSeparators(file);
    e.lastUsed = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_entries << e;
    save();
    return {};
}

QString LogLibrary::openLog(const QString& path)
{
    const QString file = canonical(path);
    if (!QFileInfo::exists(file))
        return tr("%1 is not there.").arg(QDir::toNativeSeparators(file));
    if (file == m_current)
        return {};

    // Il programma riparte su quell'altro log: e' l'unico modo di non lasciare
    // mezzo programma legato a quello di prima.
    QStringList args{QStringLiteral("--db"), file};
    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), args))
        return tr("The program could not be restarted on %1.").arg(QDir::toNativeSeparators(file));
    QCoreApplication::quit();
    return {};
}

void LogLibrary::forget(const QString& path)
{
    const QString resolved = canonical(path);
    for (int i = 0; i < m_entries.size(); ++i) {
        if (canonical(m_entries.at(i).path) == resolved) {
            m_entries.removeAt(i);
            save();
            return;
        }
    }
}

void LogLibrary::rename(const QString& path, const QString& name)
{
    const QString resolved = canonical(path);
    const QString wanted = name.trimmed();
    if (wanted.isEmpty())
        return;
    for (Entry& e : m_entries) {
        if (canonical(e.path) == resolved) {
            e.name = wanted;
            save();
            return;
        }
    }
}

} // namespace decolog::app
