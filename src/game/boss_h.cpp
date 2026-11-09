// The boss band's shared helpers: the 20 functions of 0x437CA0..0x440829 that
// three or more of the band's units reach (tools/boss_rows.py --unit H,
// 2026-09-28; analysis/boss_funcs.tsv's group column BH), each read to its
// last instruction with capstone (2026-09-28) and taken through the boss
// harness (boss_harness.h). Round eleven group BH; docs/boss_h.md has them one
// row each.
//
//   - two fillers: BareRet (the bare ret in 455 hook and table slots,
//     engine-wide) and BareRetZero (an event hook answering al 0);
//   - four hooks: the end hook that picks the way out, two exit hooks that
//     clear the actor tagged 0, and an enemy hook that retargets;
//   - the state helpers several kinds' tables share: tick the sprite's
//     script, tick it once then state 2, the death (Battle_EnemyDefeated and
//     the state bytes);
//   - kinds 8..11's shared death sequence (Torast, Kassen, Galtel, Doksen -
//     the tool's names): three dispatchers by +2, +3, +4 over their shared
//     tables, then four steps - a flash of the screen in the kind's colour at
//     halving intervals, a ring that grows, a ring that shrinks and the
//     defeat - and the ring's draw (64 Gouraud triangles);
//   - the area-79 fights' map helpers: a 2 x 2 block of the map's corner
//     cells set, the block and the flags by two enemies' status, a byte by
//     the leader's character id.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// three dispatchers and BossMap_SetCorners abort past their tables where the
// original would jump or read through whatever follows (the owner's rule for
// an unchecked index, round9 doc section 6; no route reaches it). Every call
// goes through the harness (BH_CALL / BH_AT / Phase), so the start-up fuzz can
// stand recorders in for the callees.
#include "game/boss_h.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_h_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_h::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// Kinds 8..11's colours: three bytes a kind from BossTorast_Colours (kind 8's
// at its start); the original indexes by the kind unchecked, so another kind
// reads the .data around it.
const unsigned char* KindColour(unsigned kind) { return At(AddressOf(BossTorast_Colours) + 3 * (kind - 8)); }

}  // namespace

// --- the fillers -----------------------------------------------------------------

// --- the hooks ---------------------------------------------------------------------

// --- the state helpers ---------------------------------------------------------------

// --- kinds 8..11's death ---------------------------------------------------------------

// --- the area-79 fights' map helpers ---------------------------------------------------------

void BossH_Inject() {
    if (bof3::WantsShadow("boss_h")) boss_h::SelfTest();
}
