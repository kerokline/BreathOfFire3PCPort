// The crash report, the portable half (docs/linux-round.md unit LB7): the
// `CRASH n:` lines of bof3x.log, from the facts a catcher hands over
// (crash_report.h). The catchers are per system: crash_win32.cpp (a vectored
// exception handler, MiniDumpWriteDump) and crash_posix.cpp (signals on an
// alternate stack; no minidump, and its report says so).
//
// BOF3X_CRASH_TEST=<seconds> faults on purpose that long after start-up, on
// either system, to see the reporter work: a read of address 0x10.
// tools/crash_report.py reads the dump (Windows) or these lines (--log).
#include "hook/crash.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/bind.h"
#include "hook/crash_report.h"
#include "hook/host.h"
#include "hook/log.h"
#include "hook/statehash.h"

namespace bof3::crash {
namespace {

// Addresses are native (std::uintptr_t; docs/x64-build-design.md 11): a fault
// in a system DLL lies at 0x7FFA... on x86-64 and is named, not truncated.
// Capcom's code range and the image's stay 32-bit words - the image lies below
// 4 GB on either width - and a native value is compared against them whole.
std::uint32_t g_text_lo = 0, g_text_hi = 0;   // places: Capcom's code range as this host maps it
std::uintptr_t g_engine_lo = 0, g_engine_hi = 0;
std::uintptr_t g_image_lo = 0, g_image_hi = 0;   // the game's image
char g_tag[80] = "";                             // "  (bof3x.dll)": a stack word in the engine

bool InCode(std::uintptr_t v) {
    return (v >= g_text_lo && v < g_text_hi) || (v >= g_engine_lo && v < g_engine_hi);
}

// " (VA 0x...)" for a place in the game's image when it lies elsewhere than
// 0x400000 (docs/relocatable-image-design.md 8): the logs, symbols.toml and
// tools/crash_report.py speak VAs. Empty otherwise.
const char* VaOf(std::uintptr_t a, char* out, size_t cap) {
    out[0] = '\0';
    if (a >= g_image_lo && a < g_image_hi && ImageBase() != image::kBase)
        std::snprintf(out, cap, " (VA 0x%08X)", (unsigned)(a - ImageBase() + image::kBase));
    return out;
}

// A stack word is a plausible return address if it points into code and the
// bytes before it are a call: E8 rel32, or FF /2 in its 2-, 3- and 6-byte forms.
// Only the FF opcode byte is tested, not the /2 in ModRM, so this is a filter
// that lets some non-calls through - a scan, not a walk. The same bytes on
// x86-64 (a REX prefix before FF leaves the FF where the 3-byte test looks).
bool AfterCall(std::uintptr_t v) {
    std::uint8_t b[6];
    if (!InCode(v) || !Peek(v - 6, b, sizeof b)) return false;
    return b[1] == 0xE8 || b[4] == 0xFF || b[3] == 0xFF || b[0] == 0xFF;
}

const char* SignalName(std::uint32_t s) {
    // The numbers are Linux's (x86 and x86-64 alike), the catcher's five.
    switch (s) {
        case 4: return "SIGILL";
        case 6: return "SIGABRT";
        case 7: return "SIGBUS";
        case 8: return "SIGFPE";
        case 11: return "SIGSEGV";
        default: return "a signal";
    }
}

}  // namespace

void SetRanges(std::uintptr_t engine_lo, std::uintptr_t engine_hi, std::uintptr_t image_lo, std::uintptr_t image_hi) {
    // Capcom's code range as this host maps it: BOF3.exe's .text, or the
    // stub page (host.h) - in bof3x.exe the no-access trap, where no return
    // address lies and an access is named as the trap (WriteReport).
    // CodeLo and CodeHi are VAs; these are the places.
    g_text_lo = Img(CodeLo());
    g_text_hi = Img(CodeHi());
    g_engine_lo = engine_lo, g_engine_hi = engine_hi;
    g_image_lo = image_lo, g_image_hi = image_hi;
    std::snprintf(g_tag, sizeof g_tag, "  (%s)", EngineTag());
}

void WriteReport(const Fault& f) {
    const long n = f.number;
    char where[260 + 16];   // MAX_PATH and a "+0x" offset
    const std::uintptr_t addr = f.addr;
    Where(addr, where, sizeof where);

    char va[24];
    if (f.is_signal) {
        Log("CRASH %ld: signal %u (%s, code %d) at 0x%0*llX%s (%s), thread %lu", n, (unsigned)f.code, SignalName(f.code),
            f.sub_code, static_cast<int>(2 * sizeof(std::uintptr_t)), static_cast<unsigned long long>(addr),
            VaOf(addr, va, sizeof va), where, (unsigned long)f.thread);
    } else {
#if defined(__x86_64__)
        Log("CRASH %ld: exception 0x%08lX at 0x%016llX%s (%s), thread %lu", n, (unsigned long)f.code,
            static_cast<unsigned long long>(addr), VaOf(addr, va, sizeof va), where, (unsigned long)f.thread);
#else
        Log("CRASH %ld: exception 0x%08lX at 0x%08X%s (%s), thread %lu", n, (unsigned long)f.code,
            (unsigned)addr, VaOf(addr, va, sizeof va), where, (unsigned long)f.thread);
#endif
    }
    if (f.access != Access::None) {
        const char* kind = f.access == Access::Read ? "reading" : f.access == Access::Write ? "writing"
                         : f.access == Access::Execute ? "executing" : nullptr;
#if defined(__x86_64__)
        if (kind)
            Log("CRASH %ld: %s 0x%016llX%s", n, kind, static_cast<unsigned long long>(f.access_at),
                VaOf(f.access_at, va, sizeof va));
        else if (f.is_signal)
            Log("CRASH %ld: an access at 0x%016llX, of a kind the CPU's trap %lu does not say", n,
                static_cast<unsigned long long>(f.access_at), (unsigned long)f.access_raw);
        else
            Log("CRASH %ld: access kind %lu at 0x%016llX", n, (unsigned long)f.access_raw,
                static_cast<unsigned long long>(f.access_at));
#else
        if (kind)
            Log("CRASH %ld: %s 0x%08lX%s", n, kind, (unsigned long)f.access_at, VaOf(f.access_at, va, sizeof va));
        else if (f.is_signal)
            Log("CRASH %ld: an access at 0x%08lX, of a kind the CPU's trap %lu does not say", n,
                (unsigned long)f.access_at, (unsigned long)f.access_raw);
        else
            Log("CRASH %ld: access kind %lu at 0x%08lX", n, (unsigned long)f.access_raw, (unsigned long)f.access_at);
#endif
        // The trap (docs/relocatable-image-design.md 4.5): in bof3x.exe .stub
        // is no-access, so a code value no function of ours stands for faults
        // there - named by its VA and the [[func]] it lies in.
        const std::uintptr_t at = f.access_at;
        if (Bind_Active() && at >= g_text_lo && at < g_text_hi) {
            const std::uint32_t va32 = static_cast<std::uint32_t>(at) - ImageBase() + image::kBase;
            std::uint32_t start = 0;
            const char* fn = Bind_FuncName(va32, &start);
            Log("CRASH %ld: the trap - %s VA 0x%08X, %s+0x%X: a code value no function of ours stands for (the bind "
                "table, symbols.toml impl)",
                n, f.access == Access::Execute ? "a call or jump to" : "an access to", (unsigned)va32, fn ? fn : "no [[func]]",
                (unsigned)(va32 - start));
        }
    }
#if defined(__x86_64__)
    Log("CRASH %ld: rax %016llX rbx %016llX rcx %016llX rdx %016llX rsi %016llX rdi %016llX", n,
        static_cast<unsigned long long>(f.ax), static_cast<unsigned long long>(f.bx), static_cast<unsigned long long>(f.cx),
        static_cast<unsigned long long>(f.dx), static_cast<unsigned long long>(f.si), static_cast<unsigned long long>(f.di));
    Log("CRASH %ld: r8 %016llX r9 %016llX r10 %016llX r11 %016llX r12 %016llX r13 %016llX r14 %016llX r15 %016llX", n,
        static_cast<unsigned long long>(f.r8), static_cast<unsigned long long>(f.r9), static_cast<unsigned long long>(f.r10),
        static_cast<unsigned long long>(f.r11), static_cast<unsigned long long>(f.r12), static_cast<unsigned long long>(f.r13),
        static_cast<unsigned long long>(f.r14), static_cast<unsigned long long>(f.r15));
    Log("CRASH %ld: rbp %016llX rsp %016llX rip %016llX efl %08lX", n, static_cast<unsigned long long>(f.bp),
        static_cast<unsigned long long>(f.sp), static_cast<unsigned long long>(f.ip), (unsigned long)f.flags);
#else
    Log("CRASH %ld: eax %08lX ebx %08lX ecx %08lX edx %08lX esi %08lX edi %08lX", n, (unsigned long)f.ax,
        (unsigned long)f.bx, (unsigned long)f.cx, (unsigned long)f.dx, (unsigned long)f.si, (unsigned long)f.di);
    Log("CRASH %ld: ebp %08lX esp %08lX eip %08lX efl %08lX", n, (unsigned long)f.bp, (unsigned long)f.sp,
        (unsigned long)f.ip, (unsigned long)f.flags);
#endif

    std::uint8_t code[16];
    if (Peek(f.ip, code, sizeof code)) {
        char hex[3 * sizeof code + 1];
        for (size_t i = 0; i < sizeof code; ++i) std::snprintf(hex + 3 * i, 4, "%02X ", code[i]);
#if defined(__x86_64__)
        Log("CRASH %ld: bytes at rip: %s", n, hex);
#else
        Log("CRASH %ld: bytes at eip: %s", n, hex);
#endif
    }

    // Where the game was. Read through Peek: a crash may have taken these too.
    // Task_Records +0 / +2: task 0's state and frames left to sleep; +0x18 /
    // +0x1A: the top-level mode and step; MsgBoxState +8: the message index
    // (symbols.toml Task_Records, MsgBoxState).
    std::uint16_t area = 0, task0[2] = {0, 0}, mode_step[2] = {0, 0}, message = 0;
    Peek(reinterpret_cast<std::uintptr_t>(&Game_AreaNumber), &area, sizeof area);
    Peek(Img(addr::Task_Records), task0, sizeof task0);
    Peek(Img(addr::Task_Records) + 0x18, mode_step, sizeof mode_step);
    Peek(Img(addr::MsgBoxState) + 8, &message, sizeof message);
    Log("CRASH %ld: area 0x%04X, task 0 state %u sleep %u, mode %u step %u, message 0x%04X", n, area,
        task0[0], task0[1], mode_step[0], mode_step[1], message);

    // Not a stack walk - the game's code has no frame pointers to follow and
    // the task stacks are hand-switched. Every word that looks like a return
    // address, nearest first; tools/crash_report.py names them. The words
    // are the width's: 4 bytes at 4-byte steps, or 8 at 8.
    int shown = 0;
    for (std::uint32_t off = 0; off < 0x2000 && shown < 40; off += sizeof(std::uintptr_t)) {
        std::uintptr_t v = 0;
        if (!Peek(f.sp + off, &v, sizeof v)) break;
        if (!AfterCall(v)) continue;
#if defined(__x86_64__)
        Log("CRASH %ld: stack +0x%04lX  0x%016llX%s%s", n, (unsigned long)off, static_cast<unsigned long long>(v),
            v >= g_engine_lo && v < g_engine_hi ? g_tag : "", VaOf(v, va, sizeof va));
#else
        Log("CRASH %ld: stack +0x%04lX  0x%08lX%s%s", n, (unsigned long)off, (unsigned long)v,
            v >= g_engine_lo && v < g_engine_hi ? g_tag : "", VaOf(v, va, sizeof va));
#endif
        ++shown;
    }
}

void EndReport() {
    // The state hash's buffered frames, up to the one that faulted. On the
    // reporter's thread, while the faulting one waits; a fault inside the
    // hash's own fwrite would block here, and the catcher's 20 s bound still
    // lets the crash proceed.
    StateHash_Flush();
    LogFlush();
}

}  // namespace bof3::crash
