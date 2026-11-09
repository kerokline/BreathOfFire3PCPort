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

}  // namespace

void ScenaSx2_Inject() {
    if (bof3::WantsShadow("scena_sx2")) scena_sx2::SelfTest();
}
