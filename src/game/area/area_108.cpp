// Moved here by tools/move_funcs.py from area_w2f.cpp (docs/source-layout-round.md).
#include "game/area/area_108.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2f.h"
#include "game/area_w2f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2f::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

// Sprite_ObjectsExtra record k (0xA4 bytes; k not checked, as the originals).
unsigned char* Extra(unsigned k) { return Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + k * at::kExtraStride); }

// Party record m (ObjTrio, 0x14C bytes; m not checked).
unsigned char* Record(unsigned m) { return Mem(::bof3::Img(ObjTrio_addr) + m * at::kPartyStride); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

bool Test(unsigned index) { return AH_CALL(Flags_Test)(StoryFlags(), index) != 0; }

void Set(unsigned index) { AH_CALL(Flags_Set)(StoryFlags(), index); }

// The tail's word timer counted down (a 16-bit dec): the new value.
unsigned DecTimer() {
    const unsigned v = (Word(Mem(::bof3::Img(at::kTailTimer.v))) - 1u) & 0xFFFFu;
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), v);
    return v;
}

// The movement script's position (MoveScript_Object's word +0xA) moved by v.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The map setters as the originals pass them: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// A mask of the party records 1..count-1 whose high words pass: bit i for
// record i + 1, as the originals' byte `shl` (the count masked to 5 bits, so
// no bit past the eighth). A count of 1 or less: 0.
template <typename Pass> unsigned char MemberMask(Pass pass) {
    const unsigned count = Field_MemberCount;
    unsigned char mask = 0;
    if (count <= 1) return 0;
    for (unsigned i = 0; i + 1 < count; ++i) {
        const unsigned char* const r = Record(i + 1);
        if (!pass(static_cast<std::int16_t>(rv::As<records::PartyMember>(r)->x_whole), static_cast<std::int16_t>(rv::As<records::PartyMember>(r)->z_whole))) continue;
        const unsigned s = i & 31;
        if (s < 8) mask = static_cast<unsigned char>(mask | (1u << s));
    }
    return mask;
}

bool Within2(std::int16_t v, unsigned lo) { return static_cast<std::uint16_t>(v - lo) < 2; }

// The active member's script: +0x83 = script, word +0x8A = 0.
void MemberScript(unsigned char script) {
    Field_ActiveMember[0x83] = script;
    rv::As<records::SpriteObject>(Field_ActiveMember)->script_position = 0;
}

// Handlers 5 and 6: the running object put at the table's entry (extra
// object 1's +0x83 - 1, a byte; unchecked, it reads inside .data): word +0x36
// = the x byte, word +0x34 = 0x8000 when the x flag is set, word +0x3A = the
// z byte, word +0x38 likewise.
void PlaceAtCell(U table) {
    const unsigned index = static_cast<unsigned char>(Extra(1)[0x83] - 1);
    const unsigned char* const e = Mem(table + index * 4);
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->x_whole, e[0]);
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->x_frac, e[1] != 0 ? 0x8000u : 0u);
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->z_whole, e[2]);
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->z_frac, e[3] != 0 ? 0x8000u : 0u);
}

}  // namespace

// original 0x4168E0 (area 108 +0x34[0]): the message word 0xFFFF; by the s8
// answer: 0 - sub-kind 3, story flag 0x1E; 1 - sub-kind 2, flag 0x1F; then
// tail kind 5. Any other answer: nothing more.
extern "C" void __cdecl Area108_ChoiceArmTail5(void) {
    const auto answer = static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)));
    SetMessage(0xFFFF);
    unsigned flag;
    if (answer == 0) {
        B(::bof3::Img(AreaW0c_TailSub_addr)) = 3;
        flag = 0x1E;
    } else if (answer == 1) {
        B(::bof3::Img(AreaW0c_TailSub_addr)) = 2;
        flag = 0x1F;
    } else {
        return;
    }
    Set(flag);
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 5;
}

// original 0x416930 (area 108 +0x34[1]): the message word 0xFFFF; answer 0:
// tail kind 0xA, state 0, sub-kind 4.
extern "C" void __cdecl Area108_ChoiceArmTailA(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer != 0) return;
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0xA;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 4;
}

// original 0x416960 (area 108 +0x3C[0] = +0x34[3]; PSX 0x801F43A4): counter 1
// bits 0 and 1 cleared.
extern "C" void __cdecl Area108_ClearCounter1Bits0(void) { B(::bof3::Img(at::kCounter1.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter1.v)) & 0xFC); }

// original 0x416970 (area 108 +0x3C[1]; PSX 0x801F43C0): bits 2 and 3.
extern "C" void __cdecl Area108_ClearCounter1Bits2(void) { B(::bof3::Img(at::kCounter1.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter1.v)) & 0xF3); }

// original 0x416980 (area 108 +0x3C[2]; PSX 0x801F43DC): bits 4 and 5.
extern "C" void __cdecl Area108_ClearCounter1Bits4(void) { B(::bof3::Img(at::kCounter1.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter1.v)) & 0xCF); }

// original 0x416990 (area 108 +0x3C[3]; PSX 0x801F43F8): bits 6 and 7.
extern "C" void __cdecl Area108_ClearCounter1Bits6(void) { B(::bof3::Img(at::kCounter1.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter1.v)) & 0x3F); }

// original 0x4169A0 (area 108 +0x3C[4]; PSX 0x801F4414): the active member's
// +0x80 bit 0 cleared; unless the leader's member id (+0x89) is 2, nothing
// more. The first of Area108_Places whose direction is the leader's (+8 & 7)
// and whose x, z are extra object 1's high words (+0x36, +0x3A) picks the
// scene (none: nothing more): Party_DropIn(a mask of the other members in
// the place's strip), the member's script (+0x83, +0x8A = 0) - place 0: 1,
// 2, 3 or 4 by story flags 0x18..0x1A (2 with Area108_SetCells(1)); 1: 5; 2:
// 6 with Area108_SetCells(0); 3: 7; 4: 8, or 9 with flag 0x1B. Then
// MoveScript_PartyRecords record 1's bit 0, ScriptFlags_Set40, tail kind 0xC,
// state 0, timer 0xF.
extern "C" void __cdecl Area108_PlaceScene(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) != 2) return;
    const unsigned direction = B(::bof3::Img(ObjTrio_addr) + 0x8) & 7u;
    const std::uint16_t x = rv::As<records::SpriteObject>(Extra(1))->x_whole;
    const std::uint16_t z = rv::As<records::SpriteObject>(Extra(1))->z_whole;
    unsigned place = 0;
    for (; place < at::kArea108PlaceCount; ++place) {
        const unsigned char* const e = Mem(::bof3::Img(Area108_Places_addr) + place * 3);
        if (direction == e[2] && x == e[0] && z == e[1]) break;
    }
    switch (place) {
    case 0:
        AH_CALL(Party_DropIn)(MemberMask([](std::int16_t mx, std::int16_t mz) { return mx > 0x27 && Within2(mz, 0x53); }));
        if (!Test(0x18)) {
            MemberScript(1);
        } else if (!Test(0x19)) {
            AH_CALL(Area108_SetCells)(1);
            MemberScript(2);
        } else if (!Test(0x1A)) {
            MemberScript(3);
        } else {
            MemberScript(4);
        }
        break;
    case 1:
        AH_CALL(Party_DropIn)(MemberMask([](std::int16_t mx, std::int16_t mz) { return mx < 0x39 && Within2(mz, 0x53); }));
        MemberScript(5);
        break;
    case 2:
        AH_CALL(Party_DropIn)(MemberMask([](std::int16_t mx, std::int16_t mz) { return mz > 0x3A && Within2(mx, 0x33); }));
        AH_CALL(Area108_SetCells)(0);
        MemberScript(6);
        break;
    case 3:
        AH_CALL(Party_DropIn)(MemberMask([](std::int16_t mx, std::int16_t mz) { return mx > 0x21 && Within2(mz, 0x41); }));
        MemberScript(7);
        break;
    case 4:
        AH_CALL(Party_DropIn)(MemberMask([](std::int16_t mx, std::int16_t mz) { return mz < 0x41 && Within2(mx, 0x1C); }));
        MemberScript(Test(0x1B) ? 9 : 8);
        break;
    default: return;
    }
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x10) = static_cast<unsigned char>(B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x10) | 1);
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0xC;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0xF);
}

// original 0x416D60 (area 108 +0x3C[5]; PSX 0x801F4918): PlaceAtCell over
// Area108_PlaceCellsA.
extern "C" void __cdecl Area108_PlaceAtCellA(void) { PlaceAtCell(::bof3::Img(Area108_PlaceCellsA_addr)); }

// original 0x416DE0 (area 108 +0x3C[6]; PSX 0x801F4998): over
// Area108_PlaceCellsB.
extern "C" void __cdecl Area108_PlaceAtCellB(void) { PlaceAtCell(::bof3::Img(Area108_PlaceCellsB_addr)); }

// original 0x416E60 (area 108 +0x3C[7]; PSX 0x801F4A18): story flag 0x1E:
// the active member's script 0x12; else flag 0x1F: 0x11; either way its
// script position +0x8A = 0xFFFE.
extern "C" void __cdecl Area108_ScriptByFlags1E(void) {
    if (Test(0x1E)) {
        Field_ActiveMember[0x83] = 0x12;
    } else if (Test(0x1F)) {
        Field_ActiveMember[0x83] = 0x11;
    }
    rv::As<records::SpriteObject>(Field_ActiveMember)->script_position = 0xFFFE;
}

// original 0x416EC0 (area 108 +0x3C[8]; PSX 0x801F4A98): the active member's
// +0x80 bit 0 cleared; Character_IndexById[the leader's +0x89] (a byte
// index, unchecked; it reads inside .data) at 5: story flag 0x30,
// MoveCmd_TestFB(1, 0xB), the running object's +0 = 0.
extern "C" void __cdecl Area108_FlagIfEffectState5(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    if (B(::bof3::Img(Character_IndexById_addr) + B(::bof3::Img(ObjTrio_addr) + 0x89)) != 5) return;
    Set(0x30);
    AH_CALL(MoveCmd_TestFB)(1, 0xB);
    Sprite_Current[0] = 0;
}

// original 0x416F10 (area 108 +0x3C[9]; PSX 0x801F4B14): Area108_FadeStates
// by Sprite_Current[4].
extern "C" void __cdecl Area108_FadeRun(void) {
    StateEntry("Area108_FadeRun", ::bof3::Img(Area108_FadeStates_addr), at::kArea108FadeStateCount, Sprite_Current[4])();
}

// original 0x416F30 (Area108_FadeStates[0]): Sprite_SetTint(the running
// object, 0, 0, 0, 1) to the active member's +0x9F; the object's +0 bit 0x20
// set and 0x40 cleared, +0x5C = 1, +0x5F, +0x5E, +0x5D = 0x80, state +4 = 1;
// the member's script position - 2.
extern "C" void __cdecl Area108_FadeBegin(void) {
    const unsigned char tint = AH_CALL(Sprite_SetTint)(Sprite_Current, 0, 0, 0, 1);
    Field_ActiveMember[0x9F] = tint;
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x20);
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
    Sprite_Current[0x5C] = 1;
    Sprite_Current[0x5F] = 0x80;
    Sprite_Current[0x5E] = 0x80;
    Sprite_Current[0x5D] = 0x80;
    Sprite_Current[4] = 1;
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x416FB0 (Area108_FadeStates[1]): each of +0x5D, +0x5E, +0x5F
// below 0xD0 (signed bytes) + 1; all three at 0xD0: Tint_Release(the
// member's +0x9F), +0 bit 0x20 cleared, +0x5C, +0x5F, +0x5E, +0x5D and the
// state 0; else the member's script position - 2.
extern "C" void __cdecl Area108_FadeStep(void) {
    unsigned char* const cur = Sprite_Current;
    for (unsigned off = 0x5D; off <= 0x5F; ++off)
        if (static_cast<signed char>(cur[off]) < static_cast<signed char>(0xD0)) cur[off] = static_cast<unsigned char>(cur[off] + 1);
    if (cur[0x5D] == 0xD0 && cur[0x5E] == 0xD0 && cur[0x5F] == 0xD0) {
        AH_CALL(Tint_Release)(Field_ActiveMember[0x9F]);
        Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xDF);
        Sprite_Current[0x5C] = 0;
        Sprite_Current[0x5F] = 0;
        Sprite_Current[0x5E] = 0;
        Sprite_Current[0x5D] = 0;
        Sprite_Current[4] = 0;
        return;
    }
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x417060 (area 108 +0x3C[10]; PSX 0x801F4D60): Effect_FindFree to
// the running object's +0xB; none: the script position - 2.
extern "C" void __cdecl Area108_FindEffectSlot(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    if (Sprite_Current[0xB] == 0xFF) ScriptStep(-2);
}

// original 0x417090 (area 108 +0x3C[11]; PSX 0x801F4DC8): Effect_FindFree to
// +0xB; none: the script position - 2. A slot: z +0x38 = 0x418000, x +0x34 =
// 0x158000, +0 = 1, kind +5 = 0x51, +1 = 0xA, +0x3C = the ground there << 16.
extern "C" void __cdecl Area108_SpawnEffect51(void) {
    const unsigned char found = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = found;
    const unsigned char slot = Sprite_Current[0xB];
    if (slot == 0xFF) {
        ScriptStep(-2);
        return;
    }
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    tE->z = 0x418000;
    tE->x = 0x158000;
    e[0] = 1;
    e[5] = 0x51;
    e[1] = 0xA;
    const long ground = AH_CALL(AreaMap_Elevation)(tE->x, tE->z);
    tE->height = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16);
}

// original 0x417100 (area 108 +0x3C[15] = +0x34[18]; PSX 0x801F4F0C):
// Field_Request 5: Area108_SetCells(0); else the script position - 2.
extern "C" void __cdecl Area108_CellsIfRequest5(void) {
    if (Field_Request == 5) {
        AH_CALL(Area108_SetCells)(0);
        return;
    }
    ScriptStep(-2);
}

// original 0x417120 (area 108's cell hook, Area_CellHooks): the first of
// Area108_HookCells whose x and z bytes are the cell's and whose direction
// (& 0xF) is the leader's whole pose byte; none, or story flag 0x1C set: al
// 0. Else the entry's flag toggled, Sound_PlayEffect(0x206), and a free
// effect slot (if any): flag 0x1C set, the record +0 = 1, kind +5 = 0x18,
// +0xB = 0x1B, +1 = 0, words +0x36 / +0x3A the cell's x and z; al 1.
extern "C" unsigned char __cdecl Area108_CellHook(long x, long z) {
    const auto cx = static_cast<unsigned char>(x);
    const auto cz = static_cast<unsigned char>(z);
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    unsigned i = 0;
    for (; i < at::kArea108HookCellCount; ++i) {
        const unsigned char* const e = Mem(::bof3::Img(Area108_HookCells_addr) + i * 4);
        if (cx == e[0] && cz == e[1] && (e[2] & 0xF) == pose) break;
    }
    if (i == at::kArea108HookCellCount) return 0;
    if (Test(0x1C)) return 0;
    const unsigned char* const e = Mem(::bof3::Img(Area108_HookCells_addr) + i * 4);
    AH_CALL(Flags_Toggle)(StoryFlags(), e[3]);
    AH_CALL(Sound_PlayEffect)(0x206);
    const unsigned slot = AH_CALL(Effect_FindFree)();
    if (slot != 0xFF) {
        Set(0x1C);
        unsigned char* const r = EffectAt(slot);
        r[0] = 1;
        r[5] = 0x18;
        r[0xB] = 0x1B;
        r[1] = 0;
        rv::Store<unsigned>(rv::As<records::EffectObject>(r)->x_whole, e[0]);
        rv::Store<unsigned>(rv::As<records::EffectObject>(r)->z_whole, e[1]);
    }
    return 1;
}

// original 0x417210 (Field_ModeTailKinds[12], armed by Area108_PlaceScene):
// its jump table (10 entries) by the s8 state, unsigned above 9: nothing.
//   0: the timer down to 0: Field_Kind2X / Z = extra object 1's x / z,
//      MoveScript_F3Divisor = Field_MoveSpeeds[3] << 3, the next state.
//   1: Field_Kind2Hold 0: Kind2_Place(extra object 1's +0x83 - 1),
//      Sprite_Kind2 +0x84 = 4, counter 3 = 1, the state (read again) + 1.
//   2: counter 3 at 2: counter 3 = 0; extra object 1's +0x83 at 9: state 4;
//      else the divisor from Field_MoveSpeeds[4], Kind2 X / Z the leader's,
//      the next state.
//   3: Kind2Hold 0: MapView_SetElevation(AreaMap_Elevation(Kind2 X, Z)), the
//      tail disarmed (state, kind 0, then ScriptFlags_Clear40).
//   4: counter 3 at 4: an effect 0x35 at (0x218000, 0x2F8000) with its ground
//      (not shifted) if a slot is free; Area108_Model's flags byte & 0xF1 |
//      0x41; the timer 0x1E; state 5.
//   5: the timer down to 0: story flag 0x1D, MoveCmd_TestFB(0x20, 0x2F),
//      state 9 (the ebx it entered with), extra object 0's +0 = 0, the timer
//      0x1E. Else each of Area108_Model's records (its s8 count, read again
//      each pass) has its four corners moved by its velocity >> 8 (s16), and
//      extra object 0's +0x5D, +0x5F, +0x5E less 4.
//   6..8: nothing. 9: the timer down to 0: MoveScript_Var7 = 3, the state,
//      the kind and the run's step 0.
extern "C" void __cdecl Area108_TailPlace(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    if (static_cast<U>(static_cast<std::int32_t>(state)) > 9) return;
    switch (state) {
    case 0:
        if (DecTimer() != 0) return;
        Field_Kind2X = rv::As<records::SpriteObject>(Extra(1))->x;
        Field_Kind2Z = rv::As<records::SpriteObject>(Extra(1))->z;
        MoveScript_F3Divisor = static_cast<short>(B(::bof3::Img(Field_MoveSpeeds_addr) + 0x3) << 3);
        B(::bof3::Img(AreaW0c_TailState_addr)) = static_cast<unsigned char>(B(::bof3::Img(AreaW0c_TailState_addr)) + 1);
        return;
    case 1:
        if (Field_Kind2Hold != 0) return;
        AH_CALL(Kind2_Place)(static_cast<unsigned char>(Extra(1)[0x83] - 1));
        Sprite_Kind2.speed = 4;
        B(::bof3::Img(at::kCounter3.v)) = 1;
        B(::bof3::Img(AreaW0c_TailState_addr)) = static_cast<unsigned char>(B(::bof3::Img(AreaW0c_TailState_addr)) + 1);
        return;
    case 2: {
        if (B(::bof3::Img(at::kCounter3.v)) != 2) return;
        const unsigned char script = Extra(1)[0x83];
        B(::bof3::Img(at::kCounter3.v)) = 0;
        if (script == 9) {
            B(::bof3::Img(AreaW0c_TailState_addr)) = 4;
            return;
        }
        MoveScript_F3Divisor = static_cast<short>(B(::bof3::Img(Field_MoveSpeeds_addr) + 0x4) << 3);
        Field_Kind2X = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x34));
        Field_Kind2Z = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x38));
        B(::bof3::Img(AreaW0c_TailState_addr)) = static_cast<unsigned char>(B(::bof3::Img(AreaW0c_TailState_addr)) + 1);
        return;
    }
    case 3:
        if (Field_Kind2Hold != 0) return;
        AH_CALL(MapView_SetElevation)(AH_CALL(AreaMap_Elevation)(Field_Kind2X, Field_Kind2Z));
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        AH_CALL(ScriptFlags_Clear40)();
        return;
    case 4: {
        if (B(::bof3::Img(at::kCounter3.v)) != 4) return;
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        if (slot != 0xFF) {
            unsigned char* const e = EffectAt(slot);
            auto* const tE = rv::As<records::EffectObject>(e);
            tE->z = 0x2F8000;
            tE->x = 0x218000;
            e[0] = 1;
            e[5] = 0x35;
            const long ground = AH_CALL(AreaMap_Elevation)(tE->x, tE->z);
            tE->height = static_cast<std::int16_t>(ground);
        }
        B(::bof3::Img(Area108_Model_addr) + 0x3) = static_cast<unsigned char>((B(::bof3::Img(Area108_Model_addr) + 0x3) & 0xF1) | 0x41);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x1E);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 5;
        return;
    }
    case 5: {
        if (DecTimer() == 0) {
            Set(0x1D);
            AH_CALL(MoveCmd_TestFB)(0x20, 0x2F);
            B(::bof3::Img(AreaW0c_TailState_addr)) = 9;
            Extra(0)[0] = 0;
            SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x1E);
            return;
        }
        if (static_cast<signed char>(B(::bof3::Img(Area108_Model_addr))) > 0) {
            unsigned char* p = Ptr(::bof3::Img(Area108_Model_addr) + 0x4) + 2;
            unsigned char i = 0;
            do {
                const auto vx = static_cast<std::uint16_t>(static_cast<std::int16_t>(Word(p + 0x18)) >> 8);
                const auto vy = static_cast<std::uint16_t>(static_cast<std::int16_t>(Word(p + 0x1A)) >> 8);
                const auto vz = static_cast<std::uint16_t>(static_cast<std::int16_t>(Word(p + 0x1C)) >> 8);
                for (unsigned c = 0; c < 4; ++c) SetWord(p + c * 6, Word(p + c * 6) + vx);
                for (unsigned c = 0; c < 4; ++c) SetWord(p + c * 6 + 2, Word(p + c * 6 + 2) + vy);
                for (unsigned c = 0; c < 4; ++c) SetWord(p + c * 6 + 4, Word(p + c * 6 + 4) + vz);
                p += at::kArea108ModelStride;
                ++i;
            } while (static_cast<std::int32_t>(i) < static_cast<signed char>(B(::bof3::Img(Area108_Model_addr))));
        }
        unsigned char* const extra0 = Extra(0);
        extra0[0x5D] = static_cast<unsigned char>(extra0[0x5D] + 0xFC);
        extra0[0x5F] = static_cast<unsigned char>(extra0[0x5F] + 0xFC);
        extra0[0x5E] = static_cast<unsigned char>(extra0[0x5E] + 0xFC);
        return;
    }
    case 9:
        if (DecTimer() != 0) return;
        MoveScript_Var7 = 3;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0;
        return;
    default: return;   // 6..8
    }
}

// original 0x417510 (called by Area108_PlaceScene and
// Area108_CellsIfRequest5): each of Area108_SetCells's four (x, z) words -
// on: AreaMap_SetHeight(x, z, 0xE), AreaMap_SetByte(x, z, 0x11); off: both
// 0.
extern "C" void __cdecl Area108_SetCells(unsigned char on) {
    for (unsigned i = 0; i < at::kArea108SetCellCount; ++i) {
        const unsigned char* const c = Mem(::bof3::Img(Area108_CellWords_addr) + i * 4);
        const unsigned x = Word(c);
        const unsigned z = Word(c + 2);
        AH_CALL(AreaMap_SetHeight)(x, z, on != 0 ? 0xEu : 0u);
        SetByte(x, z, on != 0 ? 0x11u : 0u);
    }
}

void Area108_Inject() {
    BOF3_INJECT(Area108_ChoiceArmTail5);
    BOF3_INJECT(Area108_ChoiceArmTailA);
    BOF3_INJECT(Area108_ClearCounter1Bits0);
    BOF3_INJECT(Area108_ClearCounter1Bits2);
    BOF3_INJECT(Area108_ClearCounter1Bits4);
    BOF3_INJECT(Area108_ClearCounter1Bits6);
    BOF3_INJECT(Area108_PlaceScene);
    BOF3_INJECT(Area108_PlaceAtCellA);
    BOF3_INJECT(Area108_PlaceAtCellB);
    BOF3_INJECT(Area108_ScriptByFlags1E);
    BOF3_INJECT(Area108_FlagIfEffectState5);
    BOF3_INJECT(Area108_FadeRun);
    BOF3_INJECT(Area108_FadeBegin);
    BOF3_INJECT(Area108_FadeStep);
    BOF3_INJECT(Area108_FindEffectSlot);
    BOF3_INJECT(Area108_SpawnEffect51);
    BOF3_INJECT(Area108_CellsIfRequest5);
    BOF3_INJECT(Area108_CellHook);
    BOF3_INJECT(Area108_TailPlace);
    BOF3_INJECT(Area108_SetCells);
}
