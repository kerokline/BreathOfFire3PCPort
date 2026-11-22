// The harness watchers on a system that is not Windows (docs/linux-round.md,
// "The 38 files, staged as one round", R2).
//
// gapwatch.cpp, write_watch.cpp and calltrace.cpp are Windows files by design:
// they work by debug registers set through a thread's CONTEXT, PAGE_GUARD
// pages and a vectored exception handler (tools/linux_probe/linux_tree.py's
// WINDOWS_ONLY). A port is a separate piece of work. Until then this file
// defines their entry functions for any other system, so that their callers
// (dllmain.cpp, exe_host.cpp, input_script.cpp, inject_all.cpp) need no #if:
//
//   * unset, each switch does what it does on Windows: nothing;
//   * set, each one stops the game with a sentence that names the switch
//     (CLAUDE.md rule 4: a switch that is asked for and does nothing is a
//     silent fork, so it is refused).
//
// The Windows builds do not compile this file (CMakeLists.txt does not list
// it), and its body is empty there in case a list ever takes it. The Linux
// build adds it (tools/linux_probe/linux_tree.py, LINUX_ONLY).
#ifndef _WIN32

#include <cstdint>

#include "hook/calltrace.h"
#include "hook/gapwatch.h"
#include "hook/log.h"
#include "hook/write_watch.h"
#include "os/os.h"

namespace {

// Whether the variable is set to a value that is not empty, as the Windows
// files test it (GetEnvironmentVariable answers 0 for both unset and empty).
bool Set(const char* name) {
    char v[2];
    return bof3::os::GetEnv(name, v, sizeof v) > 0;
}

[[noreturn]] void Refuse(const char* name, const char* what) {
    bof3::Fatal("%s is set, but %s is a Windows-only watcher (debug registers, guard pages and a vectored exception "
                "handler) and is not built for this system; unset %s (docs/linux-round.md, R2)",
                name, what, name);
}

}  // namespace

namespace bof3 {

void CallTrace_Start(void*) {
    if (Set("BOF3X_CALLTRACE")) Refuse("BOF3X_CALLTRACE", "the call trace (calltrace.cpp)");
}

void GapWatch_Start(void*) {
    if (Set("BOF3X_GAPWATCH")) Refuse("BOF3X_GAPWATCH", "the gap watch (gapwatch.cpp)");
}

// The watch is never on here (GapWatch_Start refuses it), and on Windows these
// three do nothing while it is off. Why the harness calls the first two
// (input_script.cpp, DeviceLatch and the recipe's `watch` lines): on Windows
// a watched page of .data carries PAGE_GUARD, and a read by the state hash or
// by a recipe's watch would count as a read of the game's; Pause takes every
// guard off around such a read and Tick puts them back (gapwatch.h). With no
// watch there is no guard to take off or put back, on either system: the
// empty bodies are the Windows behaviour with BOF3X_GAPWATCH unset, the only
// state this system allows (wave B, B1; the link wanted both names).
void GapWatch_Pause() {}
void GapWatch_Tick() {}
void GapWatch_Flush() {}

// Asked for by BOF3X_SHADOW=gapwatch (inject_all.cpp): the test of a watcher
// that does not exist here cannot pass, so it is refused, not skipped.
void GapWatch_SelfTest() {
    Fatal("the gap watch's self-test (BOF3X_SHADOW=gapwatch) was asked for, but the gap watch (gapwatch.cpp) is a "
          "Windows-only watcher and is not built for this system (docs/linux-round.md, R2)");
}

}  // namespace bof3

namespace write_watch {

void Start() {
    if (Set("BOF3X_WRITEWATCH")) Refuse("BOF3X_WRITEWATCH", "the write watch (write_watch.cpp)");
}

}  // namespace write_watch

#endif  // !_WIN32
