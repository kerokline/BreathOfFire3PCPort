// The task scheduler: the game's four cooperative tasks and the esp switch
// between them. docs/task_sched.md.
//
// Capcom wrote this unit by hand (0x5A98A0..0x5A9A21): it pushes a fixed
// register set, stores esp in a record and loads another, and a task is
// resumed by the `ret` that ends six pops. What a task sees across a switch
// is therefore a register contract, not a C signature:
//
//   - Task_RunAll saves ebx ecx esi edi ebp and its esp (Task_SchedulerEsp),
//     and enters a task with ebp edi esi edx ecx ebx popped from the task's
//     stack and then its `ret`. eax is never written: a task entered sees
//     the eax the previous yield left, and so does Task_RunAll's caller.
//   - A task comes back by a jmp to the landing Task_BackToScheduler with
//     edx = its record's offset; the landing reloads the scheduler's esp and
//     the walk goes on at the next record.
//   - Task_Sleep saves ebx ecx edx esi edi ebp in the pop order under its own
//     return address, so the task resumes in its caller with those six as
//     they were; eax then holds whatever the scheduler holds.
//
// So the five that switch are naked functions whose asm is the original's,
// instruction for instruction, with the same registers left in the same
// states - including eax and ebx, which nothing is known to read, because
// any compiled task body may. Task_Create and Task_ClearPrivate switch
// nothing and are plain cdecl C++. No divergence (docs/task_sched.md section
// 3): Task_Create aborts on a slot past 3 where the original would write
// past the records, the precedent battle_flow.md section 1 set.
#include "game/task_sched.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/task_sched_callees.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/stackwatch.h"

namespace task_sched {
namespace {

unsigned char* At(std::uint32_t address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
void SetWord(unsigned char* at, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(at, &w, sizeof w);
}
void SetLong(unsigned char* at, std::uint32_t v) { std::memcpy(at, &v, sizeof v); }

}  // namespace
}  // namespace task_sched

// The addresses in the asm below are the symbols.toml data, as VAs:
//   0x66C7D0 Task_Records (+0 state, +2 sleep, +4 saved esp),
//   0x66C850 Task_CurrentOffset, 0x66C854 Task_SchedulerEsp,
//   0x66C858 Task_StackTop.
// Each is a displacement from a register holding the image's move,
// Bof3_ImageMove (src/hook/host.cpp: ImageBase() - 0x400000, 0 at the
// player's base), since the image may lie elsewhere than 0x400000
// (docs/relocatable-image-design.md 1.4). The register is one the original
// leaves free at that point - pushed already, or dead because the task that
// held it is leaving - so every register a task or the caller can see is left
// as the original leaves it; only the flags of instructions whose flags are
// overwritten before anything tests them differ.

// original 0x5A98A0: once per logic frame from WinMain. The five pushes, the
// scheduler's esp stored, then each record by offset edx = 0, 0x20, 0x40,
// 0x60: the offset stored as the current one; state 0 skipped; state 1 has
// its sleep word decremented (16 bits) and is skipped unless that reached 0,
// when it becomes 2; any other state runs. Running is esp = the saved esp,
// six pops and a ret into the task, which comes back through the landing.
// After the fourth record the pops (ecx where the resume popped edx) and ret.
// As the original has it: the sleep word is decremented before it is tested,
// so a sleep of 0 lasts 65536 frames, and state 0x100 is not free (a word).
extern "C" __attribute__((naked)) void __cdecl Task_RunAll(void) {
    asm("pushl %%ebx\n\t"
        "pushl %%ecx\n\t"
        "pushl %%esi\n\t"
        "pushl %%edi\n\t"
        "pushl %%ebp\n\t"
        "movl _Bof3_ImageMove, %%ebx\n\t"  // ebx: pushed, and popped before anyone sees it
        "movl %%esp, %c0(%%ebx)\n\t"       // Task_SchedulerEsp
        "xorl %%edx, %%edx\n"
        ".Ltask_sched_walk:\n\t"
        "movl %%edx, %c1(%%ebx)\n\t"       // Task_CurrentOffset
        "cmpw $0, %c2(%%ebx,%%edx)\n\t"    // state 0: a free record
        "je .Ltask_sched_next\n\t"
        "cmpw $1, %c2(%%ebx,%%edx)\n\t"    // state 1: sleeping
        "jne .Ltask_sched_enter\n\t"
        "decw %c3(%%ebx,%%edx)\n\t"        // the sleep word, 16 bits
        "jne .Ltask_sched_next\n\t"
        "movw $2, %c2(%%ebx,%%edx)\n"
        ".Ltask_sched_enter:\n\t"
        "movl %c4(%%ebx,%%edx), %%esp\n\t" // the task's saved esp
        "popl %%ebp\n\t"
        "popl %%edi\n\t"
        "popl %%esi\n\t"
        "popl %%edx\n\t"
        "popl %%ecx\n\t"
        "popl %%ebx\n\t"
        "ret\n"                            // into the task
        ".Ltask_sched_next:\n\t"
        "addl $0x20, %%edx\n\t"
        "cmpl $0x80, %%edx\n\t"
        "jb .Ltask_sched_walk\n\t"
        "popl %%ebp\n\t"
        "popl %%edi\n\t"
        "popl %%esi\n\t"
        "popl %%ecx\n\t"
        "popl %%ebx\n\t"
        "ret"
        :
        : "i"(0x66C854),
          "i"(0x66C850),
          "i"(0x66C7D0),
          "i"(0x66C7D2),
          "i"(0x66C7D4));
}

// original 0x5A98F0: not a function but the landing inside Task_RunAll that
// Task_Sleep, Task_Restart and Task_Exit jump to with edx = the task's record
// offset: the scheduler's esp back, and the walk goes on at the next record.
// Its own entry so that BOF3X_ORIGINAL can switch it with the rest. ebx is
// the walk's move again: the leaving task's ebx is dead here (the walk next
// pops a task's ebx or the scheduler's own).
extern "C" __attribute__((naked)) void __cdecl Task_BackToScheduler(void) {
    asm("movl _Bof3_ImageMove, %%ebx\n\t"
        "movl %c0(%%ebx), %%esp\n\t"       // Task_SchedulerEsp
        "jmp .Ltask_sched_next"
        :
        : "i"(0x66C854));
}

// original 0x5A9907: the top of the stack arena, 0x4000 below esp as it
// arrives (the return address on top). eax is left that value.
// BOF3X_STACKWATCH=1 (stackwatch.h) adds a call after the store with every
// register and the flags saved, so the result is the original's either way;
// the test before the first instruction sets flags the subl then overwrites.
extern "C" __attribute__((naked)) void __cdecl Task_SetStackBase(void) {
    asm("cmpb $0, _StackWatch_Armed\n\t"
        "jne .Ltask_sched_setbase_watch\n\t"
        "movl %%esp, %%eax\n\t"
        "subl $0x4000, %%eax\n\t"
        "pushl %%ecx\n\t"                  // kept: the original writes only eax
        "movl _Bof3_ImageMove, %%ecx\n\t"
        "movl %%eax, %c0(%%ecx)\n\t"       // Task_StackTop
        "popl %%ecx\n\t"
        "ret\n"
        ".Ltask_sched_setbase_watch:\n\t"
        "movl %%esp, %%eax\n\t"
        "subl $0x4000, %%eax\n\t"
        "pushl %%ecx\n\t"                  // kept: the original writes only eax
        "movl _Bof3_ImageMove, %%ecx\n\t"
        "movl %%eax, %c0(%%ecx)\n\t"       // Task_StackTop
        "popl %%ecx\n\t"
        "pushfl\n\t"
        "pushal\n\t"
        "leal 36(%%esp), %%ecx\n\t"        // the arrival esp
        "pushl %%ecx\n\t"
        "call _StackWatch_AtSetStackBase\n\t"
        "addl $4, %%esp\n\t"
        "popal\n\t"
        "popfl\n\t"
        "ret"
        :
        : "i"(0x66C858));
}

// original 0x5A9949: the current task sleeps for `frames` (the low 16 bits;
// 1 is "until the next frame"). The caller's edx is kept through eax so that
// the six go on its stack in Task_RunAll's pop order, under the return
// address; the record gets that esp, and the task leaves through the landing.
// eax is left the caller's edx.
extern "C" __attribute__((naked)) void __cdecl Task_Sleep(int) {
    // edx carries the move plus the record offset until the landing, which
    // wants the offset: the six the task keeps are not free before they are
    // pushed, and eax holds the frames.
    asm("movl 4(%%esp), %%eax\n\t"         // frames
        "pushl %%edx\n\t"
        "movl _Bof3_ImageMove, %%edx\n\t"
        "addl %c0(%%edx), %%edx\n\t"       // + Task_CurrentOffset
        "movw $1, %c1(%%edx)\n\t"          // state: sleeping
        "movw %%ax, %c2(%%edx)\n\t"        // the sleep word
        "popl %%eax\n\t"                   // the caller's edx
        "pushl %%ebx\n\t"
        "pushl %%ecx\n\t"
        "pushl %%eax\n\t"
        "pushl %%esi\n\t"
        "pushl %%edi\n\t"
        "pushl %%ebp\n\t"
        "movl %%esp, %c3(%%edx)\n\t"       // the saved esp
        "subl _Bof3_ImageMove, %%edx\n\t"  // the record offset, for the landing
        "jmp _Task_BackToScheduler"
        :
        : "i"(0x66C850),
          "i"(0x66C7D0),
          "i"(0x66C7D2),
          "i"(0x66C7D4));
}

// original 0x5A9976 (PSX 0x8014B848): the current task starts again at `entry` on the next
// frame: state 2 (the sleep word untouched), a fresh stack computed as
// Task_Create does from the record offset (>> 5 is the slot), the entry
// above the six dwords; then the landing - it never returns. ebx is left the
// new saved esp, eax the entry.
extern "C" __attribute__((naked)) void __cdecl Task_Restart(void*) {
    // ecx holds the move: the task that held it never resumes from here.
    asm("movl 4(%%esp), %%eax\n\t"         // entry
        "movl _Bof3_ImageMove, %%ecx\n\t"
        "movl %c0(%%ecx), %%edx\n\t"       // Task_CurrentOffset
        "movw $2, %c1(%%ecx,%%edx)\n\t"    // state: runnable
        "movl %%edx, %%ebx\n\t"
        "shrl $5, %%ebx\n\t"               // the slot
        "imull $0x4000, %%ebx, %%ebx\n\t"
        "subl %c2(%%ecx), %%ebx\n\t"       // Task_StackTop
        "negl %%ebx\n\t"
        "subl $0x1C, %%ebx\n\t"
        "movl %%ebx, %c3(%%ecx,%%edx)\n\t" // the saved esp
        "movl %%eax, 0x18(%%ebx)\n\t"      // the entry, where the ret reaches it
        "jmp _Task_BackToScheduler"
        :
        : "i"(0x66C850),
          "i"(0x66C7D0),
          "i"(0x66C858),
          "i"(0x66C7D4));
}

// original 0x5A99AD: the current task ends: the six dwords +0..+0x17 of its
// record zeroed (state, sleep, saved esp and the private words +8..+0x17;
// +0x18..+0x1F are kept), then the landing - it never returns.
extern "C" __attribute__((naked)) void __cdecl Task_Exit(void) {
    // ecx holds the move: the task that held it has ended.
    asm("movl _Bof3_ImageMove, %%ecx\n\t"
        "movl %c0(%%ecx), %%edx\n\t"       // Task_CurrentOffset
        "movl $0, %c1(%%ecx,%%edx)\n\t"
        "movl $0, %c2(%%ecx,%%edx)\n\t"
        "movl $0, %c3(%%ecx,%%edx)\n\t"
        "movl $0, %c4(%%ecx,%%edx)\n\t"
        "movl $0, %c5(%%ecx,%%edx)\n\t"
        "movl $0, %c6(%%ecx,%%edx)\n\t"
        "jmp _Task_BackToScheduler"
        :
        : "i"(0x66C850),
          "i"(0x66C7D0),
          "i"(0x66C7D4),
          "i"(0x66C7D8),
          "i"(0x66C7DC),
          "i"(0x66C7E0),
          "i"(0x66C7E4));
}

// original 0x5A9914: task `slot` runs `entry` from the next walk that reaches
// it: state 2 (the sleep word untouched), its saved esp 0x1C below the top of
// its stack (Task_StackTop - slot * 0x4000, 32-bit arithmetic), and the entry
// written above the six dwords the scheduler pops - which are whatever the
// stack holds. Whatever task ran in the slot is dropped where it stood.
// The original does not test the slot: one past 3 writes over
// Task_CurrentOffset and the scheduler's esp. Ours aborts there.
extern "C" void __cdecl Task_Create(int slot, void* entry) {
    using namespace task_sched;
    const auto k = static_cast<std::uint32_t>(slot);
    if (k >= kTasks) bof3::Fatal("Task_Create: slot %d, past the four task records", slot);
    unsigned char* record = At(::bof3::Img(bof3::addr::Task_Records) + k * kRecordSize);
    SetWord(record + kState, kRunnable);
    const std::uint32_t sp = static_cast<std::uint32_t>(Task_StackTop) - k * kStackSize - kFirstFrame;
    SetLong(record + kSavedEsp, sp);
    SetLong(At(sp + kEntrySlot), static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(entry)));
}

// original 0x5A99F4: the current task's private words +8..+0x17 zeroed (four
// dwords; +0x18..+0x1F, task 0's Game_Mode and Game_Step, are kept).
extern "C" void __cdecl Task_ClearPrivate(void) {
    using namespace task_sched;
    unsigned char* record = At(::bof3::Img(bof3::addr::Task_Records) + static_cast<std::uint32_t>(Task_CurrentOffset));
    for (std::uint32_t at = kPrivate; at < kPrivate + 0x10; at += 4) SetLong(record + at, 0);
}

// ===========================================================================

void TaskSched_Inject() {
    if (bof3::WantsShadow("task_sched")) task_sched::SelfTest();
    BOF3_INJECT(Task_RunAll);
    BOF3_INJECT(Task_BackToScheduler);
    BOF3_INJECT(Task_SetStackBase);
    BOF3_INJECT(Task_Create);
    BOF3_INJECT(Task_Sleep);
    BOF3_INJECT(Task_Restart);
    BOF3_INJECT(Task_Exit);
    BOF3_INJECT(Task_ClearPrivate);
}
