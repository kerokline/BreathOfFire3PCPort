// BOF3X_SHADOW=task_sched: a differential fuzz of the task scheduler, once at
// start-up. docs/task_sched.md section 4.
//
// One byte-copy of the whole unit 0x5A98A0..0x5A9A21 (every jump in it stays
// in it, and it calls nothing), its eight entries found by offset. A round
// builds a world the scheduler can run: four task stacks in a scratch arena
// of the fuzz's own, Task_StackTop over them, the four records with seeded
// states and sleep words, and for every record that is not free a saved
// frame of six random registers under the entry of a task body of the
// fuzz's own. Then one to six frames of Task_RunAll with random registers
// in, first through the copy's eight entries and then, from the same world,
// through ours; the event logs, the registers each Task_RunAll returns, the
// records, the scheduler's three words and the whole arena compared.
//
// The task body is the recording stand-in: every entry logs the eight
// registers and esp it arrives with (a fresh task's are the arena's words
// the scheduler popped, so Task_Create's and Task_Restart's arithmetic
// shows in them), then follows the round's plan - Task_Create of another
// slot, Task_ClearPrivate, a poke at a record the walk has still to reach -
// and yields one of four ways: Task_Sleep with six random registers loaded
// (and logged again, with eax, when it resumes), Task_Exit, Task_Restart at
// its own entry, or its state 0 and a bare jmp to the landing. Every call
// into the side under test goes through an asm thunk that reads the target
// from g_side, so no side's address lands on a task stack; the two C++ ones
// of ours leave different dead bytes below esp than the copy, so the thunks
// scrub 0x100 bytes below their esp after the call.
#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/task_sched.h"
#include "game/task_sched_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

// --- what the thunks read, and what they call back into -------------------
// C linkage so the asm can name them.
extern "C" {
// The side under test, in this order (the thunks index it by 4).
struct TaskSchedFuzzSide {
    void* run_all;    // +0
    void* back;       // +4
    void* set_base;   // +8
    void* create;     // +0xC
    void* sleep;      // +0x10
    void* restart;    // +0x14
    void* exit;       // +0x18
    void* clear;      // +0x1C
};
__attribute__((used)) TaskSchedFuzzSide TaskSchedFuzz_Side;
void __cdecl TaskSchedFuzz_Probe(void);
__attribute__((used)) void __cdecl TaskSchedFuzz_Entered(const std::uint32_t* pushad);
// in[0..6] ebx ecx edx esi edi ebp eax loaded, the call made (sleep with
// `arg` pushed), out[0..6] the same registers after, out[7] esp.
void __cdecl TaskSchedFuzz_CallRunAll(const std::uint32_t* in, std::uint32_t* out);
void __cdecl TaskSchedFuzz_CallSleep(int frames, const std::uint32_t* in, std::uint32_t* out);
void __cdecl TaskSchedFuzz_CallCreate(int slot);
void __cdecl TaskSchedFuzz_CallClear(void);
[[noreturn]] void __cdecl TaskSchedFuzz_CallExit(void);
[[noreturn]] void __cdecl TaskSchedFuzz_CallRestart(void);
[[noreturn]] void __cdecl TaskSchedFuzz_CallLand(void);
std::uint32_t __cdecl TaskSchedFuzz_CallSetBase(std::uint32_t depth);
}

// The task body's entry: every register as it arrived, then the body.
extern "C" __attribute__((naked)) void __cdecl TaskSchedFuzz_Probe(void) {
    asm("pushal\n\t"
        "pushl %esp\n\t"
        "call _TaskSchedFuzz_Entered\n\t"
        "ud2");
}

// Saves the caller's ebx esi edi ebp, loads the seven registers, calls,
// stores the seven and esp, restores.
#define TSF_LOAD_REGS                                                       \
    "movl 0(%eax), %ebx\n\t"                                                \
    "movl 4(%eax), %ecx\n\t"                                                \
    "movl 8(%eax), %edx\n\t"                                                \
    "movl 12(%eax), %esi\n\t"                                               \
    "movl 16(%eax), %edi\n\t"                                               \
    "movl 20(%eax), %ebp\n\t"                                               \
    "movl 24(%eax), %eax\n\t"
#define TSF_STORE_REGS(OUT_AT)                                              \
    "pushl %eax\n\t"                                                        \
    "movl " OUT_AT "(%esp), %eax\n\t"                                       \
    "movl %ebx, 0(%eax)\n\t"                                                \
    "movl %ecx, 4(%eax)\n\t"                                                \
    "movl %edx, 8(%eax)\n\t"                                                \
    "movl %esi, 12(%eax)\n\t"                                               \
    "movl %edi, 16(%eax)\n\t"                                               \
    "movl %ebp, 20(%eax)\n\t"                                               \
    "popl %ecx\n\t"                                                         \
    "movl %ecx, 24(%eax)\n\t"                                               \
    "movl %esp, 28(%eax)\n\t"

extern "C" __attribute__((naked)) void __cdecl TaskSchedFuzz_CallRunAll(const std::uint32_t*, std::uint32_t*) {
    // after the four pushes: in at 20, out at 24
    asm("pushl %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "movl 20(%esp), %eax\n\t"
        TSF_LOAD_REGS
        "call *_TaskSchedFuzz_Side+0\n\t"
        TSF_STORE_REGS("28")               // out at 24, +4 for the pushed eax
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "popl %ebp\n\t"
        "ret");
}

extern "C" __attribute__((naked)) void __cdecl TaskSchedFuzz_CallSleep(int, const std::uint32_t*, std::uint32_t*) {
    // after the four pushes: frames at 20, in at 24, out at 28
    asm("pushl %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "movl 24(%esp), %eax\n\t"
        "pushl 20(%esp)\n\t"               // frames; out now at 32
        TSF_LOAD_REGS
        "call *_TaskSchedFuzz_Side+0x10\n\t"
        TSF_STORE_REGS("36")               // out at 32, +4 for the pushed eax
        "addl $4, %esp\n\t"
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "popl %ebp\n\t"
        "ret");
}

// Zeroes the 0x100 bytes below esp: the dead bytes a C++ callee left there.
// Only ecx is used, and nothing is pushed: a pushed register of a callee's
// would itself be a dead byte that differs.
#define TSF_SCRUB                                                           \
    "leal -0x100(%esp), %ecx\n"                                             \
    "1:\n\t"                                                                \
    "movl $0, (%ecx)\n\t"                                                   \
    "addl $4, %ecx\n\t"                                                     \
    "cmpl %esp, %ecx\n\t"                                                   \
    "jb 1b\n\t"

extern "C" __attribute__((naked)) void __cdecl TaskSchedFuzz_CallCreate(int) {
    asm("pushl $_TaskSchedFuzz_Probe\n\t"
        "pushl 8(%esp)\n\t"                // the slot
        "call *_TaskSchedFuzz_Side+0xC\n\t"
        "addl $8, %esp\n\t"
        TSF_SCRUB
        "ret");
}

extern "C" __attribute__((naked)) void __cdecl TaskSchedFuzz_CallClear(void) {
    asm("call *_TaskSchedFuzz_Side+0x1C\n\t"
        TSF_SCRUB
        "ret");
}

extern "C" __attribute__((naked)) void __cdecl TaskSchedFuzz_CallExit(void) {
    asm("call *_TaskSchedFuzz_Side+0x18\n\t"
        "ud2");
}

extern "C" __attribute__((naked)) void __cdecl TaskSchedFuzz_CallRestart(void) {
    asm("pushl $_TaskSchedFuzz_Probe\n\t"
        "call *_TaskSchedFuzz_Side+0x14\n\t"
        "ud2");
}

// The landing as the three yields reach it: edx the current record offset.
extern "C" __attribute__((naked)) void __cdecl TaskSchedFuzz_CallLand(void) {
    asm("movl 0x66C850, %edx\n\t"          // Task_CurrentOffset
        "jmp *_TaskSchedFuzz_Side+4");
}

// Task_SetStackBase called `depth` bytes below this frame; answers its eax.
extern "C" __attribute__((naked)) std::uint32_t __cdecl TaskSchedFuzz_CallSetBase(std::uint32_t) {
    asm("pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "subl 8(%ebp), %esp\n\t"
        "call *_TaskSchedFuzz_Side+8\n\t"
        "movl %ebp, %esp\n\t"
        "popl %ebp\n\t"
        "ret");
}

namespace task_sched {
namespace {

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* At(std::uint32_t address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
std::uint16_t Word(std::uint32_t address) {
    std::uint16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void SetWord(std::uint32_t address, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}
void SetLong(std::uint32_t address, std::uint32_t v) { std::memcpy(At(address), &v, sizeof v); }
// The running task's own state and sleep words, as a body that read its
// record would see them.
std::uint32_t OwnStateAndSleep() {
    std::uint32_t v;
    std::memcpy(&v, At(bof3::addr::Task_Records + static_cast<std::uint32_t>(Task_CurrentOffset)), sizeof v);
    return v;
}

// --- the random sources -----------------------------------------------------

std::uint32_t Step(std::uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
std::uint32_t g_rng = 0x9E3779B9u;   // the world
std::uint32_t g_plan;                // the task bodies' choices: the same stream on both passes
std::uint32_t Next() { return Step(g_rng); }
std::uint32_t Plan() { return Step(g_plan); }

// A state or sleep word with the boundaries every test sits on.
std::uint16_t SeededWord(std::uint32_t r) {
    static const std::uint16_t kSeeds[] = {0, 1, 2, 3, 0xFFFF, 0x100, 0x101, 0xFF, 0x8000};
    return (r & 3) == 0 ? static_cast<std::uint16_t>(r >> 8) : kSeeds[(r >> 2) % (sizeof kSeeds / sizeof kSeeds[0])];
}

// --- the world --------------------------------------------------------------

constexpr std::uint32_t kRecords = bof3::addr::Task_Records;
constexpr std::uint32_t kGlobals = kRecords;              // the records and the scheduler's three words
constexpr std::uint32_t kGlobalBytes = 0x66C85C - kRecords;
constexpr std::uint32_t kArenaBytes = kTasks * kStackSize + 0x1000;

std::uint32_t g_arena;   // VirtualAlloc'd: kTasks stacks and 0x1000 above them

std::uint32_t Record(std::uint32_t k) { return kRecords + k * kRecordSize; }
std::uint32_t Top(std::uint32_t k) { return static_cast<std::uint32_t>(Task_StackTop) - k * kStackSize; }

struct World {
    unsigned char globals[kGlobalBytes];
    unsigned char arena[kArenaBytes];
};
World g_start, g_theirs, g_ours, g_saved;

void Snap(World& w) {
    std::memcpy(w.globals, At(kGlobals), kGlobalBytes);
    std::memcpy(w.arena, At(g_arena), kArenaBytes);
}
void Put(const World& w) {
    std::memcpy(At(kGlobals), w.globals, kGlobalBytes);
    std::memcpy(At(g_arena), w.arena, kArenaBytes);
}

// --- the log ----------------------------------------------------------------

enum : std::uint32_t { kEvEnter = 1, kEvResume, kEvCreate, kEvClear, kEvPoke, kEvSleep, kEvExit, kEvRestart, kEvLand, kEvFrame, kEvSetBase };
struct Event { std::uint32_t what, current, a, b, r[8]; };
constexpr unsigned kLog = 512;
Event g_log[kLog];
unsigned g_log_n;
bool g_overflow;
Event* Log(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0) {
    if (g_log_n == kLog) {
        g_overflow = true;
        return &g_log[kLog - 1];
    }
    Event& e = g_log[g_log_n++];
    std::memset(&e, 0, sizeof e);
    e.what = what;
    e.current = static_cast<std::uint32_t>(Task_CurrentOffset);
    e.a = a;
    e.b = b;
    return &e;
}
Event g_theirs_log[kLog];
unsigned g_theirs_n;

struct Cover {
    unsigned enter, resume, create, clear, poke, sleep, exit, restart, land, frames, setbase;
    // What each record held as a walk began: free, asleep past this frame,
    // asleep and waking now, runnable (state 2, or any other word).
    unsigned walk_free, walk_asleep, walk_wakes, walk_two, walk_other;
} g_cover, g_pass;   // the whole run (the ours passes), and the pass going on

void CountWalk() {
    for (std::uint32_t k = 0; k < kTasks; ++k) {
        const std::uint16_t state = Word(Record(k) + kState);
        if (state == 0) ++g_pass.walk_free;
        else if (state == kSleeping) ++(Word(Record(k) + kSleep) == 1 ? g_pass.walk_wakes : g_pass.walk_asleep);
        else ++(state == kRunnable ? g_pass.walk_two : g_pass.walk_other);
    }
}

// --- the task body ------------------------------------------------------------

unsigned g_steps_left;   // a bound on the body's steps a round, so a broken side ends

[[noreturn]] void Body() {
    for (;;) {
        const std::uint32_t current = static_cast<std::uint32_t>(Task_CurrentOffset);
        const std::uint32_t self = current / kRecordSize;
        // Up to two things before the yield.
        for (std::uint32_t n = Plan() % 3; n; --n) {
            const std::uint32_t r = Plan();
            switch (r % 4) {
            case 0: {   // another slot made fresh (never this one: the body stands on its stack)
                const std::uint32_t k = (self + 1 + (r >> 4) % (kTasks - 1)) % kTasks;
                Log(kEvCreate, k);
                ++g_pass.create;
                TaskSchedFuzz_CallCreate(static_cast<int>(k));
                break;
            }
            case 1:
                Log(kEvClear);
                ++g_pass.clear;
                TaskSchedFuzz_CallClear();
                break;
            default: {   // a record's state or sleep word moved, if it is not free
                const std::uint32_t k = (r >> 4) % kTasks;
                if (Word(Record(k) + kState) == 0 || k == self) break;
                const std::uint16_t v = SeededWord(Plan());
                if (v == 0) break;   // a free record's saved esp is dropped by nothing, but made free here it would be
                const std::uint32_t at = Record(k) + ((r & 0x100) ? kSleep : kState);
                Log(kEvPoke, at, v);
                ++g_pass.poke;
                SetWord(at, v);
                break;
            }
            }
        }
        if (g_steps_left == 0 || --g_steps_left == 0) {
            // Out of steps: this task ends, the quietest way.
            Log(kEvExit, 1);
            ++g_pass.exit;
            TaskSchedFuzz_CallExit();
        }
        const std::uint32_t r = Plan();
        switch (r % 8) {
        case 0:
            Log(kEvExit);
            ++g_pass.exit;
            TaskSchedFuzz_CallExit();
        case 1:
            Log(kEvRestart);
            ++g_pass.restart;
            TaskSchedFuzz_CallRestart();
        case 2:
            // The landing on its own: state 0 first, or the next walk would
            // resume a frame this body has since stood on.
            SetWord(Record(self) + kState, 0);
            Log(kEvLand);
            ++g_pass.land;
            TaskSchedFuzz_CallLand();
        default: {
            static const int kFrames[] = {1, 1, 2, 3, 0, 0x10001, -1, 0x7FFF};
            const int frames = (r >> 4) % 4 ? kFrames[(r >> 6) % 8] : static_cast<int>(Plan());
            std::uint32_t in[7], out[8];
            for (auto& v : in) v = Plan();
            Log(kEvSleep, static_cast<std::uint32_t>(frames));
            ++g_pass.sleep;
            TaskSchedFuzz_CallSleep(frames, in, out);
            Event* e = Log(kEvResume, OwnStateAndSleep());
            std::memcpy(e->r, out, sizeof out);
            ++g_pass.resume;
            break;
        }
        }
    }
}

}  // namespace
}  // namespace task_sched

extern "C" void __cdecl TaskSchedFuzz_Entered(const std::uint32_t* pushad) {
    using namespace task_sched;
    // pushad: edi esi ebp esp ebx edx ecx eax, lowest first.
    Event* e = Log(kEvEnter, OwnStateAndSleep());
    std::memcpy(e->r, pushad, sizeof e->r);
    ++g_pass.enter;
    Body();
}

namespace task_sched {
namespace {

// One world: the scheduler's three words, the arena, the records.
void Build() {
    Task_StackTop = g_arena + kTasks * kStackSize + 0x100 + 4 * (Next() % 0x100);
    for (std::uint32_t at = 0; at < kArenaBytes; at += 4) SetLong(g_arena + at, Next());
    for (std::uint32_t k = 0; k < kTasks; ++k) {
        const std::uint32_t rec = Record(k);
        const std::uint32_t r = Next();
        const std::uint16_t state = (r & 3) == 0 ? 0 : SeededWord(Next());
        SetWord(rec + kState, state);
        SetWord(rec + kSleep, SeededWord(Next()));
        for (std::uint32_t at = kPrivate; at < kRecordSize; at += 4) SetLong(rec + at, Next());
        if (state == 0) {
            SetLong(rec + kSavedEsp, Next());   // never read while the record is free
            continue;
        }
        // A frame to resume: six registers under the body's entry, somewhere
        // in the top 0x400 bytes of the task's stack.
        const std::uint32_t frame = Top(k) - kFirstFrame - 4 * ((r >> 2) % 0x100);
        for (std::uint32_t i = 0; i < 6; ++i) SetLong(frame + 4 * i, Next());
        SetLong(frame + kEntrySlot, Address(reinterpret_cast<const void*>(&TaskSchedFuzz_Probe)));
        SetLong(rec + kSavedEsp, frame);
    }
    Task_CurrentOffset = Next();
    Task_SchedulerEsp = Next();
}

void Pass(const TaskSchedFuzzSide& side, std::uint32_t plan, unsigned frames, const std::uint32_t (*in)[7],
          std::uint32_t depth) {
    TaskSchedFuzz_Side = side;
    g_plan = plan;
    g_log_n = 0;
    g_overflow = false;
    g_steps_left = 40;
    g_pass = {};
    for (unsigned f = 0; f < frames; ++f) {
        std::uint32_t out[8];
        CountWalk();
        TaskSchedFuzz_CallRunAll(in[f], out);
        Event* e = Log(kEvFrame, f);
        std::memcpy(e->r, out, sizeof out);
        ++g_pass.frames;
    }
    const std::uint32_t eax = TaskSchedFuzz_CallSetBase(depth);
    Log(kEvSetBase, eax, static_cast<std::uint32_t>(Task_StackTop));
    ++g_pass.setbase;
}

}  // namespace

void SelfTest() {
    void* copy = bof3::CloneOriginal("task_sched unit", kUnit, kUnitSize);
    const auto base = Address(copy);
    const TaskSchedFuzzSide theirs = {
        At(base + kAtRunAll), At(base + kAtBack),  At(base + kAtSetBase), At(base + kAtCreate),
        At(base + kAtSleep),  At(base + kAtRestart), At(base + kAtExit),  At(base + kAtClear),
    };
    const TaskSchedFuzzSide ours = {
        reinterpret_cast<void*>(&Task_RunAll),  reinterpret_cast<void*>(&Task_BackToScheduler),
        reinterpret_cast<void*>(&Task_SetStackBase), reinterpret_cast<void*>(&Task_Create),
        reinterpret_cast<void*>(&Task_Sleep),   reinterpret_cast<void*>(&Task_Restart),
        reinterpret_cast<void*>(&Task_Exit),    reinterpret_cast<void*>(&Task_ClearPrivate),
    };
    void* arena = VirtualAlloc(nullptr, kArenaBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!arena) bof3::Fatal("task_sched: VirtualAlloc(%u) failed, error %lu", (unsigned)kArenaBytes, GetLastError());
    g_arena = Address(arena);
    std::memcpy(g_saved.globals, At(kGlobals), kGlobalBytes);

    constexpr unsigned kRounds = 20000;
    unsigned bad = 0, events = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        Build();
        const std::uint32_t plan = Next() | 1;
        const unsigned frames = 1 + Next() % 6;
        std::uint32_t in[6][7];
        for (auto& row : in)
            for (auto& v : row) v = Next();
        const std::uint32_t depth = 4 * (Next() % 0x40);
        Snap(g_start);

        Pass(theirs, plan, frames, in, depth);
        Snap(g_theirs);
        std::memcpy(g_theirs_log, g_log, sizeof(Event) * g_log_n);
        g_theirs_n = g_log_n;
        const bool their_overflow = g_overflow;

        Put(g_start);
        Pass(ours, plan, frames, in, depth);
        Snap(g_ours);
        events += g_log_n;
        {
            unsigned* to = &g_cover.enter;
            const unsigned* from = &g_pass.enter;
            for (unsigned i = 0; i < sizeof(Cover) / sizeof(unsigned); ++i) to[i] += from[i];
        }

        if (their_overflow || g_overflow) bof3::Fatal("task_sched: round %u logged more than %u events", round, kLog);
        const bool log_same = g_log_n == g_theirs_n && std::memcmp(g_log, g_theirs_log, sizeof(Event) * g_log_n) == 0;
        const bool globals_same = std::memcmp(g_theirs.globals, g_ours.globals, kGlobalBytes) == 0;
        const bool arena_same = std::memcmp(g_theirs.arena, g_ours.arena, kArenaBytes) == 0;
        if (!log_same || !globals_same || !arena_same) {
            if (++bad <= 8) {
                unsigned first = 0;
                while (first < g_log_n && first < g_theirs_n &&
                       std::memcmp(&g_log[first], &g_theirs_log[first], sizeof(Event)) == 0)
                    ++first;
                unsigned byte = 0;
                while (byte < kGlobalBytes && g_theirs.globals[byte] == g_ours.globals[byte]) ++byte;
                unsigned arena_byte = 0;
                while (arena_byte < kArenaBytes && g_theirs.arena[arena_byte] == g_ours.arena[arena_byte]) ++arena_byte;
                bof3::Log("shadow      task_sched self-test MISMATCH: round %u, events %u / %u, first differing event %u "
                          "(kind %u / %u), first differing global byte +0x%X, arena byte 0x%X",
                          round, g_theirs_n, g_log_n, first, first < g_theirs_n ? g_theirs_log[first].what : 0,
                          first < g_log_n ? g_log[first].what : 0, byte, arena_byte);
            }
        }
    }
    std::memcpy(At(kGlobals), g_saved.globals, kGlobalBytes);
    VirtualFree(arena, 0, MEM_RELEASE);

    bof3::Log("shadow      task_sched self-test: %u rounds, %u events, %u MISMATCHES; the records, the scheduler's "
              "three words, the four stacks and every task entry's registers compared",
              kRounds, events, bad);
    bof3::Log("shadow      task_sched coverage: frames %u, entries %u, resumes %u; sleeps %u, exits %u, restarts %u, "
              "landings %u; creates %u, clears %u, pokes %u; stack bases %u; records at a walk's start: free %u, "
              "asleep %u, waking %u, state 2 %u, other states %u",
              g_cover.frames, g_cover.enter, g_cover.resume, g_cover.sleep, g_cover.exit, g_cover.restart,
              g_cover.land, g_cover.create, g_cover.clear, g_cover.poke, g_cover.setbase, g_cover.walk_free,
              g_cover.walk_asleep, g_cover.walk_wakes, g_cover.walk_two, g_cover.walk_other);
    if (bad) bof3::Fatal("the task scheduler differs from the original in %u self-test rounds", bad);
}

}  // namespace task_sched
