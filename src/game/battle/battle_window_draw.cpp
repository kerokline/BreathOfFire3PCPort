// The battle windows' draw helpers (the seventh round's group BD).
// docs/battle_window_draw.md.
//
// Nineteen functions the owner's combat route reaches: the four openers of
// battle windows 1..4, and the primitives the battle window handlers (group
// BC's 0x442FA0, 0x443870, 0x443B10, 0x443D90, 0x443F60) draw with - the digit
// strip, a flat quad of a shape table, two tiles, a two-part bar, a 16 x 8
// cell, three lines - the test that keeps one enemy name per kind, and, from
// the skill list, the ability helpers: can it be used, its row, its AP cost,
// the lowest of its flag bits, a member's list of a type. Each was read to its
// last instruction (2026-09-23) against its PSX twin - BATTLE.EMI's for the
// first fourteen, GAME.EMI's for the skill test and row, the boot EXE's for the
// three ability helpers (docs/battle_window_draw.md, section 1).
//
// Every call goes through battle_window_draw::g (battle_window_draw_callees.h),
// so that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Where the original reads memory after a call,
// ours reads it after the same call.
//
// Upper halves. Some originals push a register whose upper bits are the
// caller's (the icon byte of Menu_DrawSkillRow) or a full argument dword
// (Skill_CanUse's member and id to Skill_ApCost); every callee here reads only
// the low 16 bits of a coordinate and the low byte of a colour, flag, count or
// id, so ours passes values with the upper bits C++ gives them - except where
// the original's own dword is cheap to rebuild exactly, which ours does.
//
// Floats. Every coordinate is an int that the original loads with `fild` and
// stores with `fstp dword`: a 16-bit value plus at most two bytes, exact in a
// float, so a plain conversion is the same bits.
#include "game/battle_window_draw.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_window_draw_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_window_draw {


}  // namespace battle_window_draw

using namespace battle_window_draw;

namespace {

using move_script::At;
using move_script::SetWord;
using move_script::Word;

char* PrintBuf() { return reinterpret_cast<char*>(move_script::At(::bof3::Img(battle_window_draw::at::kPrintBuf.v))); }
const char* Format(std::uint32_t at) { return reinterpret_cast<const char*>(move_script::At(at)); }

}  // namespace

// ===========================================================================
// The four window openers: battle window records 1..4, handler 3 (the battle
// windows), sub-kind 0..3 in +2 and the caller's state in +3. Window_Alloc's
// answer is not looked at - the fields are written whether or not the record
// was free, as the original has it (PSX 0x801D92D4 .. 0x801D93F8 alike).

// ===========================================================================

// ===========================================================================
// The skill list's helpers. The ability table 0x65C4C8 is 0x18 bytes a
// record: the name in 16, then the PSX record's eight bytes (b0 at +0x10, the
// cost b2 at +0x12, u16_4 at +0x14 - the sibling's names/abilities.toml).

// ===========================================================================

void BattleWindowDraw_Inject() {
    g = kOriginals;
    if (bof3::WantsShadow("battle_window_draw")) SelfTest();
}
