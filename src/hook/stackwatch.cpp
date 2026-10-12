// The main thread's stack and the start-up floating-point words.
// docs/crt-startup.md sections 4, 8 and 10.
//
// The DLL runs on a thread the launcher made (CreateRemoteThread on
// LoadLibraryW) while the game's main thread is still suspended at its first
// instruction, so the main thread is found among the process's threads: the
// one whose suspended context holds the image's entry point in eax (where
// the user-mode thread start takes it from), else the earliest created. Its
// TEB comes from NtQueryInformationThread(ThreadBasicInformation); the TEB
// words read are NT_TIB's StackBase +0x04 and StackLimit +0x08 and the
// 32-bit TEB's DeallocationStack +0xE0C (the reservation's low end).
//
// BOF3X_STACKWATCH=1 adds two lines. At Task_SetStackBase (its asm calls
// StackWatch_AtSetStackBase): Task_StackTop, the arrival esp and its depth
// below StackBase, where the four slices lie against the reservation and the
// committed range, and a copy of what the committed part of the slices holds
// then. At exit (DllMain's detach): the committed range again - the guard
// page moves down as the stack grows, so StackBase less the committed low end
// is the deepest page any code on the thread touched - the deepest non-zero
// dword, and per slice the deepest dword that changed since the copy (a
// changed dword is a write after Task_SetStackBase: a task's frames, or
// WinMain's callees reaching below Task_StackTop).
#include "hook/stackwatch.h"

#include <windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "hook/host.h"
#include "hook/log.h"

extern "C" volatile unsigned char StackWatch_Armed = 0;

namespace bof3 {
namespace {

// The literal addresses are the symbols.toml data: 0x66C858 Task_StackTop.
constexpr std::uint32_t kTaskStackTop = 0x66C858;
constexpr std::uint32_t kSlice = 0x4000;   // Task_Create: slot * 0x4000
constexpr std::uint32_t kSlices = 4;       // Task_Records: four records

// The main thread, found at start-up.
DWORD g_main_id;
std::uint32_t g_teb;
std::uint32_t g_base;      // StackBase: the top, exclusive
std::uint32_t g_dealloc;   // DeallocationStack: the reservation's low end

// True while StackWatch_Start's own check calls Task_SetStackBase: in the exe
// host that call is on the main thread, and must not take the slices' copy.
bool g_checking = false;

// The slices' committed bytes as Task_SetStackBase first saw them.
std::uint32_t g_snap_lo;   // the low end of what was copied
std::uint32_t g_snap_hi;   // Task_StackTop then
unsigned char* g_snap;

struct ThreadBasicInformation {   // THREAD_BASIC_INFORMATION
    LONG exit_status;
    void* teb;
    void* client_process;
    void* client_thread;
    ULONG_PTR affinity;
    LONG priority;
    LONG base_priority;
};
using NtQueryInformationThreadFn = LONG(WINAPI*)(HANDLE, int, void*, ULONG, ULONG*);

std::uint32_t Long(std::uint32_t address) {
    std::uint32_t v;
    std::memcpy(&v, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), sizeof v);
    return v;
}

std::uint32_t TebOf(HANDLE thread) {
    static NtQueryInformationThreadFn query = reinterpret_cast<NtQueryInformationThreadFn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread")));
    if (!query) return 0;
    ThreadBasicInformation info{};
    if (query(thread, 0 /* ThreadBasicInformation */, &info, sizeof info, nullptr) < 0) return 0;
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(info.teb));
}

// The low end of the pages running down from `address` with its page's
// state and protection. VirtualQuery's region starts at the queried page and
// runs up, so the walk down is a page at a time.
std::uint32_t RunDown(std::uint32_t address, DWORD* state, DWORD* protect) {
    MEMORY_BASIC_INFORMATION m{};
    *state = 0, *protect = 0;
    if (!VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), &m, sizeof m)) return 0;
    *state = m.State, *protect = m.Protect;
    std::uint32_t low = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(m.BaseAddress));
    while (low >= 0x1000) {
        MEMORY_BASIC_INFORMATION b{};
        if (!VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(low - 1)), &b, sizeof b)) break;
        if (b.State != m.State || b.Protect != m.Protect) break;
        low = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(b.BaseAddress));
    }
    return low;
}

// The low end of the committed, unguarded pages running down from `address`,
// or 0 when that page is not so.
std::uint32_t CommittedLow(std::uint32_t address, DWORD* protect) {
    DWORD state;
    const std::uint32_t low = RunDown(address, &state, protect);
    if (state != MEM_COMMIT || (*protect & (PAGE_GUARD | PAGE_NOACCESS))) return 0;
    return low;
}

// The main thread's reservation as VirtualQuery's regions, low to high:
// "base+size state/protect" for each, into `out`.
void Regions(std::uint32_t from, std::uint32_t to, char* out, std::size_t size) {
    std::size_t n = 0;
    out[0] = '\0';
    for (std::uint32_t a = from; a < to && n < size;) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a)), &m, sizeof m)) break;
        const char* state = m.State == MEM_COMMIT ? "commit" : m.State == MEM_RESERVE ? "reserve" : "free";
        const int w = std::snprintf(out + n, size - n, "%s0x%08X+0x%X %s/0x%lX", n ? ", " : "", (unsigned)a,
                                    (unsigned)m.RegionSize, state, m.Protect);
        if (w < 0) break;
        n += static_cast<std::size_t>(w);
        a += static_cast<std::uint32_t>(m.RegionSize);
    }
}

// The pages below `low`: their state and protection and the size of their run.
void Below(std::uint32_t low, DWORD* state, DWORD* protect, std::uint32_t* size) {
    *size = low - RunDown(low - 1, state, protect);
}

struct MainThread {
    DWORD id = 0;
    const char* how = "not found";
    std::uint32_t esp = 0, eip = 0, eax = 0;
    unsigned cw = 0, mxcsr = 0;
    bool context = false;
};

// The main thread: the one whose suspended context's eax is the entry point,
// else the earliest created that is not this one.
MainThread FindMain(std::uint32_t entry) {
    MainThread best;
    FILETIME best_time{0xFFFFFFFFu, 0xFFFFFFFFu};
    bool by_entry = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return best;
    THREADENTRY32 te{};
    te.dwSize = sizeof te;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != GetCurrentProcessId() || te.th32ThreadID == GetCurrentThreadId()) continue;
        HANDLE h = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
        if (!h) continue;
        MainThread c;
        c.id = te.th32ThreadID;
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT | CONTEXT_EXTENDED_REGISTERS;
        if (GetThreadContext(h, &ctx)) {
            c.context = true;
            c.esp = ctx.Esp, c.eip = ctx.Eip, c.eax = ctx.Eax;
            c.cw = ctx.FloatSave.ControlWord & 0xFFFF;
            std::uint32_t mx;   // FXSAVE layout: MXCSR at +24
            std::memcpy(&mx, ctx.ExtendedRegisters + 24, sizeof mx);
            c.mxcsr = mx;
        }
        FILETIME created, exited, kernel, user;
        const bool timed = GetThreadTimes(h, &created, &exited, &kernel, &user) != 0;
        CloseHandle(h);
        if (c.context && c.eax == entry) {
            c.how = "eax = entry";
            best = c, by_entry = true;
        } else if (!by_entry && timed && CompareFileTime(&created, &best_time) < 0) {
            c.how = "earliest created";
            best = c, best_time = created;
        }
    }
    CloseHandle(snap);
    return best;
}

// A slice [lo, hi) against the committed run [committed_low, StackBase) and
// the reservation (its last page is never committed).
const char* Where(std::uint32_t lo, std::uint32_t hi, std::uint32_t committed_low) {
    if (lo >= committed_low) return "committed";
    if (lo < g_dealloc + 0x1000) return "OUTSIDE the reservation";
    return hi > committed_low ? "partly committed" : "reserved, not committed";
}

}  // namespace

void StackWatch_Start() {
    // The image's own request: IMAGE_OPTIONAL_HEADER32 of the mapped BOF3.exe.
    const auto* image = reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
    const std::uint32_t reserve = nt->OptionalHeader.SizeOfStackReserve;
    const std::uint32_t commit = nt->OptionalHeader.SizeOfStackCommit;
    const std::uint32_t entry = nt->OptionalHeader.ImageBase + nt->OptionalHeader.AddressOfEntryPoint;

    // This thread's words. Hosted, the launcher's CreateRemoteThread: Windows'
    // new thread defaults, as the main thread starts with them too. In the
    // exe host this is the main thread, after Engine_Main set 0x027F.
    unsigned short cw;
    unsigned mxcsr;
    asm volatile("fnstcw %0" : "=m"(cw));
    asm volatile("stmxcsr %0" : "=m"(mxcsr));
    if (IsExeHost())
        Log("fpu: the main thread %lu after InjectAll: x87 control word 0x%04X, MXCSR 0x%08X (the game thread, exe host)",
            GetCurrentThreadId(), cw, mxcsr);
    else
        Log("fpu: injected thread %lu at InjectAll: x87 control word 0x%04X, MXCSR 0x%08X (not the game thread)",
            GetCurrentThreadId(), cw, mxcsr);

    // BOF3X_STACKWATCH=1 asks for the measurement, so a failure to make it
    // is then a Fatal; without it the lines are informational and a failure
    // is logged and the game goes on.
    bool want = false;
    if (GetEnvironmentVariableA("BOF3X_STACKWATCH", nullptr, 0) > 0) {
        char v[8] = {};
        GetEnvironmentVariableA("BOF3X_STACKWATCH", v, sizeof v);
        if (std::strcmp(v, "1") != 0) Fatal("BOF3X_STACKWATCH wants 1, got \"%s\"", v);
        want = true;
    }

    // The TEB query checked on this thread, whose TEB is known.
    const std::uint32_t own = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(NtCurrentTeb()));
    const std::uint32_t queried = TebOf(GetCurrentThread());
    // Hosted, the main thread is found among the suspended process's
    // threads; in the exe host it is this one - Engine_Main runs on it, as
    // Capcom's entry ran on it (docs/state3-design.md section 5).
    MainThread main;
    if (IsExeHost()) {
        main.id = GetCurrentThreadId();
        main.how = "this thread, Engine_Main's";
        main.context = true;
        main.eip = entry;
        asm volatile("movl %%esp, %0" : "=r"(main.esp));
        main.cw = cw;
        main.mxcsr = mxcsr;
        g_teb = own;
    } else {
        main = FindMain(entry);
        HANDLE h = main.id ? OpenThread(THREAD_QUERY_INFORMATION, FALSE, main.id) : nullptr;
        g_teb = h ? TebOf(h) : 0;
        if (h) CloseHandle(h);
    }
    if (queried != own || !g_teb) {
        if (want)
            Fatal("stack: no main thread TEB (thread %lu, TEB 0x%08X; this thread's query 0x%08X, fs:[0x18] 0x%08X)",
                  main.id, (unsigned)g_teb, (unsigned)queried, (unsigned)own);
        Log("stack: not measured: main thread %lu, TEB 0x%08X; this thread's query 0x%08X, fs:[0x18] 0x%08X",
            main.id, (unsigned)g_teb, (unsigned)queried, (unsigned)own);
        return;
    }
    g_main_id = main.id;
    g_base = Long(g_teb + 0x04);
    const std::uint32_t limit = Long(g_teb + 0x08);
    g_dealloc = Long(g_teb + 0xE0C);
    DWORD state, protect;
    std::uint32_t guard;
    Below(limit, &state, &protect, &guard);
    Log("stack: header reserve 0x%X commit 0x%X; main thread %lu (%s) base 0x%08X limit 0x%08X dealloc 0x%08X "
        "(reserved 0x%X bytes, committed 0x%X, below it 0x%X bytes %s%s)",
        (unsigned)reserve, (unsigned)commit, main.id, main.how, (unsigned)g_base, (unsigned)limit,
        (unsigned)g_dealloc, (unsigned)(g_base - g_dealloc), (unsigned)(g_base - limit), (unsigned)guard,
        state == MEM_COMMIT ? "committed" : state == MEM_RESERVE ? "reserved" : "free",
        (protect & PAGE_GUARD) ? " guard" : "");
    if (main.context && IsExeHost())
        Log("stack: the main thread in Engine_Main: esp 0x%08X (0x%X below base; the image entry 0x%08X called Engine_Main); "
            "x87 control word 0x%04X, MXCSR 0x%08X",
            (unsigned)main.esp, (unsigned)(g_base - main.esp), (unsigned)entry, main.cw, main.mxcsr);
    else if (main.context)
        Log("stack: main thread's first context: eip 0x%08X eax 0x%08X (entry 0x%08X) esp 0x%08X (0x%X below base); "
            "x87 control word 0x%04X, MXCSR 0x%08X",
            (unsigned)main.eip, (unsigned)main.eax, (unsigned)entry, (unsigned)main.esp, (unsigned)(g_base - main.esp),
            main.cw, main.mxcsr);
    char regions[512];
    Regions(g_dealloc, g_base, regions, sizeof regions);
    Log("stack: main thread's reservation by region: %s", regions);

    if (want) {
        StackWatch_Armed = 1;
        // The armed path checked once here, on this thread, before the game
        // runs: it logs, leaves eax = Task_StackTop as stored, keeps the other
        // registers; Task_StackTop is put back (the game sets it itself).
        const std::uint32_t saved = Long(kTaskStackTop);
        g_checking = true;
        std::uint32_t eax, ecx, edx;
        asm volatile("movl $0x11111111, %%ecx\n\t"
                     "movl $0x22222222, %%edx\n\t"
                     "call _Task_SetStackBase\n\t"
                     "movl %%ecx, %1\n\t"
                     "movl %%edx, %2"
                     : "=a"(eax), "=m"(ecx), "=m"(edx)
                     :
                     : "ecx", "edx", "cc", "memory");
        g_checking = false;
        const std::uint32_t stored = Long(kTaskStackTop);
        std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(kTaskStackTop)), &saved, sizeof saved);
        if (eax != stored || ecx != 0x11111111u || edx != 0x22222222u)
            Fatal("stackwatch: the armed Task_SetStackBase left eax 0x%08X (stored 0x%08X), ecx 0x%08X, edx 0x%08X",
                  (unsigned)eax, (unsigned)stored, (unsigned)ecx, (unsigned)edx);
        Log("stackwatch: armed: Task_SetStackBase and exit will log the main thread's stack "
            "(the armed path checked on this thread: eax = Task_StackTop, ecx and edx kept)");
    }
}

void StackWatch_Exit() {
    if (!StackWatch_Armed) return;
    const DWORD self = GetCurrentThreadId();
    DWORD top_protect = 0;
    const std::uint32_t low = CommittedLow(g_base - 1, &top_protect);
    if (!low) {
        Log("stackwatch exit: on thread %lu; the main thread %lu's stack is no longer committed (protect 0x%lX)",
            self, g_main_id, top_protect);
        return;
    }
    const std::uint32_t limit = self == g_main_id ? Long(g_teb + 0x08) : 0;
    DWORD state, protect;
    std::uint32_t guard;
    Below(low, &state, &protect, &guard);
    std::uint32_t deepest = g_base;
    for (std::uint32_t a = low; a < g_base; a += 4)
        if (Long(a)) { deepest = a; break; }
    Log("stackwatch exit: on thread %lu (main %lu); committed 0x%08X..0x%08X = 0x%X bytes below base, TEB limit 0x%08X; "
        "below it 0x%X bytes%s; deepest non-zero dword 0x%08X = 0x%X below base",
        self, g_main_id, (unsigned)low, (unsigned)g_base, (unsigned)(g_base - low), (unsigned)limit, (unsigned)guard,
        (protect & PAGE_GUARD) ? " guard" : "", (unsigned)deepest, (unsigned)(g_base - deepest));

    if (!g_snap) {
        Log("stackwatch exit: Task_SetStackBase never ran on the main thread; no slices to report");
        return;
    }
    const std::uint32_t top = Long(kTaskStackTop);
    for (std::uint32_t k = 0; k < kSlices; ++k) {
        const std::uint32_t hi = top - k * kSlice, lo = hi - kSlice;
        std::uint32_t changed = hi;   // the lowest dword written since the copy
        for (std::uint32_t a = lo; a < hi; a += 4) {
            if (a < low) continue;    // never committed: never touched
            std::uint32_t was = 0;    // pages committed after the copy started zero
            if (g_snap && a >= g_snap_lo && a < g_snap_hi) std::memcpy(&was, g_snap + (a - g_snap_lo), 4);
            if (Long(a) != was) { changed = a; break; }
        }
        Log("stackwatch exit: task slice %lu 0x%08X..0x%08X %s; deepest write since Task_SetStackBase 0x%X of 0x%X bytes",
            (unsigned long)k, (unsigned)lo, (unsigned)hi, Where(lo, hi, low), (unsigned)(hi - changed), (unsigned)kSlice);
    }
}

}  // namespace bof3

// From Task_SetStackBase's asm - on the game's main thread, and once on the
// injected thread for StackWatch_Start's check - with 0x4000 bytes
// between arrival_esp and the Task_StackTop just stored: the frames here stay
// well inside that (Log's buffer is 1 KB).
extern "C" void __cdecl StackWatch_AtSetStackBase(unsigned long arrival_esp) {
    using namespace bof3;
    const std::uint32_t esp = static_cast<std::uint32_t>(arrival_esp);
    const std::uint32_t top = Long(kTaskStackTop);
    const std::uint32_t bottom = top - kSlices * kSlice;
    DWORD protect = 0;
    const std::uint32_t low = CommittedLow(g_base - 1, &protect);
    Log("stackwatch: Task_SetStackBase from 0x%08X on thread %lu: esp 0x%08X (0x%X below base 0x%08X), "
        "Task_StackTop 0x%08X, slices down to 0x%08X (0x%X below base; reservation low 0x%08X, %s); "
        "committed now down to 0x%08X (0x%X)",
        (unsigned)Long(esp), GetCurrentThreadId(), (unsigned)esp, (unsigned)(g_base - esp), (unsigned)g_base,
        (unsigned)top, (unsigned)bottom, (unsigned)(g_base - bottom), (unsigned)g_dealloc,
        bottom >= g_dealloc + 0x1000 && esp <= g_base ? "inside" : "OUTSIDE the main thread's reservation", (unsigned)low, (unsigned)(g_base - low));
    // The first call on the main thread (StackWatch_Start's check runs on the
    // injected one): the slices' committed bytes as they are before any task.
    if (!g_snap && !g_checking && low && GetCurrentThreadId() == g_main_id) {
        g_snap_lo = bottom > low ? bottom : low;
        g_snap_hi = top;
        if (g_snap_lo < g_snap_hi) {
            g_snap = static_cast<unsigned char*>(std::malloc(g_snap_hi - g_snap_lo));
            if (!g_snap) Fatal("stackwatch: no memory for the slices' copy");
            std::memcpy(g_snap, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(g_snap_lo)), g_snap_hi - g_snap_lo);
        }
    }
}
