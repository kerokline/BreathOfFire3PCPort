// Stats and inventory: group W of the sixth round, the functions in
// 0x5903F0..0x591CAB that the owner's shop route reaches, and two next to
// them. docs/char-stats.md.
//
//   - Char_RecalcStats 0x590660 (PSX 0x80165434) and its four passes: the
//     weapon 0x590FC0, the armour 0x591190, the accessories 0x591490 and the
//     per-character trait list 0x590800 (PSX 0x80166150 / 0x801662CC /
//     0x8016651C / 0x80165290), with the two byte steps under them,
//     Stat_AddResist 0x590EE0 and Stat_AddCap100 0x590F30 (PSX 0x80165FE4 /
//     0x80166084). 0x590EE0 is not on the queue (the shop route never calls
//     it) but every one of its 60 call sites is in this file.
//   - The equipment previews 0x590960 and its twin 0x590AB0 (no PSX twin):
//     the stat recompute on a copy of a record, compared with the record.
//     0x590AB0 is not on the queue either (unreached); taken as the twin.
//   - Inventory_Add 0x590BB0 (PSX 0x80165AA4), the counts 0x5919B0 /
//     0x591A80, the key-item test 0x5918E0, the item-table getters 0x591680 /
//     0x591720 / 0x5917A0 / 0x591C20, the text record copy 0x591940.
//   - Two menu draws: the icon quad 0x5903F0 (PSX 0x80164EC8, re-laid on the
//     PC) and the pointing hand Menu_DrawHand 0x5905D0 (PSX 0x801651B4).
//
// Every call goes through char_stats::g (char_stats_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike.
#include "game/char_stats.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/char_stats_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace char_stats {

const Callees kOriginals = {
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_SetPolyFT4,
    Gpu_SetSprt,
    Stat_AddClamped,
    Stat_AddCap99,
    Stat_AddResist,
    Stat_AddCap100,
    Char_ApplyWeapon,
    Char_ApplyArmour,
    Char_ApplyAccessories,
    Char_ApplyTraits,
    Char_RecalcStats,
};
Callees g = kOriginals;

}  // namespace char_stats

using namespace char_stats;

// ===========================================================================
// The two menu draws

// The icon quad's CLUT row per icon, the 21 bytes the original builds on its
// stack (PSX: 21 bytes copied from 0x80149C78).
// ===========================================================================
// The stat recompute and its passes

// ===========================================================================
// The equipment previews

// ===========================================================================
// The inventory

// ===========================================================================
// The item tables: by category 1 weapons, 2 armour, 3 accessories, 4 key
// items, any other the consumables; the item's low byte indexes, unbounded.

// ===========================================================================
// The text record

// ===========================================================================

void CharStats_Inject() {
    if (bof3::WantsShadow("char_stats")) char_stats::SelfTest();
}
