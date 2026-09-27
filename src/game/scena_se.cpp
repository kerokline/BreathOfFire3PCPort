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
// MoveScript_EffectState's 24) are reproduced, as event_script.cpp's
// placements and field_event.cpp's Party_Join reproduce them (latent
// defects, docs/scena_se.md section 6).
#include "game/scena_se.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
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

// The placement count (DamageScratch, read afresh at every use as the
// original reads the word [0x903850]) and the bank word after it.
short Count() { return static_cast<short>(Word(At(at::kCount))); }
unsigned char* Object(int n) { return Sprite_Objects + n * static_cast<int>(at::kObjectStride); }

// A placement's x or z: the whole part from `whole`, the half (0x8000) when
// `half` is not 0 - `neg / sbb / and 0x8000`, then `or` the whole << 16.
std::uint32_t Coordinate(unsigned char half, unsigned char whole) {
    return (half ? 0x8000u : 0u) | static_cast<std::uint32_t>(whole) << 16;
}

// A character's record +0xB (CharacterRecords, stride 0xA4) through
// MoveScript_EffectState[id] - both indexes a whole byte, unchecked.
unsigned char* RecordFlags(unsigned char id) {
    return At(bof3::addr::CharacterRecords + 0xB + MoveScript_EffectState[id] * at::kObjectStride);
}

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
    At(at::kScriptFlags2High)[0] = static_cast<unsigned char>(At(at::kScriptFlags2High)[0] | 0x10);
    At(at::kEventBattle)[0] = id;
    unsigned char* const leader = At(at::kLeader);
    leader[1] = 5;
    const unsigned char flags = At(bof3::addr::EventBattle_Records)[id * 4u];
    leader[2] = 0;
    leader[3] = 0;
    leader[4] = 0;
    At(at::kBattleFlags)[0] = flags;
    At(at::kBattleIntro)[0] = 0;
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
    SH_AT(void (__cdecl*)(), at::kPartyPalettes)();
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
    unsigned char* const e = Effect_Objects + slot * at::kEffectStride;
    e[0] = 1;
    e[5] = at::kCellFxKind;
    e[1] = static_cast<unsigned char>(state);
    SetLong(e + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<short>(x))) << 16));
    const auto zz = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<short>(z))) << 16);
    SetLong(e + 0x38, zz);
    const long ground = SH_CALL(AreaMap_Elevation)(Long(e + 0x34), zz);
    SetWord(e + 0x3E, static_cast<unsigned>(ground) + 0x100);
    e[0xB] = 0;
}

// original 0x579D70: the pose or the facing at the end of a placement (PSX
// FUN_801A591C). Sprite_Current +0x4B = 0xFF; with +7 bit 3, +0x2A = bit 4 of
// +7 and Sprite_SetAnimation(+8 of Sprite_Current read again), else
// Sprite_FaceDirection(+8 of the object read the second time).
extern "C" void __cdecl EventObj_Face(void) {
    Sprite_Current[0x4B] = 0xFF;
    unsigned char* const c = Sprite_Current;
    const unsigned char flags = c[7];
    if (flags & 8) {
        c[0x2A] = static_cast<unsigned char>((flags >> 4) & 1);
        SH_CALL(Sprite_SetAnimation)(Sprite_Current[8]);
        return;
    }
    SH_CALL(Sprite_FaceDirection)(c[8]);
}

// original 0x57A010: event op 0x and Fx, 17 bytes (PSX FUN_801A5D5C) - the
// jump table's default in EventScript_Op, and called directly by 19 sites
// (three of them chapter code). EventOp_1x's placement (event_script.cpp)
// with object +0x83 = op[0x10]: Sprite_Objects[count] becomes
// Sprite_Current and Field_ActiveMember, the bank word op[1] op[2], its
// fields and its script context's from the op, EventObj_SetFlags(op + 0xD),
// EventObj_Face, the count one on. Nothing when the count is 30 or more; a
// negative count (signed, unchecked) places before the table, as the
// original does. The count is read again after each call, Sprite_Current at
// every use, the op's bytes where the original reads them.
extern "C" void __cdecl EventOp_0x(const unsigned char* op) {
    if (Count() >= static_cast<short>(at::kObjectCount)) return;
    unsigned char* const o = Object(Count());
    Sprite_Current = o;
    Field_ActiveMember = o;
    SetWord(At(at::kBank), static_cast<unsigned>(op[1]) << 8 | op[2]);
    SH_CALL(EventObj_Reset)();
    SH_CALL(Sprite_SetAnimationBank)(Word(At(at::kBank)));
    Sprite_Current[0] = 1;
    SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(Coordinate(op[6], op[7])));
    SetLong(Object(Count()) + 0x8C, Long(Sprite_Current + 0x34));
    SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(Coordinate(op[8], op[9])));
    SetLong(Object(Count()) + 0x90, Long(Sprite_Current + 0x38));
    SetLong(Object(Count()) + 0x94, 0);
    const long ground = SH_CALL(AreaMap_Elevation)(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground));
    Sprite_Current[1] = op[0xA];
    Sprite_Current[2] = op[0xC];
    Sprite_Current[8] = static_cast<unsigned char>(op[0] & 0xF);
    Sprite_Current[6] = static_cast<unsigned char>(op[0] >> 4);
    unsigned char* const n = Object(Count());
    SetWord(n + 0x98, op[4]);
    SetWord(n + 0x9A, op[5]);
    SetWord(n + 0x88, static_cast<unsigned>(op[0xE]) << 8 | op[0xF]);
    n[0x83] = op[0x10];
    n[0x84] = op[0xB];
    n[0xA0] = 0x7F;
    SH_CALL(EventObj_SetFlags)(op + 0xD);
    if (op[3] & 0x80) Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x20);
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5C] = static_cast<unsigned char>(op[3] & 0xF);
    SH_CALL(EventObj_Face)();
    SetWord(At(at::kCount), static_cast<unsigned>(Word(At(at::kCount)) + 1));
}

// original 0x591CC0: a member into the two party lists without a sprite
// (Party_Join's list half). The member's character record +0xB |= 3 (joined,
// and bit 1); below three members, the next slot of both lists and the count
// one on, al 1. With three or more: the first slot i of the first list whose
// member's record +0xB lacks bit 1 and whose id stands in the second list
// (slot j) takes the member in both, al 1; none, al 0. Its one caller
// (chapter 6, 0x54D053) does not read al.
extern "C" unsigned char __cdecl Party_AddToLists(unsigned member_arg) {
    const unsigned char member = static_cast<unsigned char>(member_arg);
    unsigned char* const flags = RecordFlags(member);
    *flags = static_cast<unsigned char>(*flags | 3);
    unsigned char* const lists = At(at::kPartyLists);
    const unsigned char n = Field_MemberCount;
    if (n < 3) {
        Field_MemberCount = static_cast<unsigned char>(n + 1);
        lists[n] = member;
        lists[n + 3] = member;
        return 1;
    }
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char id = lists[i];
        if (*RecordFlags(id) & 2) continue;
        for (unsigned j = 0; j < 3; ++j) {
            if (id != lists[3 + j]) continue;
            lists[i] = member;
            lists[3 + j] = member;
            return 1;
        }
    }
    return 0;
}

void ScenaSe_Inject() {
    if (bof3::WantsShadow("scena_se")) scena_se::SelfTest();
    BOF3_INJECT(Field_StartEventBattle);
    BOF3_INJECT(Scena08_PartyJoin784);
    BOF3_INJECT(Effect_SpawnAtCell);
    BOF3_INJECT(EventObj_Face);
    BOF3_INJECT(EventOp_0x);
    BOF3_INJECT(Party_AddToLists);
}
