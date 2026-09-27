// The task scheduler: Task_RunAll 0x5A98A0 and the rest of its hand-written
// unit up to 0x5A9A21 - the landing Task_BackToScheduler 0x5A98F0,
// Task_SetStackBase 0x5A9907, Task_Create 0x5A9914, Task_Sleep 0x5A9949,
// Task_Restart 0x5A9976, Task_Exit 0x5A99AD and Task_ClearPrivate 0x5A99F4.
// Every task body of the game, and every function of ours that runs in one,
// runs on the stacks these switch. docs/task_sched.md.
#pragma once

void TaskSched_Inject();

namespace task_sched {
// BOF3X_SHADOW=task_sched: the differential fuzz, once at start-up
// (task_sched_fuzz.cpp; docs/task_sched.md section 4).
void SelfTest();
}  // namespace task_sched
