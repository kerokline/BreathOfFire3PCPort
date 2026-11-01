// The task switch for x86-64 Windows: the i686 half's contract
// (game/task_switch.h) for rsp and the Win64 callee-saved set, in the shape
// tools/probes/taskstack_probe.cpp measured (docs/x64-build-design.md 7.2:
// registers survive a yield to the bit, the entry frame gives a task body
// rsp == 8 mod 16 and its home space inside the slice, a fault on a slice
// reaches the vectored handler). Included through game/task_switch.h.
//
// The set a C++ task body expects kept across a call is rbx rbp rsi rdi
// r12..r15 and xmm6..xmm15: 8 x 8 + 10 x 16 = 224 bytes under the return
// address, where the i686 set is six dwords. The frame a yield leaves, from
// the saved sp up: xmm6..xmm15 at +0..+160 (unaligned stores: after eight
// pushes rsp is 8 mod 16), r15 r14 r13 r12 rdi rsi rbp rbx at +160..+224,
// the return address at +224 (kEntrySlot). A first frame is kFirstFrame =
// 0x110 below the slice's top: the set, the entry, then 48 bytes so that the
// `ret` into the entry leaves rsp = top - 0x28, 8 mod 16, with the 32 bytes
// of home space Win64 lets a callee write above its return address inside
// the slice. Task_SetStackBase rounds the top down to 16 so that holds.
//
// The saved stack pointers are 32-bit words still (the record's +4 and
// Task_SchedulerEsp): the main thread's stack lies below 4 GB in a process
// whose executable has no DYNAMIC_BASE (x64-build-design.md 2, 7.1), a
// `movl` store narrows an rsp that is, and a `movl` load zero-extends it
// back. Task_SetStackBase is the one place that checks (design 7.1, the
// round doc's B1 line): an arrival rsp with any of its top 32 bits set is a
// Fatal naming it, never a truncation.
//
// The image's base: Bof3_ImageMove (src/hook/host.cpp) is ImageBase() -
// 0x400000 as a 32-bit word, loaded RIP-relative and sign-extended (movslq:
// a developer's image below 0x400000 gives a negative move, which a
// zero-extension would turn into a place above 4 GB); the VA is the
// displacement, as the i686 half has it. Symbol spellings through
// common/asm_name.h.
//
// The slice: kStackSize is 0x8000, twice the i686 slice, from the frame
// growth XA2 measured (design 7.4: 64-bit frames about 2.18 times the i686
// ones over 722 functions; slice 0's deepest i686 use 0x2B14 of 0x4000). An
// estimate until stage C's BOF3X_STACKWATCH=1 run on the routes sets it; the
// exe's stack reserve (exe_layout.h) must hold four slices and the gap.
//
// Register contract against the i686 half: eax is left as the previous yield
// left it (contract point 2) - rax here; Task_Sleep's "eax = the caller's
// edx" and Task_Restart's "ebx = the new sp, eax = the entry" are the i686
// instructions' by-products, which no C++ caller reads (every caller is ours,
// docs/x64-asm-policy.md 5) and this half does not reproduce.
#pragma once

#include <cstdint>

namespace task_sched {

constexpr std::uint32_t kStackSize = 0x8000;
constexpr std::uint32_t kFirstFrame = 0x110;
constexpr std::uint32_t kEntrySlot = 0xE0;

}  // namespace task_sched

#ifdef BOF3_TASK_SWITCH_FUNCTIONS

#include "common/asm_name.h"
#include "hook/log.h"

// The set, pushed (rsp 8 mod 16 before and after) and popped. The xmm stores
// are unaligned by design: eight pushes leave rsp at 8 mod 16.
#define BOF3_TASK_PUSH_SET                                                                                     \
    "pushq %%rbx\n\t"                                                                                          \
    "pushq %%rbp\n\t"                                                                                          \
    "pushq %%rsi\n\t"                                                                                          \
    "pushq %%rdi\n\t"                                                                                          \
    "pushq %%r12\n\t"                                                                                          \
    "pushq %%r13\n\t"                                                                                          \
    "pushq %%r14\n\t"                                                                                          \
    "pushq %%r15\n\t"                                                                                          \
    "subq $160, %%rsp\n\t"                                                                                     \
    "movups %%xmm6, 0(%%rsp)\n\t"                                                                              \
    "movups %%xmm7, 16(%%rsp)\n\t"                                                                             \
    "movups %%xmm8, 32(%%rsp)\n\t"                                                                             \
    "movups %%xmm9, 48(%%rsp)\n\t"                                                                             \
    "movups %%xmm10, 64(%%rsp)\n\t"                                                                            \
    "movups %%xmm11, 80(%%rsp)\n\t"                                                                            \
    "movups %%xmm12, 96(%%rsp)\n\t"                                                                            \
    "movups %%xmm13, 112(%%rsp)\n\t"                                                                           \
    "movups %%xmm14, 128(%%rsp)\n\t"                                                                           \
    "movups %%xmm15, 144(%%rsp)\n\t"
#define BOF3_TASK_POP_SET                                                                                      \
    "movups 0(%%rsp), %%xmm6\n\t"                                                                              \
    "movups 16(%%rsp), %%xmm7\n\t"                                                                             \
    "movups 32(%%rsp), %%xmm8\n\t"                                                                             \
    "movups 48(%%rsp), %%xmm9\n\t"                                                                             \
    "movups 64(%%rsp), %%xmm10\n\t"                                                                            \
    "movups 80(%%rsp), %%xmm11\n\t"                                                                            \
    "movups 96(%%rsp), %%xmm12\n\t"                                                                            \
    "movups 112(%%rsp), %%xmm13\n\t"                                                                           \
    "movups 128(%%rsp), %%xmm14\n\t"                                                                           \
    "movups 144(%%rsp), %%xmm15\n\t"                                                                           \
    "addq $160, %%rsp\n\t"                                                                                     \
    "popq %%r15\n\t"                                                                                           \
    "popq %%r14\n\t"                                                                                           \
    "popq %%r13\n\t"                                                                                           \
    "popq %%r12\n\t"                                                                                           \
    "popq %%rdi\n\t"                                                                                           \
    "popq %%rsi\n\t"                                                                                           \
    "popq %%rbp\n\t"                                                                                           \
    "popq %%rbx\n\t"

// Task_SetStackBase's check failed: the arrival rsp has a bit above the low
// 32 set, so no word of the game could hold a task's stack pointer.
extern "C" [[noreturn]] void __cdecl TaskSched_StackAbove4G(std::uintptr_t rsp) {
    bof3::Fatal("Task_SetStackBase: the main thread's stack pointer 0x%llX lies at or above 4 GB, where the task records' "
                "32-bit words cannot hold it (docs/x64-build-design.md 7.1: the executable must load with DYNAMIC_BASE "
                "off, or the task stacks need an arena below 4 GB)",
                static_cast<unsigned long long>(rsp));
}

// The walk as the i686 half's (its comment says what each step is): the set
// pushed, the scheduler's sp stored as a 32-bit word, the records by edx = 0,
// 0x20, 0x40, 0x60 from the image's base in rbx; entering a task is the
// 32-bit load into esp (zero-extended into rsp), the set popped, ret.
extern "C" __attribute__((naked)) void __cdecl Task_RunAll(void) {
    asm(BOF3_TASK_PUSH_SET
        "movslq " BOF3_SYM(Bof3_ImageMove) "(%%rip), %%rbx\n\t"
        "movl %%esp, %c0(%%rbx)\n\t"       // Task_SchedulerEsp, 32 bits
        "xorl %%edx, %%edx\n"
        ".Ltask_sched_walk:\n\t"
        "movl %%edx, %c1(%%rbx)\n\t"       // Task_CurrentOffset
        "cmpw $0, %c2(%%rbx,%%rdx)\n\t"    // state 0: a free record
        "je .Ltask_sched_next\n\t"
        "cmpw $1, %c2(%%rbx,%%rdx)\n\t"    // state 1: sleeping
        "jne .Ltask_sched_enter\n\t"
        "decw %c3(%%rbx,%%rdx)\n\t"        // the sleep word, 16 bits
        "jne .Ltask_sched_next\n\t"
        "movw $2, %c2(%%rbx,%%rdx)\n"
        ".Ltask_sched_enter:\n\t"
        "movl %c4(%%rbx,%%rdx), %%esp\n\t" // the task's saved sp, zero-extended into rsp
        BOF3_TASK_POP_SET
        "ret\n"                            // into the task
        ".Ltask_sched_next:\n\t"
        "addl $0x20, %%edx\n\t"
        "cmpl $0x80, %%edx\n\t"
        "jb .Ltask_sched_walk\n\t"
        BOF3_TASK_POP_SET
        "ret"
        :
        : "i"(0x66C854),
          "i"(0x66C850),
          "i"(0x66C7D0),
          "i"(0x66C7D2),
          "i"(0x66C7D4));
}

// The landing, with edx = the leaving task's record offset: the scheduler's
// sp back (zero-extended), the base in rbx again, the walk's next record.
extern "C" __attribute__((naked)) void __cdecl Task_BackToScheduler(void) {
    asm("movslq " BOF3_SYM(Bof3_ImageMove) "(%%rip), %%rbx\n\t"
        "movl %c0(%%rbx), %%esp\n\t"       // Task_SchedulerEsp
        "jmp .Ltask_sched_next"
        :
        : "i"(0x66C854));
}

// Task_StackTop = the arrival rsp (the return address on top) - kStackSize,
// rounded down to 16 so that a first frame's `ret` leaves a task body at
// 8 mod 16; rax is left that value, rcx and rdx kept as the i686 half keeps
// them. The 4 GB check first (design 7.1). BOF3X_STACKWATCH=1 adds the call
// with the volatile registers and the flags saved by name (pushal has no
// 64-bit form); the callee-saved ones the callee keeps.
extern "C" __attribute__((naked)) void __cdecl Task_SetStackBase(void) {
    asm("movq %%rsp, %%rax\n\t"
        "shrq $32, %%rax\n\t"
        "jnz .Ltask_sched_setbase_high\n\t"
        "movq %%rsp, %%rax\n\t"
        "subq $%c1, %%rax\n\t"
        "andq $-16, %%rax\n\t"
        "pushq %%rcx\n\t"
        "movslq " BOF3_SYM(Bof3_ImageMove) "(%%rip), %%rcx\n\t"
        "movl %%eax, %c0(%%rcx)\n\t"       // Task_StackTop
        "popq %%rcx\n\t"
        "cmpb $0, " BOF3_SYM(StackWatch_Armed) "(%%rip)\n\t"
        "jne .Ltask_sched_setbase_watch\n\t"
        "ret\n"
        ".Ltask_sched_setbase_watch:\n\t"
        "pushq %%rax\n\t"
        "pushq %%rcx\n\t"
        "pushq %%rdx\n\t"
        "pushq %%r8\n\t"
        "pushq %%r9\n\t"
        "pushq %%r10\n\t"
        "pushq %%r11\n\t"
        "pushfq\n\t"
        "leaq 64(%%rsp), %%rcx\n\t"        // the arrival rsp (below 4 GB: checked above)
        "subq $40, %%rsp\n\t"              // the home space, and rsp to 0 mod 16 for the call
        "call " BOF3_SYM(StackWatch_AtSetStackBase) "\n\t"
        "addq $40, %%rsp\n\t"
        "popfq\n\t"
        "popq %%r11\n\t"
        "popq %%r10\n\t"
        "popq %%r9\n\t"
        "popq %%r8\n\t"
        "popq %%rdx\n\t"
        "popq %%rcx\n\t"
        "popq %%rax\n\t"
        "ret\n"
        ".Ltask_sched_setbase_high:\n\t"
        "movq %%rsp, %%rcx\n\t"
        "subq $40, %%rsp\n\t"
        "call " BOF3_SYM(TaskSched_StackAbove4G)   // never returns
        :
        : "i"(0x66C858), "i"(task_sched::kStackSize));
}

// The current task sleeps for `frames` (ecx; the low 16 bits): state 1 and
// the sleep word, the set under the return address, the sp stored, the
// landing with edx = the record offset.
extern "C" __attribute__((naked)) void __cdecl Task_Sleep(int) {
    asm(BOF3_TASK_PUSH_SET
        "movslq " BOF3_SYM(Bof3_ImageMove) "(%%rip), %%r8\n\t"
        "movl %c0(%%r8), %%edx\n\t"        // Task_CurrentOffset
        "movw $1, %c1(%%r8,%%rdx)\n\t"     // state: sleeping
        "movw %%cx, %c2(%%r8,%%rdx)\n\t"   // the sleep word
        "movl %%esp, %c3(%%r8,%%rdx)\n\t"  // the saved sp, 32 bits
        "jmp " BOF3_SYM(Task_BackToScheduler)
        :
        : "i"(0x66C850),
          "i"(0x66C7D0),
          "i"(0x66C7D2),
          "i"(0x66C7D4));
}

// The current task starts again at `entry` (rcx) on the next frame: state 2,
// a fresh sp kFirstFrame below the slot's top (32-bit arithmetic, as
// Task_Create's), the entry stored as eight bytes at kEntrySlot above it,
// the landing. The task that held the registers never resumes from here.
extern "C" __attribute__((naked)) void __cdecl Task_Restart(void*) {
    asm("movslq " BOF3_SYM(Bof3_ImageMove) "(%%rip), %%r8\n\t"
        "movl %c0(%%r8), %%edx\n\t"        // Task_CurrentOffset
        "movw $2, %c1(%%r8,%%rdx)\n\t"     // state: runnable
        "movl %%edx, %%ebx\n\t"
        "shrl $5, %%ebx\n\t"               // the slot
        "imull $%c4, %%ebx, %%ebx\n\t"     // kStackSize
        "subl %c2(%%r8), %%ebx\n\t"        // Task_StackTop
        "negl %%ebx\n\t"
        "subl $%c5, %%ebx\n\t"             // kFirstFrame; the 32-bit ops zero rbx's upper half
        "movl %%ebx, %c3(%%r8,%%rdx)\n\t"  // the saved sp
        "movq %%rcx, %c6(%%rbx)\n\t"       // the entry at kEntrySlot, where the ret reaches it
        "jmp " BOF3_SYM(Task_BackToScheduler)
        :
        : "i"(0x66C850),
          "i"(0x66C7D0),
          "i"(0x66C858),
          "i"(0x66C7D4),
          "i"(task_sched::kStackSize),
          "i"(task_sched::kFirstFrame),
          "i"(task_sched::kEntrySlot));
}

// The current task ends: the six dwords +0..+0x17 of its record zeroed
// (+0x18..+0x1F kept), the landing.
extern "C" __attribute__((naked)) void __cdecl Task_Exit(void) {
    asm("movslq " BOF3_SYM(Bof3_ImageMove) "(%%rip), %%rcx\n\t"
        "movl %c0(%%rcx), %%edx\n\t"       // Task_CurrentOffset
        "movl $0, %c1(%%rcx,%%rdx)\n\t"
        "movl $0, %c2(%%rcx,%%rdx)\n\t"
        "movl $0, %c3(%%rcx,%%rdx)\n\t"
        "movl $0, %c4(%%rcx,%%rdx)\n\t"
        "movl $0, %c5(%%rcx,%%rdx)\n\t"
        "movl $0, %c6(%%rcx,%%rdx)\n\t"
        "jmp " BOF3_SYM(Task_BackToScheduler)
        :
        : "i"(0x66C850),
          "i"(0x66C7D0),
          "i"(0x66C7D4),
          "i"(0x66C7D8),
          "i"(0x66C7DC),
          "i"(0x66C7E0),
          "i"(0x66C7E4));
}

#undef BOF3_TASK_PUSH_SET
#undef BOF3_TASK_POP_SET

#endif  // BOF3_TASK_SWITCH_FUNCTIONS
