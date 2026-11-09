// The party members' sprites on the field - the second and third members
// following the leader, walking and running: the member's frame (0x51AC50),
// its states 0 and 1 (0x51AC70, 0x51AD40) with state 1's two sub-states
// (0x51AD60, 0x51AFC0 and its tail 0x51AFE0), the follow step and its helpers
// (0x51B050, 0x51B430, 0x51B5D0, 0x51B9D0, 0x51BDA0), and the cell tests the
// leader shares (0x526DB0, 0x526DD0, 0x527470, 0x528070 .. 0x528770).
// docs/member-sprites.md has each function, its evidence, and the fuzz below
// with its negative controls.
//
// "Member i" is ObjTrio + i * 0x14C. The "cells" are the bytes at 0x903850:
// cell 0 is the flag 0x5725C0 leaves (an object ahead), cells 1..5 what
// Field_ReadCells found around the sprite, cells 8..10 Field_CellAheadFlat's
// classes - and cells 8..0xF double as the two longs of the aim point.
//
// As in the original, every global is read again after each call and every
// object through Sprite_Current or Field_State each time it is used.
#include "game/member_sprites.h"

#include <cstddef>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/member_sprites_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace member_sprites {

namespace {
template <class To> To Fn(std::uint32_t address) { return reinterpret_cast<To>(static_cast<std::uintptr_t>(address)); }
}  // namespace

// Every callee. Those this module implements are ours (their names are bound
// to our functions by symbols.gen.h); the unnamed ones are other groups' or
// nobody's, Capcom's addresses typed from their call sites and, where read,
// their first instructions (docs/member-sprites.md, section 3).
const Callees kOriginals = {
    Sprite_RestoreClut, reinterpret_cast<const std::uint32_t*>(::bof3::Img(Member_States_addr)),
    Sprite_ClearSteps, MapView_GroundAt, Sprite_SetAnimation,
    reinterpret_cast<const std::uint32_t*>(::bof3::Img(at::kControlStates.v)), Sprite_ScriptTick,
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Field_Tile89)), Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Field_Tile8A)),
    Sprite_EnsureAnimation, Member_Idle, Member_FollowStep,
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderPushObjects)), Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Field_JumpSetUp)), Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Field_JumpCheckHeight)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderStepTick)),
    Member_WalkEnd,
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Field_EquipTick)), Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Field_Bit20Tick)), Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Field_Bit80Tick)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Field_FloorDamage)), Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_TileD0)),
    Member_Follow,
    Member_CatchUp, Field_DirectionTo, Member_StepAhead,
    Fn<unsigned char (__cdecl*)(long, long, unsigned, long)>(::bof3::Code(bof3::addr::Field_WayBlocked)),
    Field_CellAhead, Fn<long (__cdecl*)(long, long, unsigned)>(::bof3::Code(bof3::addr::MapView_SlopeAt)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::AreaMap_CellNibble)),
    Rand,
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_CellAheadRaised)), Field_CellAheadFlat,
    Field_ReadCells, Field_CellClass, Field_TurnUnless, Field_CellSlope, Field_CellPairTurn,
    Field_CellKind,
    AreaMap_ByteAt, Field_CellFacing, Field_ObjectAhead,
};
Callees g = kOriginals;

}  // namespace member_sprites

namespace {

using namespace move_script;
using member_sprites::g;
namespace at = member_sprites::at;

std::uint32_t Address(const volatile void* p) { return Low32(p); }

// The member records' views (game/record_views.h): Sprite_Current
// and Field_State are ObjTrio records here.
unsigned char& Cell(unsigned i) { return move_script::At(::bof3::Img(member_sprites::at::kCells.v))[i & 0xFF]; }

}  // namespace

// --- the member's frame and its states ----------------------------------------

// --- following the leader -------------------------------------------------------

// --- the cell ahead ----------------------------------------------------------------

void MemberSprites_Inject() {
    if (bof3::WantsShadow("member_sprites")) member_sprites::SelfTest();
}
