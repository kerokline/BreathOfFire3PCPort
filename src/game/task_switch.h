// The task switch, by architecture (docs/x64-build-design.md 7.5; the owner's
// call 2 of docs/x64-build-round.md 7: x86-64 Windows alone, in a shape a
// third variant joins without a redesign).
//
// task_sched.cpp keeps the C++ (Task_Create, Task_ClearPrivate, the inject
// list) and the record and word addresses; the six entries that switch or
// read the stack pointer - Task_RunAll, Task_BackToScheduler, Task_SetStackBase,
// Task_Sleep, Task_Restart, Task_Exit - and the three frame constants come
// from one header per architecture, selected here by the compiler's target
// macro and refused otherwise. The i686 header is Capcom's instructions, kept
// byte for byte (tools/codegen_same.py); the x86_64 header is the same
// contract for rsp and the Win64 callee-saved set, measured by
// tools/probes/taskstack_probe.cpp.
//
// The contract every variant keeps, stated once:
//
//   1. A task's saved stack pointer is a 32-bit word at its record's +4
//      (Task_Records 0x66C7D0, 0x20 bytes a record, +0 state, +2 sleep); the
//      scheduler's is the 32-bit word Task_SchedulerEsp 0x66C854. The stacks
//      lie on the main thread's stack below Task_StackTop 0x66C858, which the
//      narrow design keeps below 4 GB (x64-build-design.md 7.1); a variant
//      whose stack pointer can lie higher checks it in Task_SetStackBase and
//      aborts loudly.
//   2. Entering a task is: load its saved sp, pop the architecture's
//      callee-saved set, return into it. Nothing else is restored: eax (rax)
//      is left as the previous yield left it, as the original's.
//   3. A yield pushes that set under its own return address, stores its sp in
//      the record and jumps to the landing Task_BackToScheduler with the
//      record offset in the walk's register (edx), where the scheduler's sp
//      is reloaded and the walk goes on at the next record.
//   4. Task_Create (C++) and Task_Restart build a first frame of kFirstFrame
//      bytes below the slice's top with the entry at kEntrySlot above the
//      first sp - the set the scheduler pops, then the entry by `ret` - with
//      the alignment and the home space the ABI wants above it.
//   5. The x87 and SIMD control state is not saved across a switch: the ABI
//      treats it as callee-preserved and no block of ours yields with it
//      changed (x64-build-design.md 7.3).
//
// Each header defines, in namespace task_sched, kStackSize (a slice),
// kFirstFrame and kEntrySlot, and - when the includer defines
// BOF3_TASK_SWITCH_FUNCTIONS first, which task_sched.cpp alone does - the six
// entries. Other includers (task_sched_callees.h for the fuzz, the stackwatch)
// get the constants alone.
//
// This is the one place a game file holds an #if by width (x64-build-round.md
// 2.2's rule, with this header as its named exception): the switch is the
// exception by construction. An ARM64 variant is a third header - x19..x28,
// x29 (fp), x30 (lr) and d8..d15 pushed on sp, `ret` through x30 in place of
// the ret that pops - the same six entries, the same words.
#pragma once

#if defined(__i386__)
#include "game/task_switch_i686.h"
#elif defined(__x86_64__)
#include "game/task_switch_x86_64.h"
#else
#error "task_switch: no switch for this architecture (an ARM64 variant: x19..x28, fp, lr, d8..d15 on sp; docs/x64-build-design.md 7.5)"
#endif
