// Moved here by tools/move_funcs.py from area_w1e.cpp (docs/source-layout-round.md).
#include "game/area/area_67.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1e::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* Cur() { return Sprite_Current; }

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

// The message a choice handler opens: table[(s8) choice], unchecked.
signed char Choice() { return AreaW0c_ChoiceAnswer; }

void ChoiceMessage(U table, signed char choice) { SetWord(At(::bof3::Img(at::kMessage.v)), Word(At(table + static_cast<U>(choice * 2)))); }

// Handlers 3..5: party member k's record (ObjTrio + k * 0x14C) becomes
// Sprite_Current, its +0x34 / +0x38 / +0x3C (read first) the position of
// Effect_SpawnAt(3, 0, Area67_SpawnAtArg[the member's character id]); an
// answer other than 0xFF goes to Sprite_Current +0xB (read again).
void MemberSpawnAt(unsigned k) {
    unsigned char* const member = At(::bof3::Img(ObjTrio_addr) + k * at::kPartyStride);
    auto* const tMember = rv::As<records::PartyMember>(member);
    const std::int32_t x = tMember->x, y = tMember->z, z = tMember->height;
    const unsigned id = B(::bof3::Img(at::kPartyList.v) + k);
    Sprite_Current = member;
    const auto arg = static_cast<signed char>(B(::bof3::Img(at::kA67SpawnAtArg.v) + id));
    const unsigned char r = AH_CALL(Effect_SpawnAt)(3, 0, arg, x, y, z);
    if (r != 0xFF) Cur()[0xB] = r;
}

// Handlers 6..11 and 16: the same with Effect_Spawn(kind, 0, table[id], the
// member's words +0x2E / +0x30).
void MemberSpawn(unsigned k, unsigned char kind, U table) {
    unsigned char* const member = At(::bof3::Img(ObjTrio_addr) + k * at::kPartyStride);
    auto* const tMember = rv::As<records::PartyMember>(member);
    const auto x = static_cast<short>(static_cast<std::uint16_t>(tMember->screen_x));
    const auto z = static_cast<short>(static_cast<std::uint16_t>(tMember->screen_y));
    const unsigned id = B(::bof3::Img(at::kPartyList.v) + k);
    Sprite_Current = member;
    const auto arg = static_cast<signed char>(B(table + id));
    const unsigned char r = AH_CALL(Effect_Spawn)(kind, 0, arg, x, z);
    if (r != 0xFF) Cur()[0xB] = r;
}

}  // namespace

// original 0x40CA30 (area 67's choice 0, the descriptor 0x607000's +0x34 =
// 0x606FF4; its PSX descriptor 0x801F5340): message = Area67_MessagesA
// 0x607044 by the s8 choice (unchecked); choice 5 also sets the byte 0x903848
// to 6.
extern "C" void __cdecl Area67_ChoiceMark6(void) {
    const signed char choice = Choice();
    ChoiceMessage(::bof3::Img(Area67_MessagesA_addr), choice);
    if (choice == 5) B(::bof3::Img(at::kScratch48.v)) = 6;
}

// original 0x40CA60 (area 67's choice 1): message = Area67_MessagesB 0x607050
// by the s8 choice (unchecked).
extern "C" void __cdecl Area67_ChoiceMessage(void) { ChoiceMessage(::bof3::Img(Area67_MessagesB_addr), Choice()); }

// original 0x40CA80 (area 67's choice 2): message = Area67_MessagesC 0x60705C
// by the s8 choice (unchecked); choices 0 and 1 (signed) also set the byte
// 0x903848 to 5.
extern "C" void __cdecl Area67_ChoiceMark5(void) {
    const signed char choice = Choice();
    ChoiceMessage(::bof3::Img(Area67_MessagesC_addr), choice);
    if (choice >= 0 && choice <= 1) B(::bof3::Img(at::kScratch48.v)) = 5;
}

// original 0x40CAB0 (area 67's handler 0, also area 108's; PSX 0x801F3EC4):
// Sprite_Current +0x24 |= 0x10.
extern "C" void __cdecl Area67_SetBit24(void) { Cur()[0x24] = static_cast<unsigned char>(Cur()[0x24] | 0x10); }

// original 0x40CAC0 (handler 1, also area 131's; PSX 0x801F3EE4): field object
// 0's +0x34 (Sprite_Objects 0x7DEEB4, 16.16) += 0x800.
extern "C" void __cdecl Area67_Object0XUp(void) { AddLong(At(::bof3::Img(Sprite_Objects_addr) + 0x34), 0x800); }

// original 0x40CAD0 (handler 2; PSX 0x801F3F00): Effect_FindFree; a slot: its
// record +0 = 1, kind +5 = 0x2D, +0x34 / +0x38 = the running object's
// (Sprite_Current read for each).
extern "C" void __cdecl Area67_SpawnEffect2D(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x2D;
    tE->x = rv::Current<records::SpriteObject>()->x;
    tE->z = rv::Current<records::SpriteObject>()->z;
}

// original 0x40CB10 / 0x40CB60 / 0x40CBB0 (handlers 3, 4, 5; PSX 0x801F3F80,
// 0x801F400C, 0x801F4098): MemberSpawnAt for members 0, 1, 2 - the character
// id's byte of Area67_SpawnAtArg 0x604BB8 (unchecked).
extern "C" void __cdecl Area67_Member0SpawnAt(void) { MemberSpawnAt(0); }

extern "C" void __cdecl Area67_Member1SpawnAt(void) { MemberSpawnAt(1); }

extern "C" void __cdecl Area67_Member2SpawnAt(void) { MemberSpawnAt(2); }

// original 0x40CC00 / 0x40CC50 / 0x40CCA0 (handlers 6, 7, 8; PSX 0x801F4124,
// 0x801F41A4, 0x801F4224): Effect_Spawn kind 3 at members 0, 1, 2 with the
// same table.
extern "C" void __cdecl Area67_Member0Spawn3(void) { MemberSpawn(0, 3, ::bof3::Img(Area67_SpawnAtArg_addr)); }

extern "C" void __cdecl Area67_Member1Spawn3(void) { MemberSpawn(1, 3, ::bof3::Img(Area67_SpawnAtArg_addr)); }

extern "C" void __cdecl Area67_Member2Spawn3(void) { MemberSpawn(2, 3, ::bof3::Img(Area67_SpawnAtArg_addr)); }

// original 0x40CCF0 / 0x40CD40 / 0x40CD90 (handlers 9, 10, 11; PSX 0x801F42A4,
// 0x801F4324, 0x801F43A4): Effect_Spawn kind 1 at members 0, 1, 2, their byte
// of Area67_SpawnArgB 0x604BC0 (unchecked).
extern "C" void __cdecl Area67_Member0Spawn1(void) { MemberSpawn(0, 1, ::bof3::Img(Area67_SpawnArgB_addr)); }

extern "C" void __cdecl Area67_Member1Spawn1(void) { MemberSpawn(1, 1, ::bof3::Img(Area67_SpawnArgB_addr)); }

extern "C" void __cdecl Area67_Member2Spawn1(void) { MemberSpawn(2, 1, ::bof3::Img(Area67_SpawnArgB_addr)); }

// original 0x40CDE0 (handler 12, also area 131's; PSX 0x801F4424): field
// object 0's +0x34 -= 0x800.
extern "C" void __cdecl Area67_Object0XDown(void) { AddLong(At(::bof3::Img(Sprite_Objects_addr) + 0x34), static_cast<U>(-0x800)); }

// original 0x40CDF0 (handler 13; PSX 0x801F4440): field object 1's +0x34
// (0x7DEF58) += 0x800.
extern "C" void __cdecl Area67_Object1XUp(void) { AddLong(At(::bof3::Img(Sprite_Objects_addr) + 0xd8), 0x800); }

// original 0x40CE00 (handler 14; PSX 0x801F445C): field object 1's +0x34 -=
// 0x800.
extern "C" void __cdecl Area67_Object1XDown(void) { AddLong(At(::bof3::Img(Sprite_Objects_addr) + 0xd8), static_cast<U>(-0x800)); }

// original 0x40CE10 (handler 15, also handlers of areas 7, 19 and 130 - one
// body, in area 67's block; PSX 0x801F4478): Music_FadeOutStop(10).
extern "C" void __cdecl Area67_MusicFade10(void) { AH_CALL(Music_FadeOutStop)(10); }

// original 0x40CE20 (handler 16; PSX 0x801F449C): Effect_Spawn kind 4 at the
// leader, the leader's byte of Area67_SpawnArgC 0x604BC8 (unchecked).
extern "C" void __cdecl Area67_LeaderSpawn4(void) { MemberSpawn(0, 4, ::bof3::Img(Area67_SpawnArgC_addr)); }

// original 0x40CE70 (handler 17; PSX 0x801F451C): Field_ActiveMember +0x80
// bit 0 cleared (the byte read before the test); when the leader's +0x89
// (Character_IndexById's index) is 6, then ScriptFlags_Set40,
// MoveScript_Var7 = 4, the four bytes 0x903848..0x90384B and 0x8034E5 zeroed.
extern "C" void __cdecl Area67_ResetOn6(void) {
    const unsigned char effect = ObjTrio[0].char_id;
    unsigned char* const member = Field_ActiveMember;
    const auto cleared = static_cast<unsigned char>(member[0x80] & 0xFE);
    member[0x80] = cleared;
    if (effect != 6) return;
    AH_CALL(ScriptFlags_Set40)();
    MoveScript_Var7 = 4;
    B(::bof3::Img(at::kScratch48.v)) = 0;
    B(::bof3::Img(at::kScratch48.v) + 1) = 0;
    B(::bof3::Img(at::kScratch48.v) + 2) = 0;
    B(::bof3::Img(at::kScratch48.v) + 3) = 0;
    MoveScript_PartyRecords[0x65] = 0;
}

// original 0x40CEC0 (handler 18; PSX 0x801F45BC): AreaMap_SetByte(0x25, 0xB,
// 0) - the map cell (0x25, 0xB) cleared.
extern "C" void __cdecl Area67_ClearCell(void) { AH_CALL(AreaMap_SetByte)(0x25, 0xB, 0); }

// original 0x40CED0 (Field_ObjectTriggers id 31, 0x662E98, called (object,
// 0x904030) and reading neither; a gap function by the tool, placed in area
// 67's block by address): ScriptFlags_Set40, then the mode tail armed - kind
// 0x2C (the engine's 0x56DE50), state 0, argument 0x9039F5 = 1. al 0.
extern "C" unsigned char __cdecl Area67_Trigger31(void) {
    AH_CALL(ScriptFlags_Set40)();
    AreaW0a_TailKind = 0x2C;
    AreaW0c_TailState = 0;
    AreaW0c_TailSub = 1;
    return 0;
}

void Area67_Inject() {
    BOF3_INJECT(Area67_ChoiceMark6);
    BOF3_INJECT(Area67_ChoiceMessage);
    BOF3_INJECT(Area67_ChoiceMark5);
    BOF3_INJECT(Area67_SetBit24);
    BOF3_INJECT(Area67_Object0XUp);
    BOF3_INJECT(Area67_SpawnEffect2D);
    BOF3_INJECT(Area67_Member0SpawnAt);
    BOF3_INJECT(Area67_Member1SpawnAt);
    BOF3_INJECT(Area67_Member2SpawnAt);
    BOF3_INJECT(Area67_Member0Spawn3);
    BOF3_INJECT(Area67_Member1Spawn3);
    BOF3_INJECT(Area67_Member2Spawn3);
    BOF3_INJECT(Area67_Member0Spawn1);
    BOF3_INJECT(Area67_Member1Spawn1);
    BOF3_INJECT(Area67_Member2Spawn1);
    BOF3_INJECT(Area67_Object0XDown);
    BOF3_INJECT(Area67_Object1XUp);
    BOF3_INJECT(Area67_Object1XDown);
    BOF3_INJECT(Area67_MusicFade10);
    BOF3_INJECT(Area67_LeaderSpawn4);
    BOF3_INJECT(Area67_ResetOn6);
    BOF3_INJECT(Area67_ClearCell);
    BOF3_INJECT(Area67_Trigger31);
}
