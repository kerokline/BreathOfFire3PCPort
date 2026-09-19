#include "hook/crash.h"

#include <windows.h>

#include <dbghelp.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/log.h"

namespace bof3 {
namespace {

// A vectored handler, not SetUnhandledExceptionFilter: game logic runs on
// 16 KB task stacks carved out of a static buffer (docs/attract-mode.md
// section 2), outside the thread's TEB stack limits, and Windows will not walk
// frame-based handlers - the unhandled-exception filter among them - from
// such a stack. A vectored handler is called before that check.
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
std::uint32_t g_text_lo = 0, g_text_hi = 0;
std::uint32_t g_dll_lo = 0, g_dll_hi = 0;

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

// Memory that may not be there: never dereference, always ask.
bool Peek(std::uint32_t addr, void* out, size_t n) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(addr)),
                             out, n, &got) &&
           got == n;
}

void Where(std::uint32_t addr, char* out, size_t cap) {
    MEMORY_BASIC_INFORMATION mbi;
    char path[MAX_PATH];
    if (VirtualQuery(reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(addr)), &mbi, sizeof mbi) &&
        mbi.AllocationBase &&
        GetModuleFileNameA(static_cast<HMODULE>(mbi.AllocationBase), path, sizeof path)) {
        const char* name = std::strrchr(path, '\\');
        std::snprintf(out, cap, "%s+0x%lX", name ? name + 1 : path,
                      (unsigned long)(addr - reinterpret_cast<std::uintptr_t>(mbi.AllocationBase)));
    } else {
        std::snprintf(out, cap, "no module");
    }
}

bool InCode(std::uint32_t v) {
    return (v >= g_text_lo && v < g_text_hi) || (v >= g_dll_lo && v < g_dll_hi);
}

// A stack word is a plausible return address if it points into code and the
// bytes before it are a call: E8 rel32, or FF /2 in its 2-, 3- and 6-byte forms.
bool AfterCall(std::uint32_t v) {
    std::uint8_t b[6];
    if (!InCode(v) || !Peek(v - 6, b, sizeof b)) return false;
    return b[1] == 0xE8 || b[4] == 0xFF || b[3] == 0xFF || b[0] == 0xFF;
}

void Report() {
    const EXCEPTION_RECORD& r = g_job.record;
    const CONTEXT& c = g_job.context;
    LONG n = g_reports;
    char where[MAX_PATH + 16];
    auto addr = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(r.ExceptionAddress));
    Where(addr, where, sizeof where);

    Log("CRASH %ld: exception 0x%08lX at 0x%08X (%s), thread %lu", (long)n, r.ExceptionCode,
        (unsigned)addr, where, g_job.thread);
    if ((r.ExceptionCode == EXCEPTION_ACCESS_VIOLATION || r.ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
        r.NumberParameters >= 2) {
        static const char* const kKind[] = {"reading", "writing"};
        ULONG_PTR k = r.ExceptionInformation[0];
        Log("CRASH %ld: %s 0x%08lX", (long)n, k < 2 ? kKind[k] : "executing",
            (unsigned long)r.ExceptionInformation[1]);
    }
    Log("CRASH %ld: eax %08lX ebx %08lX ecx %08lX edx %08lX esi %08lX edi %08lX", (long)n, c.Eax, c.Ebx,
        c.Ecx, c.Edx, c.Esi, c.Edi);
    Log("CRASH %ld: ebp %08lX esp %08lX eip %08lX efl %08lX", (long)n, c.Ebp, c.Esp, c.Eip, c.EFlags);

    std::uint8_t code[16];
    if (Peek(c.Eip, code, sizeof code)) {
        char hex[3 * sizeof code + 1];
        for (size_t i = 0; i < sizeof code; ++i) std::snprintf(hex + 3 * i, 4, "%02X ", code[i]);
        Log("CRASH %ld: bytes at eip: %s", (long)n, hex);
    }

    // Where the game was. Read through Peek: a crash may have taken these too.
    std::uint16_t area = 0, task0[2] = {0, 0}, mode_step[2] = {0, 0}, message = 0;
    Peek(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&Game_AreaNumber)), &area, sizeof area);
    Peek(addr::Task_Records, task0, sizeof task0);
    Peek(addr::Task_Records + 0x18, mode_step, sizeof mode_step);
    Peek(addr::MsgBoxState + 8, &message, sizeof message);
    Log("CRASH %ld: area 0x%04X, task 0 state %u sleep %u, mode %u step %u, message 0x%04X", (long)n, area,
        task0[0], task0[1], mode_step[0], mode_step[1], message);

    // Not a stack walk - the game's code has no frame pointers to follow and
    // the task stacks are hand-switched. Every word that looks like a return
    // address, nearest first; tools/crash_report.py names them.
    int shown = 0;
    for (std::uint32_t off = 0; off < 0x2000 && shown < 40; off += 4) {
        std::uint32_t v = 0;
        if (!Peek(c.Esp + off, &v, sizeof v)) break;
        if (!AfterCall(v)) continue;
        Log("CRASH %ld: stack +0x%04lX  0x%08lX%s", (long)n, (unsigned long)off, (unsigned long)v,
            v >= g_dll_lo && v < g_dll_hi ? "  (bof3x.dll)" : "");
        ++shown;
    }

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
                Log("CRASH %ld: dump written: %ls", (long)n, g_dump_path);
            else
                Log("CRASH %ld: MiniDumpWriteDump failed, error 0x%08lX", (long)n, GetLastError());
        } else {
            Log("CRASH %ld: cannot create %ls, error %lu", (long)n, g_dump_path, GetLastError());
        }
    } else {
        Log("CRASH %ld: no dump - dbghelp.dll did not load", (long)n);
    }
    LogFlush();
}

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

void ImageRange(const void* module, std::uint32_t* lo, std::uint32_t* hi, const char* section) {
    auto* base = static_cast<const std::uint8_t*>(module);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    auto b = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base));
    *lo = b;
    *hi = b + nt->OptionalHeader.SizeOfImage;
    if (!section) return;
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++sec) {
        if (std::strncmp(reinterpret_cast<const char*>(sec->Name), section, 8) != 0) continue;
        *lo = b + sec->VirtualAddress;
        *hi = *lo + sec->Misc.VirtualSize;
        return;
    }
    Fatal("crash reporter: BOF3.exe has no %s section", section);
}

}  // namespace

void Crash_Start(void* dll_module) {
    ImageRange(GetModuleHandleW(nullptr), &g_text_lo, &g_text_hi, ".text");
    ImageRange(dll_module, &g_dll_lo, &g_dll_hi, nullptr);

    DWORD len = GetModuleFileNameW(static_cast<HMODULE>(dll_module), g_dump_path, MAX_PATH);
    if (len < 4 || len + 32 > MAX_PATH) Fatal("crash reporter: DLL path unusable for dump files");
    g_dump_stem = len - 3;  // bof3x.dll -> bof3x.crash-<pid>-<n>.dmp

    g_wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HANDLE thread = (g_wake && g_done) ? CreateThread(nullptr, 0, Reporter, nullptr, 0, nullptr) : nullptr;
    if (!thread) Fatal("crash reporter: cannot create its thread, error %lu", GetLastError());
    CloseHandle(thread);

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
