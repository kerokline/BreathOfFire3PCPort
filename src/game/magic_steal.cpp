// Steal's overlay (MAGIC216.EMI on the PSX; Magic_Rows row 87, the ability
// read one id down as Steal), compiled into the exe at 0x4F50B0..0x4F52E5:
// the kind-2 task, its start, and its roll. The fourth function of the
// extent, 0x4F52D0 (the end, once the message window is down), is
// MagicFx_EndWhenIdle, round eight's (magic_fx_reached.cpp), shared with
// Pilfer's overlay. docs/magic_steal.md.
//
// The same routine as Pilfer's (Steal_Start, magic_fx_reached.cpp), in an
// older shape: no thief's double, no animation, and the message queued at
// once rather than by a later phase.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals'
// copies. No divergence: each is a faithful replacement, except that a phase
// past the task's three-entry stack table aborts where the original would
// call through its own stack (docs/magic_fx_reached.md section 3, the
// precedent).
#include "game/magic_steal.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::EnemyOf;
using magic_harness::Mem;
using magic_harness::PartyOf;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The enemy record's fields the roll reads (docs/magic_fx_reached.md section 4).
constexpr unsigned kEnemyItem = 0xA8;    // u16: category << 8 | index, 0 none
constexpr unsigned kEnemyRate = 0xAA;    // u8: row of SkillSteal_RateTable
constexpr unsigned kEnemySpeed = 0xB8;   // u16
constexpr unsigned kThiefSpeed = 0xA8;   // u16 on the party record

// The item's name into Text_Records by (item, category) - unnamed, in no
// group (docs/magic_fx_reached.md section 10).
constexpr std::uint32_t kItemName = 0x4B58F0;
using ItemNameFn = void (__cdecl*)(unsigned, unsigned);

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

// The speed tier: 12 at a difference of 49 or more, one less below each of
// 49, 29, 19, 9, -10, -20, -30 and -50 (a jge chain, every compare signed).
int Tier(int d) {
    int m = 12;
    if (d < 49) m = 11;
    if (d < 29) m = 10;
    if (d < 19) m = 9;
    if (d < 9) m = 8;
    if (d < -10) m = 7;
    if (d < -20) m = 6;
    if (d < -30) m = 5;
    if (d < -50) m = 4;
    return m;
}

// The message, the queue, the phase on: every end of the roll.
void Report(unsigned id) {
    const unsigned char* const text = MH_CALL(Msg_SystemPtr)(id);
    MH_CALL(BattleQueue_Push)(1, 0xFF, static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(text)));
    Bump(Sprite_Current[1]);
}

}  // namespace

#define MSTEAL_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4F50B0: the kind-2 task (Magic_Rows row 87). Its phase +1
// through a three-entry table the original builds on its stack -
// SkillSteal_Start, SkillSteal_Roll, MagicFx_EndWhenIdle - called by the
// address each holds. The index is not checked by the original: 3..255
// would call through its own stack; ours aborts.
MSTEAL_EXPORT void __cdecl SkillSteal_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::SkillSteal_Start, bof3::addr::SkillSteal_Roll,
                                                 bof3::addr::MagicFx_EndWhenIdle};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 3) bof3::Fatal("SkillSteal_Task: phase %u, past the three-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4F50E0: the owner's (0x93B940) actor byte +8 and position
// +0x34 / +0x38 / +0x3C to the task, the owner and Sprite_Current read again
// for each; +0xB 0; the phase on.
MSTEAL_EXPORT void __cdecl SkillSteal_Start(void) {
    Sprite_Current[8] = Pointer(at::kOwner)[8];
    SetLong(Sprite_Current + 0x34, Long(Pointer(at::kOwner) + 0x34));
    SetLong(Sprite_Current + 0x38, Long(Pointer(at::kOwner) + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(Pointer(at::kOwner) + 0x3C));
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4F5140: the roll. The target (0x904B44) as an enemy record,
// unchecked (a target of 0..2 reads below the records); the tier from the
// thief's speed (party +0xA8, by 0x904B34) less the enemy's (+0xB8); the
// rate SkillSteal_RateTable[enemy +0xAA] (signed, the row unbounded).
//
//   - Rand() & mask >= rate * tier (signed): the message 0x39, or 0x3A when
//     the enemy's rate row is 0;
//   - else no item (+0xA8 == 0): 0x3A;
//   - else Inventory_Add(item >> 8, item, 1) answering 0 (no room): 0x39;
//   - else the item's name into Text_Records, the message 0x38, and the
//     enemy's item and rate row cleared.
//
// The target is read again after Rand and after the queue, as the original
// reads it. The original pushes a fourth argument 0 to Inventory_Add, which
// takes three (cdecl: the caller pops it). The mask is 0xFF, or 0 under
// DIV-0046 (BOF3X_STEAL=1), read back from the patched original's byte
// (Cheats_StealRollMask).
MSTEAL_EXPORT void __cdecl SkillSteal_Roll(void) {
    const unsigned char* const enemy = EnemyOf(Mem(at::kTarget)[0]);
    const int diff = static_cast<int>(Word(PartyOf(Mem(at::kActor)[0]) + kThiefSpeed)) -
                     static_cast<int>(Word(enemy + kEnemySpeed));
    const int tier = Tier(diff);
    const int rate = static_cast<signed char>(SkillSteal_RateTable[enemy[kEnemyRate]]);
    const std::uint32_t r = static_cast<std::uint32_t>(MH_CALL(Rand)());
    const std::uint32_t chance = static_cast<std::uint32_t>(rate) * static_cast<std::uint32_t>(tier);
    unsigned char* const target = EnemyOf(Mem(at::kTarget)[0]);
    if (static_cast<int>(r & Cheats_StealRollMask()) >= static_cast<int>(chance)) {
        Report(target[kEnemyRate] != 0 ? 0x39 : 0x3A);
        return;
    }
    const unsigned item = Word(target + kEnemyItem);
    if (item == 0) {
        Report(0x3A);
        return;
    }
    const unsigned category = item >> 8;
    if (static_cast<unsigned char>(MH_CALL(Inventory_Add)(category, item, 1)) == 0) {
        Report(0x39);
        return;
    }
    MH_AT(ItemNameFn, kItemName)(item, category);
    const unsigned char* const text = MH_CALL(Msg_SystemPtr)(0x38);
    MH_CALL(BattleQueue_Push)(1, 0xFF, static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(text)));
    unsigned char* const after = EnemyOf(Mem(at::kTarget)[0]);
    after[kEnemyRate] = 0;
    SetWord(after + kEnemyItem, 0);
    Bump(Sprite_Current[1]);
}

void MagicSteal_Inject() {
    if (bof3::WantsShadow("magic_steal")) magic_steal::SelfTest();
    BOF3_INJECT(SkillSteal_Task);
    BOF3_INJECT(SkillSteal_Start);
    BOF3_INJECT(SkillSteal_Roll);
}
