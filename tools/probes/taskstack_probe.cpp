// A 64-bit probe of the task switch's x86-64 shape, for docs/x64-build-design.md
// (the 64-bit build round, group XA2; stage B1 writes the real one in
// src/game/task_sched.cpp). The 32-bit switch is Capcom's instruction for
// instruction: a task's saved esp in a 32-bit word of its record, the
// scheduler's esp in Task_SchedulerEsp, six pops and a `ret` into the task.
// This probe runs the same contract with rsp and the x86-64 callee-saved set
// (rbx rbp rsi rdi r12..r15, xmm6..xmm15) on a task arena below 4 GB, the
// saved stack pointers 32-bit words still, and measures:
//
//   1. that a task's locals in callee-saved registers survive a yield (a
//      double and an integer loop spanning yields against a straight run);
//   2. the entry frame's shape: the entry address above the 224-byte register
//      set, the 32 bytes of home space Win64 gives a callee above its return
//      address, and rsp == 8 (mod 16) at the entry (kFirstFrame, kEntrySlot);
//   3. whether a vectored exception handler is reached for a fault taken
//      while rsp lies on the arena, outside the TIB's StackBase..StackLimit
//      (the crash reporter is a vectored handler, src/hook/crash.cpp), and
//      what the TIB says there;
//   4. where VirtualAlloc(nullptr) puts the arena in an executable with and
//      without DYNAMIC_BASE (the two builds below).
//
//   x86_64-w64-mingw32-clang++ -O2 -o taskstack_fixed.exe tools/probes/taskstack_probe.cpp \
//       -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase -Wl,--disable-reloc-section
//   x86_64-w64-mingw32-clang++ -O2 -o taskstack_aslr.exe tools/probes/taskstack_probe.cpp
//   ./taskstack_fixed.exe ; ./taskstack_aslr.exe
//
// Measured 2026-11-01: the results are in docs/x64-build-design.md 7.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// The record shape of Task_Records (0x66C7D0): +0 state (u16), +2 sleep
// (u16), +4 saved stack pointer (u32), 0x20 bytes a record, four records.
constexpr unsigned kTasks = 4, kRecordSize = 0x20, kSlice = 0x4000;
// x86-64: 8 GPRs + 10 xmm = 64 + 160 = 224 bytes of saved set under the
// return address; the entry at sp + 224; 32 bytes of home space and the
// 8 (mod 16) rule above it: the first frame is 0x110 below the slice's top.
constexpr std::uint32_t kSetBytes = 224, kEntrySlot = kSetBytes, kFirstFrame = kSetBytes + 48;
static_assert(kFirstFrame % 16 == 0, "the first sp is 16-aligned, so rsp == 8 (mod 16) after the ret");

struct alignas(16) Records {
    std::uint8_t r[kTasks * kRecordSize];
};
Records g_records;
std::uint32_t g_scheduler_sp;   // Task_SchedulerEsp's shape: a 32-bit word
std::uint32_t g_current;        // Task_CurrentOffset
std::uint32_t g_stack_top;      // Task_StackTop

std::uint16_t& State(unsigned k) { return *reinterpret_cast<std::uint16_t*>(g_records.r + k * kRecordSize); }
std::uint32_t& SavedSp(unsigned k) { return *reinterpret_cast<std::uint32_t*>(g_records.r + k * kRecordSize + 4); }

}  // namespace

// --- the switch, x86-64 -----------------------------------------------------
//
// The walk as the original's: records by offset edx = 0, 0x20, 0x40, 0x60;
// state 0 skipped, state 1 sleeping (not modelled here: no sleep word),
// else entered. rbx holds the records' base (in the real switch, the image's
// base: Bof3_ImageMove's 64-bit successor, loaded RIP-relative).

extern "C" __attribute__((naked)) void Task_RunAll64(void) {
    asm("pushq %%rbx\n\t"
        "pushq %%rbp\n\t"
        "pushq %%rsi\n\t"
        "pushq %%rdi\n\t"
        "pushq %%r12\n\t"
        "pushq %%r13\n\t"
        "pushq %%r14\n\t"
        "pushq %%r15\n\t"
        "subq $160, %%rsp\n\t"
        "movups %%xmm6, 0(%%rsp)\n\t"
        "movups %%xmm7, 16(%%rsp)\n\t"
        "movups %%xmm8, 32(%%rsp)\n\t"
        "movups %%xmm9, 48(%%rsp)\n\t"
        "movups %%xmm10, 64(%%rsp)\n\t"
        "movups %%xmm11, 80(%%rsp)\n\t"
        "movups %%xmm12, 96(%%rsp)\n\t"
        "movups %%xmm13, 112(%%rsp)\n\t"
        "movups %%xmm14, 128(%%rsp)\n\t"
        "movups %%xmm15, 144(%%rsp)\n\t"
        "movl %%esp, %c[sched](%%rip)\n\t"      // the scheduler's sp, a 32-bit word (the stack is below 4 GB)
        "leaq %c[recs](%%rip), %%rbx\n\t"
        "xorl %%edx, %%edx\n"
        ".Lwalk:\n\t"
        "movl %%edx, %c[cur](%%rip)\n\t"
        "cmpw $0, (%%rbx,%%rdx)\n\t"
        "je .Lnext\n\t"
        "movl 4(%%rbx,%%rdx), %%esp\n\t"        // the task's saved sp: zero-extended into rsp
        "movups 0(%%rsp), %%xmm6\n\t"
        "movups 16(%%rsp), %%xmm7\n\t"
        "movups 32(%%rsp), %%xmm8\n\t"
        "movups 48(%%rsp), %%xmm9\n\t"
        "movups 64(%%rsp), %%xmm10\n\t"
        "movups 80(%%rsp), %%xmm11\n\t"
        "movups 96(%%rsp), %%xmm12\n\t"
        "movups 112(%%rsp), %%xmm13\n\t"
        "movups 128(%%rsp), %%xmm14\n\t"
        "movups 144(%%rsp), %%xmm15\n\t"
        "addq $160, %%rsp\n\t"
        "popq %%r15\n\t"
        "popq %%r14\n\t"
        "popq %%r13\n\t"
        "popq %%r12\n\t"
        "popq %%rdi\n\t"
        "popq %%rsi\n\t"
        "popq %%rbp\n\t"
        "popq %%rbx\n\t"
        "ret\n"
        ".Lnext:\n\t"
        "addl $0x20, %%edx\n\t"
        "cmpl $0x80, %%edx\n\t"
        "jb .Lwalk\n\t"
        "movups 0(%%rsp), %%xmm6\n\t"
        "movups 16(%%rsp), %%xmm7\n\t"
        "movups 32(%%rsp), %%xmm8\n\t"
        "movups 48(%%rsp), %%xmm9\n\t"
        "movups 64(%%rsp), %%xmm10\n\t"
        "movups 80(%%rsp), %%xmm11\n\t"
        "movups 96(%%rsp), %%xmm12\n\t"
        "movups 112(%%rsp), %%xmm13\n\t"
        "movups 128(%%rsp), %%xmm14\n\t"
        "movups 144(%%rsp), %%xmm15\n\t"
        "addq $160, %%rsp\n\t"
        "popq %%r15\n\t"
        "popq %%r14\n\t"
        "popq %%r13\n\t"
        "popq %%r12\n\t"
        "popq %%rdi\n\t"
        "popq %%rsi\n\t"
        "popq %%rbp\n\t"
        "popq %%rbx\n\t"
        "ret"
        :
        : [sched] "i"(&g_scheduler_sp), [recs] "i"(&g_records), [cur] "i"(&g_current));
}

// The landing: the scheduler's sp back, the walk goes on at the next record.
extern "C" __attribute__((naked)) void Task_BackToScheduler64(void) {
    asm("movl %c[sched](%%rip), %%esp\n\t"
        "leaq %c[recs](%%rip), %%rbx\n\t"
        "movl %c[cur](%%rip), %%edx\n\t"
        "jmp .Lnext"
        :
        : [sched] "i"(&g_scheduler_sp), [recs] "i"(&g_records), [cur] "i"(&g_current));
}

// A yield: the set under the return address, sp stored, the landing.
extern "C" __attribute__((naked)) void Task_Yield64(void) {
    asm("pushq %%rbx\n\t"
        "pushq %%rbp\n\t"
        "pushq %%rsi\n\t"
        "pushq %%rdi\n\t"
        "pushq %%r12\n\t"
        "pushq %%r13\n\t"
        "pushq %%r14\n\t"
        "pushq %%r15\n\t"
        "subq $160, %%rsp\n\t"
        "movups %%xmm6, 0(%%rsp)\n\t"
        "movups %%xmm7, 16(%%rsp)\n\t"
        "movups %%xmm8, 32(%%rsp)\n\t"
        "movups %%xmm9, 48(%%rsp)\n\t"
        "movups %%xmm10, 64(%%rsp)\n\t"
        "movups %%xmm11, 80(%%rsp)\n\t"
        "movups %%xmm12, 96(%%rsp)\n\t"
        "movups %%xmm13, 112(%%rsp)\n\t"
        "movups %%xmm14, 128(%%rsp)\n\t"
        "movups %%xmm15, 144(%%rsp)\n\t"
        "leaq %c[recs](%%rip), %%rcx\n\t"
        "movl %c[cur](%%rip), %%edx\n\t"
        "movl %%esp, 4(%%rcx,%%rdx)\n\t"
        "jmp Task_BackToScheduler64"
        :
        : [recs] "i"(&g_records), [cur] "i"(&g_current));
}

// The task ends: state 0, the landing.
extern "C" __attribute__((naked)) void Task_Exit64(void) {
    asm("leaq %c[recs](%%rip), %%rcx\n\t"
        "movl %c[cur](%%rip), %%edx\n\t"
        "movw $0, (%%rcx,%%rdx)\n\t"
        "jmp Task_BackToScheduler64"
        :
        : [recs] "i"(&g_records), [cur] "i"(&g_current));
}

namespace {

// Task_Create's shape: state 2, the first sp kFirstFrame below the slot's
// top, the entry above the set.
void Create(unsigned slot, void (*entry)()) {
    State(slot) = 2;
    const std::uint32_t sp = g_stack_top - slot * kSlice - kFirstFrame;
    SavedSp(slot) = sp;
    std::memset(reinterpret_cast<void*>(static_cast<std::uintptr_t>(sp)), 0, kFirstFrame);
    const std::uint64_t e = reinterpret_cast<std::uintptr_t>(entry);
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(sp + kEntrySlot)), &e, 8);
}

// --- 1. registers across a yield --------------------------------------------

volatile double g_sink_d;
volatile long long g_sink_i;
double g_task_d;
long long g_task_i;
unsigned g_yields;
std::uintptr_t g_entry_rsp;

__attribute__((noinline)) double Work(unsigned n, bool yield) {
    // Several live values across the calls: the compiler keeps them in
    // callee-saved registers (xmm6..xmm15 for the doubles).
    double a = 1.0, b = 0.5, c = 0.25, d = 0.125, e = 1.5, f = 2.5;
    long long i1 = 7, i2 = 11, i3 = 13, i4 = 17;
    for (unsigned k = 1; k <= n; ++k) {
        a = a * 1.0001 + b;
        b = b * 0.999 + c;
        c = c + d * 0.5;
        d = d * 1.01 + e * 0.001;
        e = e - f * 0.0001;
        f = f + a * 0.00001;
        i1 = i1 * 3 + i2;
        i2 = i2 * 5 + i3;
        i3 = i3 * 7 + i4;
        i4 = i4 * 11 + static_cast<long long>(k);
        if (yield) {
            ++g_yields;
            Task_Yield64();
        }
        g_sink_d = a;   // keep the compiler honest: the values must exist after the call
        g_sink_i = i1;
    }
    g_task_i = i1 + i2 + i3 + i4;
    return a + b + c + d + e + f;
}

void TaskA() {
    std::uintptr_t rsp;
    asm("leaq 8(%%rsp), %0" : "=r"(rsp));   // the entry's rsp, before this function's own prologue
    g_entry_rsp = rsp;
    g_task_d = Work(50, true);
    Task_Exit64();
}

// The other task scribbles every callee-saved register between A's yields.
void TaskB() {
    for (unsigned k = 0; k < 60; ++k) {
        asm volatile(
            "movq $-1, %%rbx\n\t"
            "movq $-2, %%rsi\n\t"
            "movq $-3, %%rdi\n\t"
            "movq $-4, %%r12\n\t"
            "movq $-5, %%r13\n\t"
            "movq $-6, %%r14\n\t"
            "movq $-7, %%r15\n\t"
            "pcmpeqd %%xmm6, %%xmm6\n\t"
            "movdqa %%xmm6, %%xmm7\n\t"
            "movdqa %%xmm6, %%xmm8\n\t"
            "movdqa %%xmm6, %%xmm9\n\t"
            "movdqa %%xmm6, %%xmm10\n\t"
            "movdqa %%xmm6, %%xmm11\n\t"
            "movdqa %%xmm6, %%xmm12\n\t"
            "movdqa %%xmm6, %%xmm13\n\t"
            "movdqa %%xmm6, %%xmm14\n\t"
            "movdqa %%xmm6, %%xmm15\n\t"
            :
            :
            : "rbx", "rsi", "rdi", "r12", "r13", "r14", "r15", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12",
              "xmm13", "xmm14", "xmm15", "memory");
        Task_Yield64();
    }
    Task_Exit64();
}

// --- 3. a fault on the arena --------------------------------------------------

std::uintptr_t g_recover;
LONG g_veh_hits;
std::uintptr_t g_veh_rsp, g_veh_base, g_veh_limit;

LONG WINAPI OnException(EXCEPTION_POINTERS* p) {
    if (p->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
    ++g_veh_hits;
    g_veh_rsp = p->ContextRecord->Rsp;
    g_veh_base = __readgsqword(0x08);
    g_veh_limit = __readgsqword(0x10);
    p->ContextRecord->Rip = g_recover;
    return EXCEPTION_CONTINUE_EXECUTION;
}

void TaskFault() {
    asm volatile(
        "leaq 1f(%%rip), %%rax\n\t"
        "movq %%rax, %0\n\t"
        "movl 0, %%eax\n"   // reads address 0: the fault
        "1:\n\t"
        : "=m"(g_recover)
        :
        : "rax", "memory");
    Task_Exit64();
}

std::uintptr_t g_fiber_local;
void WINAPI FiberBody(void* caller) {
    int local = 0;
    g_fiber_local = reinterpret_cast<std::uintptr_t>(&local);
    SwitchToFiber(caller);
}

}  // namespace

int main() {
    const void* module = GetModuleHandleW(nullptr);
    std::printf("module %p, TIB StackBase 0x%llX StackLimit 0x%llX\n", module, (unsigned long long)__readgsqword(0x08),
                (unsigned long long)__readgsqword(0x10));

    // 4. the arena where VirtualAlloc(nullptr) puts it, and other blocks of ours
    void* arena = VirtualAlloc(nullptr, kTasks * kSlice, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    void* block = VirtualAlloc(nullptr, 256 * 1024, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);   // mp3.cpp's decoder stack
    void* fiber = ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
    void* own = CreateFiberEx(256 * 1024, 1024 * 1024, FIBER_FLAG_FLOAT_SWITCH, &FiberBody, fiber);
    if (own) SwitchToFiber(own);
    std::printf("VirtualAlloc(nullptr): the arena at %p, a 256 KB block at %p; a 1 MB fiber's stack at 0x%llX; "
                "malloc(1 MB) %p, malloc(17 MB) %p\n",
                arena, block, (unsigned long long)g_fiber_local, std::malloc(1 << 20), std::malloc(17 << 20));
    if (!arena || reinterpret_cast<std::uintptr_t>(arena) > 0xFFFFFFFFull - kTasks * kSlice) {
        std::printf("the arena is not below 4 GB: the switch's 32-bit words cannot hold it; stopping\n");
        return 1;
    }
    g_stack_top = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(arena) + kTasks * kSlice);

    // 1 and 2: two tasks, A working across yields, B scribbling the set
    std::memset(&g_records, 0, sizeof g_records);
    Create(0, &TaskA);
    Create(1, &TaskB);
    unsigned walks = 0;
    while (State(0) || State(1)) {
        Task_RunAll64();
        ++walks;
    }
    g_yields = 0;
    const double straight_d = Work(50, false);
    const long long straight_i = g_task_i;
    std::printf("the switch: %u walks, task A yielded 50 times; doubles %s (%.17g vs %.17g), integers %s\n", walks,
                g_task_d == straight_d ? "preserved" : "LOST", g_task_d, straight_d, g_task_i == straight_i ? "preserved" : "LOST");
    std::printf("the entry frame: rsp at the entry 0x%llX = slice top - 0x%llX, rsp mod 16 = %llu (8 wanted), the "
                "home space above it inside the slice: %s\n",
                (unsigned long long)g_entry_rsp, (unsigned long long)(g_stack_top - g_entry_rsp), (unsigned long long)(g_entry_rsp % 16),
                g_entry_rsp + 8 + 32 <= g_stack_top ? "yes" : "NO");

    // 3: a fault on the arena, the vectored handler
    AddVectoredExceptionHandler(1, &OnException);
    std::memset(&g_records, 0, sizeof g_records);
    Create(2, &TaskFault);
    while (State(2)) Task_RunAll64();
    std::printf("a fault on the arena: vectored handler reached %ld time(s); rsp there 0x%llX (%s the TIB's "
                "StackLimit..StackBase 0x%llX..0x%llX), continued and the task exited\n",
                g_veh_hits, (unsigned long long)g_veh_rsp,
                g_veh_rsp >= g_veh_limit && g_veh_rsp < g_veh_base ? "inside" : "outside", (unsigned long long)g_veh_limit,
                (unsigned long long)g_veh_base);
    return 0;
}
