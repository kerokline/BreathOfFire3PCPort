// The task scheduler: the game's four cooperative tasks and the stack switch
// between them. docs/task_sched.md.
//
// Capcom wrote this unit by hand (0x5A98A0..0x5A9A21): it pushes a fixed
// register set, stores esp in a record and loads another, and a task is
// resumed by the `ret` that ends six pops. What a task sees across a switch
// is therefore a register contract, not a C signature, and the six entries
// that switch or read the stack pointer are naked functions whose asm is the
// original's, instruction for instruction, with the same registers left in
// the same states - including eax and ebx, which nothing is known to read,
// because any compiled task body may. They live in game/task_switch.h, one
// header per architecture (the i686 half is Capcom's text; the x86_64 half
// the same contract for rsp, docs/x64-build-design.md 7), which also states
// the contract and gives the frame constants. Task_Create and Task_ClearPrivate
// switch nothing and are plain cdecl C++, here. No divergence
// (docs/task_sched.md section 3): Task_Create aborts on a slot past 3 where
// the original would write past the records, the precedent battle_flow.md
// section 1 set.
#include "game/task_sched.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/task_sched_callees.h"   // includes game/task_switch.h
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/stackwatch.h"

namespace task_sched {
}  // namespace task_sched

// ===========================================================================

void TaskSched_Inject() {
    if (bof3::WantsShadow("task_sched")) task_sched::SelfTest();
}
