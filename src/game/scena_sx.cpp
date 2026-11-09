// The engine callees the chapter and area groups call by raw address and
// nobody owned - round ten, group SX, taken with the scenario harness
// (scenario_harness.h). docs/scena_sx.md has each function, every caller,
// and why one of the nineteen addresses the round listed is not here.
//
//   Char_LevelUp            0x498DE0  a record's level from its EXP, the gains of each level
//   Party_PlaceForBattle    0x532ED0  the members placed at an event battle's formation
//   Party_ReloadPalettes    0x533E00  each member's palette reloaded
//   Party_HealJoined        0x533E50  every joined record healed, the members' copies refreshed
//   Party_Remove            0x534030  a member out of both lists and the field objects
//   Sprite_FlashClut        0x534DB0  Sprite_Current's CLUT row one colour, a sound
//   Char_LoseHp             0x537480  a member's HP down, never below 1
//   Field_SetStatus80       0x56D6F0  Field_StatusBits |= 0x80
//   Field_CellTriggerAt     0x56D800  a chapter's 5-byte cell records searched
//   MapView_FillCells       0x56FCA0  the view's 56 x 28 cell items refilled
//   Camera_TurnToDegrees    0x57C550  Camera_Angles[0] turned toward an angle in degrees
//   Camera_EaseAngleFB      0x57C6B0  Cond_AngleFB eased toward an angle over frames
//   Sprite_FindFree         0x57CD90  the first free Sprite_Objects record
//   AbilityList_Add         0x590C90  an ability into a member's list or the shared one
//   KeyItem_Add             0x591900  a key item into the 32-byte list
//   Inventory_Remove        0x591B60  items out of a category's lists
//   Zenny_Sub               0x591BC0  the party's zenny down, if it has enough
//   Zenny_Add               0x591BE0  the party's zenny up, capped
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. The unchecked indexes the
// originals make (a member id past Character_IndexById's 24, a category
// past the inventory's five, a member count above three) are reproduced;
// where the original would fault or read its own stack frame (a CLUT colour
// past its four, a divide by 0), ours aborts with a message (docs/scena_sx.md
// section 6).
#include "game/scena_sx.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sx_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sx::at;
using move_script::SetLong;

// The record views (game/record_views.h): rv::As<records::SpriteObject>(p)
// views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record. rv::As<records::PartyMember>(p) is given party members
// (ObjTrio, Field_State).
using move_script::SetWord;
using move_script::Word;

// CharacterRecords' record n (stride 0xA4), unchecked.
unsigned char* RecordScenaSx(unsigned n) { return move_script::At(::bof3::Img(CharacterRecords_addr) + n * scena_sx::at::kRecordStride); }
// A member id's record: CharacterRecords[Character_IndexById[id]], both
// indexes a whole byte, unchecked (the original reads them the same way).
unsigned char* MemberRecord(unsigned char id) { return RecordScenaSx(Character_IndexById[id]); }

// A signed byte's value.
int S8(unsigned char b) { return static_cast<signed char>(b); }

}  // namespace

void ScenaSx_Inject() {
    if (bof3::WantsShadow("scena_sx")) scena_sx::SelfTest();
}
