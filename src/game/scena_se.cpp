// The engine-side helpers the scenario chapters share - round ten, group SE,
// taken with the scenario harness (scenario_harness.h). docs/scena_se.md has
// each function, who calls it, and why three of the nine addresses the plan
// listed are not here.
//
//   Field_StartEventBattle  0x4410B0  53 call sites in 14 chapters
//   Scena08_PartyJoin784    0x519F70  chapter 8's call table A entry 5
//   Effect_SpawnAtCell      0x524870  the cell pickups' effect (18 copies call it)
//   EventObj_Face           0x579D70  the end of every event-script placement
//   EventOp_0x              0x57A010  event op 0x / Fx, and 19 direct callers
//   Party_AddToLists        0x591CC0  chapter 6's party change
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. The unchecked indexes the
// originals make (a negative placement count, a member id past
// Character_IndexById's 24) are reproduced, as event_script.cpp's
// placements and field_event.cpp's Party_Join reproduce them (latent
// defects, docs/scena_se.md section 6).
#include "game/scena_se.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_se_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_se::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does.
// rv::As<records::SpriteObject>(p) views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record: ObjectScenaSe(n) is a Sprite_Objects record.
// rv::As<records::EffectObject>(p) is given effect objects (Effect_Objects).

}  // namespace

// original 0x4410B0: an event battle begins. Field_ScriptFlags2 bit 12 (the
// encounter bit the leader's state 5 clears, docs/event_leader.md), the event
// battle byte, the leader's state 5 with its sub-state bytes +2..+4 cleared,
// the battle's flags byte from the event battle's 4-byte record
// (EventBattle_Records[id], its +0) and the opening kind 0. The id is the
// argument's low byte, unchecked against the table (the original reads it
// the same way). No PSX twin paired.
extern "C" void __cdecl Field_StartEventBattle(unsigned id_arg) {
    const unsigned char id = static_cast<unsigned char>(id_arg);
    At(::bof3::Img(at::kScriptFlags2High))[0] = static_cast<unsigned char>(At(::bof3::Img(at::kScriptFlags2High))[0] | 0x10);
    EventBattle_Id = id;
    unsigned char* const leader = At(::bof3::Img(ObjTrio_addr));
    leader[1] = 5;
    const unsigned char flags = At(::bof3::Img(bof3::addr::EventBattle_Records))[id * 4u];
    leader[2] = 0;
    leader[3] = 0;
    leader[4] = 0;
    EventBattle_Flags = flags;
    BattlePhases_Initiative = 0;
}

// original 0x519F70: chapter 8's call table A entry 5, and the tail of entry
// 6 (0x519FA0, which loads a party set first and jumps here). The member
// count to 0; Party_Join(7), (8), (4), their answers unread; then 0x533E00,
// the members' palettes reloaded - a tail jmp in the original, whose eax no
// caller reads (Scenario_CallA is void).
extern "C" void __cdecl Scena08_PartyJoin784(void) {
    Field_MemberCount = 0;
    SH_CALL(Party_Join)(7);
    SH_CALL(Party_Join)(8);
    SH_CALL(Party_Join)(4);
    SH_AT(void (__cdecl*)(), at::kPartyPalettes.v)();
}

// original 0x524870: an effect object of kind 0x34 at the cell (x, z). With
// Effect_FindFree's slot not 0xFF: +0 in use, +5 the kind, +1 the state from
// the first argument's byte, +0x34 / +0x38 the cell's words sign-extended as
// 16.16, +0x3E the ground there (AreaMap_Elevation of the two, x read back from
// the record) plus 0x100, +0xB 0. The slot is used as FindFree answers it,
// unchecked (FindFree gives 0..19 or 0xFF). The eax the original leaves is
// no caller's (the 18 cell pickups ignore it).
extern "C" void __cdecl Effect_SpawnAtCell(unsigned state, unsigned x, unsigned z) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = rv::Bytes(Effect_Objects) + slot * at::kEffectStride;
    e[0] = 1;
    e[5] = at::kCellFxKind;
    e[1] = static_cast<unsigned char>(state);
    rv::As<records::EffectObject>(e)->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<short>(x))) << 16);
    const auto zz = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<short>(z))) << 16);
    rv::As<records::EffectObject>(e)->z = zz;
    const long ground = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(e)->x, zz);
    rv::Store<unsigned>(rv::As<records::SpriteHead>(e)->height_whole, static_cast<unsigned>(ground) + 0x100);
    e[0xB] = 0;
}

void ScenaSe_Inject() {
    if (bof3::WantsShadow("scena_se")) scena_se::SelfTest();
    BOF3_INJECT(Field_StartEventBattle);
    BOF3_INJECT(Scena08_PartyJoin784);
    BOF3_INJECT(Effect_SpawnAtCell);
}
