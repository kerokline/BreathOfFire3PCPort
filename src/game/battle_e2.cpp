// Round twelve group BE2 (docs/takeover-queue-field-battle.md section 3; the
// 48 rows BE2 of analysis/round12_cut.tsv): the battle engine's
// 0x433650..0x437030, each read to its last instruction with capstone
// (2026-09-29, tools/band_rows.py --group BE2 for the extents and call rows)
// and taken through the boss harness as an engine group (boss_harness.h,
// docs/boss_harness.md section 10). docs/battle_e2.md has every function one
// row each.
//
//   - the effect tasks: the actor watch's state 3 (a status icon over the
//     watched actor, cycled every 0x3C frames) with its two helpers; the
//     party put back from the backup records 0x939AE0 after Accession's
//     single-member form (BattleFx_Dispatch slot 12's two states and slot
//     17's task, one per other member); four markers and cursors drawn over
//     window records 18, 19 and 21 (slots 9, 10, 13, 14);
//   - the enemy's action pick (Battle_BeginAction's call for an enemy, the
//     transformation route's path) and its target helpers;
//   - the enemy ops of EnemyOp_Steps 4 and 5 (the turn's start, the creature
//     cue - PSX twin Battle_PlayCreatureCue), of EnemyOp_EnterSubs 1 (a slide
//     in) and of EnemyOp_ActSubs 3 and 5 (a knock-back, the HP change).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original divides by a count or a maximum that can be 0, indexes past a
// table (the enemies by a byte above 7, the party by a byte above 2, a task
// slot BattleTask_Create answered 0xFF for, a dispatcher's state byte) or
// spins without end (BattleFx_NextStatusIcon with no status bit and +0xB of
// 16 or more), ours aborts with a message (the owner's rule for an
// unchecked index, round9 doc section 6); docs/battle_e2.md section 7 lists
// each. Every call goes through the harness (BH_CALL / BH_AT / Phase), so the
// start-up fuzz can stand recorders in for the callees.
#include "game/battle_e2.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_e2_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e2::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
U UL(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
U Key(const void* p) { return Low32(p); }
unsigned char* Sc() { return Sprite_Current; }
// rv::Current<T>() is typed by who runs: a battle task slot, or the enemy an
// enemy op runs on (docs/battle_e2.md 1.1, 1.3).
// An enemy object and a party member by an actor's index, as the originals
// address them (32-bit wrap: an index below its base reads below the array).
unsigned char* EnemyObj(U index) { return move_script::At(::bof3::Img(Battle_Enemies_addr) + index * battle_e2::at::kEnemyStride); }
unsigned char* Member(U index) { return move_script::At(::bof3::Img(ObjTrio_addr) + index * battle_e2::at::kPartyStride); }
unsigned char* Task(U slot) { return move_script::At(::bof3::Img(BattleTask_Slots_addr) + slot * battle_e2::at::kTaskStride); }

// A task slot BattleTask_Create answered: 0xFF (none free) is not tested by
// the original, which then writes past the 48 slots (known-defects D163's
// form). Ours aborts.
unsigned char* NewTask(const char* who, unsigned slot) {
    slot &= 0xFF;
    if (slot >= 48)
        bof3::Fatal("%s: BattleTask_Create answered %u - the original writes past the 48 task slots (docs/battle_e2.md "
                    "section 7)",
                    who, slot);
    return Task(slot);
}

}  // namespace

// ===========================================================================
// The actor watch's state 3: the status icon (BattleFx_ActorWatch slot 3)
// ===========================================================================

// ===========================================================================
// The party put back from its backup records (slots 12 and 17)
// ===========================================================================

// ===========================================================================
// The markers over window records 21, 18 and 19 (slots 9, 10, 13, 14)
// ===========================================================================

// ===========================================================================
// The enemy's action and its target (the action's begin)
// ===========================================================================

// ===========================================================================
// The enemy ops (EnemyOp_Steps 4 and 5, EnemyOp_EnterSubs 1, EnemyOp_ActSubs 3
// and 5)
// ===========================================================================

// ============================================================================

void BattleE2_Inject() {
    if (bof3::WantsShadow("battle_e2")) battle_e2::SelfTest();
}
