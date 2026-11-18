#include "hook/log.h"

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

#include "hook/statehash.h"
#include "os/os.h"

namespace bof3 {
namespace {

os::OutFile g_log = os::kNoFile;

// The log is written by a thread of its own (known-defects D304): a WriteFile
// on the game's thread waited for the disk, and with the disk saturated by
// other work Windows' write throttling held it - and so the frame - for up to
// 34 s while a scripted run logged its line a frame. Lines go to a queue
// here; the writer takes the whole queue and writes it outside the lock.
// Before the writer runs (start-up, under the loader lock), with the queue
// full, and in Drain, the calling thread writes itself, in order. No heap:
// the crash reporter logs through this too.
//
// The crash path (crash.cpp's Report) flushes through Drain, which takes
// g_lock: a thread that faulted while holding it - in Write, or the writer in
// its copy - leaves it held. The report therefore runs on a thread of its own
// and the faulting thread waits for it with a bound (20 s), never forever; a
// held lock costs the report's lines, not the crash's progress.
constexpr std::uint32_t kQueue = 4u << 20;
char g_queue[kQueue];
std::uint32_t g_queued = 0;
char g_flight[kQueue];            // the chunk the writer has taken
std::uint32_t g_in_flight = 0;    // its length while the writer writes it
std::mutex g_lock;
os::Event g_wake = nullptr;       // auto-reset: one Write's set wakes the writer once
os::Thread g_writer = nullptr;
bool g_async = false;             // set by the writer once it runs; read and written under g_lock

void WriteNow(const char* text, std::uint32_t len) {
    if (g_log != os::kNoFile && len) os::OutWrite(g_log, text, len);
}

void Writer(void*) {
    g_lock.lock();
    g_async = g_log != os::kNoFile;
    g_lock.unlock();
    for (;;) {
        os::EventWait(g_wake, os::kWaitForever);
        g_lock.lock();
        if (g_queued == 0 || g_in_flight != 0) {
            g_lock.unlock();
            continue;
        }
        std::memcpy(g_flight, g_queue, g_queued);
        g_in_flight = g_queued;
        g_queued = 0;
        g_lock.unlock();
        WriteNow(g_flight, g_in_flight);
        g_lock.lock();
        g_in_flight = 0;
        g_lock.unlock();
    }
}

// Everything queued handed to the system, by the calling thread. Waits for a
// chunk the writer has in hand; one whose writer has gone (ExitProcess ends
// every other thread before DLL_PROCESS_DETACH) is written again here, whole:
// a repeated part beats a lost one.
void Drain() {
    for (;;) {
        g_lock.lock();
        if (g_in_flight != 0 && g_writer && os::ThreadEnded(g_writer)) {
            WriteNow(g_flight, g_in_flight);
            g_in_flight = 0;
        }
        if (g_in_flight == 0) {
            WriteNow(g_queue, g_queued);
            g_queued = 0;
            g_lock.unlock();
            return;
        }
        g_lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Write(const char* text, int len) {
    if (g_log == os::kNoFile || len <= 0) return;
    const std::uint32_t n = static_cast<std::uint32_t>(len);
    g_lock.lock();
    if (!g_async) {
        WriteNow(text, n);
        g_lock.unlock();
        return;
    }
    if (g_queued + n > kQueue) {   // the disk is 4 MB behind: wait for it, as before
        g_lock.unlock();
        Drain();
        g_lock.lock();
    }
    if (g_queued + n > kQueue) {
        WriteNow(g_queue, g_queued);
        g_queued = 0;
        WriteNow(text, n);
    } else {
        std::memcpy(g_queue + g_queued, text, n);
        g_queued += n;
    }
    g_lock.unlock();
    os::EventSet(g_wake);
}

int Format(char* buf, int cap, const char* fmt, va_list ap) {
    int n = std::vsnprintf(buf, static_cast<size_t>(cap), fmt, ap);
    if (n < 0) return 0;
    return n < cap ? n : cap - 1;
}

}  // namespace

// `dll_module` is any address inside the engine's module (the DLL's handle,
// its base, on Windows); the log is that module's path with the extension
// .log: bof3x.dll -> bof3x.log.
void LogOpen(void* dll_module) {
    std::string utf8;
    if (!os::ModulePath(dll_module, &utf8) || utf8.size() < 4) return;
    std::filesystem::path path(os::ToWide(utf8.c_str()));
    if (path.empty()) return;
    path.replace_extension(L".log");
    g_log = os::OutCreate(path);
    if (g_log == os::kNoFile) return;
    // From DllMain the thread starts once the loader lock is let go; until
    // then Write writes itself.
    g_wake = os::EventCreate(false, false);
    if (g_wake) g_writer = os::StartThread(Writer, nullptr);
}

void LogClose() {
    if (g_log == os::kNoFile) return;
    Drain();
    g_lock.lock();
    g_async = false;   // nothing is queued after this
    os::OutClose(g_log);
    g_log = os::kNoFile;
    g_lock.unlock();
}

void LogFlush() {
    if (g_log == os::kNoFile) return;
    Drain();
    os::OutFlush(g_log);
}

// By hand, with no heap: the crash report names its dump through this, and a
// thread that faulted inside the allocator may hold the heap's lock. UTF-16
// on Windows (a lone surrogate becomes U+FFFD, as WideCharToMultiByte makes
// it), UTF-32 on POSIX.
Utf8Path::Utf8Path(const wchar_t* path) {
    std::size_t n = 0;
    bool fits = true;
    for (const wchar_t* p = path ? path : L""; *p && fits; ++p) {
        std::uint32_t c = static_cast<std::uint32_t>(*p);
        if constexpr (sizeof(wchar_t) == 2) {
            if (c >= 0xD800 && c < 0xDC00 && p[1] >= 0xDC00 && p[1] < 0xE000) {
                c = 0x10000 + ((c - 0xD800) << 10) + (static_cast<std::uint32_t>(p[1]) - 0xDC00);
                ++p;
            } else if (c >= 0xD800 && c < 0xE000) {
                c = 0xFFFD;
            }
        } else if ((c >= 0xD800 && c < 0xE000) || c > 0x10FFFF) {
            c = 0xFFFD;
        }
        unsigned char b[4];
        std::size_t k;
        if (c < 0x80) {
            b[0] = static_cast<unsigned char>(c), k = 1;
        } else if (c < 0x800) {
            b[0] = static_cast<unsigned char>(0xC0 | (c >> 6)), b[1] = static_cast<unsigned char>(0x80 | (c & 0x3F)), k = 2;
        } else if (c < 0x10000) {
            b[0] = static_cast<unsigned char>(0xE0 | (c >> 12)), b[1] = static_cast<unsigned char>(0x80 | ((c >> 6) & 0x3F));
            b[2] = static_cast<unsigned char>(0x80 | (c & 0x3F)), k = 3;
        } else {
            b[0] = static_cast<unsigned char>(0xF0 | (c >> 18)), b[1] = static_cast<unsigned char>(0x80 | ((c >> 12) & 0x3F));
            b[2] = static_cast<unsigned char>(0x80 | ((c >> 6) & 0x3F)), b[3] = static_cast<unsigned char>(0x80 | (c & 0x3F)), k = 4;
        }
        if (n + k >= sizeof text) {
            fits = false;
            break;
        }
        std::memcpy(text + n, b, k);
        n += k;
    }
    text[n] = '\0';
    if (!fits) std::snprintf(text, sizeof text, "(a path over %u UTF-8 bytes)", static_cast<unsigned>(sizeof text - 1));
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
    if (g_log == os::kNoFile) {
        // Before DllMain's LogOpen: a namespace-scope table of the DLL's CRT
        // start-up that placed a code VA (bind.cpp). The log is opened here, as
        // DllMain would open it, so the message is not lost.
        LogOpen(reinterpret_cast<void*>(&LogOpen));
    }
    Write("FATAL: ", 7);
    Write(buf, n);
    Write("\r\n", 2);
    LogFlush();
    StateHash_Flush();   // EndProcess below flushes no stdio buffer
    if (os::GetEnv("BOF3X_SELFTEST_ONLY", nullptr, 0) == 0) os::ShowMessage("bof3x", buf, true, true);
    os::EndProcess(3);
}

}  // namespace bof3
