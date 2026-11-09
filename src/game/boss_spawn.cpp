// The boss set-ups' spawn helpers (0x4948E0..0x494A7D): six engine functions
// the boss band calls 57 times with a tag (tools/boss_rows.py's frontier:
// 0x494920 by 15 units, 0x4949D0 by 22, 0x4949F0 by 9, 0x494A60 by 7,
// 0x4948E0 and 0x494980 by 2 each). Each read to its last instruction with
// capstone (2026-09-28) and taken by round eleven group BH as a small engine
// module (docs/boss_h.md section 5), through the boss harness.
//
// A field actor here is one of the 30 field objects (Sprite_Objects,
// 0x7DEE80, stride 0xA4) whose type byte +6 is 7; its tag is the byte +0x9E.
// The battle keeps the field's objects, and a boss set-up finds the actors of
// its scene by tag to hide, show, pose or place them (Boss01's exit hook
// 0x437E10 copies two enemies' places and poses onto the actors tagged 6 and
// 7). What bit 0x40 of an actor's +0 means is not read here; the name says
// what the code does.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Three
// write through BossActor_Find's answer without testing it (Capcom's, the
// same code in all three); when no actor carries the tag the original writes
// near address 0 and faults. Ours aborts with a message there instead (the
// owner's rule for an unchecked index or pointer, round9 doc section 6); no
// route or fuzz reaches it, and docs/boss_h.md section 6 describes it.
#include "game/boss_spawn.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using move_script::SetLong;
using move_script::SetWord;

}  // namespace

void BossSpawn_Inject() {
    if (bof3::WantsShadow("boss_spawn")) boss_spawn::SelfTest();
}
