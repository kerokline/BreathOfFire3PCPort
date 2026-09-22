#include "hook/log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>

namespace bof3 {
namespace {

HANDLE g_log = INVALID_HANDLE_VALUE;

void Write(const char* text, int len) {
    if (g_log == INVALID_HANDLE_VALUE || len <= 0) return;
    DWORD written = 0;
    WriteFile(g_log, text, static_cast<DWORD>(len), &written, nullptr);
}

int Format(char* buf, int cap, const char* fmt, va_list ap) {
    int n = std::vsnprintf(buf, static_cast<size_t>(cap), fmt, ap);
    if (n < 0) return 0;
    return n < cap ? n : cap - 1;
}

}  // namespace

void LogOpen(void* dll_module) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(static_cast<HMODULE>(dll_module), path, MAX_PATH);
    if (n < 4 || n >= MAX_PATH) return;
    // bof3x.dll -> bof3x.log
    wchar_t* ext = path + n - 3;
    ext[0] = L'l';
    ext[1] = L'o';
    ext[2] = L'g';
    g_log = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
}

void LogClose() {
    if (g_log != INVALID_HANDLE_VALUE) CloseHandle(g_log);
    g_log = INVALID_HANDLE_VALUE;
}

void LogFlush() {
    if (g_log != INVALID_HANDLE_VALUE) FlushFileBuffers(g_log);
}

void Log(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = Format(buf, sizeof buf - 2, fmt, ap);
    va_end(ap);
    buf[n++] = '\r';
    buf[n++] = '\n';
    Write(buf, n);
}

void Fatal(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = Format(buf, sizeof buf, fmt, ap);
    va_end(ap);
    Write("FATAL: ", 7);
    Write(buf, n);
    Write("\r\n", 2);
    if (g_log != INVALID_HANDLE_VALUE) FlushFileBuffers(g_log);
    if (GetEnvironmentVariableA("BOF3X_SELFTEST_ONLY", nullptr, 0) == 0)
        MessageBoxA(nullptr, buf, "bof3x", MB_OK | MB_ICONERROR | MB_TASKMODAL);
    TerminateProcess(GetCurrentProcess(), 3);
    for (;;) {
    }
}

}  // namespace bof3
