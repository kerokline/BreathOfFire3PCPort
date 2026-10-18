// The gap watch (gapwatch.h; docs/install-gap-reads.md).
#include "hook/gapwatch.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "bof3/symbols.gen.h"
#include "hook/exe_image.h"
#include "hook/exe_layout.h"
#include "hook/host.h"
#include "hook/input_script.h"
#include "hook/log.h"

namespace bof3 {
namespace {

constexpr std::uint32_t kPage = 0x1000;
constexpr DWORD kGuardViolation = 0x80000001;   // STATUS_GUARD_PAGE_VIOLATION
constexpr DWORD kTrapFlag = 0x100;
constexpr std::uint8_t kPushfd = 0x9C;
constexpr int kMaxRanges = 2048;   // 388 rows a build in recipes/exe-gaps.tsv at most (2026-10-17), every class
constexpr int kMaxPages = 256;     // 43 gap pages on psx-de, 71 with unread and language
constexpr int kMaxThreads = 16;
constexpr int kPendingPerThread = 4;   // pages one instruction can touch: a movs between two, each split
constexpr int kTop = 8;                // a page's busiest readers kept by address
constexpr int kCallers = 4;            // return addresses kept for a first read
constexpr int kFlushEvery = 60;        // latches between rewrites of the file

// Everything the handler touches is static: it runs on whatever stack took
// the fault, and game logic runs on 16 KB task stacks (calltrace.cpp).
struct WRange {
    std::uint32_t lo, hi;
    const char* how;
    const char* from;
    volatile LONG claimed;   // the first access is being recorded
    volatile LONG ready;     // and has been
    std::uint32_t eip, frame, rframe, area, thread, kind;
    std::uint32_t callers[kCallers];
    volatile LONG reads, writes;
    volatile LONG edge;      // accesses starting up to 7 bytes below lo (a wider read may reach it)
    std::uint32_t edge_eip;
};
struct WPage {
    std::uint32_t base;
    DWORD protect;           // without the guard
    volatile LONG faults, range_faults, window, parked, parks;
    std::uint32_t top_eip[kTop];
    volatile LONG top_n[kTop];
    volatile LONG top_other;
};
struct Pending {
    volatile LONG thread;
    int n;
    int page[kPendingPerThread];
    bool pushfd;
};

WRange g_range[kMaxRanges];   // sorted by lo, disjoint
int g_ranges = 0;
WPage g_page[kMaxPages];      // sorted by base
int g_pages = 0;
Pending g_pending[kMaxThreads];
LONG g_budget = 16;
volatile LONG g_faults = 0, g_steps = 0, g_unpending = 0;
std::uint32_t g_dll_lo = 0, g_dll_hi = 0;
bool g_on = false;            // the real watch
bool g_pause_for_hash = false;
PVOID g_handler = nullptr;

// The file.
HANDLE g_out = INVALID_HANDLE_VALUE;
char g_build[32] = "";
char g_route[96] = "none";
char g_classes[64] = "gap";
unsigned g_ticks = 0;
LARGE_INTEGER g_t0{}, g_freq{};

std::uint32_t Va(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void* Ptr(std::uint32_t va) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(va)); }

int FindPage(std::uint32_t base) {
    int lo = 0, hi = g_pages;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (g_page[mid].base < base) lo = mid + 1;
        else hi = mid;
    }
    return lo < g_pages && g_page[lo].base == base ? lo : -1;
}

// The first range whose hi is above addr.
int FirstAbove(std::uint32_t addr) {
    int lo = 0, hi = g_ranges;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (g_range[mid].hi <= addr) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

void Guard(int p) {
    DWORD old = 0;
    VirtualProtect(Ptr(g_page[p].base), kPage, g_page[p].protect | PAGE_GUARD, &old);
}
void Unguard(int p) {
    DWORD old = 0;
    VirtualProtect(Ptr(g_page[p].base), kPage, g_page[p].protect, &old);
}

bool InDll(std::uint32_t v) { return v >= g_dll_lo + 6 && v < g_dll_hi; }

// A stack word that points into our DLL just after a call: E8 rel32, or FF
// in the 2-, 3- and 6-byte forms - crash.cpp's filter, a scan and not a walk.
bool AfterCall(std::uint32_t v) {
    if (!InDll(v)) return false;
    const auto* b = static_cast<const std::uint8_t*>(Ptr(v - 6));
    return b[1] == 0xE8 || b[4] == 0xFF || b[3] == 0xFF || b[0] == 0xFF;
}

void Callers(const CONTEXT* ctx, std::uint32_t* out) {
    for (int k = 0; k < kCallers; ++k) out[k] = 0;
    const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const std::uint32_t top = Va(tib->StackBase);
    std::uint32_t at = ctx->Esp;
    if (at >= top || (at & 3)) return;
    const std::uint32_t end = top - at > 0x800 ? at + 0x800 : top;
    int n = 0;
    for (; at + 4 <= end && n < kCallers; at += 4) {
        const std::uint32_t v = *static_cast<const std::uint32_t*>(Ptr(at));
        if (AfterCall(v)) out[n++] = v;
    }
}

void Top(WPage& pg, std::uint32_t eip) {
    for (int k = 0; k < kTop; ++k) {
        if (pg.top_eip[k] == eip) {
            InterlockedIncrement(&pg.top_n[k]);
            return;
        }
        if (pg.top_eip[k] == 0) {
            pg.top_eip[k] = eip;
            InterlockedIncrement(&pg.top_n[k]);
            return;
        }
    }
    InterlockedIncrement(&pg.top_other);
}

Pending* PendingOf(LONG me) {
    for (Pending& p : g_pending)
        if (p.thread == me) return &p;
    return nullptr;
}

LONG CALLBACK OnException(EXCEPTION_POINTERS* info) {
    EXCEPTION_RECORD* rec = info->ExceptionRecord;
    CONTEXT* ctx = info->ContextRecord;
    const LONG me = static_cast<LONG>(GetCurrentThreadId());

    if (rec->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        Pending* p = PendingOf(me);
        if (!p) return EXCEPTION_CONTINUE_SEARCH;
        for (int k = 0; k < p->n; ++k)
            if (!g_page[p->page[k]].parked) Guard(p->page[k]);
        // The instruction stepped was a pushfd: it saved the trap flag we set,
        // and a popfd would step again with nobody expecting it
        // (calltrace.cpp's 0x5A9A30 case).
        if (p->pushfd) *static_cast<DWORD*>(Ptr(ctx->Esp)) &= ~kTrapFlag;
        p->n = 0;
        p->pushfd = false;
        InterlockedExchange(&p->thread, 0);
        InterlockedIncrement(&g_steps);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (rec->ExceptionCode != kGuardViolation || rec->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
    const auto addr = static_cast<std::uint32_t>(rec->ExceptionInformation[1]);
    const int pi = FindPage(addr & ~(kPage - 1));
    if (pi < 0) return EXCEPTION_CONTINUE_SEARCH;   // not ours: a thread stack's guard, someone else's
    WPage& pg = g_page[pi];
    InterlockedIncrement(&g_faults);
    InterlockedIncrement(&pg.faults);
    const std::uint32_t eip = ctx->Eip;
    Top(pg, eip);
    const auto kind = static_cast<std::uint32_t>(rec->ExceptionInformation[0]);   // 0 read, 1 write, 8 execute

    const int ri = FirstAbove(addr);
    if (ri < g_ranges && g_range[ri].lo <= addr) {
        WRange& r = g_range[ri];
        InterlockedIncrement(&pg.range_faults);
        InterlockedIncrement(kind == 1 ? &r.writes : &r.reads);
        if (InterlockedCompareExchange(&r.claimed, 1, 0) == 0) {
            r.eip = eip;
            r.kind = kind;
            r.thread = static_cast<std::uint32_t>(me);
            r.frame = *static_cast<const volatile std::uint32_t*>(Ptr(Va(&Frame_Counter)));
            r.area = *static_cast<const volatile std::uint16_t*>(Ptr(Va(&Game_AreaNumber)));
            r.rframe = InputScript_Frame();
            Callers(ctx, r.callers);
            InterlockedExchange(&r.ready, 1);
        }
    } else if (ri < g_ranges && g_range[ri].lo - addr <= 7) {
        WRange& r = g_range[ri];
        if (InterlockedIncrement(&r.edge) == 1) r.edge_eip = eip;
    }

    // Past the budget the page stays unguarded until the next latch re-arms it.
    if (InterlockedIncrement(&pg.window) > g_budget) {
        if (InterlockedExchange(&pg.parked, 1) == 0) InterlockedIncrement(&pg.parks);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    Pending* p = PendingOf(me);
    if (!p) {
        for (Pending& q : g_pending)
            if (InterlockedCompareExchange(&q.thread, me, 0) == 0) {
                p = &q;
                p->n = 0;
                p->pushfd = false;
                break;
            }
        if (!p) Fatal("gapwatch: more than %d threads stepping over guarded pages at once", kMaxThreads);
    }
    if (p->n == kPendingPerThread)
        Fatal("gapwatch: one instruction at 0x%08lX touched more than %d guarded pages", (unsigned long)eip,
              kPendingPerThread);
    p->page[p->n++] = pi;
    if (*static_cast<const std::uint8_t*>(Ptr(eip)) == kPushfd) p->pushfd = true;
    ctx->EFlags |= kTrapFlag;
    return EXCEPTION_CONTINUE_EXECUTION;
}

// The pages the ranges lie on, sorted, each with its protection without a guard.
void BuildPages() {
    g_pages = 0;
    for (int i = 0; i < g_ranges; ++i)
        for (std::uint32_t b = g_range[i].lo & ~(kPage - 1); b < g_range[i].hi; b += kPage) {
            if (g_pages && g_page[g_pages - 1].base == b) continue;
            if (g_pages == kMaxPages) Fatal("gapwatch: more than %d pages to guard", kMaxPages);
            MEMORY_BASIC_INFORMATION m{};
            if (!VirtualQuery(Ptr(b), &m, sizeof m) || m.State != MEM_COMMIT || (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                Fatal("gapwatch: the page 0x%08X is not committed and accessible (state 0x%lX, protect 0x%lX)", (unsigned)b,
                      m.State, m.Protect);
            WPage& pg = g_page[g_pages++];
            std::memset(&pg, 0, sizeof pg);
            pg.base = b;
            pg.protect = m.Protect;
        }
}

void Install() {
    g_handler = AddVectoredExceptionHandler(1, OnException);   // first: the guard faults and steps are ours
    if (!g_handler) Fatal("gapwatch: no vectored handler");
}

void ResetTables() {
    std::memset(g_range, 0, sizeof g_range);
    std::memset(g_page, 0, sizeof g_page);
    std::memset(g_pending, 0, sizeof g_pending);
    g_ranges = 0;
    g_pages = 0;
    g_faults = 0;
    g_steps = 0;
    g_unpending = 0;
}

// Re-arms every page and opens a new window: the parked pages too. The
// calling thread is never mid-step here, so a pending entry of its own is a
// step that never came (a popfd that cleared the flag): dropped.
void Rearm() {
    if (Pending* p = PendingOf(static_cast<LONG>(GetCurrentThreadId()))) {
        p->n = 0;
        p->pushfd = false;
        InterlockedExchange(&p->thread, 0);
        InterlockedIncrement(&g_unpending);
    }
    for (int i = 0; i < g_pages; ++i) {
        g_page[i].window = 0;
        g_page[i].parked = 0;
        Guard(i);
    }
}

// An address as the file writes it: the exe's own by value (the stub page is
// Capcom's address space), a module's as name+offset (our DLL is ASLR'd;
// tools/gapwatch.py symbolises bof3x.dll+offset).
void Name(std::uint32_t v, char* out, size_t cap) {
    if (v == 0) {
        std::snprintf(out, cap, "-");
        return;
    }
    HMODULE m = nullptr;
    char path[MAX_PATH];
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCWSTR>(Ptr(v)), &m) &&
        m && GetModuleFileNameA(m, path, sizeof path)) {
        const char* base = std::strrchr(path, '\\');
        base = base ? base + 1 : path;
        if (Va(m) == exe_layout::kImageBase) std::snprintf(out, cap, "0x%08X", (unsigned)v);
        else std::snprintf(out, cap, "%s+0x%X", base, (unsigned)(v - Va(m)));
    } else {
        std::snprintf(out, cap, "0x%08X", (unsigned)v);
    }
}

char g_text[1 << 19];

void Write() {
    if (g_out == INVALID_HANDLE_VALUE) return;
    size_t n = 0;
    auto put = [&](const char* fmt, auto... args) {
        if (n >= sizeof g_text - 1024) Fatal("gapwatch: the summary is longer than %u bytes", (unsigned)sizeof g_text);
        n += static_cast<size_t>(std::snprintf(g_text + n, sizeof g_text - n, fmt, args...));
    };
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    const double secs = g_freq.QuadPart ? double(now.QuadPart - g_t0.QuadPart) / double(g_freq.QuadPart) : 0.0;
    int read = 0;
    for (int i = 0; i < g_ranges; ++i) read += g_range[i].ready ? 1 : 0;
    put("# gapwatch (src/hook/gapwatch.cpp; docs/install-gap-reads.md): build %s, route %s, classes %s, budget %ld "
        "faults a page a latch\r\n", g_build, g_route, g_classes, (long)g_budget);
    put("# latches %u, Frame_Counter %u, recipe frame %u, %.1f s since armed (%.2f ms a latch); %d ranges on %d pages, "
        "%d touched; %ld guard faults, %ld steps, %ld steps that never came\r\n",
        g_ticks, (unsigned)*static_cast<const volatile std::uint32_t*>(Ptr(Va(&Frame_Counter))), InputScript_Frame(), secs,
        g_ticks ? secs * 1000.0 / g_ticks : 0.0, g_ranges, g_pages, read, (long)g_faults, (long)g_steps, (long)g_unpending);
    put("%s", "range\tlo\thi\thow\tsymbol\tread\tfirst_frame\tfirst_recipe_frame\tfirst_area\tkind\tthread\treader\tcallers"
        "\treads\twrites\tedge\tedge_reader\r\n");
    char a[64], c[4][64], e[64];
    for (int i = 0; i < g_ranges; ++i) {
        const WRange& r = g_range[i];
        Name(r.edge_eip, e, sizeof e);
        if (!r.ready) {
            put("range\t0x%06X\t0x%06X\t%s\t%s\tno\t-\t-\t-\t-\t-\t-\t-\t%ld\t%ld\t%ld\t%s\r\n", (unsigned)r.lo,
                (unsigned)r.hi, r.how, r.from, (long)r.reads, (long)r.writes, (long)r.edge, e);
            continue;
        }
        Name(r.eip, a, sizeof a);
        for (int k = 0; k < kCallers; ++k) Name(r.callers[k], c[k], sizeof c[k]);
        put("range\t0x%06X\t0x%06X\t%s\t%s\tyes\t%u\t%u\t0x%04X\t%s\t%u\t%s\t%s,%s,%s,%s\t%ld\t%ld\t%ld\t%s\r\n",
            (unsigned)r.lo, (unsigned)r.hi, r.how, r.from, (unsigned)r.frame, (unsigned)r.rframe, (unsigned)r.area,
            r.kind == 0 ? "read" : r.kind == 1 ? "write" : "execute", (unsigned)r.thread, a, c[0], c[1], c[2], c[3],
            (long)r.reads, (long)r.writes, (long)r.edge, e);
    }
    put("%s", "page\tbase\tranges\tfaults\trange_faults\tlatches_parked\tbusiest_readers\r\n");
    for (int i = 0; i < g_pages; ++i) {
        const WPage& pg = g_page[i];
        int nr = 0;
        for (int k = 0; k < g_ranges; ++k)
            if (g_range[k].lo < pg.base + kPage && g_range[k].hi > pg.base) ++nr;
        put("page\t0x%06X\t%d\t%ld\t%ld\t%ld\t", (unsigned)pg.base, nr, (long)pg.faults, (long)pg.range_faults,
            (long)pg.parks);
        for (int k = 0; k < kTop && pg.top_eip[k]; ++k) {
            Name(pg.top_eip[k], a, sizeof a);
            put("%s%s:%ld", k ? "," : "", a, (long)pg.top_n[k]);
        }
        if (pg.top_other) put(",other:%ld", (long)pg.top_other);
        put("%s", "\r\n");
    }
    SetFilePointer(g_out, 0, nullptr, FILE_BEGIN);
    DWORD written = 0;
    WriteFile(g_out, g_text, static_cast<DWORD>(n), &written, nullptr);
    SetEndOfFile(g_out);
    FlushFileBuffers(g_out);
}

bool ClassWanted(const char* how) {
    const size_t len = std::strlen(how);
    for (const char* p = g_classes; *p;) {
        const char* end = std::strchr(p, ',');
        const size_t n = end ? static_cast<size_t>(end - p) : std::strlen(p);
        if (n == len && std::strncmp(p, how, n) == 0) return true;
        if (!end) break;
        p = end + 1;
    }
    return false;
}

void ReadDll(void* module) {
    auto* base = static_cast<const std::uint8_t*>(module);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    g_dll_lo = Va(base);
    g_dll_hi = g_dll_lo + nt->OptionalHeader.SizeOfImage;
}

void Check(bool ok, const char* what) {
    if (!ok) Fatal("gapwatch self-test: %s", what);
}

bool Guarded(std::uint32_t va) {
    MEMORY_BASIC_INFORMATION m{};
    return VirtualQuery(Ptr(va), &m, sizeof m) && (m.Protect & PAGE_GUARD);
}

// The reads the self-test plants: out of line, so the reader's address is
// one instruction of ours the test can bound.
__attribute__((noinline)) std::uint8_t PlantedRead(const volatile std::uint8_t* p) { return *p; }
__attribute__((noinline)) std::uint32_t PlantedRead32(const std::uint8_t* p) {
    std::uint32_t v;
    asm volatile("movl (%1), %0" : "=r"(v) : "r"(p) : "memory");   // one instruction, unaligned
    return v;
}
__attribute__((noinline)) void PlantedWrite(volatile std::uint8_t* p, std::uint8_t v) { *p = v; }
__attribute__((noinline)) void PlantedCopy(void* to, const void* from, size_t n) {
    // Byte by byte through volatile pointers: the compiler may not fold it
    // into a call that reads the source from somewhere else.
    auto* d = static_cast<volatile std::uint8_t*>(to);
    auto* s = static_cast<const volatile std::uint8_t*>(from);
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
}

}  // namespace

void GapWatch_SelfTest() {
    if (g_on || g_handler) Fatal("gapwatch self-test: the watch is already armed");
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&GapWatch_SelfTest), &self);
    ReadDll(self);

    // Off: no page of .data guarded (the watch arms only in GapWatch_Start).
    const bool off = GetEnvironmentVariableA("BOF3X_GAPWATCH", nullptr, 0) == 0;
    unsigned data_pages = 0;
    if (off) {
        for (std::uint32_t b = exe_layout::kDataVa; b < exe_layout::kDataVa + exe_layout::kDataBytes; b += kPage, ++data_pages)
            Check(!Guarded(b), "BOF3X_GAPWATCH unset, yet a page of .data is guarded");
    }

    // Three scratch pages: two watched, the third never guarded.
    auto* s = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 3 * kPage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Check(s != nullptr, "VirtualAlloc of the scratch pages failed");
    const std::uint8_t plant[4] = {0x5A, 0xA5, 0x3C, 0xC3};
    std::memcpy(s + 0x100, plant, 4);
    s[0x800] = 0x77;
    std::memcpy(s + kPage + 0x10, plant, 4);
    s[2 * kPage + 0x40] = 0x99;
    std::uint8_t before[2 * kPage];
    std::memcpy(before, s, sizeof before);

    ResetTables();
    const std::uint32_t b = Va(s);
    g_range[0] = {};
    g_range[0].lo = b + 0x100;
    g_range[0].hi = b + 0x104;
    g_range[0].how = "gap";
    g_range[0].from = "selftest_a";
    g_range[1] = {};
    g_range[1].lo = b + kPage + 0x10;
    g_range[1].hi = b + kPage + 0x14;
    g_range[1].how = "gap";
    g_range[1].from = "selftest_b";
    g_ranges = 2;
    BuildPages();
    Check(g_pages == 2, "two scratch ranges should be two pages");
    const LONG saved_budget = g_budget;
    g_budget = 4;
    Install();
    Rearm();
    Check(Guarded(b) && Guarded(b + kPage) && !Guarded(b + 2 * kPage), "the scratch pages are not guarded as set");

    // 1. A read of the page outside every range: a fault, no range touched, the guard back.
    Check(PlantedRead(s + 0x800) == 0x77, "a read outside the range read the wrong value");
    Check(g_page[0].faults == 1 && g_page[0].range_faults == 0 && !g_range[0].ready, "a read outside the range was counted as the range's");
    Check(Guarded(b), "the page is not guarded again after a read");

    // 2. The planted read: logged once, the value it read the planted one.
    Rearm();
    Check(PlantedRead(s + 0x101) == 0xA5, "the planted read did not read the planted value");
    Check(g_range[0].ready && g_range[0].reads == 1 && g_range[0].kind == 0, "the planted read was not recorded as the range's first read");
    const std::uint32_t first_eip = g_range[0].eip;
    Check(first_eip >= Va(reinterpret_cast<void*>(&PlantedRead)) && first_eip < Va(reinterpret_cast<void*>(&PlantedRead)) + 64,
          "the first read's reader is not PlantedRead");
    Check(Guarded(b), "the page is not guarded again after the planted read");

    // 3. Again: counted, but the first read stays the first.
    Check(PlantedRead(s + 0x102) == 0x3C, "the second read read the wrong value");
    Check(g_range[0].reads == 2 && g_range[0].eip == first_eip, "the second read replaced the first");

    // 4. A write of the same value: counted as a write.
    PlantedWrite(s + 0x103, 0xC3);
    Check(g_range[0].writes == 1, "a write was not counted as one");

    // 5. One instruction across both pages (an unaligned word on the
    // boundary: two faults, one step, both guards back), then a copy across
    // them through the second range, under a budget it does not reach.
    Rearm();
    std::uint32_t word = 0;
    std::memcpy(&word, before + kPage - 2, 4);
    Check(PlantedRead32(s + kPage - 2) == word, "the read across the page boundary read the wrong value");
    Check(Guarded(b) && Guarded(b + kPage), "a page is not guarded again after the read across the boundary");
    g_budget = 256;
    Rearm();
    std::uint8_t got[0x40];
    PlantedCopy(got, s + kPage - 0x20, sizeof got);
    g_budget = 4;
    Check(std::memcmp(got, before + kPage - 0x20, sizeof got) == 0, "the copy across two pages read the wrong bytes");
    Check(g_range[1].ready && g_range[1].reads == 4, "the copy did not touch the second range's four bytes");
    Check(Guarded(b) && Guarded(b + kPage), "a page is not guarded again after the copy");

    // 6. The budget: past four faults in one window the page is parked, then re-armed.
    Rearm();
    const LONG f0 = g_page[0].faults;
    for (int i = 0; i < 10; ++i) Check(PlantedRead(s + 0x800) == 0x77, "a read in the budget loop read the wrong value");
    Check(g_page[0].faults - f0 == g_budget + 1, "the page faulted past its budget");
    Check(g_page[0].parked && g_page[0].parks >= 1 && !Guarded(b), "the page over its budget is not parked");
    Rearm();
    Check(Guarded(b) && !g_page[0].parked, "the latch did not re-arm the parked page");

    // 7. The page never guarded: no fault.
    const LONG all = g_faults;
    Check(PlantedRead(s + 2 * kPage + 0x40) == 0x99, "the unguarded page read the wrong value");
    Check(g_faults == all, "a read of an unguarded page faulted");

    // Down, and the bytes untouched.
    for (int i = 0; i < g_pages; ++i) Unguard(i);
    RemoveVectoredExceptionHandler(g_handler);
    g_handler = nullptr;
    Check(std::memcmp(s, before, sizeof before) == 0, "the watched bytes changed");
    const long faults = g_faults, steps = g_steps;
    VirtualFree(s, 0, MEM_RELEASE);
    ResetTables();
    g_budget = saved_budget;
    if (off)
        Log("gapwatch self-test: passed - a planted read logged once at its reader, the value untouched, the guard back "
            "after each access; a write, a copy across two pages, a park past the budget and its re-arm, an unguarded page "
            "that never faulted (%ld faults, %ld steps); BOF3X_GAPWATCH unset and none of .data's %u pages guarded",
            faults, steps, data_pages);
    else
        Log("gapwatch self-test: passed - a planted read logged once at its reader, the value untouched, the guard back "
            "after each access; a write, a copy across two pages, a park past the budget and its re-arm, an unguarded page "
            "that never faulted (%ld faults, %ld steps); BOF3X_GAPWATCH set, so the off check is skipped",
            faults, steps);
}

void GapWatch_Start(void* dll_module) {
    wchar_t path[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"BOF3X_GAPWATCH", path, MAX_PATH);
    if (n == 0) return;
    if (n >= MAX_PATH - 64) Fatal("BOF3X_GAPWATCH is longer than %u characters", (unsigned)(MAX_PATH - 64));
    if (!IsExeHost())
        Fatal("BOF3X_GAPWATCH watches a disc-built image's gaps, which only the exe host maps (bof3x.exe with "
              "BOF3X_EXEIMAGE); the hosted build runs Capcom's .data, which has none");
    if (!ExeImage_MappedBuild())
        Fatal("BOF3X_GAPWATCH: no disc-built image is mapped (BOF3X_EXEIMAGE unset, or a pc-zh image): there is no gap "
              "to watch");
    std::snprintf(g_build, sizeof g_build, "%s", ExeImage_MappedBuild());
    {
        char v[sizeof g_classes];
        const DWORD k = GetEnvironmentVariableA("BOF3X_GAPWATCH_CLASSES", v, sizeof v);
        if (k >= sizeof v) Fatal("BOF3X_GAPWATCH_CLASSES is longer than %u", (unsigned)sizeof v);
        if (k) std::snprintf(g_classes, sizeof g_classes, "%s", v);
        for (const char* p = g_classes; *p;) {
            const char* end = std::strchr(p, ',');
            const size_t len = end ? static_cast<size_t>(end - p) : std::strlen(p);
            if (!((len == 3 && !std::strncmp(p, "gap", 3)) || (len == 6 && !std::strncmp(p, "unread", 6)) ||
                  (len == 8 && !std::strncmp(p, "language", 8))))
                Fatal("BOF3X_GAPWATCH_CLASSES: \"%s\" - the classes are gap, unread and language", g_classes);
            if (!end) break;
            p = end + 1;
        }
        char num[16];
        if (GetEnvironmentVariableA("BOF3X_GAPWATCH_BUDGET", num, sizeof num)) {
            g_budget = static_cast<LONG>(std::strtol(num, nullptr, 10));
            if (g_budget < 1) Fatal("BOF3X_GAPWATCH_BUDGET must be at least 1, got %s", num);
        }
    }
    {
        char input[MAX_PATH];
        const DWORD k = GetEnvironmentVariableA("BOF3X_INPUT", input, sizeof input);
        if (k && k < sizeof input) {
            const char* base = input;
            for (const char* p = input; *p; ++p)
                if (*p == '\\' || *p == '/') base = p + 1;
            std::snprintf(g_route, sizeof g_route, "%s", base);
            if (char* dot = std::strrchr(g_route, '.')) *dot = 0;
        }
    }
    ReadDll(dll_module);

    ResetTables();
    for (int i = 0; i < ExeImage_MappedRanges(); ++i) {
        const ExeImageRange r = ExeImage_MappedRange(i);
        if (!ClassWanted(r.how)) continue;
        if (g_ranges == kMaxRanges) Fatal("gapwatch: more than %d ranges to watch", kMaxRanges);
        WRange& w = g_range[g_ranges++];
        w.lo = r.lo;
        w.hi = r.hi;
        w.how = r.how;
        w.from = r.from;
        if (g_ranges > 1 && w.lo < g_range[g_ranges - 2].hi) Fatal("gapwatch: data.toml's ranges are not in order at 0x%06X", (unsigned)w.lo);
    }
    if (g_ranges == 0) Fatal("gapwatch: the image (%s) has no range of the classes %s", g_build, g_classes);
    BuildPages();
    // What the handler reads must never be on a guarded page.
    for (std::uint32_t at : {Va(&Frame_Counter), Va(&Game_AreaNumber)})
        if (FindPage(at & ~(kPage - 1)) >= 0) Fatal("gapwatch: 0x%08X, which the handler reads, is on a watched page", (unsigned)at);

    // The file: a directory gets one named for the build and the route.
    std::wstring out(path, n);
    const DWORD attr = GetFileAttributesW(out.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        wchar_t name[160];
        std::swprintf(name, 160, L"\\gapwatch_%hs_%hs.tsv", g_build, g_route);
        out += name;
    }
    g_out = CreateFileW(out.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_out == INVALID_HANDLE_VALUE) Fatal("gapwatch: cannot create %s, error %lu", Utf8Path(out.c_str()).text, GetLastError());
    g_pause_for_hash = GetEnvironmentVariableA("BOF3X_STATEHASH", nullptr, 0) > 0;

    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_t0);
    Install();
    g_on = true;
    Rearm();
    Write();
    Log("gapwatch: %d ranges of %s (%s) on %d pages guarded, budget %ld faults a page a latch, route %s; to %s%s", g_ranges,
        g_classes, g_build, g_pages, (long)g_budget, g_route, Utf8Path(out.c_str()).text,
        g_pause_for_hash ? "; unguarded around the state hash's reads" : "");
}

void GapWatch_Pause() {
    if (!g_on || !g_pause_for_hash) return;
    for (int i = 0; i < g_pages; ++i) Unguard(i);
}

void GapWatch_Tick() {
    if (!g_on) return;
    Rearm();
    if (++g_ticks % kFlushEvery == 0) Write();
}

void GapWatch_Flush() {
    if (g_on) Write();
}

}  // namespace bof3
