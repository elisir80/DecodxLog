#include "core/VoiceAnnouncer.h"

#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#include <objbase.h>
#include <sapi.h>
#endif

namespace decolog::core {

QString VoiceAnnouncer::spell(const QString& callsign, bool phonetic)
{
    static const QHash<QChar, QString> nato{
        {'A', "Alfa"},   {'B', "Bravo"},   {'C', "Charlie"}, {'D', "Delta"},   {'E', "Echo"},
        {'F', "Foxtrot"}, {'G', "Golf"},   {'H', "Hotel"},   {'I', "India"},   {'J', "Juliett"},
        {'K', "Kilo"},   {'L', "Lima"},    {'M', "Mike"},    {'N', "November"}, {'O', "Oscar"},
        {'P', "Papa"},   {'Q', "Quebec"},  {'R', "Romeo"},   {'S', "Sierra"},  {'T', "Tango"},
        {'U', "Uniform"}, {'V', "Victor"}, {'W', "Whiskey"}, {'X', "X-ray"},   {'Y', "Yankee"},
        {'Z', "Zulu"},   {'/', "stroke"},
    };
    QStringList parts;
    for (QChar c : callsign.trimmed().toUpper()) {
        if (phonetic && nato.contains(c))
            parts << nato.value(c);
        else if (c == QLatin1Char('/'))
            parts << QStringLiteral("/");
        else
            parts << QString(c);
    }
    // Le virgole fanno fare una piccola pausa a tutte le sintesi.
    return parts.join(phonetic ? QStringLiteral(", ") : QStringLiteral(" "));
}

#ifdef Q_OS_WIN

// ── Windows: SAPI ─────────────────────────────────────────────────────────────

struct VoiceAnnouncer::Backend {
    ISpVoice* voice{nullptr};
    bool comInitialized{false};
    QStringList names;
    QList<ISpObjectToken*> tokens;

    Backend()
    {
        const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        comInitialized = SUCCEEDED(init);
        if (FAILED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, reinterpret_cast<void**>(&voice))))
            voice = nullptr;
        // Le voci "Desktop" di SAPI e quelle moderne di Windows 10/11 (OneCore).
        for (const wchar_t* category : {L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices",
                                        L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices"}) {
            ISpObjectTokenCategory* cat = nullptr;
            if (FAILED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_ISpObjectTokenCategory,
                                        reinterpret_cast<void**>(&cat))))
                continue;
            IEnumSpObjectTokens* list = nullptr;
            if (SUCCEEDED(cat->SetId(category, FALSE)) && SUCCEEDED(cat->EnumTokens(nullptr, nullptr, &list))) {
                ISpObjectToken* token = nullptr;
                while (list->Next(1, &token, nullptr) == S_OK) {
                    LPWSTR description = nullptr;
                    if (SUCCEEDED(token->GetStringValue(nullptr, &description)) && description) {
                        const QString name = QString::fromWCharArray(description);
                        CoTaskMemFree(description);
                        if (!names.contains(name)) {
                            names << name;
                            tokens << token;
                            continue;
                        }
                    }
                    token->Release();
                }
                list->Release();
            }
            cat->Release();
        }
    }

    ~Backend()
    {
        for (ISpObjectToken* t : tokens)
            t->Release();
        if (voice) {
            voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
            voice->Release();
        }
        if (comInitialized)
            CoUninitialize();
    }
};

VoiceAnnouncer::VoiceAnnouncer(QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Backend>())
{
}

VoiceAnnouncer::~VoiceAnnouncer() = default;

bool VoiceAnnouncer::available() const { return d->voice != nullptr; }
QString VoiceAnnouncer::backend() const { return QStringLiteral("Windows SAPI"); }
QStringList VoiceAnnouncer::voices() const { return d->names; }

void VoiceAnnouncer::setVoice(const QString& name)
{
    const qsizetype i = d->names.indexOf(name);
    if (d->voice && i >= 0)
        d->voice->SetVoice(d->tokens.at(i));
}

void VoiceAnnouncer::setRate(int rate)
{
    if (d->voice)
        d->voice->SetRate(qBound(-10, rate, 10));
}

void VoiceAnnouncer::setVolume(int volume)
{
    if (d->voice)
        d->voice->SetVolume(static_cast<USHORT>(qBound(0, volume, 100)));
}

void VoiceAnnouncer::say(const QString& text)
{
    if (!d->voice || text.trimmed().isEmpty())
        return;
    const std::wstring w = text.toStdWString();
    // Asincrono e in coda: SAPI parla da un suo thread.
    d->voice->Speak(w.c_str(), SPF_ASYNC | SPF_IS_NOT_XML, nullptr);
}

void VoiceAnnouncer::stop()
{
    if (d->voice)
        d->voice->Speak(nullptr, SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
}

#else

// ── macOS e Linux: un programma di sintesi alla volta ─────────────────────────

struct VoiceAnnouncer::Backend {
    QString program;
    QString voice;
    int rate{0};
    int volume{100};
    QStringList queue;
    QProcess* process{nullptr};
};

VoiceAnnouncer::VoiceAnnouncer(QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Backend>())
{
    for (const char* candidate : {"say", "spd-say", "espeak-ng", "espeak"}) {
        const QString path = QStandardPaths::findExecutable(QLatin1String(candidate));
        if (!path.isEmpty()) {
            d->program = path;
            break;
        }
    }
}

VoiceAnnouncer::~VoiceAnnouncer()
{
    if (d->process) {
        d->process->disconnect(this);
        d->process->kill();
        d->process->waitForFinished(500);
    }
}

bool VoiceAnnouncer::available() const { return !d->program.isEmpty(); }
QString VoiceAnnouncer::backend() const { return d->program; }
QStringList VoiceAnnouncer::voices() const { return {}; }
void VoiceAnnouncer::setVoice(const QString& name) { d->voice = name; }
void VoiceAnnouncer::setRate(int rate) { d->rate = qBound(-10, rate, 10); }
void VoiceAnnouncer::setVolume(int volume) { d->volume = qBound(0, volume, 100); }

void VoiceAnnouncer::say(const QString& text)
{
    if (d->program.isEmpty() || text.trimmed().isEmpty())
        return;
    d->queue << text;
    if (d->process)
        return;

    const auto next = [this](const auto& self) -> void {
        if (d->queue.isEmpty()) {
            d->process->deleteLater();
            d->process = nullptr;
            return;
        }
        const QString sentence = d->queue.takeFirst();
        QStringList args;
        const QString name = QFileInfo(d->program).fileName();
        if (name == QLatin1String("say")) {
            args << QStringLiteral("-r") << QString::number(175 + d->rate * 10);
        } else if (name == QLatin1String("spd-say")) {
            args << QStringLiteral("-w") << QStringLiteral("-r") << QString::number(d->rate * 10)
                 << QStringLiteral("-i") << QString::number(d->volume * 2 - 100);
        } else {
            args << QStringLiteral("-s") << QString::number(175 + d->rate * 10)
                 << QStringLiteral("-a") << QString::number(d->volume * 2);
        }
        args << sentence;
        d->process->start(d->program, args);
        Q_UNUSED(self);
    };
    d->process = new QProcess(this);
    connect(d->process, &QProcess::finished, this, [this, next] { next(next); });
    next(next);
}

void VoiceAnnouncer::stop()
{
    d->queue.clear();
    if (d->process)
        d->process->kill();
}

#endif

} // namespace decolog::core
