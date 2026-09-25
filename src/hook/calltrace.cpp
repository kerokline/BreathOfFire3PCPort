#include "hook/calltrace.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace bof3 {
namespace {

constexpr int kMaxEntries = 8192;
constexpr std::uint8_t kInt3 = 0xCC;
constexpr std::uint8_t kPushfd = 0x9C;
constexpr DWORD kTrapFlag = 0x100;

struct Hit {
    std::uint32_t entry;
    std::uint32_t caller;  // the dword at esp on arrival: the return address
    std::uint32_t frame;   // Task_RunAll calls so far
    std::uint32_t thread;
};

// Everything the handler touches is static: it runs on whatever stack took
// the breakpoint, and for game logic that is a 16 KB task stack
// (docs/attract-mode.md section 2).
std::uint32_t g_entry[kMaxEntries];  // sorted
std::uint8_t g_byte[kMaxEntries];    // the original first byte
volatile LONG g_armed[kMaxEntries];
int g_entries = 0;

Hit g_hit[kMaxEntries];
volatile LONG g_hits = 0;
LONG g_flushed = 0;

std::uint32_t g_frame_entry = 0;  // Task_RunAll, if it is in the list
volatile LONG g_frame = 0;

// BOF3X_CALLTRACE_MODE=all: every entry is re-armed like Task_RunAll, so every
// call is counted, not only the first. Two exceptions per call.
bool g_all = false;
// STOP, DETAIL and SPIN below are read only with MODE=all.
// BOF3X_CALLTRACE_STOP=frame: the counts file is final at that frame, so two
// runs of different speed can be compared over the same span.
LONG g_stop_frame = 0;
// BOF3X_CALLTRACE_DETAIL=lo-hi: every call made in those logic frames, in
// order, to bof3x.calldetail.tsv. For finding which calls a differing frame
// hash is made of.
LONG g_detail_lo = 0, g_detail_hi = -1;
constexpr std::uint32_t kMaxDetail = 1u << 16;  // per frame
struct Call {
    std::uint32_t entry, caller;
};
Call g_detail[kMaxDetail];
std::uint32_t g_details = 0;
HANDLE g_out_detail = INVALID_HANDLE_VALUE;
// BOF3X_CALLTRACE_SPIN=n: burn n loop turns per counted call - a way to run
// the game at a second speed, to find what depends on speed.
std::uint32_t g_spin = 0;
std::uint32_t g_count[kMaxEntries];

// (return address, entry) -> calls. Open addressing, never deleted from.
constexpr std::uint32_t kEdgeSlots = 1u << 16;
struct Edge {
    std::uint32_t caller, entry, count;
};
Edge g_edge[kEdgeSlots];
std::uint32_t g_edges = 0;

// Per logic frame: calls made and an FNV-1a hash of their (entry, caller)
// sequence - a state hash far richer than the oracle's three words.
std::uint32_t g_frame_calls = 0;
std::uint32_t g_frame_hash = 0x811C9DC5u;

// The entry each thread is single-stepping over, to be re-armed on the trap.
constexpr int kMaxThreads = 16;
struct Pending {
    volatile LONG thread;
    std::uint32_t entry;
};
Pending g_pending[kMaxThreads];

// Functions registered with Inject, as [entry, end) in the original image.
// They are never armed, and a call made from inside one - or from inside
// bof3x.dll, where our version of it lives - is recorded with kOwnedCaller in
// place of the return address. With both rules the counts, edges and frame
// hash describe only "what the owned code asked of Capcom's code", which is
// the same question with BOF3X_ORIGINAL set and without: that is what makes
// original-vs-ours comparable (docs/call-trace.md section 7).
constexpr std::uint32_t kOwnedCaller = 0xFFFFFFFFu;
constexpr int kMaxOwnedRanges = 2048;  // 326 ours after the second parallel round, 2026-09-22
struct Range {
    std::uint32_t lo, hi;
};
Range g_owned_range[kMaxOwnedRanges];
int g_owned_ranges = 0;
Range g_dll = {0, 0};

std::uint32_t NormalCaller(std::uint32_t caller) {
    if (caller >= g_dll.lo && caller < g_dll.hi) return kOwnedCaller;
    for (int i = 0; i < g_owned_ranges; ++i)
        if (caller >= g_owned_range[i].lo && caller < g_owned_range[i].hi) return kOwnedCaller;
    return caller;
}

HANDLE g_out = INVALID_HANDLE_VALUE;
HANDLE g_out_frames = INVALID_HANDLE_VALUE;
HANDLE g_out_counts = INVALID_HANDLE_VALUE;

int Find(std::uint32_t addr) {
    const std::uint32_t* begin = g_entry;
    const std::uint32_t* end = begin + g_entries;
    const std::uint32_t* p = std::lower_bound(begin, end, addr);
    return (p != end && *p == addr) ? static_cast<int>(p - g_entry) : -1;
}

std::uint8_t* At(std::uint32_t addr) {
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(addr));
}

void Hash(std::uint32_t v) {
    for (int b = 0; b < 4; ++b, v >>= 8) g_frame_hash = (g_frame_hash ^ (v & 0xFF)) * 0x01000193u;
}

void CountEdge(std::uint32_t caller, std::uint32_t entry) {
    std::uint32_t i = ((caller * 0x9E3779B1u) ^ entry) >> 16;
    for (std::uint32_t n = 0; n < kEdgeSlots; ++n, i = (i + 1) & (kEdgeSlots - 1)) {
        Edge& e = g_edge[i];
        if (e.count == 0) {
            e.caller = caller;
            e.entry = entry;
            ++g_edges;
        } else if (e.caller != caller || e.entry != entry) {
            continue;
        }
        ++e.count;
        return;
    }
    Fatal("calltrace: more than %u distinct call edges", (unsigned)kEdgeSlots);
}

// WriteAll through Flush run only from the Task_RunAll breakpoint: WinMain's
// stack.
void WriteAll(HANDLE h, const char* text, int len) {
    DWORD written = 0;
    WriteFile(h, text, static_cast<DWORD>(len), &written, nullptr);
}

// The process is ended by taskkill, so totals are rewritten in place as the
// run goes instead of once at exit.
void DumpCounts() {
    SetFilePointer(g_out_counts, 0, nullptr, FILE_BEGIN);
    char line[96];
    int len = std::snprintf(line, sizeof line, "# to frame %ld; caller 0 = total for the entry\r\n",
                            (long)g_frame);
    WriteAll(g_out_counts, line, len);
    for (int i = 0; i < g_entries; ++i) {
        if (!g_count[i]) continue;
        len = std::snprintf(line, sizeof line, "0x%08lX\t0x00000000\t%lu\r\n",
                            (unsigned long)g_entry[i], (unsigned long)g_count[i]);
        WriteAll(g_out_counts, line, len);
    }
    for (std::uint32_t i = 0; i < kEdgeSlots; ++i) {
        const Edge& e = g_edge[i];
        if (!e.count) continue;
        len = std::snprintf(line, sizeof line, "0x%08lX\t0x%08lX\t%lu\r\n", (unsigned long)e.entry,
                            (unsigned long)e.caller, (unsigned long)e.count);
        WriteAll(g_out_counts, line, len);
    }
    SetEndOfFile(g_out_counts);
}

void EndFrame() {
    char line[64];
    for (std::uint32_t k = 0; k < g_details; ++k) {
        int n = std::snprintf(line, sizeof line, "%ld\t0x%08lX\t0x%08lX\r\n", (long)g_frame,
                              (unsigned long)g_detail[k].entry, (unsigned long)g_detail[k].caller);
        WriteAll(g_out_detail, line, n);
    }
    g_details = 0;
    int len = std::snprintf(line, sizeof line, "%ld\t%lu\t%08lX\r\n", (long)g_frame,
                            (unsigned long)g_frame_calls, (unsigned long)g_frame_hash);
    WriteAll(g_out_frames, line, len);
    g_frame_calls = 0;
    g_frame_hash = 0x811C9DC5u;
    if (g_stop_frame ? g_frame == g_stop_frame : (g_frame & 0xFF) == 0) DumpCounts();
}

void Flush() {
    LONG n = g_hits;
    if (n > kMaxEntries) n = kMaxEntries;
    for (; g_flushed < n; ++g_flushed) {
        const Hit& h = g_hit[g_flushed];
        char line[64];
        int len = std::snprintf(line, sizeof line, "%lu\t0x%08lX\t0x%08lX\t%lu\r\n",
                                (unsigned long)h.frame, (unsigned long)h.entry,
                                (unsigned long)h.caller, (unsigned long)h.thread);
        DWORD written = 0;
        WriteFile(g_out, line, static_cast<DWORD>(len), &written, nullptr);
    }
}

// int3 at a listed entry: record, put the original byte back and resume at
// the entry (Eip is one past the int3). To re-arm, the trap flag is set and
// the entry noted in g_pending; the single-step after that one instruction
// writes the int3 back. Anything else is passed on untouched.
LONG CALLBACK OnException(EXCEPTION_POINTERS* info) {
    EXCEPTION_RECORD* rec = info->ExceptionRecord;
    CONTEXT* ctx = info->ContextRecord;

    if (rec->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        LONG me = static_cast<LONG>(GetCurrentThreadId());
        for (Pending& p : g_pending) {
            if (p.thread != me) continue;
            *At(p.entry) = kInt3;
            // An entry that begins with pushfd has just saved the trap flag
            // we set to step it, and a popfd would set it again with nobody
            // expecting the step: 0x5A9A30, the MMX probe of the software
            // renderer's set-up, did exactly that and the CRT's __except
            // ended the process with 0x80000004 (docs/window-modes.md 4a).
            // The flags it saved are made the ones an untraced run saves.
            if (g_byte[Find(p.entry)] == kPushfd)
                *reinterpret_cast<DWORD*>(static_cast<std::uintptr_t>(ctx->Esp)) &= ~kTrapFlag;
            p.thread = 0;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        // A step no entry asked for: a trap flag of ours that escaped. Loud,
        // because passed on it reaches the CRT's __except and the process
        // ends with no CRASH line.
        Fatal("calltrace: a single step at 0x%08lX that no traced entry asked for",
              static_cast<unsigned long>(ctx->Eip));
    }
    if (rec->ExceptionCode != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;

    auto addr = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(rec->ExceptionAddress));
    int i = Find(addr);
    if (i < 0) return EXCEPTION_CONTINUE_SEARCH;

    bool first = InterlockedExchange(&g_armed[i], 0) == 1;
    *At(addr) = g_byte[i];
    std::uint32_t caller = *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(ctx->Esp));
    if (first) {
        LONG slot = InterlockedIncrement(&g_hits) - 1;
        if (slot < kMaxEntries) {
            g_hit[slot].entry = addr;
            g_hit[slot].caller = caller;
            g_hit[slot].frame = static_cast<std::uint32_t>(g_frame);
            g_hit[slot].thread = GetCurrentThreadId();
        }
    }
    if (addr == g_frame_entry) {
        if (g_all) EndFrame();
        InterlockedIncrement(&g_frame);
        Flush();
    } else if (g_all) {
        // Counts are kept for the game's one logic thread; a second thread
        // in .text would race here, and docs/call-trace.md section 3 found none.
        if (!g_stop_frame || g_frame < g_stop_frame) ++g_count[i];
        for (volatile std::uint32_t k = 0; k < g_spin; k = k + 1) {
        }
        ++g_frame_calls;
        caller = NormalCaller(caller);
        if (g_frame >= g_detail_lo && g_frame <= g_detail_hi) {
            if (g_details == kMaxDetail) Fatal("calltrace: more than %u calls in a detailed frame", (unsigned)kMaxDetail);
            g_detail[g_details].entry = addr;
            g_detail[g_details].caller = caller;
            ++g_details;
        }
        Hash(addr);
        Hash(caller);
        if (!g_stop_frame || g_frame < g_stop_frame) CountEdge(caller, addr);
    }
    if (g_all || addr == g_frame_entry) {
        LONG me = static_cast<LONG>(GetCurrentThreadId());
        Pending* free_slot = nullptr;
        for (Pending& p : g_pending)
            if (!free_slot && InterlockedCompareExchange(&p.thread, me, 0) == 0) free_slot = &p;
        if (!free_slot) Fatal("calltrace: more than %d threads inside traced code", kMaxThreads);
        free_slot->entry = addr;
        ctx->EFlags |= kTrapFlag;
    }
    ctx->Eip = addr;
    return EXCEPTION_CONTINUE_EXECUTION;
}

void TextRange(std::uint32_t* lo, std::uint32_t* hi) {
    auto* base = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(image::kBase));
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++sec) {
        if (std::memcmp(sec->Name, ".text", 6) != 0) continue;
        *lo = image::kBase + sec->VirtualAddress;
        *hi = *lo + sec->Misc.VirtualSize;
        return;
    }
    Fatal("calltrace: BOF3.exe has no .text section");
}

}  // namespace

void CallTrace_Start(void* dll_module) {
    char list[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("BOF3X_CALLTRACE", list, sizeof list);
    if (n == 0) return;
    if (n >= sizeof list) Fatal("BOF3X_CALLTRACE is longer than %u bytes", (unsigned)sizeof list);

    std::uint32_t lo = 0, hi = 0;
    TextRange(&lo, &hi);

    std::FILE* f = std::fopen(list, "r");
    if (!f) Fatal("calltrace: cannot open entry list %s", list);
    char line[512];  // longer than any comment line tools/calltrace.py writes
    int skipped = 0;
    while (std::fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        char* end = line;
        auto addr = static_cast<std::uint32_t>(std::strtoul(line, &end, 16));
        if (end == line) Fatal("calltrace: %s has a line that is not a hex address: %s", list, line);
        if (addr < lo || addr >= hi)
            Fatal("calltrace: %s lists 0x%08X, outside .text 0x%08X..0x%08X", list, (unsigned)addr,
                  (unsigned)lo, (unsigned)hi);
        if (IsOwned(addr)) {
            std::uint32_t size = static_cast<std::uint32_t>(std::strtoul(end, nullptr, 16));
            if (size == 0) Fatal("calltrace: %s gives no size for owned function 0x%08X", list, (unsigned)addr);
            if (g_owned_ranges == kMaxOwnedRanges) Fatal("calltrace: more than %d owned functions", kMaxOwnedRanges);
            g_owned_range[g_owned_ranges++] = {addr, addr + size};
            // Task_RunAll is the frame counter, so it stays armed when it is
            // ours (docs/task_sched.md section 5): a frame is still an arrival
            // at 0x5A98A0 from WinMain - the int3 then sits on the detour's jmp
            // and the step re-arms it at our function's first instruction.
            // It makes no call, so its owned range above changes no caller.
            if (addr != addr::Task_RunAll) {
                ++skipped;
                continue;
            }
        }
        if (g_entries == kMaxEntries) Fatal("calltrace: more than %d entries in %s", kMaxEntries, list);
        g_entry[g_entries++] = addr;
    }
    std::fclose(f);
    std::sort(g_entry, g_entry + g_entries);
    g_entries = static_cast<int>(std::unique(g_entry, g_entry + g_entries) - g_entry);

    {
        auto* dll = reinterpret_cast<const std::uint8_t*>(dll_module);
        auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(dll);
        auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(dll + dos->e_lfanew);
        g_dll.lo = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(dll));
        g_dll.hi = g_dll.lo + nt->OptionalHeader.SizeOfImage;
    }

    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(static_cast<HMODULE>(dll_module), path, MAX_PATH);
    static const wchar_t kTail[] = L"calltrace.tsv";  // bof3x.dll -> bof3x.calltrace.tsv
    // Room for the longest of the four names written through `path`:
    // calldetail.tsv, callframes.tsv and callcounts.tsv are a character longer.
    constexpr DWORD kLongestTail = sizeof L"calldetail.tsv" / sizeof(wchar_t);
    if (len < 4 || len - 3 + kLongestTail > MAX_PATH)
        Fatal("calltrace: DLL path unusable for the output file");
    std::memcpy(path + len - 3, kTail, sizeof kTail);
    g_out = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_out == INVALID_HANDLE_VALUE) Fatal("calltrace: cannot create %ls", path);
    char mode[16];
    DWORD mn = GetEnvironmentVariableA("BOF3X_CALLTRACE_MODE", mode, sizeof mode);
    if (mn != 0) {
        if (std::strcmp(mode, "all") != 0) Fatal("calltrace: BOF3X_CALLTRACE_MODE must be 'all' or unset");
        g_all = true;
        char num[16];
        if (GetEnvironmentVariableA("BOF3X_CALLTRACE_STOP", num, sizeof num))
            g_stop_frame = static_cast<LONG>(std::strtoul(num, nullptr, 10));
        char range[32];
        if (GetEnvironmentVariableA("BOF3X_CALLTRACE_DETAIL", range, sizeof range)) {
            long lo = 0, hi = 0;
            if (std::sscanf(range, "%ld-%ld", &lo, &hi) != 2 || lo > hi)
                Fatal("calltrace: BOF3X_CALLTRACE_DETAIL must be lo-hi, got %s", range);
            g_detail_lo = lo;
            g_detail_hi = hi;
            static const wchar_t kDetail[] = L"calldetail.tsv";
            std::memcpy(path + len - 3, kDetail, sizeof kDetail);
            g_out_detail = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL, nullptr);
            if (g_out_detail == INVALID_HANDLE_VALUE) Fatal("calltrace: cannot create the calldetail file");
        }
        if (GetEnvironmentVariableA("BOF3X_CALLTRACE_SPIN", num, sizeof num))
            g_spin = static_cast<std::uint32_t>(std::strtoul(num, nullptr, 10));
        static const wchar_t kFrames[] = L"callframes.tsv";
        static const wchar_t kCounts[] = L"callcounts.tsv";
        std::memcpy(path + len - 3, kFrames, sizeof kFrames);
        g_out_frames = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        std::memcpy(path + len - 3, kCounts, sizeof kCounts);
        g_out_counts = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        if (g_out_frames == INVALID_HANDLE_VALUE || g_out_counts == INVALID_HANDLE_VALUE)
            Fatal("calltrace: cannot create the callframes / callcounts files");
    }
    static const char kHeader[] = "# frame\tentry\tcaller\tthread\r\n";
    DWORD written = 0;
    WriteFile(g_out, kHeader, sizeof kHeader - 1, &written, nullptr);

    // First in the chain (1), so the int3s and steps are ours before any other
    // vectored handler sees them.
    if (!AddVectoredExceptionHandler(1, OnException)) Fatal("calltrace: no vectored handler");

    // Left writable on purpose: the handler restores bytes from arbitrary
    // threads and stacks and must not call VirtualProtect there.
    DWORD old = 0;
    if (!VirtualProtect(At(lo), hi - lo, PAGE_EXECUTE_READWRITE, &old))
        Fatal("calltrace: VirtualProtect(.text) failed, error %lu", GetLastError());

    // The address, not the name: once Task_RunAll is ours the name is our
    // function in bof3x.dll, and WinMain's call still arrives here.
    const std::uint32_t frame_entry = addr::Task_RunAll;
    // Hits are written out from the Task_RunAll breakpoint, so without it
    // nothing would ever reach the file.
    if (Find(frame_entry) < 0)
        Fatal("calltrace: %s does not list Task_RunAll 0x%08X", list, (unsigned)frame_entry);
    g_frame_entry = frame_entry;
    for (int i = 0; i < g_entries; ++i) {
        g_byte[i] = *At(g_entry[i]);
        g_armed[i] = 1;
        *At(g_entry[i]) = kInt3;
    }
    FlushInstructionCache(GetCurrentProcess(), At(lo), hi - lo);
    Log("calltrace: %d entries armed from %s, %d left unarmed as owned, mode %s", g_entries, list,
        skipped, g_all ? "all calls" : "first call");
}

}  // namespace bof3
