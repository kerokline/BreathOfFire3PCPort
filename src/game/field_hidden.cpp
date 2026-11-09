// Group DE of the eighth round (docs/field_hidden.md): fifteen field-side
// functions, each read to its last instruction with capstone against
// bof3/BOF3.exe. Faithful: no divergence. The start-up fuzz is
// field_hidden_fuzz.cpp.
//
//   Member_ResumeUnlessHeld     0x51BA60..0x51BA73 (0x14)   Member_States entry 3
//   Member_EffectState          0x51BBD0..0x51BCE4 (0x115)  Member_States entry 7
//   PartyAction_Finish          0x51DA30..0x51DA95 (0x66)   entry 2 of every form-0 state table
//   PartyAction5_Form0          0x51E910..0x51E921 (0x12)   PartyAction5_Forms entry 0
//   PartyAction5_Form0Begin     0x51E930..0x51EAE0 (0x1B1)  PartyAction5_Form0States entry 0
//   PartyAction5_Form0Resolve   0x51EAF0..0x51EBCA (0xDB)   PartyAction5_Form0States entry 1
//   Field_CellPickup            0x51EBD0..0x51ECEE (0x11F)
//   PartyAction5_ByForm         0x51F1B0..0x51F1C2 (0x13)   entry 5 of 0x6609D0 (DC's 0x52FB60)
//   Sprite_PoseFromSet          0x589110..0x589154 (0x45)
//   Sprite_CopyFrames           0x589160..0x5891B8 (0x59)
//   Sprite_AnimFromSet          0x5891C0..0x5891EC (0x2D)
//   EventOp_Ex                  0x5898D0..0x58996E (0x9F)
//   Encounter_Place             0x591F30..0x5920DA (0x1AB)
//   Encounter_PickRow           0x592570..0x59259A (0x2B)
//   Encounter_FillSlots         0x5925A0..0x5925FC (0x5D)
//
// Every call goes through field_hidden::g (field_hidden_callees.h), so that
// the start-up fuzz can stand recorders in for them.
#include "game/field_hidden.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/field_hidden_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_hidden {

using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Fn<Byte0>(::bof3::Code(bof3::addr::Sprite_ScriptTick)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Sprite_ScriptTickOnce)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Effect_FindFree)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sprite_SetAnimation)),
    Fn<std::uint32_t (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sprite_EnsureAnimation)),
    Fn<Byte0>(::bof3::Code(kTargetAhead.v)),
    Fn<long (__cdecl*)(long, long, unsigned long)>(::bof3::Code(bof3::addr::MapView_SlopeAt)),
    Fn<long (__cdecl*)(long, long)>(::bof3::Code(bof3::addr::MapView_GroundAt)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sound_PlayEffect)),
    Fn<std::uint32_t (__cdecl*)(long, long, unsigned)>(::bof3::Code(bof3::addr::Sprite_ObjectAt)),
    Fn<std::uint32_t (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Field_CellPickup)),
    Fn<std::uint32_t (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::AreaMap_ByteAt)),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(kSpawnAtCell.v)),
    Rand,   // the CRT's rand, ours since crt_rest
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kFoundZenny.v)),
    Fn<unsigned char* (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Item_NamePtr)),
    Fn<std::uint32_t (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(bof3::addr::Inventory_Add)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Msg_OpenSystem)),
    Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kClearCell.v)),
    Fn<unsigned char* (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sprite_SetFrameQueueUpload)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sprite_ScriptStart)),
    Fn<void (__cdecl*)(unsigned, unsigned char*, unsigned)>(::bof3::Code(bof3::addr::Sprite_CopyFrames)),
    Fn<long (__cdecl*)(long, long)>(::bof3::Code(bof3::addr::AreaMap_Elevation)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Encounter_PickRow)),
    Fn<std::uint32_t (__cdecl*)(const unsigned char*)>(::bof3::Code(bof3::addr::Encounter_FillSlots)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Encounter_PlaceParty)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Encounter_PlaceEnemies)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Encounter_EnemiesReachable)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Gte_PushMatrix)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Gte_PopMatrix)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Encounter_AimCamera)),
    Fn<std::uint32_t (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Party_Count)),
    Fn<std::uint32_t (__cdecl*)(long, long, unsigned, unsigned)>(::bof3::Code(bof3::addr::Encounter_OnScreen)),
};
Callees g = kOriginals;

}  // namespace field_hidden

using namespace field_hidden;

namespace {

unsigned char Al(std::uint32_t eax) { return static_cast<unsigned char>(eax); }
unsigned char* Effect(unsigned index) { return rv::Bytes(Effect_Objects) + (index << 7); }

}  // namespace

#define FH_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// Two party-member states (Member_States 0x65F960, Field_MemberFrame's)
// ===========================================================================

// ===========================================================================
// The party actions (0x6609D0 by party set -> a forms table by u16 +0x2C ->
// a states table by +2)
// ===========================================================================

// ===========================================================================
// A pose from a set of frames
// ===========================================================================

// ===========================================================================
// The event op Ex
// ===========================================================================

// ===========================================================================
// The encounter placement's driver
// ===========================================================================

// ===========================================================================

void FieldHidden_Inject() {
    if (bof3::WantsShadow("field_hidden")) field_hidden::SelfTest();
}
