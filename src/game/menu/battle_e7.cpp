// Round twelve group BE7 (docs/takeover-queue-field-battle.md section 3): the
// battle windows of 0x597FC0..0x59DB61, the 31 rows of analysis/round12_cut.tsv
// group BE7, each read to its last instruction with capstone (2026-09-29) and
// taken through the boss harness as an engine group (boss_harness.h,
// docs/boss_harness.md section 10). docs/battle_e7.md has every function one
// row each; names are from the code and its callers (no PSX twin of these is
// named in the sibling's corpus).
//
//   - the result screen (Window_Handler4Kinds' kinds 0 and 1): the level-up
//     window (six stat gains and two abilities learned), the drops window
//     (two items a row), one item's line, and the translucent frame both
//     draw - also BattleResultWin_DrawExp's frame;
//   - the gene windows (the window-kind handler 0x598890's kinds 1..4 and the
//     grid's draw its kind 0 calls): the grid window's boxes and frame, a
//     three-choice box, two list windows (six and twelve rows) with their
//     frame, a form icon and a flashing cursor box - each kind a state
//     dispatcher by the record's +3 over an open state and slides;
//   - the battle menu's equipment window 0x59D640 (Window_Handler8Kinds'
//     kind 3): a member's name, four stats, the preview of a set, the six
//     equipped items.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// four stack-table dispatchers abort past their tables where the original
// calls through whatever the stack holds beyond them, and the three open
// states abort on a task slot past the pool where the original writes past it
// (the owner's rule for an unchecked index, round9 doc section 6; both are
// described in docs/battle_e7.md section 7, never fixed). One divergence has
// patch sites inside these bodies and survives in ours: DIV-0041
// (widescreen.cpp, the faster slide) at the three list slide-outs; ours reads each
// bound as widescreen.cpp holds it (docs/operand-store.md). Every call goes
// through the harness (BH_CALL / BH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/battle_e7.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e7_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e7::at;
using U = std::uint32_t;
using move_script::SetWord;
// move_script::Word is named in full here (step B0, analysis/tu_renames.tsv)

unsigned char& BBattleE7(U address) { return move_script::At(address)[0]; }
unsigned W(const unsigned char* p) { return move_script::Word(p); }
U Addr(const void* p) { return Low32(p); }
const char* Str(U address) { return reinterpret_cast<const char*>(move_script::At(address)); }
char* Buf(U address) { return reinterpret_cast<char*>(move_script::At(address)); }
const unsigned char* Text(U address) { return move_script::At(address); }

}  // namespace

// ===========================================================================
// The result screen (Window_Handler4Kinds 0x597F60: kind 0 through 0x597FA0,
// kind 1)
// ===========================================================================

// ===========================================================================
// The gene windows (the window-kind handler 0x598890: kind 0's grid through
// 0x5989B0 / 0x5989F0, kinds 1..4 the stack dispatchers below)
// ===========================================================================

// ===========================================================================
// The battle menu's equipment window (Window_Handler8Kinds kind 3, 0x59CC10)
// ===========================================================================

// ============================================================================

void BattleE7_Inject() {
    if (bof3::WantsShadow("battle_e7")) battle_e7::SelfTest();
}
