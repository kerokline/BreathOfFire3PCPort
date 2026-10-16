// BOF3X_STALLWATCH: where the main thread is when the frame loop stops
// turning (stallwatch.h, known-defects D304).
#include "hook/stallwatch.h"

#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "hook/log.h"

namespace bof3 {
namespace {

constexpr DWORD kPollMs = 50;
constexpr DWORD kRepeatMs = 1000;            // a long stall is sampled again each second
constexpr unsigned kMaxFrames = 32;          // return-address candidates per sample
constexpr std::uint32_t kScanBytes = 0x10000;   // the stack read above esp

volatile LONG g_beats = 0;
const volatile std::uint32_t* g_frame = nullptr;
HANDLE g_main = nullptr;
std::uint32_t g_stack_base = 0;   // NT_TIB StackBase of the main thread: the top, exclusive
DWORD g_threshold = 0;
HANDLE g_out = INVALID_HANDLE_VALUE;
LARGE_INTEGER g_qpf;

void Line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Line(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(buf, sizeof buf - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > static_cast<int>(sizeof buf) - 3) n = sizeof buf - 3;
    buf[n++] = '\r';
    buf[n++] = '\n';
    DWORD w = 0;
    WriteFile(g_out, buf, static_cast<DWORD>(n), &w, nullptr);
}

double NowMs() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return static_cast<double>(q.QuadPart) * 1000.0 / static_cast<double>(g_qpf.QuadPart);
}

bool Executable(std::uint32_t a, MEMORY_BASIC_INFORMATION* last) {
    const auto lo = reinterpret_cast<std::uintptr_t>(last->BaseAddress);
    if (!(last->RegionSize && a >= lo && a - lo < last->RegionSize)) {
        if (VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a)), last, sizeof *last) == 0) {
            last->RegionSize = 0;
            return false;
        }
    }
    if (last->State != MEM_COMMIT) return false;
    const DWORD p = last->Protect & 0xFF;
    return p == PAGE_EXECUTE || p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE ||
           p == PAGE_EXECUTE_WRITECOPY;
}

struct Sample {
    bool ok;
    std::uint32_t eip, esp;
    unsigned n;
    std::uint32_t at[kMaxFrames];
};

// The main thread suspended only between SuspendThread and ResumeThread, and
// nothing in between takes a lock: VirtualQuery and plain reads of committed
// stack.
void Take(Sample* s) {
    s->ok = false;
    s->n = 0;
    if (SuspendThread(g_main) == static_cast<DWORD>(-1)) return;
    CONTEXT c = {};
    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (GetThreadContext(g_main, &c)) {
        s->ok = true;
        s->eip = c.Eip;
        s->esp = c.Esp;
        MEMORY_BASIC_INFORMATION last = {};
        std::uint32_t top = c.Esp + kScanBytes;
        if (g_stack_base && top > g_stack_base) top = g_stack_base;
        for (std::uint32_t p = c.Esp & ~3u; p + 4 <= top && s->n < kMaxFrames; p += 4) {
            const std::uint32_t v = *reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(p));
            if (v > 0x10000 && Executable(v, &last)) s->at[s->n++] = v;
        }
    }
    ResumeThread(g_main);
}

// module+offset, resolved after the thread is running again.
void Name(std::uint32_t a, char* out, std::size_t cap) {
    HMODULE m = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(static_cast<std::uintptr_t>(a)), &m) && m) {
        char path[MAX_PATH];
        const DWORD n = GetModuleFileNameA(m, path, MAX_PATH);
        const char* base = path;
        for (DWORD i = 0; i < n; ++i)
            if (path[i] == '\\' || path[i] == '/') base = path + i + 1;
        std::snprintf(out, cap, "%s+0x%X", n ? base : "?",
                      static_cast<unsigned>(a - static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(m))));
    } else {
        std::snprintf(out, cap, "0x%08X", static_cast<unsigned>(a));
    }
}

void Report(const Sample& s, double ms, std::uint32_t frame, bool turning) {
    if (!s.ok) {
        Line("stall       %.0f ms at Frame_Counter %u (%s): the main thread could not be sampled (error %lu)", ms,
             static_cast<unsigned>(frame), turning ? "loop turning, no logic frame" : "loop not turning",
             GetLastError());
        return;
    }
    char text[1800];
    std::size_t at = 0;
    char name[96];
    Name(s.eip, name, sizeof name);
    at += std::snprintf(text + at, sizeof text - at, "eip %s esp 0x%08X; stack", name, static_cast<unsigned>(s.esp));
    for (unsigned i = 0; i < s.n && at < sizeof text - 100; ++i) {
        Name(s.at[i], name, sizeof name);
        at += std::snprintf(text + at, sizeof text - at, " %s", name);
    }
    Line("stall       %.0f ms at Frame_Counter %u (%s): %s", ms, static_cast<unsigned>(frame),
         turning ? "loop turning, no logic frame" : "loop not turning", text);
}

DWORD WINAPI Watch(void*) {
    LONG beats = g_beats;
    std::uint32_t frame = *g_frame;
    double beat_at = NowMs(), frame_at = beat_at, sampled_at = 0;
    bool stalled = false;
    for (;;) {
        Sleep(kPollMs);
        const double now = NowMs();
        const LONG b = g_beats;
        const std::uint32_t f = *g_frame;
        if (b != beats) {
            beats = b;
            beat_at = now;
        }
        if (f != frame) {
            frame = f;
            frame_at = now;
        }
        // The loop not turning is the stall; turning without a frame (a
        // pause, an inactive window under BOF3X_BACKGROUND=0) is reported
        // the same way, so either shows.
        const double since = now - frame_at;
        const bool turning = now - beat_at < g_threshold;
        if (since >= g_threshold) {
            if (!stalled || now - sampled_at >= kRepeatMs) {
                Sample s;
                Take(&s);
                Report(s, since, f, turning);
                stalled = true;
                sampled_at = now;
            }
        } else if (stalled) {
            Line("stall       over at Frame_Counter %u", static_cast<unsigned>(f));
            stalled = false;
        }
    }
}

}  // namespace

void StallWatch_Beat() { InterlockedIncrement(&g_beats); }

void StallWatch_Start(const volatile void* frame_counter) {
    char v[16] = {};
    const DWORD n = GetEnvironmentVariableA("BOF3X_STALLWATCH", v, sizeof v);
    if (n == 0) return;
    char* end = nullptr;
    const unsigned long ms = n < sizeof v ? std::strtoul(v, &end, 10) : 0;
    if (n >= sizeof v || end == v || *end != 0 || ms < 100 || ms > 600000)
        Fatal("BOF3X_STALLWATCH wants milliseconds, 100 or more, got \"%s\"", v);
    g_threshold = static_cast<DWORD>(ms);
    g_frame = static_cast<const volatile std::uint32_t*>(frame_counter);
    QueryPerformanceFrequency(&g_qpf);
    g_stack_base = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(reinterpret_cast<NT_TIB*>(NtCurrentTeb())->StackBase));
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main,
                         THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0))
        Fatal("BOF3X_STALLWATCH: no handle on the main thread (error %lu)", GetLastError());
    // bof3x.stall.log beside this DLL (and so beside bof3x.log).
    HMODULE self = nullptr;
    wchar_t path[MAX_PATH];
    DWORD len = 0;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&StallWatch_Start), &self))
        len = GetModuleFileNameW(self, path, MAX_PATH);
    wchar_t* slash = nullptr;
    for (DWORD i = 0; i < len; ++i)
        if (path[i] == L'\\' || path[i] == L'/') slash = path + i;
    if (!slash || (slash - path) + 18 >= MAX_PATH) Fatal("BOF3X_STALLWATCH: no folder for bof3x.stall.log");
    wcscpy(slash + 1, L"bof3x.stall.log");
    g_out = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_out == INVALID_HANDLE_VALUE)
        Fatal("BOF3X_STALLWATCH: %s not opened (error %lu)", Utf8Path(path).text, GetLastError());
    HANDLE t = CreateThread(nullptr, 0, Watch, nullptr, 0, nullptr);
    if (!t) Fatal("BOF3X_STALLWATCH: no watcher thread (error %lu)", GetLastError());
    CloseHandle(t);
    Log("stall       BOF3X_STALLWATCH %lu ms: the main thread sampled to %s when no logic frame runs for that long",
        ms, Utf8Path(path).text);
    Line("stall       watching from Frame_Counter %u, threshold %lu ms", static_cast<unsigned>(*g_frame), ms);
}

}  // namespace bof3
