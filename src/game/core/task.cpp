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
