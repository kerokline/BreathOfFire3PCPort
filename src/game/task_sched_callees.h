// The task scheduler's layout, shared by task_sched.cpp and its fuzz.
//
// The unit 0x5A98A0..0x5A9A21 calls nothing: every transfer that leaves it is
// the esp switch itself - a `ret` into a task, or a task's jmp back to the
// landing. So there is no callee table here, as the other groups have; what
// the two files share is the record layout and where each entry lies inside
// the unit, which the fuzz needs to find the entries in its one copy of it.
// docs/task_sched.md section 1.
#pragma once

#include <cstdint>

namespace task_sched {

// Task_Records 0x66C7D0: four records of 0x20 bytes.
constexpr std::uint32_t kTasks = 4;
constexpr std::uint32_t kRecordSize = 0x20;
constexpr std::uint32_t kState = 0;      // u16: 0 free, 1 sleeping, any other runnable
constexpr std::uint32_t kSleep = 2;      // u16: frames left to sleep, decremented before the test
constexpr std::uint32_t kSavedEsp = 4;   // u32: where Task_RunAll's six pops start
constexpr std::uint32_t kPrivate = 8;    // +8..+0x1F the task's own words
constexpr std::uint16_t kSleeping = 1;
constexpr std::uint16_t kRunnable = 2;

// The stacks: task k owns the kStackSize bytes below Task_StackTop - k *
// kStackSize. A fresh task's saved esp is kFirstFrame below its top, and its
// entry sits kEntrySlot above that: six dwords popped, then the entry by ret.
constexpr std::uint32_t kStackSize = 0x4000;
constexpr std::uint32_t kFirstFrame = 0x1C;
constexpr std::uint32_t kEntrySlot = 0x18;

// The unit, and each entry's offset in it (the fuzz clones it whole: every
// jump inside it stays inside it).
constexpr std::uint32_t kUnit = 0x5A98A0;
constexpr std::uint32_t kUnitSize = 0x182;   // to Task_ClearPrivate's ret at 0x5A9A21
constexpr std::uint32_t kAtRunAll = 0x00;
constexpr std::uint32_t kAtBack = 0x50;      // 0x5A98F0
constexpr std::uint32_t kAtSetBase = 0x67;   // 0x5A9907
constexpr std::uint32_t kAtCreate = 0x74;    // 0x5A9914
constexpr std::uint32_t kAtSleep = 0xA9;     // 0x5A9949
constexpr std::uint32_t kAtRestart = 0xD6;   // 0x5A9976
constexpr std::uint32_t kAtExit = 0x10D;     // 0x5A99AD
constexpr std::uint32_t kAtClear = 0x154;    // 0x5A99F4

}  // namespace task_sched
