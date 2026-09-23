// DecoDXLog — il biglietto lasciato quando il programma cade. Vedi CrashLog.h.
#include "CrashLog.h"

#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#endif

namespace decolog::crashlog {

#ifdef Q_OS_WIN
namespace {

// Tutto preparato prima: quando il programma cade non si alloca niente, si
// scrive con quello che c'e'.
wchar_t g_folder[MAX_PATH] = {};
char g_version[32] = {};
const char* volatile g_stage = "startup";
DWORD g_mainThread = 0;

void writeLine(HANDLE file, const char* text)
{
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
}

LONG WINAPI onCrash(EXCEPTION_POINTERS* info)
{
    SYSTEMTIME now;
    GetSystemTime(&now);
    wchar_t path[MAX_PATH + 64];
    std::swprintf(path, sizeof(path) / sizeof(path[0]), L"%ls\\crash-%04u%02u%02u-%02u%02u%02u.txt", g_folder,
                  now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return EXCEPTION_CONTINUE_SEARCH;

    char line[512];
    std::snprintf(line, sizeof(line), "DecoDXLog %s\r\n%04u-%02u-%02u %02u:%02u:%02u UTC\r\nstage: %s\r\n",
                  g_version, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
                  g_stage ? g_stage : "?");
    writeLine(file, line);
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    // Il thread conta: una lista toccata da due thread insieme cade cosi'.
    std::snprintf(line, sizeof(line), "exception 0x%08lx at %p, thread %lu (%s)\r\n",
                  static_cast<unsigned long>(record->ExceptionCode), record->ExceptionAddress,
                  static_cast<unsigned long>(GetCurrentThreadId()),
                  GetCurrentThreadId() == g_mainThread ? "main" : "other");
    writeLine(file, line);
    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
        std::snprintf(line, sizeof(line), "%s address %p\r\n",
                      record->ExceptionInformation[0] == 0 ? "read" : record->ExceptionInformation[0] == 1 ? "write" : "exec",
                      reinterpret_cast<void*>(record->ExceptionInformation[1]));
        writeLine(file, line);
    }

    // La strada: si riparte dal contesto dell'errore e si risale, frame per
    // frame, con le tabelle di svolgimento che il compilatore mette in ogni
    // modulo x64. Ogni indirizzo si scrive come modulo+scostamento.
    CONTEXT context = *info->ContextRecord;
    writeLine(file, "stack:\r\n");
    for (int depth = 0; depth < 64 && context.Rip != 0; ++depth) {
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
        char module[MAX_PATH] = "?";
        if (imageBase != 0) {
            char full[MAX_PATH];
            if (GetModuleFileNameA(reinterpret_cast<HMODULE>(imageBase), full, MAX_PATH) > 0) {
                const char* slash = std::strrchr(full, '\\');
                std::snprintf(module, sizeof(module), "%s", slash ? slash + 1 : full);
            }
        }
        std::snprintf(line, sizeof(line), "  #%02d %s+0x%llx\r\n", depth, module,
                      static_cast<unsigned long long>(context.Rip - imageBase));
        writeLine(file, line);
        if (!function) {
            // Una funzione foglia: l'indirizzo di ritorno sta in cima alla pila.
            context.Rip = *reinterpret_cast<DWORD64*>(context.Rsp);
            context.Rsp += 8;
            continue;
        }
        void* handlerData = nullptr;
        DWORD64 establisher = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context, &handlerData,
                         &establisher, nullptr);
    }
    CloseHandle(file);
    // Windows fa il resto: il programma si chiude come prima, e l'errore resta
    // anche nel registro di sistema.
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void install(const QString& folder, const QString& version)
{
    QDir().mkpath(folder);
    const std::wstring native = QDir::toNativeSeparators(folder).toStdWString();
    std::wcsncpy(g_folder, native.c_str(), MAX_PATH - 1);
    std::snprintf(g_version, sizeof(g_version), "%s", version.toUtf8().constData());
    g_mainThread = GetCurrentThreadId();
    SetUnhandledExceptionFilter(onCrash);
}

void setStage(const char* stage)
{
    g_stage = stage;
}

#else

void install(const QString&, const QString&) {}
void setStage(const char*) {}

#endif

} // namespace decolog::crashlog
