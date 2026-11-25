// Moved here by tools/move_funcs.py from task_sched.cpp (docs/source-layout-round.md).
#define BOF3_TASK_SWITCH_FUNCTIONS   // this file defines the six switch entries (game/task_switch.h)
#include "game/core/task.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/task_sched_callees.h"   // includes game/task_switch.h
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/stackwatch.h"
#include "hook/task_yield.h"

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

// original 0x5A9914: task `slot` runs `entry` from the next walk that reaches
// it: state 2 (the sleep word untouched), its saved esp kFirstFrame (0x1C)
// below the top of its stack (Task_StackTop - slot * kStackSize (0x4000),
// 32-bit arithmetic), and the entry written above the registers the
// scheduler pops - which are whatever the stack holds. Whatever task ran in
// the slot is dropped where it stood. The constants are the architecture's
// (task_switch.h), and the entry is stored at a return address's width.
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
    const auto e = reinterpret_cast<std::uintptr_t>(entry);   // what the ret pops: a pointer's width
    std::memcpy(At(sp + kEntrySlot), &e, sizeof e);
    // Ours, not the game's (docs/state-by-name-sites.md 2): the task dropped
    // where it stood leaves no yield word behind. A static of ours outside the
    // span; nothing of the game reads it.
    bof3::task_yield::g_entries[k] = bof3::task_yield::Entry{};
}

// --- the yield word (docs/state-by-name-round.md 3.2; hook/task_yield.h) ------
//
// Not a function of Capcom's: a sleep a resume entry can stand for, written by
// the task bodies of ours in place of their callee's Task_Sleep(frames). The
// game sees the same sleep through the same callee; the entry is a static of
// ours. Task_Restart and Task_Exit (game/task_switch_*.h, untouched) need no
// clear of their own: they act on the running task, and a running task's
// entry is always clear - Task_YieldAt clears it as its sleep returns, a
// resume entry clears it before anything else, and Task_Create clears the
// slot it makes (docs/state-by-name-sites.md 2 has the argument).

namespace {

bof3::task_yield::Entry* RunningEntry() {
    const auto off = static_cast<std::uint32_t>(Task_CurrentOffset);
    if (off % task_sched::kRecordSize != 0 || off / task_sched::kRecordSize >= task_sched::kTasks) return nullptr;
    return &bof3::task_yield::g_entries[off / task_sched::kRecordSize];
}

}  // namespace

void Task_YieldAt(std::uint32_t site, int frames, const void* locals, std::uint32_t bytes, bof3::task_yield::SleepFn sleep) {
    using namespace bof3::task_yield;
    const Site* s = FindSite(site);
    if (!s || s->bytes != bytes)
        bof3::Fatal("Task_YieldAt: site %u with %u bytes of locals is not a row of task_yield.h's kSites", site, bytes);
    Entry* e = RunningEntry();
    if (e) {
        *e = Entry{};
        e->site = site;
        e->bytes = bytes;
        if (bytes) std::memcpy(e->locals, locals, bytes);
    }
    sleep(frames);
    if (e) *e = Entry{};
}

void Task_YieldResume(std::uint32_t site, void* locals, std::uint32_t bytes) {
    using namespace bof3::task_yield;
    Entry* e = RunningEntry();
    if (!e) bof3::Fatal("Task_YieldResume: site %u's resume entry runs with Task_CurrentOffset 0x%X, no task record's",
                        site, static_cast<unsigned>(Task_CurrentOffset));
    if (e->site != site || e->bytes != bytes)
        bof3::Fatal("Task_YieldResume: task %u entered at site %u's resume entry holds site %u with %u bytes (%u wanted)",
                    static_cast<unsigned>(e - g_entries), site, e->site, e->bytes, bytes);
    if (bytes) std::memcpy(locals, e->locals, bytes);
    *e = Entry{};
}

// original 0x5A99F4: the current task's private words +8..+0x17 zeroed (four
// dwords; +0x18..+0x1F, task 0's Game_Mode and Game_Step, are kept).
extern "C" void __cdecl Task_ClearPrivate(void) {
    using namespace task_sched;
    unsigned char* record = At(::bof3::Img(bof3::addr::Task_Records) + static_cast<std::uint32_t>(Task_CurrentOffset));
    for (std::uint32_t at = kPrivate; at < kPrivate + 0x10; at += 4) SetLong(record + at, 0);
}

void Task_Inject() {
    BOF3_INJECT(Task_RunAll);
    BOF3_INJECT(Task_BackToScheduler);
    BOF3_INJECT(Task_SetStackBase);
    BOF3_INJECT(Task_Create);
    BOF3_INJECT(Task_Sleep);
    BOF3_INJECT(Task_Restart);
    BOF3_INJECT(Task_Exit);
    BOF3_INJECT(Task_ClearPrivate);
}
