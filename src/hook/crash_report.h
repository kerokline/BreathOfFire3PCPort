// The crash report's portable half (crash.cpp) and what each system's catcher
// gives it (crash_win32.cpp, crash_posix.cpp): docs/linux-round.md unit LB7.
//
// A catcher copies the facts of a fault into a Fault, on the faulting thread,
// and hands it to a reporter thread of its own; the reporter calls
// WriteReport, adds what only its system has (Windows: the minidump), then
// EndReport. The `CRASH n:` lines WriteReport writes are one format on both
// systems but for the first line, which names a Windows exception code or a
// POSIX signal; tools/crash_report.py --log reads them.
#pragma once

#include <cstddef>
#include <cstdint>

namespace bof3::crash {

enum class Access : std::uint8_t { None, Read, Write, Execute, Other };

struct Fault {
    long number = 0;             // the report's n: 0 for the first
    bool is_signal = false;      // POSIX: `code` is a signal number; Windows: an exception code
    std::uint32_t code = 0;
    int sub_code = 0;            // POSIX si_code
    std::uintptr_t addr = 0;     // the instruction that faulted
    std::uint32_t thread = 0;
    Access access = Access::None;
    std::uintptr_t access_raw = 0;   // Access::Other: Windows' ExceptionInformation[0]; POSIX: the CPU's trap number
    std::uintptr_t access_at = 0;    // the address the access was to
    // The integer registers, as wide as the build's.
    std::uintptr_t ax = 0, bx = 0, cx = 0, dx = 0, si = 0, di = 0, bp = 0, sp = 0, ip = 0;
    std::uintptr_t r8 = 0, r9 = 0, r10 = 0, r11 = 0, r12 = 0, r13 = 0, r14 = 0, r15 = 0;   // x86-64 only
    std::uint32_t flags = 0;
};

// At start-up, before a fault can be reported: the engine module's extent
// (the DLL's, or the program's on POSIX) and the game image's. Capcom's code
// range is the report's own (host.h CodeLo / CodeHi).
void SetRanges(std::uintptr_t engine_lo, std::uintptr_t engine_hi, std::uintptr_t image_lo, std::uintptr_t image_hi);

// The lines from the first to the stack scan, through Log.
void WriteReport(const Fault& f);
// The state hash's buffered frames and the log, flushed: the report's last step.
void EndReport();

// Each catcher's own, for the report's use.
//   Peek: memory that may not be there, read without faulting.
//   Where: "module+0xOFFSET" for an address, or "no module".
//   EngineTag: the engine module's name, as a stack line tags its addresses.
bool Peek(std::uintptr_t addr, void* out, std::size_t n);
void Where(std::uintptr_t addr, char* out, std::size_t cap);
const char* EngineTag();

}  // namespace bof3::crash
