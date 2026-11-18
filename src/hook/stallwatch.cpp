// BOF3X_STALLWATCH: where the main thread is when the frame loop stops
// turning (stallwatch.h, known-defects D304).
//
// The sample - the main thread held still, its program counter, stack
// pointer and stack read - is os/sampler.h's, which exists on Windows only:
// POSIX has no call that reads another thread's registers, and a signal
// handler on the main thread that hands its context over is a design of its
// own, not written. So on POSIX the switch is refused at start (a Fatal that
// says so), and the rest - the watcher thread, the clock, the log beside the
// module, the scan of the stack through MemQuery - is the same code on both.
#include "hook/stallwatch.h"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>

#include "hook/log.h"
#include "os/os.h"

namespace bof3 {
namespace {

constexpr std::uint32_t kPollMs = 50;
constexpr std::uint32_t kRepeatMs = 1000;    // a long stall is sampled again each second
constexpr unsigned kMaxFrames = 32;          // return-address candidates per sample
constexpr std::uint32_t kScanBytes = 0x10000;   // the stack read above esp

std::atomic<std::int32_t> g_beats{0};
const volatile std::uint32_t* g_frame = nullptr;
os::SampledThread* g_main = nullptr;
std::uintptr_t g_stack_base = 0;   // the main thread's stack base (os/stack.h): the top, exclusive
std::uint32_t g_threshold = 0;
std::FILE* g_out = nullptr;
double g_counter_ms = 0;           // milliseconds per tick of os::Counter

void Line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void Line(const char* fmt, ...) {
    char buf[2048];
    const os::LocalTime t = os::LocalNow();
    const int head = std::snprintf(buf, sizeof buf, "%02u:%02u:%02u.%03u ", t.hour, t.minute, t.second, t.millisecond);
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(buf + head, sizeof buf - 2 - head, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    n += head;
    if (n > static_cast<int>(sizeof buf) - 3) n = sizeof buf - 3;
    buf[n++] = '\r';
    buf[n++] = '\n';
    // Each line handed to the system as it is written, as WriteFile did: the
    // log is read while the game is stuck, and may be all that is left.
    std::fwrite(buf, 1, static_cast<std::size_t>(n), g_out);
    std::fflush(g_out);
}

double NowMs() { return static_cast<double>(os::Counter()) * g_counter_ms; }

// Addresses are native (std::uintptr_t): the main thread's stack and the
// modules a sample names lie wherever the width's process puts them, and a
// system DLL's return address is above 4 GB on x86-64 (docs/x64-build-design.md
// 11). The words read off the stack are the width's.
bool Executable(std::uintptr_t a, os::MemInfo* last) {
    if (!(last->size && a >= last->base && a - last->base < last->size)) {
        if (!os::MemQuery(reinterpret_cast<const void*>(a), last)) {
            last->size = 0;
            return false;
        }
    }
    return last->state == os::MemState::Committed && last->executable;
}

struct Sample {
    bool ok;
    std::uintptr_t eip, esp;
    unsigned n;
    std::uintptr_t at[kMaxFrames];
};

// Called while the main thread is held (os/sampler.h): nothing here takes a
// lock - MemQuery and plain reads of committed stack.
void Scan(void* ctx, std::uintptr_t ip, std::uintptr_t sp) {
    Sample* const s = static_cast<Sample*>(ctx);
    s->ok = true;
    s->eip = ip;
    s->esp = sp;
    os::MemInfo last = {};
    // The scan ends with the region esp is in: the thread's stack, or the
    // block the game's own code runs on there (esp outside the TEB's range).
    std::uintptr_t top = sp + kScanBytes;
    os::MemInfo at = {};
    if (os::MemQuery(reinterpret_cast<const void*>(sp), &at)) {
        const std::uintptr_t end = at.base + at.size;
        if (end < top) top = end;
    } else if (g_stack_base && top > g_stack_base) {
        top = g_stack_base;
    }
    for (std::uintptr_t p = sp & ~std::uintptr_t{3}; p + sizeof p <= top && s->n < kMaxFrames; p += sizeof p) {
        const std::uintptr_t v = *reinterpret_cast<const std::uintptr_t*>(p);
        if (v > 0x10000 && Executable(v, &last)) s->at[s->n++] = v;
    }
}

void Take(Sample* s) {
    s->ok = false;
    s->n = 0;
    os::SampleThread(g_main, &Scan, s);
}

// module+offset, resolved after the thread is running again.
void Name(std::uintptr_t a, char* out, std::size_t cap) {
    std::string path;
    std::uintptr_t base = 0;
    if (os::ModulePath(reinterpret_cast<const void*>(a), &path, &base) && base) {
        const std::size_t slash = path.find_last_of("\\/");
        const char* leaf = slash == std::string::npos ? path.c_str() : path.c_str() + slash + 1;
        std::snprintf(out, cap, "%s+0x%X", *leaf ? leaf : "?", static_cast<unsigned>(a - base));
    } else {
        if (sizeof(std::uintptr_t) > 4)
            std::snprintf(out, cap, "0x%016llX", static_cast<unsigned long long>(a));
        else
            std::snprintf(out, cap, "0x%08X", static_cast<unsigned>(a));
    }
}

void Report(const Sample& s, double ms, std::uint32_t frame, bool turning) {
    if (!s.ok) {
        Line("stall       %.0f ms at Frame_Counter %u (%s): the main thread could not be sampled (error %lu)", ms,
             static_cast<unsigned>(frame), turning ? "loop turning, no logic frame" : "loop not turning",
             static_cast<unsigned long>(os::LastError()));
        return;
    }
    char text[1800];
    std::size_t at = 0;
    char name[96];
    Name(s.eip, name, sizeof name);
    if (sizeof(std::uintptr_t) > 4)
        at += std::snprintf(text + at, sizeof text - at, "rip %s rsp 0x%016llX; stack", name, static_cast<unsigned long long>(s.esp));
    else
        at += std::snprintf(text + at, sizeof text - at, "eip %s esp 0x%08X; stack", name, static_cast<unsigned>(s.esp));
    for (unsigned i = 0; i < s.n && at < sizeof text - 100; ++i) {
        Name(s.at[i], name, sizeof name);
        at += std::snprintf(text + at, sizeof text - at, " %s", name);
    }
    Line("stall       %.0f ms at Frame_Counter %u (%s): %s", ms, static_cast<unsigned>(frame),
         turning ? "loop turning, no logic frame" : "loop not turning", text);
}

[[noreturn]] void Watch() {
    std::int32_t beats = g_beats.load();
    std::uint32_t frame = *g_frame;
    double beat_at = NowMs(), frame_at = beat_at, sampled_at = 0;
    bool stalled = false;
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
        const double now = NowMs();
        const std::int32_t b = g_beats.load();
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

void StallWatch_Beat() { g_beats.fetch_add(1); }

void StallWatch_Start(const volatile void* frame_counter) {
    char v[16] = {};
    const std::uint32_t n = os::GetEnv("BOF3X_STALLWATCH", v, sizeof v);
    if (n == 0) return;
    char* end = nullptr;
    const unsigned long ms = n < sizeof v ? std::strtoul(v, &end, 10) : 0;
    if (n >= sizeof v || end == v || *end != 0 || ms < 100 || ms > 600000)
        Fatal("BOF3X_STALLWATCH wants milliseconds, 100 or more, got \"%s\"", v);
    // The compile-time split (os/sampler.h): the POSIX backend has no sampler.
    if (!os::CanSampleThreads())
        Fatal("BOF3X_STALLWATCH: this system has no way to hold the main thread and read where it is (os/sampler.h: "
              "Windows only until a signal-based sampler is written); unset it");
    g_threshold = static_cast<std::uint32_t>(ms);
    g_frame = static_cast<const volatile std::uint32_t*>(frame_counter);
    g_counter_ms = 1000.0 / static_cast<double>(os::CounterFrequency());
    g_stack_base = reinterpret_cast<std::uintptr_t>(os::CurrentStack().base);
    g_main = os::SampleThisThread();
    if (!g_main) Fatal("BOF3X_STALLWATCH: no handle on the main thread (error %lu)", static_cast<unsigned long>(os::LastError()));
    // bof3x.stall.log beside this DLL (and so beside bof3x.log).
    std::string module;
    if (!os::ModulePath(reinterpret_cast<const void*>(&StallWatch_Start), &module))
        Fatal("BOF3X_STALLWATCH: no folder for bof3x.stall.log");
    std::filesystem::path path = std::filesystem::path(os::ToWide(module.c_str()));
    if (module.empty() || !path.has_parent_path()) Fatal("BOF3X_STALLWATCH: no folder for bof3x.stall.log");
    path.replace_filename("bof3x.stall.log");
    const std::string shown = os::ToUtf8(path.wstring().c_str());
    g_out = os::OpenPath(path, "wb");
    if (!g_out) Fatal("BOF3X_STALLWATCH: %s not opened (error %lu)", shown.c_str(), static_cast<unsigned long>(os::LastError()));
    try {
        std::thread(&Watch).detach();
    } catch (const std::system_error& e) {
        Fatal("BOF3X_STALLWATCH: no watcher thread (%s)", e.what());
    }
    Log("stall       BOF3X_STALLWATCH %lu ms: the main thread sampled to %s when no logic frame runs for that long",
        ms, shown.c_str());
    Line("stall       watching from Frame_Counter %u, threshold %lu ms", static_cast<unsigned>(*g_frame), ms);
}

}  // namespace bof3
