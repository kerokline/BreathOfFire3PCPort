// Moved here by tools/move_funcs.py from area_w3e.cpp (docs/source-layout-round.md).
#include "game/area/area_136.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3e::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(::bof3::Img(Chapter_FlagRow_addr)); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// Area 136's handlers 2..4, 7 and 8: Sprite_Current = `record`; an effect of
// kind `kind`, a 0, b the table byte for the character in party slot `slot`,
// at the record's words +0x2E / +0x30; a slot answered: Sprite_Current's +0xB.
void SpawnByCharacter(U record, U list, U table, unsigned char kind) {
    unsigned char* const r = Mem(record);
    const auto x = static_cast<short>(static_cast<std::uint16_t>(rv::As<records::SpriteHead>(r)->screen_x));
    const auto z = static_cast<short>(static_cast<std::uint16_t>(rv::As<records::SpriteHead>(r)->screen_y));
    const unsigned char who = B(list);
    Sprite_Current = r;
    const auto b = static_cast<signed char>(B(table + who));
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, b, x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// Area 136's handlers 1 and 5: map cells (9, 0x59..0x5B) set to `value`.
void CellsRow59(unsigned value) {
    AH_CALL(AreaMap_SetByte)(9, 0x59, value);
    AH_CALL(AreaMap_SetByte)(9, 0x5A, value);
    AH_CALL(AreaMap_SetByte)(9, 0x5B, value);
}

// Area 136's choices 0 and 1: the answer read, then the message word 0xFFFF;
// answer 0: ScriptFlags_Set40, tail kind 19 at state 0 with sub-kind `sub`.
void ChoiceTail19(unsigned char sub) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x13;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = sub;
}

// Area 136's choices 2 and 3: the message word from `table` by the signed
// answer (two words; an answer past them reads on in .data, unchecked);
// answer 0: counter 0 = 6.
void ChoiceMessage(U table) {
    const auto answer = static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)));
    SetWord(Mem(::bof3::Img(at::kMessage.v)), Word(Mem(static_cast<U>(table + static_cast<std::int32_t>(answer) * 2))));
    if (answer == 0) B(::bof3::Img(at::kCounter0.v)) = 6;
}

}  // namespace

// original 0x41EFE0 (area 136 +0x34[0]): ChoiceTail19, sub-kind 0.
extern "C" void __cdecl Area136_ChoiceTail19Sub0(void) { ChoiceTail19(0); }

// original 0x41F010 (area 136 +0x34[1]): ChoiceTail19, sub-kind 1.
extern "C" void __cdecl Area136_ChoiceTail19Sub1(void) { ChoiceTail19(1); }

// original 0x41F040 (area 136 +0x34[2]): ChoiceMessage, Area136_ChoiceMessages2.
extern "C" void __cdecl Area136_ChoiceMessage2(void) { ChoiceMessage(::bof3::Img(Area136_ChoiceMessages2_addr)); }

// original 0x41F070 (area 136 +0x34[3]): ChoiceMessage, Area136_ChoiceMessages3.
extern "C" void __cdecl Area136_ChoiceMessage3(void) { ChoiceMessage(::bof3::Img(Area136_ChoiceMessages3_addr)); }

// original 0x41F0A0 (area 136 +0x34[4] = +0x3C[0]; PSX 0x801F2D68): the z one
// step ahead of Sprite_Current (+0x38 + Field_DirectionSteps[+8 & 7]'s z,
// dwords, wrapping) at most Area136_ZLimits[sub-kind] << 16 (unsigned): the
// script object's +7 = 1, MoveCmd_Move(it, 5), the script position - 2
// (the object read again after the call).
extern "C" void __cdecl Area136_MoveUntilLimit(void) {
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    const U step = static_cast<U>(Long(Mem(::bof3::Img(Field_DirectionSteps_addr) + (cur[8] & 7u) * 8 + 4)));
    const U ahead = step + static_cast<U>(tCur->z);
    const U limit = static_cast<U>(B(::bof3::Img(Area136_ZLimits_addr) + B(::bof3::Img(AreaW0c_TailSub_addr)))) << 16;
    if (ahead > limit) return;
    MoveScript_Object[7] = 1;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, 5);
    ScriptStep(-2);
}

// original 0x41F100 (area 136 +0x34[5] = +0x3C[1]; PSX 0x801F2E08): cells
// (9, 0x59..0x5B) 0x51.
extern "C" void __cdecl Area136_CellsRow51(void) { CellsRow59(0x51); }

// original 0x41F130 (area 136 +0x34[6] = +0x3C[2]; PSX 0x801F2E50): party
// record 1, effect kind 2 by Area136_EffectByCharA[slot 1's character].
extern "C" void __cdecl Area136_SpawnMember1Kind2(void) {
    SpawnByCharacter(::bof3::Img(ObjTrio_addr) + at::kPartyStride, ::bof3::Img(Cond_Flags_addr) + 0xd3, ::bof3::Img(Area136_EffectByCharA_addr), 2);
}

// original 0x41F180 (area 136 +0x34[7] = +0x3C[3]; PSX 0x801F2ED0): party
// record 2, effect kind 2 by Area136_EffectByCharA[slot 2's character].
extern "C" void __cdecl Area136_SpawnMember2Kind2(void) {
    SpawnByCharacter(::bof3::Img(ObjTrio_addr) + 2 * at::kPartyStride, ::bof3::Img(Cond_Flags_addr) + 0xd4, ::bof3::Img(Area136_EffectByCharA_addr), 2);
}

// original 0x41F1D0 (area 136 +0x34[8] = +0x3C[4]; PSX 0x801F2F50): party
// record 1, effect kind 1 by Area136_EffectByCharB[slot 1's character].
extern "C" void __cdecl Area136_SpawnMember1Kind1(void) {
    SpawnByCharacter(::bof3::Img(ObjTrio_addr) + at::kPartyStride, ::bof3::Img(Cond_Flags_addr) + 0xd3, ::bof3::Img(Area136_EffectByCharB_addr), 1);
}

// original 0x41F220 (area 136 +0x34[9] = +0x3C[5]; PSX 0x801F2FD0): cells
// (9, 0x59..0x5B) 0x10.
extern "C" void __cdecl Area136_CellsRow10(void) { CellsRow59(0x10); }

// original 0x41F250 (area 136 +0x34[10] = +0x3C[6]; PSX 0x801F3018): counter
// 0 = 0xA when the low byte of the word 0x939A00 is 0x62..0x66, else 0x1E.
extern "C" void __cdecl Area136_Counter0ByTile(void) {
    const unsigned tile = Word(Mem(::bof3::Img(at::kTile.v))) & 0xFFu;
    B(::bof3::Img(at::kCounter0.v)) = tile >= 0x62 && tile <= 0x66 ? 0xA : 0x1E;
}

// original 0x41F280 (area 136 +0x34[11] = +0x3C[7]; PSX 0x801F3048): party
// record 1, effect kind 3 by Area136_EffectByCharB[slot 1's character].
extern "C" void __cdecl Area136_SpawnMember1Kind3(void) {
    SpawnByCharacter(::bof3::Img(ObjTrio_addr) + at::kPartyStride, ::bof3::Img(Cond_Flags_addr) + 0xd3, ::bof3::Img(Area136_EffectByCharB_addr), 3);
}

// original 0x41F2D0 (area 136 +0x34[12] = +0x3C[8]; PSX 0x801F30C8): the
// leader's record, effect kind 1 by Area136_EffectByCharB[slot 1's character]
// (slot 1's, not slot 0's: as read).
extern "C" void __cdecl Area136_SpawnLeaderKind1(void) {
    SpawnByCharacter(::bof3::Img(ObjTrio_addr), ::bof3::Img(Cond_Flags_addr) + 0xd3, ::bof3::Img(Area136_EffectByCharB_addr), 1);
}

// original 0x41F320 (area 136 +0x34[14] = +0x3C[10], PSX 0x801F3170; area 119
// +0x34[2] = +0x3C[1], area 131 +0x3C[7], area 188 +0x34[5] = +0x3C[1]):
// Camera_ShiftY + 2, MapView_Redraw 2.
extern "C" void __cdecl Area136_ShiftCameraUp2(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 2);
    MapView_Redraw = 2;
}

// original 0x41F340 (Field_ModeTailKinds 19): by the signed state (0..13; any
// other nothing), through the byte map 0x41F4D0 and the eight-entry jump table
// 0x41F4B0 inside the function:
//   0   KeyItem_Has(6): state 10 when held, else 1;
//   1   Field_Request not 2: Msg_OpenScript(0x83), Field_Request 2, state 2;
//   2   Field_Request not 2: kind, state, sub-kind 0, ScriptFlags_Clear40;
//   10  Flags_Set(the flag row, 0x2E); then Field_Request not 2:
//       Msg_OpenScript(0x82), Field_Request 2, Flags_Toggle(story, 0x29),
//       state 0xB;
//   11  Field_Request not 2: Party_DropIn(0), state 0xC;
//   12  counter 3 at 0x28: Field_ChangeArea(0x88, ...) to (0x330000, 0x470000)
//       with flags 0x81 for sub-kind 0, else (0x80000, 0x220000) with 0x82;
//       state 0xD;
//   13  counter 3 at 0: ScriptFlags_Clear40, kind, state, sub-kind 0,
//       Flags_Clear(the flag row, read after the call, 0x2E);
//   3..9 nothing.
extern "C" void __cdecl Area136_Tail19(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    switch (state) {
    case 0:
        B(::bof3::Img(AreaW0c_TailState_addr)) = AH_CALL(KeyItem_Has)(6) != 0 ? 10 : 1;
        return;
    case 1:
        if (Field_Request == 2) return;
        AH_CALL(Msg_OpenScript)(0x83);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 2;
        return;
    case 2:
        if (Field_Request == 2) return;
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailSub_addr)) = 0;
        AH_CALL(ScriptFlags_Clear40)();
        return;
    case 10:
        AH_CALL(Flags_Set)(FlagRow(), 0x2E);
        if (Field_Request == 2) return;
        AH_CALL(Msg_OpenScript)(0x82);
        Field_Request = 2;
        AH_CALL(Flags_Toggle)(StoryFlags(), 0x29);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xB;
        return;
    case 11:
        if (Field_Request == 2) return;
        AH_CALL(Party_DropIn)(0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xC;
        return;
    case 12:
        if (B(::bof3::Img(at::kCounter3.v)) != 0x28) return;
        if (B(::bof3::Img(AreaW0c_TailSub_addr)) == 0)
            AH_CALL(Field_ChangeArea)(0x88, 0x330000, 0x470000, 0x81);
        else
            AH_CALL(Field_ChangeArea)(0x88, 0x80000, 0x220000, 0x82);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xD;
        return;
    case 13: {
        if (B(::bof3::Img(at::kCounter3.v)) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        unsigned char* const row = FlagRow();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailSub_addr)) = 0;
        AH_CALL(Flags_Clear)(row, 0x2E);
        return;
    }
    default: return;   // 3..9 (the byte map's entry 7, a ret), and past 13
    }
}

// original 0x41F4E0 (area 136 +0x40; PSX 0x801F33BC): the byte 0x905E68 at 1
// and Cond_ByteFD 4: Sprite_ObjectsExtra[0] +0x83 = 2; at 4 and Cond_ByteFD
// 1: = 4.
extern "C" void __cdecl Area136_InitExtraObject(void) {
    const unsigned char zone = B(::bof3::Img(AreaW1a_EntryZone_addr));
    if (zone == 1) {
        if (Cond_ByteFD == 4) B(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x83) = 2;
        return;
    }
    if (zone == 4 && Cond_ByteFD == 1) B(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x83) = 4;
}

void Area136_Inject() {
    BOF3_INJECT(Area136_ChoiceTail19Sub0);
    BOF3_INJECT(Area136_ChoiceTail19Sub1);
    BOF3_INJECT(Area136_ChoiceMessage2);
    BOF3_INJECT(Area136_ChoiceMessage3);
    BOF3_INJECT(Area136_MoveUntilLimit);
    BOF3_INJECT(Area136_CellsRow51);
    BOF3_INJECT(Area136_SpawnMember1Kind2);
    BOF3_INJECT(Area136_SpawnMember2Kind2);
    BOF3_INJECT(Area136_SpawnMember1Kind1);
    BOF3_INJECT(Area136_CellsRow10);
    BOF3_INJECT(Area136_Counter0ByTile);
    BOF3_INJECT(Area136_SpawnMember1Kind3);
    BOF3_INJECT(Area136_SpawnLeaderKind1);
    BOF3_INJECT(Area136_ShiftCameraUp2);
    BOF3_INJECT(Area136_Tail19);
    BOF3_INJECT(Area136_InitExtraObject);
}
