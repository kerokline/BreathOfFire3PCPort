#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// The crash catcher on Linux (docs/linux-round.md LB7): sigaction with
// SA_SIGINFO for SIGSEGV, SIGBUS, SIGILL, SIGFPE and SIGABRT, on an alternate
// stack for the thread that armed it. The report's lines are crash.cpp's, the
// same as Windows' but for the first (a signal, not an exception code); the
// registers come from the handler's ucontext_t, a module and its offset from
// dladdr. There is no minidump on this system: the report says so in its line.
//
// Shaped as the Windows catcher (crash_win32.cpp) is: the handler copies the
// facts and hands them to a reporter thread made at start-up, which has a
// stack of its own and may take the log's lock; the faulting thread waits for
// it 20 s at most, so a lock the fault left held costs the report, never the
// crash. Then the fault is passed on: a handler that was there before ours is
// called, else the disposition we found goes back and the fault happens again
// under it - the process ends as it would have without us. At most
// kMaxReports reports; a second fault while one is reported is passed on
// unreported.
//
// The exe host's step 23 (Crash_EndUnhandledAsFatal, crash.h) changes the
// end of a fault nobody before us handles - its old disposition SIG_DFL or
// SIG_IGN - to Windows' end of an exception nobody handled (exe_host.cpp
// Unhandled): the reporter, its CRASH lines written, calls Fatal ("a signal
// nobody handled"), which logs, shows its message unless the run is a
// self-test, and ends the process with code 3. The faulting thread waits for
// it without a bound once the report is written (Fatal's message waits for
// the player); a report not written in 20 s ends the process with code 3 from
// the handler (_exit), the lines lost as before. Why code 3 and not the
// signal's own end, which the catcher gave before step 23 existed here: the
// work order says so (exe_host_steps.h 23), a player sees Fatal's message in
// place of a window that vanishes, and one rule serves the tools on both
// systems - the log's FATAL line and the exit code 3 (log.cpp Fatal). What it
// costs: the core file the signal's default would write; a developer who
// wants one runs the ELF under a debugger, which stops at the signal first.
//
// The main thread's stack is placed (os/placed.h; exe_host_posix.cpp row 3)
// with a no-access guard page under it, and the catcher is armed on that
// stack (step 16): an overflow faults on the guard page with the stack
// pointer at its edge, and the handler runs on the alternate stack below, so
// it is reported - with a line that names the guard page - and ended as any
// other fault.
//
// What the handler itself does is async-signal-safe: copies, atomics,
// sem_post, nanosleep, sigaction, raise, _exit. The reporter is an ordinary
// thread.
#include "hook/crash.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/uio.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include "common/image_base.h"
#include "hook/crash_report.h"
#include "hook/exe_layout.h"
#include "hook/log.h"
#include "os/os.h"

namespace bof3 {
namespace {

constexpr int kSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
constexpr int kMaxReports = 4;
constexpr int kWaitMs = 20000;

struct sigaction g_old[NSIG];   // what each of kSignals had before ours
crash::Fault g_fault;
std::atomic<int> g_busy{0};
std::atomic<int> g_reports{0};
std::atomic<bool> g_done{false};
sem_t g_wake;
int g_pipe[2] = {-1, -1};       // Peek's second way, where process_vm_readv is refused
char g_engine[256] = "the engine";
std::uintptr_t g_engine_lo = 0, g_engine_hi = 0;   // the engine's loaded extent (Crash_Start)
std::uintptr_t g_image_lo = 0, g_image_hi = 0;     // the game's image (Crash_Start)

// Step 23 (the header comment): set once by Crash_EndUnhandledAsFatal.
std::atomic<bool> g_end_as_fatal{false};
bool g_fault_ends = false;              // the reported fault's end is Fatal's: set by its handler before the wake
std::atomic<bool> g_ending{false};      // the reporter is in Fatal: the process is ending with code 3
std::atomic<std::uint32_t> g_reporter_tid{0};

// The alternate stack: for the thread that armed the catcher (sigaltstack is
// a thread's own). A fault on another thread runs the handler on that
// thread's stack, which serves unless that stack is what broke.
alignas(16) unsigned char g_alt_stack[64 * 1024];
// That thread and the stack it ran on when it armed it: an access in the page
// under that stack is its overflow (Report's line). In the exe host the main
// thread on its placed stack (exe_host_posix.cpp row 3, the guard below it).
std::uint32_t g_armed_tid = 0;
std::uintptr_t g_armed_stack_lo = 0, g_armed_stack_hi = 0;
constexpr std::uintptr_t kPage = 0x1000;

const char* Name(int sig) {
    switch (sig) {
        case SIGILL: return "SIGILL";
        case SIGABRT: return "SIGABRT";
        case SIGBUS: return "SIGBUS";
        case SIGFPE: return "SIGFPE";
        case SIGSEGV: return "SIGSEGV";
        case SIGTRAP: return "SIGTRAP";
        default: return "a signal";
    }
}

// Whether a handler was there before ours: a function to pass the fault to.
bool Handled(const struct sigaction& old) {
    if (old.sa_flags & SA_SIGINFO) return old.sa_sigaction != nullptr;
    return old.sa_handler != SIG_DFL && old.sa_handler != SIG_IGN;
}

// The faulting thread, once the process's end is the reporter's Fatal: it
// waits for that end (Fatal's message waits for the player).
[[noreturn]] void HoldForTheEnd() {
    for (;;) {
        const timespec t{1, 0};
        nanosleep(&t, nullptr);
    }
}

// The siginfo and the context as the portable report's facts.
void Fill(int sig, const siginfo_t* si, const ucontext_t* uc) {
    crash::Fault& f = g_fault;
    f = crash::Fault{};
    f.number = g_reports.load();
    f.is_signal = true;
    f.code = static_cast<std::uint32_t>(sig);
    f.sub_code = si->si_code;
    f.thread = os::ThreadId();
    const greg_t* g = uc->uc_mcontext.gregs;
#if defined(__x86_64__)
    f.ax = g[REG_RAX], f.bx = g[REG_RBX], f.cx = g[REG_RCX], f.dx = g[REG_RDX], f.si = g[REG_RSI], f.di = g[REG_RDI];
    f.r8 = g[REG_R8], f.r9 = g[REG_R9], f.r10 = g[REG_R10], f.r11 = g[REG_R11];
    f.r12 = g[REG_R12], f.r13 = g[REG_R13], f.r14 = g[REG_R14], f.r15 = g[REG_R15];
    f.bp = g[REG_RBP], f.sp = g[REG_RSP], f.ip = g[REG_RIP];
    f.flags = static_cast<std::uint32_t>(g[REG_EFL]);
    const std::uintptr_t err = g[REG_ERR], trap = g[REG_TRAPNO];
#elif defined(__i386__)
    f.ax = g[REG_EAX], f.bx = g[REG_EBX], f.cx = g[REG_ECX], f.dx = g[REG_EDX], f.si = g[REG_ESI], f.di = g[REG_EDI];
    f.bp = g[REG_EBP], f.sp = g[REG_ESP], f.ip = g[REG_EIP];
    f.flags = static_cast<std::uint32_t>(g[REG_EFL]);
    const std::uintptr_t err = g[REG_ERR], trap = g[REG_TRAPNO];
#else
#error "crash_posix: no registers for this architecture"
#endif
    f.addr = f.ip;
    // An access fault names its address in si_addr; the page fault's error
    // code (trap 14) says which kind: bit 4 an instruction fetch, bit 1 a
    // write, else a read - Windows' 8, 1 and 0.
    const bool sent = si->si_code <= 0 || si->si_code == SI_KERNEL;   // by kill or raise, or a fault with no address
    if ((sig == SIGSEGV || sig == SIGBUS) && !sent) {
        f.access_at = reinterpret_cast<std::uintptr_t>(si->si_addr);
        if (trap == 14) {
            f.access = (err & 0x10) ? crash::Access::Execute : (err & 0x2) ? crash::Access::Write : crash::Access::Read;
            f.access_raw = (err & 0x10) ? 8 : (err & 0x2) ? 1 : 0;
        } else {
            f.access = crash::Access::Other;
            f.access_raw = trap;
        }
    }
}

// The fault passed on (the header comment).
void PassOn(int sig, siginfo_t* si, void* uc) {
    const struct sigaction& old = g_old[sig];
    if (old.sa_flags & SA_SIGINFO) {
        if (old.sa_sigaction) {
            old.sa_sigaction(sig, si, uc);
            return;
        }
    } else if (old.sa_handler != SIG_DFL && old.sa_handler != SIG_IGN) {
        old.sa_handler(sig);
        return;
    }
    sigaction(sig, &old, nullptr);
    // A fault the CPU raised happens again when the handler returns; a signal
    // sent (kill, raise, abort) does not, so it is sent again, pending until then.
    if (si->si_code <= 0) raise(sig);
}

void OnSignal(int sig, siginfo_t* si, void* uc) {
    const int saved_errno = errno;
    // Step 23's end for this fault (the header comment); never for a fault of
    // the reporter's own thread, which would wait for itself.
    const bool ends = g_end_as_fatal.load() && !Handled(g_old[sig]) && os::ThreadId() != g_reporter_tid.load();
    if (ends && g_ending.load()) HoldForTheEnd();   // another fault's Fatal is ending the process
    if (g_reports.load() < kMaxReports && g_busy.exchange(1) == 0) {
        Fill(sig, si, static_cast<const ucontext_t*>(uc));
        g_fault_ends = ends;
        g_done.store(false);
        sem_post(&g_wake);
        // Bounded: if the reporter cannot finish, the crash must still proceed.
        for (int ms = 0; ms < kWaitMs && !g_done.load(); ++ms) {
            const timespec t{0, 1000000};
            nanosleep(&t, nullptr);
        }
        if (ends) {
            if (g_done.load()) HoldForTheEnd();   // the reporter is in Fatal
            _exit(3);                             // no report in 20 s: Fatal's code all the same
        }
        g_reports.fetch_add(1);
        g_busy.store(0);
    } else if (ends) {
        // Not reported: another fault's report is under way (or the four are
        // spent). Its Fatal ends the process if it is a step-23 end; else
        // this one ends it, with Fatal's code.
        for (int ms = 0; ms < kWaitMs && !g_ending.load(); ++ms) {
            const timespec t{0, 1000000};
            nanosleep(&t, nullptr);
        }
        if (g_ending.load()) HoldForTheEnd();
        _exit(3);
    }
    errno = saved_errno;
    PassOn(sig, si, uc);
}

void Report() {
    const crash::Fault& f = g_fault;
    const long n = f.number;
    crash::WriteReport(f);
    // An overflow of the armed thread's stack (the header comment): Windows
    // names it by its exception code (0xC00000FD); here the access says it.
    if (f.access != crash::Access::None && f.thread == g_armed_tid && g_armed_stack_lo &&
        f.access_at >= g_armed_stack_lo - kPage && f.access_at < g_armed_stack_lo)
        Log("CRASH %ld: a stack overflow - the access is to the guard page 0x%llX..0x%llX under thread %lu's stack "
            "0x%llX..0x%llX",
            n, static_cast<unsigned long long>(g_armed_stack_lo - kPage), static_cast<unsigned long long>(g_armed_stack_lo),
            (unsigned long)f.thread, static_cast<unsigned long long>(g_armed_stack_lo),
            static_cast<unsigned long long>(g_armed_stack_hi));
    Log("CRASH %ld: no dump - this system has no minidump; the lines above are the whole report", n);
    crash::EndReport();
}

// The one reporter thread: waits on g_wake, reports g_fault, sets g_done;
// for a step-23 end it then ends the process as Windows' Unhandled does.
void Reporter() {
    g_reporter_tid.store(os::ThreadId());
    for (;;) {
        while (sem_wait(&g_wake) != 0 && errno == EINTR) {
        }
        Report();
        const bool ends = g_fault_ends;
        if (ends) g_ending.store(true);
        g_done.store(true);
        if (ends)
            Fatal("a signal nobody handled: %u (%s, code %d) at 0x%0*llX (see the CRASH lines above)", (unsigned)g_fault.code,
                  Name(static_cast<int>(g_fault.code)), g_fault.sub_code, static_cast<int>(2 * sizeof(std::uintptr_t)),
                  static_cast<unsigned long long>(g_fault.addr));
    }
}

// The extent of the loaded object holding `inside`: its PT_LOAD segments.
struct Extent {
    std::uintptr_t inside, lo, hi;
};
int FindExtent(dl_phdr_info* info, size_t, void* data) {
    Extent* e = static_cast<Extent*>(data);
    std::uintptr_t lo = UINTPTR_MAX, hi = 0;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)& p = info->dlpi_phdr[i];
        if (p.p_type != PT_LOAD) continue;
        const std::uintptr_t a = info->dlpi_addr + p.p_vaddr;
        if (a < lo) lo = a;
        if (a + p.p_memsz > hi) hi = a + p.p_memsz;
    }
    if (e->inside >= lo && e->inside < hi) {
        e->lo = lo, e->hi = hi;
        return 1;
    }
    return 0;
}

// BOF3X_CRASH_TEST=seconds: fault on purpose that long after start-up, to see
// the reporter work. The read is of address 0x10, which is never mapped.
void SelfTest(unsigned long seconds) {
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    Log("crash reporter: BOF3X_CRASH_TEST faulting now");
    volatile int* nowhere = reinterpret_cast<int*>(0x10);
    std::printf("%d\n", *nowhere);
}

// The exit trace's handler (crash.h Crash_WatchBreaks): SIGTRAP, a breakpoint
// (int3) or a single step. A trap resumes after its instruction, so passing
// it on means its old disposition back and the signal sent again: by
// default the process ends, as an unhandled breakpoint ends it on Windows.
// The callback logs on the trapping thread, as Windows' vectored handler
// does.
BreakFn g_on_break = nullptr;
struct sigaction g_old_trap;
std::atomic<int> g_breaks{0};
void OnTrap(int sig, siginfo_t* si, void* uc) {
    if (g_breaks.fetch_add(1) < 4) {
        const greg_t* g = static_cast<const ucontext_t*>(uc)->uc_mcontext.gregs;
#if defined(__x86_64__)
        const std::uintptr_t ip = g[REG_RIP], sp = g[REG_RSP];
#else
        const std::uintptr_t ip = g[REG_EIP], sp = g[REG_ESP];
#endif
        char what[48];
        std::snprintf(what, sizeof what, "signal %d (SIGTRAP, code %d)", sig, si->si_code);
        g_on_break(what, ip, sp, static_cast<std::uint32_t>(g[REG_EFL]));
    }
    if (g_old_trap.sa_flags & SA_SIGINFO) {
        if (g_old_trap.sa_sigaction) {
            g_old_trap.sa_sigaction(sig, si, uc);
            return;
        }
    } else if (g_old_trap.sa_handler != SIG_DFL && g_old_trap.sa_handler != SIG_IGN) {
        g_old_trap.sa_handler(sig);
        return;
    }
    // Step 23 (the header comment): a trap nobody handled ends the process as
    // Windows' Unhandled ends an unhandled breakpoint. On the trapping thread,
    // as the callback above logged there.
    if (g_end_as_fatal.load()) {
        const greg_t* g = static_cast<const ucontext_t*>(uc)->uc_mcontext.gregs;
#if defined(__x86_64__)
        const std::uintptr_t ip = g[REG_RIP];
#else
        const std::uintptr_t ip = g[REG_EIP];
#endif
        Fatal("a signal nobody handled: %d (SIGTRAP, code %d) at 0x%0*llX (see the exittrace lines above)", sig, si->si_code,
              static_cast<int>(2 * sizeof(std::uintptr_t)), static_cast<unsigned long long>(ip));
    }
    sigaction(sig, &g_old_trap, nullptr);
    raise(sig);
}

}  // namespace

void Crash_EndUnhandledAsFatal() {
    g_end_as_fatal.store(true);
    Log("crash reporter: from here a fault no other handler takes ends the process as Fatal does, code 3, after its "
        "CRASH lines (step 23; Windows' SetUnhandledExceptionFilter)");
}

bool Crash_WatchBreaks(BreakFn on_break) {
    g_on_break = on_break;
    struct sigaction sa{};
    sa.sa_sigaction = OnTrap;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    return sigaction(SIGTRAP, &sa, &g_old_trap) == 0;
}

namespace crash {

// Memory that may not be there: never dereference, always ask. The kernel
// copies it (process_vm_readv of this process) or, where that call is
// refused (a seccomp profile), writes it into a pipe: either fails on a page
// that is not readable instead of faulting.
bool Peek(std::uintptr_t addr, void* out, size_t n) {
    iovec local{out, n}, remote{reinterpret_cast<void*>(addr), n};
    const ssize_t got = process_vm_readv(getpid(), &local, 1, &remote, 1, 0);
    if (got >= 0 || (errno != ENOSYS && errno != EPERM)) return got == static_cast<ssize_t>(n);
    if (g_pipe[0] < 0) return false;
    if (write(g_pipe[1], reinterpret_cast<const void*>(addr), n) != static_cast<ssize_t>(n)) return false;
    return read(g_pipe[0], out, n) == static_cast<ssize_t>(n);
}

// The game's image is no loaded object here (the start-up maps it as
// anonymous memory, exe_host_posix.cpp row 1): named by its VA. The engine is
// the ELF (or the shared object holding this file): its name and the offset
// from image::g_ownBase, the ELF's base, as crt_rest.cpp's messages give it.
// Anything else (the C library, SDL, DXVK) by dladdr.
void Where(std::uintptr_t addr, char* out, size_t cap) {
    if (addr >= g_image_lo && addr < g_image_hi) {
        std::snprintf(out, cap, "the game's image, VA 0x%08X",
                      (unsigned)(static_cast<std::uint32_t>(addr) - ImageBase() + image::kBase));
        return;
    }
    if (addr >= g_engine_lo && addr < g_engine_hi) {
        const std::uintptr_t base = image::g_ownBase ? image::g_ownBase : g_engine_lo;
        std::snprintf(out, cap, "%s+0x%lX", g_engine, (unsigned long)(addr - base));
        return;
    }
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(addr), &info) && info.dli_fname && info.dli_fname[0]) {
        const char* name = std::strrchr(info.dli_fname, '/');
        std::snprintf(out, cap, "%s+0x%lX", name ? name + 1 : info.dli_fname,
                      (unsigned long)(addr - reinterpret_cast<std::uintptr_t>(info.dli_fbase)));
    } else {
        std::snprintf(out, cap, "no module");
    }
}

const char* EngineTag() { return g_engine; }

}  // namespace crash

// `dll_module`: an address inside the engine's own module (the program, or
// the shared object that holds this file).
void Crash_Start(void* dll_module) {
    std::string path;
    if (os::ModulePath(dll_module, &path)) {
        const std::size_t slash = path.rfind('/');
        std::snprintf(g_engine, sizeof g_engine, "%s", path.c_str() + (slash == std::string::npos ? 0 : slash + 1));
    }
    Extent engine{reinterpret_cast<std::uintptr_t>(dll_module), 0, 0};
    if (!dl_iterate_phdr(FindExtent, &engine)) Fatal("crash reporter: no loaded object holds %p", dll_module);
    // The game's image: the linked span the engine maps it in (exe_layout.h),
    // from its base to the end of the pool.
    g_engine_lo = engine.lo, g_engine_hi = engine.hi;
    g_image_lo = Img(exe_layout::kImageBase), g_image_hi = Img(exe_layout::kPoolVa + exe_layout::kPoolSize);
    crash::SetRanges(engine.lo, engine.hi, g_image_lo, g_image_hi);
    g_armed_tid = os::ThreadId();
    const os::StackRange stack = os::CurrentStack();
    g_armed_stack_lo = reinterpret_cast<std::uintptr_t>(stack.limit);
    g_armed_stack_hi = reinterpret_cast<std::uintptr_t>(stack.base);

    if (pipe2(g_pipe, O_CLOEXEC | O_NONBLOCK) != 0) g_pipe[0] = g_pipe[1] = -1;
    if (sem_init(&g_wake, 0, 0) != 0) Fatal("crash reporter: sem_init failed, error %d", errno);
    std::thread(Reporter).detach();

    stack_t alt{};
    alt.ss_sp = g_alt_stack;
    alt.ss_size = sizeof g_alt_stack;
    if (sigaltstack(&alt, nullptr) != 0) Fatal("crash reporter: sigaltstack failed, error %d", errno);
    struct sigaction sa{};
    sa.sa_sigaction = OnSignal;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    for (int sig : kSignals)
        if (sigaction(sig, &sa, &g_old[sig]) != 0) Fatal("crash reporter: sigaction(%d) failed, error %d", sig, errno);
    Log("crash reporter armed (signals 4, 6, 7, 8, 11; thread %lu on an alternate stack, its own stack 0x%llX..0x%llX; "
        "no minidump on this system)",
        (unsigned long)g_armed_tid, static_cast<unsigned long long>(g_armed_stack_lo),
        static_cast<unsigned long long>(g_armed_stack_hi));

    char num[16];
    if (os::GetEnv("BOF3X_CRASH_TEST", num, sizeof num) && num[0]) {
        const unsigned long seconds = std::strtoul(num, nullptr, 10);
        std::thread(SelfTest, seconds).detach();
    }
}

}  // namespace bof3
