// The crash catcher on Windows - Windows-only by design (docs/linux-round.md,
// LB1 and LB7): it catches faults through a vectored exception handler and
// writes a dump with MiniDumpWriteDump. The report's lines are crash.cpp's;
// Linux's catcher is crash_posix.cpp.
#include "hook/crash.h"

#include <windows.h>

#include <dbghelp.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "common/image_base.h"
#include "hook/cpu_context.h"
#include "hook/crash_report.h"
#include "hook/log.h"

namespace bof3 {
namespace {

// A vectored handler, not SetUnhandledExceptionFilter: game logic runs on
// 16 KB task stacks carved out of a static buffer (docs/attract-mode.md
// section 2), outside the thread's TEB stack limits, and Windows will not walk
// frame-based handlers - the unhandled-exception filter among them - from
// such a stack. A vectored handler is called before that check. (Reasoning
// from how Windows dispatches, not yet observed: docs/crash-reporter.md
// section 2.)
//
// The price is that it sees first-chance exceptions, including any the game
// goes on to handle itself. So it reports only codes that are faults rather
// than signalling, reports at most kMaxReports per process, and always
// returns EXCEPTION_CONTINUE_SEARCH.
constexpr LONG kMaxReports = 4;

// The handler runs on the faulting stack, which may be small or the thing that
// is broken. It copies the facts here and hands over to a thread that was
// created at start-up and has a stack of its own.
struct Job {
    EXCEPTION_RECORD record;
    CONTEXT context;
    EXCEPTION_POINTERS* pointers;  // on the faulting thread's stack; it waits
    DWORD thread;
};
Job g_job;
volatile LONG g_busy = 0;
volatile LONG g_reports = 0;
HANDLE g_wake = nullptr;
HANDLE g_done = nullptr;

wchar_t g_dump_path[MAX_PATH];
size_t g_dump_stem = 0;  // length of "...\bof3x."

using WriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                  PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
WriteDumpFn g_write_dump = nullptr;

bool IsFault(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_DATATYPE_MISALIGNMENT:
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            return true;
        default:
            return false;
    }
}

// The record and the context as the portable report's facts.
crash::Fault FaultOf(const EXCEPTION_RECORD& r, const CONTEXT& c, LONG n, DWORD thread) {
    crash::Fault f;
    f.number = n;
    f.is_signal = false;
    f.code = r.ExceptionCode;
    f.addr = reinterpret_cast<std::uintptr_t>(r.ExceptionAddress);
    f.thread = thread;
    if ((r.ExceptionCode == EXCEPTION_ACCESS_VIOLATION || r.ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
        r.NumberParameters >= 2) {
        // ExceptionInformation[0]: 0 read, 1 write, 8 execute (DEP).
        const ULONG_PTR k = r.ExceptionInformation[0];
        f.access = k == 0 ? crash::Access::Read : k == 1 ? crash::Access::Write : k == 8 ? crash::Access::Execute
                                                                                        : crash::Access::Other;
        f.access_raw = k;
        f.access_at = r.ExceptionInformation[1];
    }
#if defined(__x86_64__)
    f.ax = c.Rax, f.bx = c.Rbx, f.cx = c.Rcx, f.dx = c.Rdx, f.si = c.Rsi, f.di = c.Rdi;
    f.r8 = c.R8, f.r9 = c.R9, f.r10 = c.R10, f.r11 = c.R11, f.r12 = c.R12, f.r13 = c.R13, f.r14 = c.R14, f.r15 = c.R15;
    f.bp = c.Rbp, f.sp = c.Rsp, f.ip = c.Rip;
#else
    f.ax = c.Eax, f.bx = c.Ebx, f.cx = c.Ecx, f.dx = c.Edx, f.si = c.Esi, f.di = c.Edi;
    f.bp = c.Ebp, f.sp = c.Esp, f.ip = c.Eip;
#endif
    f.flags = c.EFlags;
    return f;
}

void Report() {
    const LONG n = g_reports;
    crash::WriteReport(FaultOf(g_job.record, g_job.context, n, g_job.thread));

    if (g_write_dump) {
        _snwprintf(g_dump_path + g_dump_stem, MAX_PATH - g_dump_stem, L"crash-%lu-%ld.dmp",
                      GetCurrentProcessId(), (long)n);
        HANDLE f = CreateFileW(g_dump_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                               nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei;
            mei.ThreadId = g_job.thread;
            mei.ExceptionPointers = g_job.pointers;
            mei.ClientPointers = FALSE;
            // Data segments carry the game's globals, which is where D4 was
            // read from (docs/known-defects.md).
            auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory);
            BOOL ok = g_write_dump(GetCurrentProcess(), GetCurrentProcessId(), f, type, &mei, nullptr, nullptr);
            CloseHandle(f);
            if (ok)
                Log("CRASH %ld: dump written: %s", (long)n, Utf8Path(g_dump_path).text);
            else
                Log("CRASH %ld: MiniDumpWriteDump failed, error 0x%08lX", (long)n, GetLastError());
        } else {
            Log("CRASH %ld: cannot create %s, error %lu", (long)n, Utf8Path(g_dump_path).text, GetLastError());
        }
    } else {
        Log("CRASH %ld: no dump - dbghelp.dll did not load", (long)n);
    }
    crash::EndReport();
}

// The one reporter thread: waits on g_wake, reports g_job, sets g_done.
DWORD WINAPI Reporter(LPVOID) {
    // Loaded here rather than in DllMain (loader lock) or in the handler.
    if (HMODULE h = LoadLibraryA("dbghelp.dll"))
        g_write_dump = reinterpret_cast<WriteDumpFn>(
            reinterpret_cast<void*>(GetProcAddress(h, "MiniDumpWriteDump")));
    for (;;) {
        WaitForSingleObject(g_wake, INFINITE);
        Report();
        SetEvent(g_done);
    }
}

// One report at a time: a second fault while g_busy is passed on unreported.
LONG CALLBACK OnException(EXCEPTION_POINTERS* info) {
    if (!IsFault(info->ExceptionRecord->ExceptionCode)) return EXCEPTION_CONTINUE_SEARCH;
    if (g_reports >= kMaxReports) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return EXCEPTION_CONTINUE_SEARCH;

    g_job.record = *info->ExceptionRecord;
    g_job.context = *info->ContextRecord;
    g_job.pointers = info;
    g_job.thread = GetCurrentThreadId();
    SetEvent(g_wake);
    // Bounded: if the reporter cannot finish, the crash must still proceed.
    WaitForSingleObject(g_done, 20000);
    InterlockedIncrement(&g_reports);
    InterlockedExchange(&g_busy, 0);
    return EXCEPTION_CONTINUE_SEARCH;
}

// BOF3X_CRASH_TEST=seconds: fault on purpose that long after start-up, to see
// the reporter work. The read is of address 0x10, which is never mapped.
DWORD WINAPI SelfTest(LPVOID seconds) {
    Sleep(static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(seconds)) * 1000);
    Log("crash reporter: BOF3X_CRASH_TEST faulting now");
    volatile int* nowhere = reinterpret_cast<int*>(0x10);
    return static_cast<DWORD>(*nowhere);
}

// A module's extent, from its own headers (the width's: host.cpp), as the
// 32-bit words the image and the DLL fit - Low32 is the Fatal if either lies
// above 4 GB (image_base.h).
void ImageRange(const void* module, std::uint32_t* lo, std::uint32_t* hi) {
    auto* base = static_cast<const std::uint8_t*>(module);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto b = Low32(base);
    *lo = b;
    *hi = b + nt->OptionalHeader.SizeOfImage;
}

// The exit trace's handler (crash.h Crash_WatchBreaks). Called last, so it
// sees only what every other vectored handler passed on - the calltrace's
// among them.
BreakFn g_on_break = nullptr;
LONG CALLBACK OnBreak(EXCEPTION_POINTERS* info) {
    static volatile LONG reports = 0;
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_SINGLE_STEP && code != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&reports) > 4) return EXCEPTION_CONTINUE_SEARCH;
    char what[32];
    std::snprintf(what, sizeof what, "exception 0x%08lX", code);
    g_on_break(what, CtxIp(*info->ContextRecord), CtxSp(*info->ContextRecord),
               static_cast<std::uint32_t>(info->ContextRecord->EFlags));
    return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

bool Crash_WatchBreaks(BreakFn on_break) {
    g_on_break = on_break;
    return AddVectoredExceptionHandler(0, OnBreak) != nullptr;
}

namespace crash {

// Memory that may not be there: never dereference, always ask.
bool Peek(std::uintptr_t addr, void* out, size_t n) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(addr), out, n, &got) && got == n;
}

void Where(std::uintptr_t addr, char* out, size_t cap) {
    MEMORY_BASIC_INFORMATION mbi;
    char path[MAX_PATH];
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof mbi) && mbi.AllocationBase &&
        GetModuleFileNameA(static_cast<HMODULE>(mbi.AllocationBase), path, sizeof path)) {
        const char* name = std::strrchr(path, '\\');
        std::snprintf(out, cap, "%s+0x%lX", name ? name + 1 : path,
                      (unsigned long)(addr - reinterpret_cast<std::uintptr_t>(mbi.AllocationBase)));
    } else {
        std::snprintf(out, cap, "no module");
    }
}

const char* EngineTag() { return "bof3x.dll"; }

}  // namespace crash

void Crash_Start(void* dll_module) {
    std::uint32_t dll_lo = 0, dll_hi = 0, image_lo = 0, image_hi = 0;
    ImageRange(dll_module, &dll_lo, &dll_hi);
    ImageRange(GetModuleHandleW(nullptr), &image_lo, &image_hi);
    crash::SetRanges(dll_lo, dll_hi, image_lo, image_hi);

    DWORD len = GetModuleFileNameW(static_cast<HMODULE>(dll_module), g_dump_path, MAX_PATH);
    if (len < 4 || len + 32 > MAX_PATH) Fatal("crash reporter: DLL path unusable for dump files");
    g_dump_stem = len - 3;  // bof3x.dll -> bof3x.crash-<pid>-<n>.dmp

    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HANDLE thread = (g_wake && g_done) ? CreateThread(nullptr, 0, Reporter, nullptr, 0, nullptr) : nullptr;
    if (!thread) Fatal("crash reporter: cannot create its thread, error %lu", GetLastError());
    CloseHandle(thread);

    // Last in the vectored chain (0). The call tracer's int3s and single steps
    // are not in IsFault's list, so they are never reported either way.
    if (!AddVectoredExceptionHandler(0, OnException)) Fatal("crash reporter: no vectored handler");
    Log("crash reporter armed");

    char num[16];
    if (GetEnvironmentVariableA("BOF3X_CRASH_TEST", num, sizeof num)) {
        auto seconds = static_cast<std::uintptr_t>(std::strtoul(num, nullptr, 10));
        HANDLE t = CreateThread(nullptr, 0, SelfTest, reinterpret_cast<LPVOID>(seconds), 0, nullptr);
        if (t) CloseHandle(t);
    }
}

}  // namespace bof3
