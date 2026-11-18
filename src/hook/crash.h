// Crash reporter: when the game faults, say where, in bof3x.log, and on
// Windows leave a minidump next to it (docs/crash-reporter.md). Always on;
// beyond one idle reporter thread, costs nothing until a fault is raised.
// BOF3X_CRASH_TEST=<seconds> faults on purpose, to test it.
//
// It observes and never handles: every fault is passed on, so the game, its
// own handlers and the system's crash handling behave exactly as without it.
// The report's lines are crash.cpp's; the catcher is crash_win32.cpp's on
// Windows and crash_posix.cpp's on Linux (docs/linux-round.md LB7).
#pragma once

#include <cstdint>

namespace bof3 {

// `dll_module`: the engine's module (on Windows the DLL's handle; on POSIX
// any address inside the program or shared object that holds the engine).
void Crash_Start(void* dll_module);

// The exit trace's last handler (win_main.cpp, BOF3X_EXITTRACE): a
// breakpoint or a single step that no other handler took is handed to
// `on_break`, at most four times a process, on the thread that raised it,
// then passed on. `what` names it as the system does: "exception 0x80000003"
// on Windows (a vectored handler, last in the chain), "signal 5 (SIGTRAP,
// code 1)" on POSIX (a SIGTRAP handler; passed on, the process ends as
// SIGTRAP's default disposition ends it). False if it cannot be armed.
using BreakFn = void (*)(const char* what, std::uintptr_t ip, std::uintptr_t sp, std::uint32_t flags);
bool Crash_WatchBreaks(BreakFn on_break);

}  // namespace bof3
