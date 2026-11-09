// The engine callees the chapter and area groups call by raw address and
// nobody owned after group SX - round ten, group SX2, taken with the scenario
// harness (scenario_harness.h). docs/scena_sx2.md has each function, every
// caller, and what was read around them and left.
//
//   Effect_HoldFlag1C        0x469FE0  an effect of kind 4 that holds story flag 0x1C for n frames
//   Party_PlaceInFormation   0x532FD0  Sprite_Current at the formation's offset for a slot, grounded
//   AreaMap_SetHeight        0x572620  one byte of the area block's height layer
//   Flags_Toggle             0x57C160  bits[i >> 3] ^= 1 << (i & 7)
//   Camera_TurnStep          0x57C5A0  Camera_Angles[0] a step toward an angle; Light_AnglesCopy[0] at the end
//   Camera_TurnFBToDegrees   0x57C600  Camera_TurnStepFB with the angle and step in degrees
//   Camera_TurnStepFB        0x57C650  Cond_AngleFB a step toward an angle; Light_AnglesCopy[2] at the end
//   Member_SetState2_8       0x57C8A0  a party member's state 2, sub-state 8, +3 0, +0xB a value
//   Sound_StopChannels       0x587860  every Sound_Channels buffer stopped and forgotten
//   Sound_SetCueVolume       0x587890  the volume of each voice of a sound cue
//   KeyItem_Remove           0x591920  a key item out of the 32-byte list
//   AbilityList_ForType      0x591EC0  the one of a record's four ability lists an id's type files it in
//   Gpu_SetSprt16            0x5A7730  a primitive's code 0x7C (SPRT_16) and depth 0.01
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. The unchecked indexes the
// originals make (a member byte past ObjTrio's three, a formation group past
// its rows, a cue's bank 0) are reproduced (docs/scena_sx2.md section 6).
#include "game/scena_sx2.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sx2_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sx2::at;
constexpr auto& AtScenaSx2 = move_script::At;
constexpr auto& LongScenaSx2 = move_script::Long;
using move_script::SetLong;

std::uint32_t UScenaSx2(std::int32_t v) { return static_cast<std::uint32_t>(v); }

// A signed byte over 4 the way the original divides (cdq / and 3 / add / sar
// 2): C's truncating division.
std::int32_t Quarter(unsigned char b) { return static_cast<signed char>(b) / 4; }

}  // namespace

// original 0x469FE0 (PSX 0x8019BBB8): Effect_FindFree; with a record (al not 0xFF), its +0 1
// (in use), +5 4 (the kind: Effect_KindHandlers[4] = 0x469FB0 EffectKind04_HoldTick, which counts
// +9 down a frame and at 0 clears story flag 0x1C and releases the record),
// +9 the argument's low byte (the frames); then Flags_Set(0x904030, 0x1C).
// No record: nothing, the flag not set either. Areas 49, 77, 86, 112, 117,
// 118 and Area109_SwitchHook 0x4FEEB0 call it, each with 0xF; Area49_CellHook
// answers 0 while flag 0x1C is set (a switch's cool-down). PSX 0x8019BBB8
// (pairs_propagated "gap74").
extern "C" void __cdecl Effect_HoldFlag1C(unsigned frames) {
    const unsigned i = SH_CALL(Effect_FindFree)();
    if (i == 0xFF) return;
    unsigned char* const e = rv::Bytes(&Effect_Objects[i]);
    e[0] = 1;
    e[5] = 4;
    e[9] = static_cast<unsigned char>(frames);
    SH_CALL(Flags_Set)(AtScenaSx2(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1C);
}

// original 0x532FD0: Sprite_Current placed at (x, z) plus the event battle's
// formation offset for a slot, then grounded (Party_PlaceForBattle's one
// callee). The group g is the byte 0x904060, plus 1 at two members, plus 4 at
// three (bytes, wrapping; Field_MemberCount read once); the row is
// Formation_SlotVectors[g * 3 + slot] (slot the third argument's low byte;
// both unchecked), a pair of dwords (dx, dz). The formation byte f (0x904AAC)
// names a pair of s8 at BattleFormation_Offsets[2 f]: with a not 0, +0x34 =
// dx * (a / 4) + x and +0x38 = dz * (a / 4) + z; else +0x34 = x - dz * (b /
// 4) and +0x38 = dx * (b / 4) + z - the vector turned a quarter. Products
// wrap at 32 bits; f and the pair are read again for z. Then +0x3E =
// MapView_GroundAt(+0x34, +0x38)'s word (Sprite_Current read again after the
// call). No PSX twin paired.
extern "C" void __cdecl Party_PlaceInFormation(int x, int z, unsigned slot) {
    const unsigned char count = Field_MemberCount;
    const unsigned char lead = Cond_Flags[0xd0];
    unsigned char g = lead;
    if (count == 2)
        g = static_cast<unsigned char>(lead + 1);
    else if (count == 3)
        g = static_cast<unsigned char>(lead + 4);
    const unsigned char* const pairs = AtScenaSx2(::bof3::Img(bof3::addr::BattleFormation_Offsets));
    const unsigned char* const row = AtScenaSx2(::bof3::Img(bof3::addr::Formation_SlotVectors) + (g * 3u + (slot & 0xFF)) * 8);
    const unsigned f = AtScenaSx2(::bof3::Img(at::kFormation))[0];
    const unsigned char a = pairs[f * 2];
    std::uint32_t zd;
    if (a != 0) {
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(UScenaSx2(LongScenaSx2(row)) * UScenaSx2(Quarter(a)) + UScenaSx2(x)));
        const unsigned f2 = AtScenaSx2(::bof3::Img(at::kFormation))[0];
        zd = UScenaSx2(Quarter(pairs[f2 * 2])) * UScenaSx2(LongScenaSx2(row + 4));
    } else {
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(UScenaSx2(x) - UScenaSx2(Quarter(pairs[f * 2 + 1])) * UScenaSx2(LongScenaSx2(row + 4))));
        const unsigned f2 = AtScenaSx2(::bof3::Img(at::kFormation))[0];
        zd = UScenaSx2(Quarter(pairs[f2 * 2 + 1])) * UScenaSx2(LongScenaSx2(row));
    }
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, static_cast<std::int32_t>(zd + UScenaSx2(z)));
    unsigned char* const s = Sprite_Current;
    const long ground = SH_CALL(MapView_GroundAt)(rv::As<records::SpriteObject>(s)->x, rv::As<records::SpriteObject>(s)->z);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<std::uint32_t>(ground));
}

void ScenaSx2_Inject() {
    if (bof3::WantsShadow("scena_sx2")) scena_sx2::SelfTest();
    BOF3_INJECT(Effect_HoldFlag1C);
    BOF3_INJECT(Party_PlaceInFormation);
}
