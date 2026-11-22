// The stack watch on Linux (docs/linux-round.md LB7): the start-up
// floating-point words, as stackwatch_win32.cpp logs them; the stack
// measurement itself is Windows' and refuses here.
//
// What the Windows file measures has no Linux counterpart without a new
// design: the main thread's TEB words (StackBase, StackLimit,
// DeallocationStack), the image header's SizeOfStackReserve and
// SizeOfStackCommit, the guard page that moves down as the stack grows and
// VirtualQuery's committed run below it, and - hosted - the game's main
// thread found suspended among the process's threads. On Linux the exe host
// runs the main thread on a stack it maps whole at the Windows place, with a
// fixed guard page under it (exe_host_posix.cpp row 3; os/placed.h): there is
// no growth to follow, and an overflow is crash_posix.cpp's to report. The
// owner's call 2 of wave B (2026-11-20, docs/linux-round.md "Wave B, staged"):
// a developer's switch, kept a refusal here and said so in the switch's
// ledger line. So BOF3X_STACKWATCH=1 is a Fatal here, said once at
// start-up, never a run that silently measures nothing; StackWatch_Armed
// stays 0, so Task_SetStackBase's asm never calls StackWatch_AtSetStackBase,
// and a call that came all the same is a Fatal too.
#include "hook/stackwatch.h"

#include "hook/log.h"
#include "os/os.h"

extern "C" volatile unsigned char StackWatch_Armed = 0;

namespace bof3 {

void StackWatch_Start() {
    unsigned short cw;
    unsigned mxcsr;
    asm volatile("fnstcw %0" : "=m"(cw));
    asm volatile("stmxcsr %0" : "=m"(mxcsr));
    Log("fpu: thread %lu at start-up: x87 control word 0x%04X, MXCSR 0x%08X", (unsigned long)os::ThreadId(), cw, mxcsr);
    if (os::GetEnv("BOF3X_STACKWATCH", nullptr, 0) > 0)
        Fatal("BOF3X_STACKWATCH=1 is refused on Linux (the owner's call 2 of wave B, 2026-11-20): it is a developer's "
              "switch that measures how far Windows grows the main thread's stack - the TEB's words, the guard page "
              "Windows moves down, VirtualQuery's committed run. Here the start-up maps that stack whole at "
              "0x60000..0x160000 with a fixed guard page under it, so nothing grows to watch, and an overflow is a fault "
              "on the guard page that the crash report names. Unset BOF3X_STACKWATCH, or measure on Windows "
              "(docs/linux-round.md, B2b as run)");
    Log("stack: not measured on this system (the stack watch is Windows'; BOF3X_STACKWATCH refuses here)");
}

void StackWatch_Exit() {
    if (!StackWatch_Armed) return;   // never armed on this system (StackWatch_Start)
    Fatal("stackwatch: armed on Linux, where StackWatch_Start never arms it");
}

}  // namespace bof3

extern "C" void __cdecl StackWatch_AtSetStackBase(unsigned long) {
    bof3::Fatal("stackwatch: Task_SetStackBase called the stack watch on Linux, where it is never armed");
}
