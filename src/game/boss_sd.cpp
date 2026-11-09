// Round eleven group BSD: the boss band's fights 17..21 and kinds 18, 21..27 -
// 52 functions of 0x43A590..0x43B74A (tools/boss_rows.py --groups,
// analysis/boss_funcs.tsv's group column BSD), each read to its last
// instruction with capstone (2026-09-28) and taken through the boss harness
// (boss_harness.h). docs/boss_sd.md has them one row each. The enemy names are
// tools/boss_rows.py --disc's (the US disc's area records):
//
//   - kind 18 (Mutant, area 52) and set-up 17 (its fight, row 7);
//   - kinds 21, 22, 23, 26 (Claw, Cawer, Patrio, Dodai 1 / 2, area 79) and
//     set-ups 18, 19, 20 (one image on the PSX - the sibling's dedup
//     BOSS018 = BOSS019 = BOSS020 - rows 7, 6, 5; three copies of one set of
//     hooks differing in the tag of the actor they place and the chapter
//     steps they hand back);
//   - kinds 24, 25 (Emitai, Golem, area 81) and set-up 21 (row 7), whose end
//     hook set-up 23 (BSE's) installs too;
//   - kind 27 (Garr, area 80; set-up 22 is BSE's).
//
// A kind is a dispatcher (BossKind_Table[kind]) by the sprite's +1 through the
// kind's twelve-entry step table - EnemyOp_Steps with the kind's own entrance
// at 0 and, for some kinds, their own action dispatcher at 6 - its entrance
// (the enemy's +0xFC / +0xF4 / +0xF8 pointed at the kind's tables), an action
// dispatcher by +2 with the kind's death at 4, and the enemy's +0xF4 hook: a
// three-entry table by the hook's word. A set-up stores the fight's three
// hooks (BattleHook_End / _Exit / _Event) and returns.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers and hooks abort past their tables where the original would jump
// through whatever follows, and set-up 21's exit hook aborts where the
// original writes through BossActor_Find's null (the owner's rule for an
// unchecked index or pointer, round9 doc section 6; nothing reaches either).
// Every call goes through the harness (BH_CALL / BH_AT / Phase), so the
// start-up fuzz can stand recorders in for the callees.
#include "game/boss_sd.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sd_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sd::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

// The end phase's steps (nobody's; the harness's standard set).
void EndStep(U step) { BH_WORD(Handler, step)(); }

// Set-ups 17 and 21's end hooks: the win sets the field's move-script counter
// 0 (`counter`) and takes the end phase's step 1, else step 2.
void EndCounter(unsigned char counter) {
    if (Battle_EndFlags & 2) {
        B(::bof3::Img(at::kCounter0.v)) = counter;
        EndStep(boss_sd::at::kEndWin.v);
        return;
    }
    EndStep(boss_sd::at::kEndOther.v);
}

}  // namespace

// === Kind 18 (Mutant, area 52) =====================================================

// === Set-up 17 (Mutant, area 52 row 7) ===================================================

// === Kinds 21, 22, 23 (Claw, Cawer, Patrio, area 79) =========================================

// === Kind 26 (Dodai 1 / 2, area 79) =============================================================

// === Set-ups 18, 19, 20 (area 79: Claw, Cawer, Patrio, Dodai; rows 7, 6, 5) ==========================

// === Kinds 24, 25 (Emitai, Golem, area 81) ========================================================

// === Set-up 21 (Emitai, Golem, area 81 row 7) =====================================================

// === Kind 27 (Garr, area 80) ======================================================================

void BossSd_Inject() {
    if (bof3::WantsShadow("boss_sd")) boss_sd::SelfTest();
}
