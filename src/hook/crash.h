// Crash reporter: when the game faults, say where, in bof3x.log, and leave a
// minidump next to it (docs/crash-reporter.md). Always on; beyond one idle
// reporter thread, costs nothing until an exception is raised.
// BOF3X_CRASH_TEST=<seconds> faults on purpose, to test it.
//
// It observes and never handles: every exception is passed on, so the game,
// its own handlers and Windows Error Reporting behave exactly as without it.
#pragma once

namespace bof3 {

void Crash_Start(void* dll_module);

}  // namespace bof3
