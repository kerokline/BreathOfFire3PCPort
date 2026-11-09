// The event script's field side: the party leader's frame (0x52D8F0) and its
// first two states (0x52D920, 0x52DA50 with its sub-state 0x52DA70), the area
// set-up's party placement (0x533110, 0x5334A0, 0x533580, 0x533BA0, 0x533CE0),
// a member joining (0x533EF0), the party-set loader (0x5367E0..0x536AC0), and
// the small helpers they share. docs/field-event.md has each function, its
// evidence, and the fuzz below with its negative controls.
//
// "Member i" is ObjTrio + i * 0x14C, the party's three field objects; their
// +0x80..+0x123 is a copy of an actor record 0x903A70 + a * 0xA4, a the
// member's Character_IndexById entry (kept in the member's +0x148).
//
// As in the original, every global is read again after each call and every
// object through Sprite_Current or Field_State each time it is used.
#include "game/field_event.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_event_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_event {

namespace {
// A function pointer as another function type, through an integer.
template <class To> To Fn(std::uint32_t address) { return reinterpret_cast<To>(static_cast<std::uintptr_t>(address)); }
}  // namespace

// Every callee. Those this module implements are ours (their names are bound
// to our functions by symbols.gen.h); the unnamed ones are Capcom's addresses,
// typed from their call sites and, where read, their first instructions
// (docs/field-event.md, section 3).
const Callees kOriginals = {
    Sprite_RestoreClut, Field_CopyInput, reinterpret_cast<const std::uint32_t*>(::bof3::Img(Field_LeaderStates_addr)),
    Sprite_ClearSteps, MapView_GroundAt, Field_LeaderAnimation,
    reinterpret_cast<const std::uint32_t*>(::bof3::Img(Field_LeaderControlSteps_addr)), Sprite_ScriptTick,
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Field_Tile89)), Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Field_Tile8A)),
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderIdleTest)),
    {Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderCellEvent)), Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderTalkTest)),
     Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderSwapTest)), Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderMenuTest)),
     Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderCheckTest)), Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderRequest4Test)),
     Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderRequest9Test))},
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderDirection)),
    Area_ZoneAt, Rand, Party_Count, Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(bof3::addr::Actor_EquipCount)),
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sprite_EnsureAnimation)),
    Flags_Test, Sprite_SetAnimationBank, Field_MemberSprite, Field_PartyLoad, Sprite_ReleaseTint, Sprite_LoadPalette,
    Field_PartyPosition, Fn<void (__cdecl*)(long, long, unsigned)>(::bof3::Code(bof3::addr::Field_LeaderPlaceOffset)),
    Party_ClearActive, Field_LeaderFrame, Field_MemberFrame,
    reinterpret_cast<const std::uint32_t*>(::bof3::Img(Field_PendingJumps_addr)),
    Party_ClearAll, Sprite_SetTint, Field_MemberTimers,
    Party_JoinReset, Member_ClearState,
    PartySet_LoadFirst, PartySet_LoadSecond, File_LoadDone, Task_Sleep, PartySet_Find, PartySet_Select,
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::PartySet_ErrorLoop)), LoadDatFile,
    MapView_CheckHeightScale, AreaMap_Elevation,
    Tint_Release,
};
Callees g = kOriginals;

}  // namespace field_event

namespace {

using namespace move_script;
namespace at = field_event::at;

std::uint32_t Address(const volatile void* p) { return Low32(p); }
unsigned char* Member(unsigned i) { return rv::Bytes(&ObjTrio[i]); }

}  // namespace

// --- the party leader's frame and its states --------------------------------

// --- the zone counter ------------------------------------------------------

// DIVERGENCE DIV-0153: 1 through PatchBytes("MantleCap") in
// FieldEvent_Inject, after the self-test. Accessory 0x15 (the Holy Mantle)
// doubles the threshold in a byte; zone 1's 120..151 wraps to 0..46. With
// the flag the doubling stops at 0xFF.
namespace field_event { std::uint8_t g_mantle_cap = 0; }

// --- small helpers -----------------------------------------------------------

// original 0x572590 (PSX 0x8015598C): MapView_HeightScale = 1 for the object
// at 0x905DA0 with its +0xB bit 0, or for any other Sprite_Current when
// Field_State +0x138 bit 0 is set; otherwise left as it is.
extern "C" void __cdecl MapView_CheckHeightScale(void) {
    unsigned char* const c = Sprite_Current;
    if (c == At(::bof3::Img(FieldEvent_Object905DA0_addr))) {
        if (c[0xB] & 1) MapView_HeightScale = 1;
        return;
    }
    if (Field_State[0x138] & 1) MapView_HeightScale = 1;
}

// original 0x572570 (PSX 0x801559E4): the ground at (x, z), with the height
// scale set for the call and cleared after it.
extern "C" long __cdecl MapView_GroundAt(long x, long z) {
    field_event::g.height_check();
    const long ground = field_event::g.elevation(x, z);
    MapView_HeightScale = 0;
    return ground;
}

// --- the party's placement ---------------------------------------------------

// --- the party set -------------------------------------------------------------

void FieldEvent_Inject() {
    if (bof3::WantsShadow("field_event")) field_event::SelfTest();
    {
        // DIV-0153: after the self-test, which compared Capcom's roll.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("MantleCap", Low32(&field_event::g_mantle_cap), &was, &is, 1);
        bof3::Log("DIV-0153    the Holy Mantle's doubled encounter threshold stops at 255 instead of wrapping (on unless the line above says OFF)");
    }
    BOF3_INJECT(MapView_GroundAt);
    BOF3_INJECT(MapView_CheckHeightScale);
}
