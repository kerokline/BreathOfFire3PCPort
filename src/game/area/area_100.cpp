// Moved here by tools/move_funcs.py from area_w2d.cpp (docs/source-layout-round.md).
#include "game/area/area_100.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2d.h"
#include "game/area_w2d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2d::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The map byte setter, as the originals pass it: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// The tail armed: ScriptFlags_Set40, kind `kind`, state 0.
void ArmTail(unsigned char kind) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// The tail disarmed: ScriptFlags_Clear40, kind and state 0.
void Disarm() {
    AH_CALL(ScriptFlags_Clear40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// Area 100's handlers 6 and 7 (0x4140A0, 0x4140F0) and its tail's state 1:
// an effect of `kind` at (0x460000, 0x340000), if a slot is free.
void SpawnAt46(unsigned char kind) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = kind;
    tE->x = 0x460000;
    tE->z = 0x340000;
}

// Area 100's handlers 6 and 7: Cond_ByteFE 1, MoveCmd_TestFB(0x4A, 0x2F),
// then the effect.
void TestAndSpawn(unsigned char kind) {
    Cond_ByteFE = 1;
    AH_CALL(MoveCmd_TestFB)(0x4A, 0x2F);
    SpawnAt46(kind);
}

// Area 100's four gate cells (0x49, 0x1E..0x1F) and (0x4D, 0x33..0x34) set
// to `value` (its init: 0x50, its trigger 16: 0xA1).
void GateCells(unsigned value) {
    SetByte(0x49, 0x1E, value);
    SetByte(0x49, 0x1F, value);
    SetByte(0x4D, 0x33, value);
    SetByte(0x4D, 0x34, value);
}

}  // namespace

// original 0x413FC0 (area 100 +0x3C[0]; PSX 0x801F34FC): by story flags 0x94
// (1) and 0x93 (2): Field_ChangeArea to area 0x3B (neither), 0x24 (0x94),
// 0x74 (0x93) or 0x70 (both), flags 0x81.
extern "C" void __cdecl Area100_ChangeAreaByFlags(void) {
    const unsigned a = AH_CALL(Flags_Test)(StoryFlags(), 0x94) != 0 ? 1u : 0u;
    const unsigned b = AH_CALL(Flags_Test)(StoryFlags(), 0x93) != 0 ? 2u : 0u;
    switch (a | b) {
    case 1: AH_CALL(Field_ChangeArea)(0x24, 0x478000, 0x240000, 0x81); break;
    case 2: AH_CALL(Field_ChangeArea)(0x74, 0x478000, 0x340000, 0x81); break;
    case 3: AH_CALL(Field_ChangeArea)(0x70, 0xC8000, 0x580000, 0x81); break;
    default: AH_CALL(Field_ChangeArea)(0x3B, 0x478000, 0x2A0000, 0x81); break;
    }
}

// original 0x414070 (area 100 +0x3C[2]; PSX 0x801F3608): story flag 0x45.
extern "C" void __cdecl Area100_SetFlag45(void) { AH_CALL(Flags_Set)(StoryFlags(), 0x45); }

// original 0x414080 (areas 49 +0x34[17] = +0x3C[15], 100 +0x3C[3], 133
// +0x3C[2]; PSX 0x801F3630 in area 100): Camera_ShiftY + 4, MapView_Redraw 2.
extern "C" void __cdecl Area100_ShiftCameraUp4(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 4);
    MapView_Redraw = 2;
}

// original 0x414090 (areas 49 +0x34[16] = +0x3C[14], 100 +0x3C[4], 133
// +0x3C[1]; PSX 0x801F3658 in area 100): Camera_ShiftY - 4, MapView_Redraw 2.
extern "C" void __cdecl Area100_ShiftCameraDown4(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 4);
    MapView_Redraw = 2;
}

// original 0x4140A0 (area 100 +0x3C[6]; PSX 0x801F369C): TestAndSpawn, kind
// 0x9B.
extern "C" void __cdecl Area100_SpawnEffect9B(void) { TestAndSpawn(0x9B); }

// original 0x4140F0 (area 100 +0x3C[7]; PSX 0x801F3724): TestAndSpawn, kind
// 0xB7 (whose handler is Area100_EffectB7Run).
extern "C" void __cdecl Area100_SpawnEffectB7(void) { TestAndSpawn(0xB7); }

// original 0x414140 (area 100 +0x34[0] = +0x3C[8]; PSX 0x801F37AC): the
// message word Area100_ChoiceMessages[the s8 answer] (unchecked; it reads
// inside .data); then by the answer: 1 - flag 0x93 cleared, 0x94 and 0x33
// set; 2 - 0x93 and 0x94 cleared, 0x33 set; 3 - key item 6 held: 0x93, 0x94,
// 0x33 set, else message 9 and 0x33 cleared; 4 - nothing; any other - 0x93
// set, 0x94 cleared, 0x33 set.
extern "C" void __cdecl Area100_ChoiceFlags93(void) {
    const auto answer = static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)));
    SetMessage(Word(Mem(::bof3::Img(Area100_ChoiceMessages_addr) + static_cast<U>(static_cast<std::int32_t>(answer) * 2))));
    unsigned char* const flags = StoryFlags();
    switch (answer) {
    case 1:
        AH_CALL(Flags_Clear)(flags, 0x93);
        AH_CALL(Flags_Set)(flags, 0x94);
        AH_CALL(Flags_Set)(flags, 0x33);
        return;
    case 2:
        AH_CALL(Flags_Clear)(flags, 0x93);
        AH_CALL(Flags_Clear)(flags, 0x94);
        AH_CALL(Flags_Set)(flags, 0x33);
        return;
    case 3:
        if (AH_CALL(KeyItem_Has)(6) != 0) {
            AH_CALL(Flags_Set)(flags, 0x93);
            AH_CALL(Flags_Set)(flags, 0x94);
            AH_CALL(Flags_Set)(flags, 0x33);
            return;
        }
        SetMessage(9);
        AH_CALL(Flags_Clear)(flags, 0x33);
        return;
    case 4: return;
    default:
        AH_CALL(Flags_Set)(flags, 0x93);
        AH_CALL(Flags_Clear)(flags, 0x94);
        AH_CALL(Flags_Set)(flags, 0x33);
        return;
    }
}

// original 0x414240 (Field_ModeTailKinds[28], armed by Area100_Trigger13): by
// the s8 state (above 4 or negative: nothing), 0 - Kind2_Place(0), state 1;
// 1 - once counter 0 is 1, an effect of kind 0x9B at (0x460000, 0x340000),
// the timer 0x1E, state 2; 2 - the timer down, at 0 state 3 and counter 0 +
// 1; 3 - once counter 0 is 0, Msg_OpenScript(3), Field_Request 2, state 4; 4
// - once Field_Request is not 2, the tail disarmed. The table is 0x41431C (5
// entries, in .text).
extern "C" void __cdecl Area100_TailEffect9B(void) {
    switch (static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)))) {
    case 0:
        AH_CALL(Kind2_Place)(0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 1;
        return;
    case 1:
        if (B(::bof3::Img(at::kCounter0.v)) != 1) return;
        SpawnAt46(0x9B);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x1E);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 2;
        return;
    case 2: {
        const auto left = static_cast<std::uint16_t>(Word(Mem(::bof3::Img(at::kTailTimer.v))) - 1);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), left);
        if (left != 0) return;
        const unsigned char count = B(::bof3::Img(at::kCounter0.v));
        B(::bof3::Img(AreaW0c_TailState_addr)) = 3;
        B(::bof3::Img(at::kCounter0.v)) = static_cast<unsigned char>(count + 1);
        return;
    }
    case 3:
        if (B(::bof3::Img(at::kCounter0.v)) != 0) return;
        AH_CALL(Msg_OpenScript)(3);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 4;
        return;
    case 4:
        if (Field_Request == 2) return;
        Disarm();
        return;
    default: return;
    }
}

// original 0x414330 (Area_StepHook's case for area 100): Cond_ByteFD 3, story
// flag 0x33, x's high word 0x45..0x47 and z's 0x33..0x35 (16-bit compares),
// the leader's pose 0, 7 or 6: counter 0 = 0, Party_DropIn(0), al 1. Else al
// 0.
extern "C" unsigned char __cdecl Area100_StepHook(long x, long z) {
    if (Cond_ByteFD != 3) return 0;
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x33) == 0) return 0;
    if (static_cast<std::uint16_t>(High(x) - 0x45) >= 3) return 0;
    if (static_cast<std::uint16_t>(High(z) - 0x33) >= 3) return 0;
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    if (pose != 0 && pose != 7 && pose != 6) return 0;
    B(::bof3::Img(at::kCounter0.v)) = 0;
    AH_CALL(Party_DropIn)(0);
    return 1;
}

// original 0x414390 (Field_ObjectTriggers id 16; a gap of the tool, area
// 100's block): story flag 0x45, MoveCmd_TestFB(0x4A, 0x1E),
// Sound_PlayEffect(0x109), the four gate cells 0xA1. al 0.
extern "C" unsigned char __cdecl Area100_Trigger16(unsigned char*, unsigned char*) {
    AH_CALL(Flags_Set)(StoryFlags(), 0x45);
    AH_CALL(MoveCmd_TestFB)(0x4A, 0x1E);
    AH_CALL(Sound_PlayEffect)(0x109);
    GateCells(0xA1);
    return 0;
}

// original 0x4143F0 (Field_ObjectTriggers id 13; a gap of the tool): story
// flag 0x40, MoveCmd_TestFB(0x4A, 0x2F), tail kind 28 armed. al 0.
extern "C" unsigned char __cdecl Area100_Trigger13(unsigned char*, unsigned char*) {
    AH_CALL(Flags_Set)(StoryFlags(), 0x40);
    AH_CALL(MoveCmd_TestFB)(0x4A, 0x2F);
    ArmTail(0x1C);
    return 0;
}

// original 0x414420 (Field_ObjectTriggers id 30; a gap of the tool): tail kind
// 44 (engine, 0x56DE50) armed with sub-kind 2. al 0.
extern "C" unsigned char __cdecl Area100_Trigger30(unsigned char*, unsigned char*) {
    ArmTail(0x2C);
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 2;
    return 0;
}

// original 0x414440 (area 100 +0x40; PSX 0x801F3C2C): story flag 0x45 clear:
// the four gate cells 0x50.
extern "C" void __cdecl Area100_InitCells(void) {
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x45) != 0) return;
    GateCells(0x50);
}

// original 0x414490 (Effect_KindHandlers[0xB7], 0x65562C; a gap of the tool):
// Area100_EffectStates by Sprite_Current[1] (the running effect record).
extern "C" void __cdecl Area100_EffectB7Run(void) {
    StateEntry("Area100_EffectB7Run", ::bof3::Img(Area100_EffectStates_addr), at::kArea100EffectStateCount, Sprite_Current[1])();
}

// original 0x4144B0 (Area100_EffectStates[1]): the point (x +0x34, z +0x38,
// y +0x3C) of the running record copied to the stack and handed to 0x4220D0
// (area_w2d_callees.h).
extern "C" void __cdecl Area100_EffectB7Ring(void) {
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::EffectObject>(cur);
    long point[4];
    point[0] = tCur->x;
    point[1] = tCur->z;
    point[2] = tCur->height;
    point[3] = 0;
    AH_AT(void (__cdecl*)(const long*), at::kRingAt.v)(point);
}

void Area100_Inject() {
    BOF3_INJECT(Area100_ChangeAreaByFlags);
    BOF3_INJECT(Area100_SetFlag45);
    BOF3_INJECT(Area100_ShiftCameraUp4);
    BOF3_INJECT(Area100_ShiftCameraDown4);
    BOF3_INJECT(Area100_SpawnEffect9B);
    BOF3_INJECT(Area100_SpawnEffectB7);
    BOF3_INJECT(Area100_ChoiceFlags93);
    BOF3_INJECT(Area100_TailEffect9B);
    BOF3_INJECT(Area100_StepHook);
    BOF3_INJECT(Area100_Trigger16);
    BOF3_INJECT(Area100_Trigger13);
    BOF3_INJECT(Area100_Trigger30);
    BOF3_INJECT(Area100_InitCells);
    BOF3_INJECT(Area100_EffectB7Run);
    BOF3_INJECT(Area100_EffectB7Ring);
}
