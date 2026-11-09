// Round twelve group FE1: the field engine's resident code 0x52D080..0x533BA0,
// taken with the scenario harness in field mode (scenario_harness.h, docs/
// scenario_harness.md section 7). docs/field_e1.md has each function, every
// caller, the tables and the fuzz.
//
//   FieldPanel_DrawHeader        0x52D080  two panel sprites (0, 1) at (x + 8, y) and (x + 0x108, y)
//   FieldPanel_DrawKindIcon      0x52D0C0  panel sprite 2 at (x, y), its CLUT and UV by a kind byte
//   FieldPanel_DrawKindRow       0x52D140  a kind's row: three sprites, its name, count and points
//   FieldPanel_DrawTotal         0x52D320  the total of the points, its rank (0x9045F4) and the rank's sprites
//   FieldPanel_DrawMessage       0x52D560  sprite 0xC and a script-pool message at (x, y)
//   FieldPanel_DrawShade         0x52D5C0  a half-transparent tile over the 320 x 240 screen
//   FieldPanel_DrawBox3          0x52D610  a window, its sprite frame and three script-pool lines
//   FieldPanel_DrawBox2          0x52D750  a wider window and two lines
//   Inventory_Holds38To4DAt99    0x52D880  al: 22 or more slots of item 0x38..0x4D held at 99
//   FieldPanel_DrawBlink         0x52D8C0  sprite 0x47 every other 8 frames while 0x939A28
//   Field_PathClear              0x52EC20  al: the way from the leader to (x, z) walkable, x then z
//   Field_FormActionState        0x52F4F0  leader state 6: the party set's handler (0x660A44) until done
//   PartyAction_ScriptEnd        0x52F5C0  a party action's step: the script once, done when it ends
//   Field_PassageTrigger         0x52F8F0  passage step 3: the object trigger on the passage's event
//   Field_JumpState              0x52F950  leader state 8: jmp Field_JumpSteps[+2]
//   Field_JumpBegin .. JumpIn3   0x52F970..0x52FB50  the jump's steps and sub-steps (FE2's helpers)
//   Field_ContentState           0x52FBB0  leader state 11: the content step (0x660A1C), the script tick
//   Field_ContentTake            0x52FBD0  a field object's content taken: zenny or an item, its flag
//   Field_ContentEnd             0x52FD90  the object after: closed back, removed or its cell marked
//   Field_AreaRunState           0x52FE90  leader state 12: area 104's or 121's run
//   Field_GiveZenny              0x5307C0  the zenny found: sound, "n" printed, message 5, Zenny_Add
//   Field_CellAroundLarge        0x531120  al: a cell of `code` around a sprite whose +0x34 / +0x38 are not whole
//   Field_CellAroundSide         0x531540  al: the two cells of `code` on one side, the facing turned to it
//   Field_GatewayExit            0x531820  al: the area's gateway exit, set pending
//   Party_PlaceAtSlots           0x532C10  the members to the slot positions, the camera to the leader
//   Party_ScriptTicks            0x532D10  each member's movement script, one tick
//   Party_PlacesByList           0x532D50  the members' places by the second list, then poses or a drop-in
//   Field_LeaderPlaceOffset      0x533690  the leader placed at (x, z) set off by the facing
//   Field_PendingJumpTurn        0x5338B0  pending jump 1: the member released by its facing (kind 2 / 3)
//   Field_PendingJumpKind4       0x5338F0  pending jump 2: released with kind 4
//   Field_PendingDrop            0x533900  pending jump 3: the next member dropped in from above
//   Field_PendingNext            0x5339A0  al: the next member of the pending jump made current
//   Field_PendingRelease         0x533A50  al: the member at 0x904EF2 released into a kind's state
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Sprite_Current and Field_State are read
// afresh at every use, as the original reads [0x937F88] / [0x905D98] (a callee
// may move either). Where the original would jump through a table entry that
// is not code, or walk its stack past a table, ours aborts with a message
// (docs/field_e1.md section 7).
#include "game/field_e1.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_e1_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_e1::at;
// move_script::At: named in full below (source-layout B0, a clash on At)
// move_script::Long: named in full below (source-layout B0, a clash on Long)
using move_script::SetLong;
// move_script::SetWord: named in full below (source-layout B0, a clash on SetWord)
// move_script::Word: named in full below (source-layout B0, a clash on Word)

unsigned char& BFieldE1(std::uint32_t a) { return *move_script::At(a); }
std::uint16_t W(std::uint32_t a) { return move_script::Word(move_script::At(a)); }
void SetW(std::uint32_t a, unsigned v) { move_script::SetWord(move_script::At(a), v); }
std::int32_t L(std::uint32_t a) { return move_script::Long(move_script::At(a)); }

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Fs() { return Field_State; }

// The record views (game/record_views.h): in the leader states Sprite_Current
// is an ObjTrio record, and Field_State always is.
short S16(std::uint32_t v) { return static_cast<short>(v); }

// `cdq / xor / sub`: the absolute value with the original's wrap (the most
// negative stays negative).
std::int32_t Abs32(std::int32_t v) {
    const auto u = static_cast<std::uint32_t>(v);
    const std::uint32_t s = static_cast<std::uint32_t>(v >> 31);
    return static_cast<std::int32_t>((u ^ s) - s);
}

}  // namespace

// --- the panel draws (callers 0x466BE0..0x4670C0, 0x528A90..0x52A420: Capcom's) ----

// The sprite frame of a panel window: its top row (0x25, 0x26 x `across`,
// 0x27, 0x28) at y, the two sides (0x468950) from y + 0x18 over `side`, the
// bottom row (0x30, 0x31 x `bottom`, 0x32) at y + `low`.
// --- the way, the leader's states ---------------------------------------------------

// The field object Field_State +0x139 names (Sprite_Objects, the byte whole
// and unchecked), read again at every use as the original reads it.
// --- the cells around a sprite ------------------------------------------------------

// --- the party's placements -----------------------------------------------------------

// --- the pending jump's members (Field_PendingJumps 1..3) ---------------------------

void FieldE1_Inject() {
    if (bof3::WantsShadow("field_e1")) field_e1::SelfTest();
}
