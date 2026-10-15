#include "Core/SehGuard.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <dbghelp.h>
#endif

#include <stdio.h>

#ifdef _WIN32
namespace {

void writeText(HANDLE file, const char *text)
{
    if (!file || file == INVALID_HANDLE_VALUE || !text) {
        return;
    }
    DWORD n = 0;
    WriteFile(file, text, static_cast<DWORD>(lstrlenA(text)), &n, nullptr);
}

/** 崩溃日志写到 exe 同级 log/crash.txt（与 LogSink 根目录一致） */
bool crashLogPath(wchar_t *out, size_t outChars)
{
    if (!out || outChars < 32) {
        return false;
    }
    wchar_t modPath[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, modPath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return false;
    }
    wchar_t *slash = wcsrchr(modPath, L'\\');
    if (!slash) {
        return false;
    }
    *slash = L'\0';
    wchar_t logDir[MAX_PATH];
    if (swprintf_s(logDir, L"%s\\log", modPath) <= 0) {
        return false;
    }
    CreateDirectoryW(logDir, nullptr);
    return swprintf_s(out, outChars, L"%s\\crash.txt", logDir) > 0;
}

LONG WINAPI crashFilter(EXCEPTION_POINTERS *info)
{
    wchar_t path[MAX_PATH];
    HANDLE file = INVALID_HANDLE_VALUE;
    if (crashLogPath(path, MAX_PATH)) {
        file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (!info || !info->ExceptionRecord) {
        writeText(file, "no exception info\r\n");
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    char line[640];
    sprintf_s(line, "code=0x%08X addr=%p\r\n",
              static_cast<unsigned>(info->ExceptionRecord->ExceptionCode),
              info->ExceptionRecord->ExceptionAddress);
    writeText(file, line);

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(process, nullptr, TRUE);

    CONTEXT context = *info->ContextRecord;
    STACKFRAME64 frame = {};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;

    for (int i = 0; i < 48; ++i) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame,
                         &context, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr)) {
            break;
        }
        if (frame.AddrPC.Offset == 0) {
            break;
        }
        HMODULE mod = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                               | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(frame.AddrPC.Offset),
                           &mod);
        char modName[MAX_PATH] = "?";
        if (mod) {
            GetModuleFileNameA(mod, modName, MAX_PATH);
        }
        const auto base = reinterpret_cast<uintptr_t>(mod);
        const auto pc = static_cast<uintptr_t>(frame.AddrPC.Offset);
        char symbol[512] = {};
        DWORD64 disp = 0;
        char buf[sizeof(SYMBOL_INFO) + 512] = {};
        auto *sym = reinterpret_cast<SYMBOL_INFO *>(buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 500;
        if (SymFromAddr(process, frame.AddrPC.Offset, &disp, sym)) {
            sprintf_s(symbol, " %s+0x%llX", sym->Name, static_cast<unsigned long long>(disp));
        }
        sprintf_s(line, "#%02d %s + 0x%llX%s\r\n",
                  i,
                  modName,
                  static_cast<unsigned long long>(base ? pc - base : pc),
                  symbol);
        writeText(file, line);
    }
    SymCleanup(process);
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void installCrashDump()
{
    SetUnhandledExceptionFilter(crashFilter);
}
#else
void installCrashDump() {}
#endif
