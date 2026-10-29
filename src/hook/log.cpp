#include "hook/log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "hook/statehash.h"

namespace bof3 {
namespace {

HANDLE g_log = INVALID_HANDLE_VALUE;

// The log is written by a thread of its own (known-defects D304): a WriteFile
// on the game's thread waited for the disk, and with the disk saturated by
// other work Windows' write throttling held it - and so the frame - for up to
// 34 s while a scripted run logged its line a frame. Lines go to a queue
// here; the writer takes the whole queue and writes it outside the lock.
// Before the writer runs (start-up, under the loader lock), with the queue
// full, and in Drain, the calling thread writes itself, in order. No heap:
// the crash reporter logs through this too.
constexpr DWORD kQueue = 4u << 20;
char g_queue[kQueue];
DWORD g_queued = 0;
char g_flight[kQueue];     // the chunk the writer has taken
DWORD g_in_flight = 0;     // its length while the writer writes it
CRITICAL_SECTION g_lock;
HANDLE g_wake = nullptr;
HANDLE g_writer = nullptr;
bool g_async = false;      // set by the writer once it runs; read and written under g_lock

void WriteNow(const char* text, DWORD len) {
    DWORD written = 0;
    if (g_log != INVALID_HANDLE_VALUE && len) WriteFile(g_log, text, len, &written, nullptr);
}

DWORD WINAPI Writer(void*) {
    EnterCriticalSection(&g_lock);
    g_async = g_log != INVALID_HANDLE_VALUE;
    LeaveCriticalSection(&g_lock);
    for (;;) {
        WaitForSingleObject(g_wake, INFINITE);
        EnterCriticalSection(&g_lock);
        if (g_queued == 0 || g_in_flight != 0) {
            LeaveCriticalSection(&g_lock);
            continue;
        }
        std::memcpy(g_flight, g_queue, g_queued);
        g_in_flight = g_queued;
        g_queued = 0;
        LeaveCriticalSection(&g_lock);
        WriteNow(g_flight, g_in_flight);
        EnterCriticalSection(&g_lock);
        g_in_flight = 0;
        LeaveCriticalSection(&g_lock);
    }
}

// Everything queued handed to WriteFile, by the calling thread. Waits for a
// chunk the writer has in hand; one whose writer has gone (ExitProcess ends
// every other thread before DLL_PROCESS_DETACH) is written again here, whole:
// a repeated part beats a lost one.
void Drain() {
    for (;;) {
        EnterCriticalSection(&g_lock);
        if (g_in_flight != 0 && g_writer && WaitForSingleObject(g_writer, 0) == WAIT_OBJECT_0) {
            WriteNow(g_flight, g_in_flight);
            g_in_flight = 0;
        }
        if (g_in_flight == 0) {
            WriteNow(g_queue, g_queued);
            g_queued = 0;
            LeaveCriticalSection(&g_lock);
            return;
        }
        LeaveCriticalSection(&g_lock);
        Sleep(1);
    }
}

void Write(const char* text, int len) {
    if (g_log == INVALID_HANDLE_VALUE || len <= 0) return;
    const DWORD n = static_cast<DWORD>(len);
    EnterCriticalSection(&g_lock);
    if (!g_async) {
        WriteNow(text, n);
        LeaveCriticalSection(&g_lock);
        return;
    }
    if (g_queued + n > kQueue) {   // the disk is 4 MB behind: wait for it, as before
        LeaveCriticalSection(&g_lock);
        Drain();
        EnterCriticalSection(&g_lock);
    }
    if (g_queued + n > kQueue) {
        WriteNow(g_queue, g_queued);
        g_queued = 0;
        WriteNow(text, n);
    } else {
        std::memcpy(g_queue + g_queued, text, n);
        g_queued += n;
    }
    LeaveCriticalSection(&g_lock);
    SetEvent(g_wake);
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
    InitializeCriticalSection(&g_lock);
    g_log = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_log == INVALID_HANDLE_VALUE) return;
    // From DllMain the thread starts once the loader lock is let go; until
    // then Write writes itself.
    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (g_wake) g_writer = CreateThread(nullptr, 0, Writer, nullptr, 0, nullptr);
}

void LogClose() {
    if (g_log == INVALID_HANDLE_VALUE) return;
    Drain();
    EnterCriticalSection(&g_lock);
    g_async = false;   // nothing is queued after this
    CloseHandle(g_log);
    g_log = INVALID_HANDLE_VALUE;
    LeaveCriticalSection(&g_lock);
}

void LogFlush() {
    if (g_log == INVALID_HANDLE_VALUE) return;
    Drain();
    FlushFileBuffers(g_log);
}

Utf8Path::Utf8Path(const wchar_t* path) {
    if (WideCharToMultiByte(CP_UTF8, 0, path, -1, text, sizeof text, nullptr, nullptr) == 0)
        std::snprintf(text, sizeof text, "(a path over %u UTF-8 bytes)", static_cast<unsigned>(sizeof text - 1));
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
    if (g_log == INVALID_HANDLE_VALUE) {
        // Before DllMain's LogOpen: a namespace-scope table of the DLL's CRT
        // start-up that placed a code VA (bind.cpp). The log is opened here, as
        // DllMain would open it, so the message is not lost.
        HMODULE self = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&LogOpen), &self))
            LogOpen(self);
    }
    Write("FATAL: ", 7);
    Write(buf, n);
    Write("\r\n", 2);
    LogFlush();
    StateHash_Flush();   // TerminateProcess below flushes no stdio buffer
    if (GetEnvironmentVariableA("BOF3X_SELFTEST_ONLY", nullptr, 0) == 0)
        MessageBoxA(nullptr, buf, "bof3x", MB_OK | MB_ICONERROR | MB_TASKMODAL);
    TerminateProcess(GetCurrentProcess(), 3);
    for (;;) {
    }
}

}  // namespace bof3
