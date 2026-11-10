// Moved here by tools/move_funcs.py from battle_flow.cpp, battle_fx_tasks.cpp, rest_3a.cpp (docs/source-layout-round.md).
#include "game/battle/battle_task.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_flow_callees.h"
#include "game/battle_fx_tasks_callees.h"
#include "game/boss_harness.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3a_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3a::at;

}  // namespace

using namespace battle_flow;

using namespace battle_fx_tasks;

namespace battle_flow {

using move_script::At;

using move_script::Long;

}  // namespace battle_flow

namespace {

// A pointer the original keeps in a dword of .data / .bss, read afresh.
unsigned char* PtrAtBattleFlow(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_flow::Long(battle_flow::At(address)))));
}

unsigned char* CurrentTask() { return PtrAtBattleFlow(::bof3::Img(BattleFlow_TaskCurrent_addr)); }

unsigned char* Task(unsigned index) { return battle_flow::At(::bof3::Img(BattleTask_Slots_addr) + index * battle_flow::at::kTaskSize); }

// The battle-task slot being run (0x93B8C4), and a slot's owner (+0x80).
unsigned char* Cur() { return rv::Bytes(BattleFlow_TaskCurrent.Native()); }

void CallIndex(const battle_fx_tasks::Handler* table, unsigned n, unsigned index, const char* who) {
    if (index >= n) bof3::Fatal("%s: index %u, past the %u-entry table the original builds on its stack", who, index, n);
    table[index]();
}

using U = std::uint32_t;

unsigned char* Slot() { return rv::Bytes(BattleFlow_TaskCurrent.Native()); }

}  // namespace

// original 0x435110: each of the 48 battle-task slots whose byte +0 is not
// zero - the slot to 0x93B8C4 and to Sprite_Current, its +0x80 dword to
// 0x93B940 - runs the handler of its kind (+6) from a four-entry table the
// original builds on its own stack: 0x4352A0, 0x435350, 0x4378B0 (the magic
// effect BattleTask_Create's callers start), 0x4357D0.
//
// As the original has it: a slot runs when its byte +0 is non-zero, whatever
// the bits (BattleTask_Create takes a slot by bit 0 alone); byte +0 and the
// kind are read afresh for each slot, so a handler that frees or takes a
// later slot is seen the same frame. The kind is not checked: 4..255 would
// call through the original's stack above the table; ours aborts.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleTask_RunAll(void) {
    for (unsigned i = 0; i < battle_flow::at::kTaskCount; ++i) {
        unsigned char* const t = Task(i);
        auto* const task_rec = rv::As<records::BattleTask>(t);
        if (task_rec->flags == 0) continue;
        const std::int32_t owner = static_cast<std::int32_t>(task_rec->owner);
        BattleFlow_TaskCurrent = task_rec;
        Sprite_Current = t;
        BattleE2_Owner = owner;
        const unsigned kind = task_rec->runner;
        if (kind >= 4) bof3::Fatal("BattleTask_RunAll: slot %u has kind %u, past the four-entry table", i, kind);
        battle_flow::g.tasks[kind]();
    }
}

// original 0x435180 (PSX 0x801E584C, the same): the first of the 48 slots
// whose bit 0 is clear gets bit 0, the kind at +6 and the parameter at +5;
// its index is returned, or 0xFF when every slot is taken.
//
// As the original has it: kind and parameter are the arguments' low bytes;
// the other bits of byte +0 are kept. The callers do not test for 0xFF.
extern "C" unsigned char __cdecl BattleTask_Create(unsigned kind, unsigned parameter) {
    for (unsigned i = 0; i < battle_flow::at::kTaskCount; ++i) {
        auto* const task_rec = rv::As<records::BattleTask>(Task(i));
        if ((task_rec->flags & 1) != 0) continue;
        task_rec->flags = static_cast<unsigned char>(task_rec->flags | 1);
        task_rec->runner = static_cast<unsigned char>(kind);
        task_rec->kind = static_cast<unsigned char>(parameter);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x4351F0 (PSX 0x801E58C8): the running slot (0x93B8C4) freed -
// bytes +0, +5, +6, +1..+4, +0x48, +0x5D..+0x5F zeroed.
extern "C" void __cdecl BattleTask_FreeCurrent(void) {
    static constexpr unsigned kBytes[] = {0, 5, 6, 1, 2, 3, 4, 0x48, 0x5D, 0x5E, 0x5F};
    for (const unsigned b : kBytes) CurrentTask()[b] = 0;
}

// original 0x435260 (PSX 0x801E5978): the same eleven bytes of all 48 slots.
extern "C" void __cdecl BattleTask_ClearAll(void) {
    static constexpr unsigned kBytes[] = {0, 5, 6, 1, 2, 3, 4, 0x48, 0x5D, 0x5E, 0x5F};
    for (unsigned i = 0; i < battle_flow::at::kTaskCount; ++i)
        for (const unsigned b : kBytes) Task(i)[b] = 0;
}

// original 0x4352A0 (no PSX twin paired): battle-task kind 0. Entry +5 of the
// running slot (0x93B8C4) of a nineteen-entry table the original builds on
// its stack: 0x437CC0 (a bare ret), the damage popup 0x432B70, 0x432F90, the
// pose task 0x433190, 0x4332B0, 0x433380, the actor watch 0x433460, the
// follower 0x4337F0, then 0x43C740 .. 0x452B60 (unread).
//
// As the original has it: the index is not checked; 19..255 would call
// through the words above the table on the original's stack. Ours aborts
// instead (CLAUDE.md rule 4, as battle_flow's BattleTask_RunAll).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_Dispatch(void) {
    CallIndex(battle_fx_tasks::g.fx, 19, Cur()[5], "BattleFx_Dispatch");
}

// original 0x435350 (no PSX twin paired): battle-task kind 1, the magic
// effects BattleTask_Create(1, n)'s callers start. Entry +5 of the running
// slot of a 110-entry table the original builds on its stack (0x1B8 bytes):
// 0x437CC0 first, then the effects (battle_fx_tasks_callees.h). Unchecked as
// kind 0's; ours aborts past 109.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMagicFx_Dispatch(void) {
    CallIndex(battle_fx_tasks::g.magic_fx, 110, Cur()[5], "BattleMagicFx_Dispatch");
}

// original 0x4357D0 (BattleTask_RunAll's kind 3): by the running slot's +5
// through an eight-entry table it builds on its stack: BareRet,
// Magic002Ball_Task, BossGazerFx_Dispatch, BossAnglerFx_Dispatch,
// BossDLordFx_Dispatch, BossMyriaFx_Dispatch, BossWeretigrFx_Task,
// BossArwanFx_Dispatch. The entry is called with the table under it, so the
// words an entry finds above its return address are the table's (the
// dispatchers among them hand the first on): ours passes the eight.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleBossFx_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BareRet,
                               bof3::addr::Magic002Ball_Task,
                               bof3::addr::BossGazerFx_Dispatch,
                               bof3::addr::BossAnglerFx_Dispatch,
                               bof3::addr::BossDLordFx_Dispatch,
                               bof3::addr::BossMyriaFx_Dispatch,
                               bof3::addr::BossWeretigrFx_Task,
                               bof3::addr::BossArwanFx_Dispatch};
    const unsigned index = Slot()[5];
    if (index >= 8)
        bof3::Fatal("BattleBossFx_Dispatch: index %u, past the 8-entry table the original builds on its stack (docs/rest_3a.md "
                    "section 7)",
                    index);
    using Eight = void (__cdecl*)(U, U, U, U, U, U, U, U);
    BH_WORD(Eight, kTable[index])(::bof3::CodePlace(kTable[0]), ::bof3::CodePlace(kTable[1]), ::bof3::CodePlace(kTable[2]),
                                  ::bof3::CodePlace(kTable[3]), ::bof3::CodePlace(kTable[4]), ::bof3::CodePlace(kTable[5]),
                                  ::bof3::CodePlace(kTable[6]), ::bof3::CodePlace(kTable[7]));
}

void BattleTask_Inject() {
    BOF3_INJECT(BattleTask_RunAll);
    BOF3_INJECT(BattleTask_Create);
    BOF3_INJECT(BattleTask_FreeCurrent);
    BOF3_INJECT(BattleTask_ClearAll);
    BOF3_INJECT(BattleFx_Dispatch);
    BOF3_INJECT(BattleMagicFx_Dispatch);
    BOF3_INJECT(BattleBossFx_Dispatch);
}
