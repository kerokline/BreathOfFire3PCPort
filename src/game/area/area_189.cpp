// Moved here by tools/move_funcs.py from area_w4e.cpp (docs/source-layout-round.md).
#include "game/area/area_189.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4e.h"
#include "game/area_w4e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4e::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

unsigned char* Cur() { return Sprite_Current; }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

unsigned char* Row14() { return Mem(::bof3::Img(Cond_Flags_addr) + 0x70); }

bool Test(const unsigned char* bits, unsigned index) { return AH_CALL(Flags_Test)(bits, index) != 0; }

// The low word of Cond_AngleFB, the only part any store here writes.
void SetAngleFB(unsigned v) { SetWord(reinterpret_cast<unsigned char*>(&Cond_AngleFB), v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count) bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The tail armed from a choice: ScriptFlags_Set40, then kind and state (the
// sub-kind left as it is).
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// fld / fstp of a float: the bits copied, but a signalling NaN quieted (bit
// 22 set) as the x87 does. Kept as the instructions themselves.
void CopyFloatX87(unsigned char* to, const unsigned char* from) {
    __asm__ volatile("flds (%1)\n\tfstps (%0)" : : "r"(to), "r"(from) : "memory");
}

// 0x511C10 (x, z): the map's height, in ax.
using HeightFn = short (__cdecl*)(long, long);

short HeightAt(U x, U z) { return AH_AT(HeightFn, at::kHeightAt.v)(static_cast<long>(x), static_cast<long>(z)); }

// Area 189: the pending area change of its walk and exit button:
// Field_ChangeArea(the area word, x, z, the flags byte) from the four cells.
void ChangeToPending() {
    AH_CALL(Field_ChangeArea)(Word(Mem(::bof3::Img(AreaW0b_Place_addr))), Long(Mem(::bof3::Img(at::kPendingX.v))), Long(Mem(::bof3::Img(AreaEntry_PendingZ_addr))), B(::bof3::Img(AreaEntry_PendingFlags_addr)));
}

// The walk's facing for the next area: Area189_FacingJitter[Rand() & 3] +
// the leader's +8, the low nibble (Sprite_Current read after Rand).
void JitterFacing() {
    const unsigned r = static_cast<unsigned>(AH_CALL(Rand)()) & 3;
    const unsigned char* const cur = Cur();
    B(::bof3::Img(AreaW3g_HealByte_addr)) = static_cast<unsigned char>((B(::bof3::Img(Area189_FacingJitter_addr) + r) + cur[8]) & 0xF);
}

// A 16.16 coordinate's high word with its low byte cleared, signed (the
// walk's compares: `and al, 0` on the word, then a signed 16-bit cmp).
std::int16_t Block(U v) { return static_cast<std::int16_t>((v >> 16) & 0xFF00); }

std::int16_t BlockWord(const unsigned char* p) { return static_cast<std::int16_t>(Word(p) & 0xFF00); }

}  // namespace

// original 0x42A7D0 (area 189 +0x40; PSX 0x801F4C38): story flag 0x82 set:
// Draw_PassFlags 0x1F.
extern "C" void __cdecl Area189_Init(void) {
    if (Test(StoryFlags(), 0x82)) Draw_PassFlags = 0x1F;
}

// original 0x42A7F0 (area 189 +0x34[0]): message 0xFFFF; answer 0:
// ScriptFlags_Set40, tail kind 50 (Area189_Tail50) at state 0.
extern "C" void __cdecl Area189_ChoiceTail50A(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a == 0) ArmTail(0x32, 0);
}

// original 0x42A820 (area 189 +0x34[1]): the same at state 1.
extern "C" void __cdecl Area189_ChoiceTail50B(void) {
    const unsigned char a = Answer();
    SetMessage(0xFFFF);
    if (a == 0) ArmTail(0x32, 1);
}

// original 0x42A850 (Field_ModeTailKinds[50]): once Field_Request is not 2:
// ScriptFlags_Clear40; Field_StatusBits bit 0 cleared (read after the call,
// with the state); state not 0: Field_ChangeArea(0x96, 0x180000, 0x300000, 1),
// else (0xC1, 0x1A8000, 0x1C0000, 1); kind and state 0.
extern "C" void __cdecl Area189_Tail50(void) {
    if (Field_Request == 2) return;
    AH_CALL(ScriptFlags_Clear40)();
    const unsigned char bits = Field_StatusBits;
    const unsigned char state = B(::bof3::Img(AreaW0c_TailState_addr));
    Field_StatusBits = static_cast<unsigned char>(bits & 0xFE);
    if (state != 0) AH_CALL(Field_ChangeArea)(0x96, 0x180000, 0x300000, 1);
    else AH_CALL(Field_ChangeArea)(0xC1, 0x1A8000, 0x1C0000, 1);
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// original 0x42A8B0 (Field_LeaderStates[13], 0x66094C): Area189_LeaderStates
// by Sprite_Current +2 (unchecked there; ours aborts past its five).
extern "C" void __cdecl Area189_LeaderRun(void) {
    StateEntry("Area189_LeaderRun", ::bof3::Img(Area189_LeaderStates_addr), at::kArea189LeaderStateCount, Cur()[2])();
}

// original 0x42A8D0 (Area189_LeaderStates 0): the walk set up.
// Gte_SetGeomOffset(0xA0, 0xB9), Gte_SetGeomScreen(0x12C); Camera_Angles[0]
// 0xFD12; the leader's facing +8 = the byte 0x904153; Camera_Distance 0xF280;
// MapView_Redraw 4; Cond_AngleFB's low word ((2 - that byte, read again) &
// 0xF) << 8; Field_Kind2X / Z the leader's x / z; +0x3C the height at (x, z)
// (0x511C10) << 16; Area189_ZeroSpeeds; +0x24, +0x4B 0; Field_StepCount 0.
// The pace: (Rand() & 0x1F) + 0x96, doubled when any of Party_Count(0)'s
// members answers Actor_EquipCount(member, 3, 0x15), halved (arithmetic)
// when any answers (member, 3, 0x14), doubled again with Cond_ByteFF 0; the
// word 0x802E74 = the pace. Story flags 0x75, 0x76, 0x8B..0x8E cleared; +2 + 1.
extern "C" void __cdecl Area189_LeaderStart(void) {
    AH_CALL(Gte_SetGeomOffset)(0xA0, 0xB9);
    AH_CALL(Gte_SetGeomScreen)(0x12C);
    unsigned char* cur = Cur();
    const unsigned char facing = B(::bof3::Img(AreaW3g_HealByte_addr));
    Camera_Angles[0] = static_cast<short>(0xFD12);
    cur[8] = facing;
    const unsigned char again = B(::bof3::Img(AreaW3g_HealByte_addr));
    Camera_Distance = static_cast<short>(0xF280);
    MapView_Redraw = 4;
    SetAngleFB(((2u - again) & 0xF) << 8);
    cur = Cur();
    Field_Kind2X = rv::As<records::SpriteHead>(cur)->x;
    Field_Kind2Z = rv::As<records::SpriteHead>(cur)->z;
    const short height = HeightAt(static_cast<U>(rv::As<records::SpriteHead>(cur)->x), static_cast<U>(Long(cur + 0x38)));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->height, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(height)) << 16));
    AH_CALL(Area189_ZeroSpeeds)();
    Cur()[0x24] = 0;
    Cur()[0x4B] = 0;
    Field_StepCount = 0;
    std::int32_t pace = static_cast<std::int32_t>((static_cast<U>(AH_CALL(Rand)()) & 0x1F) + 0x96);
    const unsigned count = static_cast<unsigned>(AH_CALL(Party_Count)(0)) & 0xFF;
    for (unsigned i = 0; i < count; ++i) {
        if (AH_CALL(Actor_EquipCount)(ObjTrio[i].char_index, 3, 0x15) != 0) {
            pace <<= 1;
            break;
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        if (AH_CALL(Actor_EquipCount)(ObjTrio[i].char_index, 3, 0x14) != 0) {
            pace >>= 1;
            break;
        }
    }
    if (Cond_ByteFF == 0) pace <<= 1;
    SetWord(Mem(::bof3::Img(ObjTrio_addr) + 0x134), static_cast<unsigned>(pace));
    AH_CALL(Flags_Clear)(StoryFlags(), 0x75);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x76);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x8B);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x8C);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x8D);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x8E);
    cur = Cur();
    cur[2] = static_cast<unsigned char>(cur[2] + 1);
}

// original 0x42AA80 (Area189_LeaderStates 1): Prim_VertexScratch = (x, z, y)
// of the kind-2 point: Field_Kind2X and Z each ((v - 1) & 0xFFFFFF) + 1, >> 9,
// - 0x4000, and -(the leader's s16 +0x3E / 2, toward 0); Gte_RotTransPers of
// it to MapView_ScreenXY; Gte_StoreDepthF(+0x7C); +0x78 = MapView_ScreenXY's
// y (through the x87); +0x14 = 1. Cond_Flags row 14 bit 9 set and bit 0xA
// clear: Effect_Objects[0] bit 6, +2 = 3, +0x10 = -0x2000, +9 = 0x78, the
// word 0x8034E6 = 0x3C. Else +2 = 2.
extern "C" void __cdecl Area189_LeaderProject(void) {
    auto scaled = [](U v) { return static_cast<short>((static_cast<std::int32_t>(((v - 1u) & 0xFFFFFFu) + 1u) >> 9) - 0x4000); };
    Prim_VertexScratch[0].x = scaled(static_cast<U>(Field_Kind2X));
    Prim_VertexScratch[0].y = scaled(static_cast<U>(Field_Kind2Z));
    const auto y = static_cast<std::int32_t>(static_cast<std::int16_t>(rv::Current<records::SpriteHead>()->height_whole));
    Prim_VertexScratch[0].z = static_cast<short>(-(y / 2));
    long depth[2];
    AH_CALL(Gte_RotTransPers)(reinterpret_cast<short*>(Prim_VertexScratch), reinterpret_cast<unsigned long*>(MapView_ScreenXY), &depth[1]);
    AH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(Cur() + 0x7C));
    CopyFloatX87(Cur() + 0x78, Mem(::bof3::Img(MapView_ScreenXY_addr) + 0x4));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_y, 1);
    if (Test(Row14(), 9) && !Test(Row14(), 0xA)) {
        Effect_Objects[0].flags = static_cast<unsigned char>(Effect_Objects[0].flags | 0x40);
        unsigned char* const cur = Cur();
        cur[2] = 3;
        rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->step_z, -0x2000);
        cur[9] = 0x78;
        SetWord(Mem(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), 0x3C);
        return;
    }
    Cur()[2] = 2;
}

// original 0x42AB90 (Area189_LeaderStates 2; also the tail jump of
// Area189_StepArrive): nothing under Field_ScriptFlags bit 8,
// Field_ScriptFlags2 bit 6 or Field_Request, or while Area189_ExitButton or
// Area189_MenuButton answers. Area189_TurnInput: 0 - +9 = 0 and
// Area189_LeaderHalt; 1 - Field_State +0x137 = 1, Cond_AngleFB's low word =
// (it + the s8 +0xB) & 0xFFF, +2 = 4; 2 - Area189_StepBegin, then the point
// +9 steps on (x + +0xC * +9, z + +0x10 * +9, 32-bit): its blocks (the high
// words with the low byte cleared, signed) x 0x1000..0x1400 with z from
// 0x1800 - message 2, request 2, the halt; else with Cond_Flags row 14 bit 4,
// x 0x1500..0x1900 and z at most 0xE00 - message 3, request 2, the halt;
// else +9 - 1, Area189_StepMove, Field_State +0x137 = 1, +2 = 3. Any other
// answer: nothing.
extern "C" void __cdecl Area189_LeaderControl(void) {
    if ((Field_ScriptFlags & 0x100) != 0) return;
    if ((Field_ScriptFlags2 & 0x40) != 0) return;
    if (Field_Request != 0) return;
    if (AH_CALL(Area189_ExitButton)() != 0) return;
    if (AH_CALL(Area189_MenuButton)() != 0) return;
    switch (AH_CALL(Area189_TurnInput)()) {
    case 0:
        Cur()[9] = 0;
        AH_CALL(Area189_LeaderHalt)();
        return;
    case 1: {
        Field_State[0x137] = 1;
        unsigned char* const cur = Cur();
        SetAngleFB((static_cast<U>(Cond_AngleFB) + static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(cur[0xB])))) & 0xFFF);
        cur[2] = 4;
        return;
    }
    case 2: break;
    default: return;
    }
    AH_CALL(Area189_StepBegin)();
    const unsigned char* const cur = Cur();
    const U n = cur[9];
    const std::int16_t bx = Block(static_cast<U>(rv::As<records::SpriteHead>(cur)->step_x) * n + static_cast<U>(rv::As<records::SpriteHead>(cur)->x));
    const std::int16_t bz = Block(static_cast<U>(rv::As<records::SpriteHead>(cur)->step_z) * n + static_cast<U>(rv::As<records::SpriteHead>(cur)->z));
    if (bx >= 0x1000 && bx <= 0x1400 && bz >= 0x1800) {
        AH_CALL(Msg_OpenScript)(2);
        Field_Request = 2;
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    if (Test(Row14(), 4) && bx >= 0x1500 && bx <= 0x1900 && bz <= 0xE00) {
        AH_CALL(Msg_OpenScript)(3);
        Field_Request = 2;
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    AH_CALL(Area189_StepMove)();
    Field_State[0x137] = 1;
    Cur()[2] = 3;
}

// original 0x42AD30 (Area189_LeaderStates 3): Field_State +0x137 = 1; +9 at
// 0: a tail jump to Area189_StepArrive; else +9 - 1 and a tail jump to
// Area189_StepMove.
extern "C" void __cdecl Area189_LeaderStep(void) {
    Field_State[0x137] = 1;
    unsigned char* const cur = Cur();
    const unsigned char left = cur[9];
    if (left == 0) {
        AH_CALL(Area189_StepArrive)();
        return;
    }
    cur[9] = static_cast<unsigned char>(left - 1);
    AH_CALL(Area189_StepMove)();
}

// original 0x42AD60 (Area189_LeaderStates 4): Cond_AngleFB's low word = (it +
// the s8 +0xB) & 0xFFF; equal to ((2 - +8) & 0xF) << 8: Field_State +0x137 =
// 0, +2 = 2.
extern "C" void __cdecl Area189_LeaderTurn(void) {
    unsigned char* const cur = Cur();
    const U angle = (static_cast<U>(Cond_AngleFB) + static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(cur[0xB])))) & 0xFFF;
    SetAngleFB(angle);
    const U target = ((2u - cur[8]) & 0xF) << 8;
    if (target != angle) return;
    Field_State[0x137] = 0;
    Cur()[2] = 2;
}

// original 0x42ADB0 (called by Area189_LeaderControl): +9 = 8; +0xC / +0x10 =
// Area189_StepVectors[+8] (the whole byte, unchecked) doubled; +0x14 = ((the
// height at the point +9 steps on) << 16 - +0x3C) / +9, signed (+9 read again
// after the height call; ours aborts on 0, where the original divides by it).
extern "C" void __cdecl Area189_StepBegin(void) {
    Cur()[9] = 8;
    unsigned char* cur = Cur();
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->step_x, static_cast<std::int32_t>(static_cast<U>(Long(Mem(::bof3::Img(Area189_StepVectors_addr) + cur[8] * 8u))) << 1));
    cur = Cur();
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->step_z, static_cast<std::int32_t>(static_cast<U>(Long(Mem(::bof3::Img(Area189_StepVectors_addr) + 4 + cur[8] * 8u))) << 1));
    cur = Cur();
    const U n = cur[9];
    const U z = static_cast<U>(rv::As<records::SpriteHead>(cur)->step_z) * n + static_cast<U>(rv::As<records::SpriteHead>(cur)->z);
    const U x = static_cast<U>(rv::As<records::SpriteHead>(cur)->step_x) * n + static_cast<U>(rv::As<records::SpriteHead>(cur)->x);
    const short height = HeightAt(x, z);
    unsigned char* const s = Cur();
    const auto rise = static_cast<std::int32_t>((static_cast<U>(static_cast<std::int32_t>(height)) << 16) - static_cast<U>(rv::As<records::SpriteHead>(s)->height));
    const unsigned steps = s[9];
    if (steps == 0) bof3::Fatal("Area189_StepBegin: the step count +9 is 0 after the height call (the original divides by it)");
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(s)->step_y, rise / static_cast<std::int32_t>(steps));
}

// original 0x42AE30 (called by Area189_LeaderControl, Area189_LeaderStep): x
// += +0xC, Field_Kind2X = x; z += +0x10, Field_Kind2Z = z; +0x3C += +0x14;
// MapView_Redraw 2.
extern "C" void __cdecl Area189_StepMove(void) {
    unsigned char* const cur = Cur();
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->x, static_cast<std::int32_t>(static_cast<U>(rv::As<records::SpriteHead>(cur)->x) + static_cast<U>(rv::As<records::SpriteHead>(cur)->step_x)));
    Field_Kind2X = rv::As<records::SpriteHead>(cur)->x;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->z, static_cast<std::int32_t>(static_cast<U>(rv::As<records::SpriteHead>(cur)->z) + static_cast<U>(rv::As<records::SpriteHead>(cur)->step_z)));
    Field_Kind2Z = rv::As<records::SpriteHead>(cur)->z;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->height, static_cast<std::int32_t>(static_cast<U>(rv::As<records::SpriteHead>(cur)->height) + static_cast<U>(rv::As<records::SpriteHead>(cur)->step_y)));
    MapView_Redraw = 2;
}

// original 0x42AE80 (Area189_LeaderStep's tail jump at +9 = 0): the step's
// arrival (below, docs/area_w4e.md section 1).
extern "C" void __cdecl Area189_StepArrive(void) {
    AH_CALL(Area189_ZeroSpeeds)();
    // the frame word: 0x1E0 - the reserve + 8 if any, Field_StatusBits bit 0
    // and Cond_ByteFF cleared, message 5; 0x3C0 - the reserve + 8 if any, bit
    // 0 and Cond_ByteFF set, the word 0, the count + 1, message 6
    const auto frames = static_cast<std::uint16_t>(Word(Mem(::bof3::Img(Cond_Flags_addr) + 0xcc)) + 1);
    Field_StepCount = static_cast<unsigned short>(Field_StepCount + 1);
    SetWord(Mem(::bof3::Img(Cond_Flags_addr) + 0xcc), frames);
    if (frames == 0x1E0) {
        const unsigned char reserve = B(::bof3::Img(Cond_Flags_addr) + 0xcf);
        if (reserve != 0) B(::bof3::Img(Cond_Flags_addr) + 0xcf) = static_cast<unsigned char>(reserve + 8);
        const unsigned char bits = Field_StatusBits;
        Cond_ByteFF = 0;
        Field_StatusBits = static_cast<unsigned char>(bits & 0xFE);
        AH_CALL(Msg_OpenScript)(5);
        Field_Request = 2;
    }
    if (Word(Mem(::bof3::Img(Cond_Flags_addr) + 0xcc)) == 0x3C0) {
        const unsigned char reserve = B(::bof3::Img(Cond_Flags_addr) + 0xcf);
        if (reserve != 0) B(::bof3::Img(Cond_Flags_addr) + 0xcf) = static_cast<unsigned char>(reserve + 8);
        const unsigned char bits = Field_StatusBits;
        const unsigned char count = B(::bof3::Img(at::kWalkCount.v));
        Field_StatusBits = static_cast<unsigned char>(bits | 1);
        Cond_ByteFF = 1;
        SetWord(Mem(::bof3::Img(Cond_Flags_addr) + 0xcc), 0);
        B(::bof3::Img(at::kWalkCount.v)) = static_cast<unsigned char>(count + 1);
        AH_CALL(Msg_OpenScript)(6);
        Field_Request = 2;
    }
    // the reserve counts down (3 a step with Cond_ByteFF 0, else 1, not below
    // 0); at 0 message 1 and the halt. None: an event every (Rand() & 7) + 20
    // * Cond_ByteFF + 7 steps - the fifth in a row Area189_RaiseByte1E,
    // Cond_ByteFE 2 and sound 0x10B; the others Area189_DrainHp, Cond_ByteFE
    // 1 and sound 0x108.
    unsigned char reserve = B(::bof3::Img(Cond_Flags_addr) + 0xcf);
    if (reserve != 0) {
        for (unsigned n = Cond_ByteFF == 0 ? 3 : 1; n != 0; --n) {
            if (reserve != 0) --reserve;
        }
        B(::bof3::Img(Cond_Flags_addr) + 0xcf) = reserve;
        B(::bof3::Img(Area189_StepsSinceEvent_addr)) = 0;
        B(::bof3::Img(Area189_EventsInRow_addr)) = 0;
        if (reserve == 0) {
            AH_CALL(Msg_OpenScript)(1);
            Field_Request = 2;
            AH_CALL(Area189_LeaderHalt)();
            return;
        }
    } else {
        const U r = static_cast<U>(AH_CALL(Rand)()) & 7;
        const auto limit = static_cast<std::int32_t>(r + Cond_ByteFF * 20u + 7u);
        const unsigned char steps = B(::bof3::Img(Area189_StepsSinceEvent_addr));
        if (static_cast<std::int32_t>(steps) < limit) {
            B(::bof3::Img(Area189_StepsSinceEvent_addr)) = static_cast<unsigned char>(steps + 1);
        } else {
            const unsigned char events = B(::bof3::Img(Area189_EventsInRow_addr));
            B(::bof3::Img(Area189_StepsSinceEvent_addr)) = 0;
            if (events >= 4) {
                AH_CALL(Area189_RaiseByte1E)();
                Cond_ByteFE = 2;
                AH_CALL(Sound_PlayEffect)(0x10B);
                B(::bof3::Img(Area189_EventsInRow_addr)) = 0;
            } else {
                AH_CALL(Area189_DrainHp)();
                Cond_ByteFE = 1;
                AH_CALL(Sound_PlayEffect)(0x108);
                B(::bof3::Img(Area189_EventsInRow_addr)) = static_cast<unsigned char>(B(::bof3::Img(Area189_EventsInRow_addr)) + 1);
            }
        }
    }
    if (Field_Request == 2) {
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    // Cond_Flags row 14 bit 4 clear, the x block 0x1600 or 0x1700 and the z
    // block at most 0x1000: Scena14_LeaveToC4 and the halt.
    const bool bit4 = Test(Row14(), 4);
    unsigned char* cur = Cur();
    if (!bit4) {
        const std::int16_t bx = BlockWord(cur + 0x36);
        if ((bx == 0x1600 || bx == 0x1700) && BlockWord(cur + 0x3A) <= 0x1000) {
            AH_CALL(Scena14_LeaveToC4)();
            AH_CALL(Area189_LeaderHalt)();
            return;
        }
    }
    if (AH_CALL(Scenario_ArriveHook)(rv::As<records::SpriteHead>(cur)->x, rv::As<records::SpriteHead>(cur)->z) != 0) {
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    // each high word moved a block inward when at or past an edge: x at most
    // 0x900 or from 0x1F00, z at most 0xD00 or from 0x1900 (the blocks,
    // signed)
    cur = Cur();
    if (BlockWord(cur + 0x36) <= 0x900) AddWord(cur + 0x36, 0x100);
    if (BlockWord(cur + 0x3A) <= 0xD00) AddWord(cur + 0x3A, 0x100);
    if (BlockWord(cur + 0x36) >= 0x1F00) AddWord(cur + 0x36, -0x100);
    if (BlockWord(cur + 0x3A) >= 0x1900) AddWord(cur + 0x3A, -0x100);
    Field_Kind2X = rv::As<records::SpriteHead>(cur)->x;
    const unsigned flags = Field_ScriptFlags;
    Field_Kind2Z = rv::As<records::SpriteHead>(cur)->z;
    MapView_Redraw = 2;
    if ((flags & 0x20) == 0 && Field_StepCount >= Word(Mem(::bof3::Img(ObjTrio_addr) + 0x134))) {
        // the pace's steps walked: the edge count 0; a flag for each of six
        // blocks the leader stands on; area 0x7D at (0x190000, 0x190000),
        // flags 4, the facing jittered
        Field_StepCount = 0;
        static const struct { std::int16_t x, z; unsigned char flag; } kMarks[] = {
            {0xB00, 0xF00, 0x75}, {0x1500, 0x1100, 0x76}, {0xC00, 0x1400, 0x8B}, {0x1000, 0x1100, 0x8C}, {0x1D00, 0x1700, 0x8D}, {0x1400, 0x1400, 0x8E}};
        for (const auto& m : kMarks) {
            if (BlockWord(cur + 0x36) != m.x || BlockWord(cur + 0x3A) != m.z) continue;
            AH_CALL(Flags_Set)(StoryFlags(), m.flag);
            cur = Cur();
        }
        SetWord(Mem(::bof3::Img(AreaW0b_Place_addr)), 0x7D);
        SetLong(Mem(::bof3::Img(at::kPendingX.v)), 0x190000);
        SetLong(Mem(::bof3::Img(AreaEntry_PendingZ_addr)), 0x190000);
        B(::bof3::Img(AreaEntry_PendingFlags_addr)) = 4;
        JitterFacing();
        ChangeToPending();
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    if ((flags & 0x100) != 0 || (Field_ScriptFlags2 & 0x40) != 0) {
        AH_CALL(Area189_LeaderHalt)();
        return;
    }
    if ((Field_InputHeld & (Word(Mem(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f000)) | 0xB000u)) != 0) {
        AH_CALL(Area189_LeaderControl)();
        return;
    }
    AH_CALL(Area189_LeaderHalt)();
}

// original 0x42B2F0 (called and tail-jumped to by Area189_LeaderControl and
// Area189_StepArrive): Area189_ZeroSpeeds; +0x4B = 0; Field_State +0x137 = 0;
// +2 = 2.
extern "C" void __cdecl Area189_LeaderHalt(void) {
    AH_CALL(Area189_ZeroSpeeds)();
    Cur()[0x4B] = 0;
    Field_State[0x137] = 0;
    Cur()[2] = 2;
}

// original 0x42B320 (called by Area189_LeaderControl): Input_Pressed bit 11:
// the place Area189_ExitPlaces[Cond_ByteFF 0 ? 1 : 0] (area, x and z bytes
// << 16) pending with flags 4; the leader's x, z and Game_AreaNumber saved to
// 0x904148 / 0x90414C / 0x904150, the byte 0x904152 + 1; the facing
// jittered; Field_ChangeArea from the pending cells; al 1. Else al 0.
extern "C" unsigned char __cdecl Area189_ExitButton(void) {
    if ((Input_Pressed & 0x800) == 0) return 0;
    const unsigned e = Cond_ByteFF == 0 ? 1u : 0u;
    B(::bof3::Img(AreaEntry_PendingFlags_addr)) = 4;
    const U place = ::bof3::Img(Area189_ExitPlaces_addr) + e * 3;
    SetWord(Mem(::bof3::Img(AreaW0b_Place_addr)), B(place));
    SetLong(Mem(::bof3::Img(at::kPendingX.v)), static_cast<std::int32_t>(static_cast<U>(B(place + 1)) << 16));
    SetLong(Mem(::bof3::Img(AreaEntry_PendingZ_addr)), static_cast<std::int32_t>(static_cast<U>(B(place + 2)) << 16));
    const unsigned char* const cur = Cur();
    SetLong(Mem(::bof3::Img(AreaW2d_ReturnPoint_addr)), rv::As<records::SpriteHead>(cur)->x);
    SetLong(Mem(::bof3::Img(AreaW4d_ReturnZ_addr)), rv::As<records::SpriteHead>(cur)->z);
    const unsigned char saved = B(::bof3::Img(at::kSavedCount.v));
    SetWord(Mem(::bof3::Img(AreaW4d_ReturnArea_addr)), Game_AreaNumber);
    B(::bof3::Img(at::kSavedCount.v)) = static_cast<unsigned char>(saved + 1);
    JitterFacing();
    ChangeToPending();
    return 1;
}

// original 0x42B400 (called by Area189_LeaderControl): Field_MenuButton
// pressed (Input_Pressed) outside Field_ScriptFlags bit 6:
// Sound_PlayEffect(0x105), Field_Request 1, +2 = 2 (Sprite_Current read after
// the call), al 1. Else al 0.
extern "C" unsigned char __cdecl Area189_MenuButton(void) {
    if ((Field_MenuButton & Input_Pressed) == 0) return 0;
    if ((Field_ScriptFlags & 0x40) != 0) return 0;
    AH_CALL(Sound_PlayEffect)(0x105);
    unsigned char* const cur = Cur();
    Field_Request = 1;
    cur[2] = 2;
    return 1;
}

// original 0x42B440 (called by Area189_LeaderStart, _StepArrive, _LeaderHalt):
// +0xC, +0x10, +0x14 = 0.
extern "C" void __cdecl Area189_ZeroSpeeds(void) {
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_x, 0);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_z, 0);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_y, 0);
}

// original 0x42B460 (called by Area189_LeaderControl): Input_Held bit 12:
// +0x4B = 1, al 2. Bit 13: +0x4B = 2, +8 + 1, +0xB = 0xF0; else bit 15: +0x4B =
// 3, +8 - 1, +0xB = 0x10 - either: +8 &= 0xF, al 1. None: al 0.
extern "C" unsigned char __cdecl Area189_TurnInput(void) {
    const unsigned held = Input_Held;
    if ((held & 0x1000) != 0) {
        Cur()[0x4B] = 1;
        return 2;
    }
    if ((held & 0x2000) != 0) {
        Cur()[0x4B] = 2;
        Cur()[8] = static_cast<unsigned char>(Cur()[8] + 1);
        Cur()[0xB] = 0xF0;
    } else if ((held & 0x8000) != 0) {
        Cur()[0x4B] = 3;
        Cur()[8] = static_cast<unsigned char>(Cur()[8] - 1);
        Cur()[0xB] = 0x10;
    } else {
        return 0;
    }
    Cur()[8] = static_cast<unsigned char>(Cur()[8] & 0xF);
    return 1;
}

// original 0x42B4E0 (called by Area189_StepArrive): for each of the seven
// actor records walked, the byte +0x1E below 9: + 1 and Char_RecalcStats.
extern "C" void __cdecl Area189_RaiseByte1E(void) {
    for (unsigned r = 0; r < at::kCharWalked; ++r) {
        unsigned char& b = B(::bof3::Img(Field_ActorStates_addr) + 0xe + r * at::kCharStride);
        if (b >= 9) continue;
        b = static_cast<unsigned char>(b + 1);
        AH_CALL(Char_RecalcStats)(Mem(::bof3::Img(CharacterRecords_addr) + r * at::kCharStride));
    }
}

// original 0x42B510 (called by Area189_StepArrive): for each of the seven
// actor records walked, d = (+0x40 * 2 + 0x32) / 100; the word +0x18 above d:
// - d; else 1 (0 included).
extern "C" void __cdecl Area189_DrainHp(void) {
    for (unsigned r = 0; r < at::kCharWalked; ++r) {
        unsigned char* const w = Mem(::bof3::Img(Field_ActorStates_addr) + 0x8 + r * at::kCharStride);
        const U bound = Word(w + 0x28);
        U d = (bound * 2u + 0x32u) / 100u;
        const U hp = Word(w);
        if (hp <= (d & 0xFFFF)) d = hp - 1;
        SetWord(w, hp - d);
    }
}

// original 0x42B550 (areas 191 and 192 +0x34[0]): message 0x68 for answer 0,
// else 0xFFFF.
extern "C" void __cdecl Area191_ChoiceMessage68(void) { SetMessage(Answer() != 0 ? 0xFFFF : 0x68); }

void Area189_Inject() {
    BOF3_INJECT(Area189_Init);
    BOF3_INJECT(Area189_ChoiceTail50A);
    BOF3_INJECT(Area189_ChoiceTail50B);
    BOF3_INJECT(Area189_Tail50);
    BOF3_INJECT(Area189_LeaderRun);
    BOF3_INJECT(Area189_LeaderStart);
    BOF3_INJECT(Area189_LeaderProject);
    BOF3_INJECT(Area189_LeaderControl);
    BOF3_INJECT(Area189_LeaderStep);
    BOF3_INJECT(Area189_LeaderTurn);
    BOF3_INJECT(Area189_StepBegin);
    BOF3_INJECT(Area189_StepMove);
    BOF3_INJECT(Area189_StepArrive);
    BOF3_INJECT(Area189_LeaderHalt);
    BOF3_INJECT(Area189_ExitButton);
    BOF3_INJECT(Area189_MenuButton);
    BOF3_INJECT(Area189_ZeroSpeeds);
    BOF3_INJECT(Area189_TurnInput);
    BOF3_INJECT(Area189_RaiseByte1E);
    BOF3_INJECT(Area189_DrainHp);
    BOF3_INJECT(Area191_ChoiceMessage68);
}
