// Moved here by tools/move_funcs.py from area_w2c.cpp (docs/source-layout-round.md).
#include "game/area/area_91.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/behaviour.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

// The dword at `cell` as a pointer.
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The choice's answer as the choices read it with movsx.
int Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// A dword of `v` shifted right as the originals' `sar` after an `imul`: the
// 32-bit product (wrapping), then an arithmetic shift.
std::int32_t MulSar(std::int32_t v, U by, unsigned n) {
    return static_cast<std::int32_t>(static_cast<U>(v) * by) >> n;
}

// fild the int, fadd the float, fstp to a float: at the game's 53-bit x87
// precision the IEEE double add, rounded once to a float (psx_gte_float.cpp).
float AddFloat(std::int32_t v, float f) { return static_cast<float>(static_cast<double>(v) + static_cast<double>(f)); }

void StoreFloat(unsigned char* at, float v) { std::memcpy(at, &v, sizeof v); }

// A state table's entry by index, or a Fatal past its count (area 91's
// handler 0 dispatches through one unchecked).
using Handler = void (__cdecl*)();

Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X (the original jumps to itself forever at %u, "
                    "through the descriptor's words above it)",
                    who, index, count, static_cast<unsigned>(table), count);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// Area 91's CLUT strips as words, by an index that may run before a row.
unsigned SourceAt(std::int32_t i) { return Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + static_cast<U>(i) * 2u)); }

void SetStripAt(std::int32_t i, unsigned v) { SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + static_cast<U>(i) * 2u), v); }

}  // namespace

// original 0x411F10 (area 90 +0x34[0], a choice): the message word 0xFFFF;
// the object whose talk opened the box (0x903804) gets +0x18 and +0x1C (dwords)
// from the byte pair of Area90_FocusPairs by the s8 answer, unchecked - the
// pointer and the answer read again for the second.
extern "C" void __cdecl Area90_ChoiceFocusPair(void) {
    unsigned char* focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const unsigned first = B(::bof3::Img(Area90_FocusPairs_addr) + static_cast<U>(Answer()) * 2u);
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, static_cast<std::int32_t>(first));
    focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    SetLong(focus + 0x1C, B(::bof3::Img(Area90_FocusPairs_addr) + 1u + static_cast<U>(Answer()) * 2u));
}

// original 0x411F50 (area 91 +0x34[0]): answer (byte) not 0: message 7 and the
// mark 6. 0: Inventory_Count(0, 0x59, 0) as a word not 0: Inventory_Remove(0,
// 0x59, 1) and message 5; else message 6 and the mark 6.
extern "C" void __cdecl Area91_ChoiceTakeItem59(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(7);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    if (AH_CALL(Inventory_Count)(0, 0x59, 0) != 0) {
        AH_CALL(Inventory_Remove)(0, 0x59, 1);
        SetMessage(5);
        return;
    }
    SetMessage(6);
    B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
}

// original 0x411FB0 (area 91 +0x34[3] and [4]): answer not 0: message 7 and
// the mark 6; 0: message 0xFFFF.
extern "C" void __cdecl Area91_ChoiceMessage7(void) {
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(7);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x411FE0 (area 91 +0x3C[0], PSX 0x801F2D6C): jmp [Area91_States +
// Sprite_Current[4] * 4], unchecked. The six states; a state of 6 names the
// descriptor's +0x3C array, whose one entry is this handler (the original
// loops forever), 7 and above the descriptor's other words: ours aborts.
extern "C" void __cdecl Area91_ObjectRun(void) {
    StateEntry("Area91_ObjectRun", ::bof3::Img(Area91_States_addr), at::kArea91StateCount, Sprite_Current[4])();
}

// original 0x412000 (Area91_States 0): the party list's first byte not 6 or 3:
// the active member's +0x80 bit 0 cleared. Else Inventory_Count(0, 0x57, 0) as
// a word 0: Cond_ByteFE = 1, the member's bit cleared (the member read before
// that store), Sound_PlayEffect(0x201); not 0: ScriptFlags_Set40 and
// Sprite_Current +4 (the state) + 1.
//
// DIV-0220 EasierMasters (off by default): with the first byte 6 or 3 the
// item is not counted and the "held" path is taken - the Definitive Edition
// romhack's change; evidence: its patched US disc, AREA091 section 13 at PSX
// 0x801F2DFC (the count's call replaced by ScriptFlags_Set40, whose non-zero
// return takes the branch; the path's own ScriptFlags_Set40 then ORs the same
// bits again, so one call here leaves the same state).
extern "C" void __cdecl Area91_State0CheckItem57(void) {
    const unsigned char list = B(::bof3::Img(Cond_Flags_addr) + 0xd2);
    if (list != 6 && list != 3) {
        Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
        return;
    }
    if (!bof3::BehaviourOn(bof3::behaviours::Id::EasierMasters) && AH_CALL(Inventory_Count)(0, 0x57, 0) == 0) {
        unsigned char* const member = Field_ActiveMember;
        Cond_ByteFE = 1;
        member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
        AH_CALL(Sound_PlayEffect)(0x201);
        return;
    }
    AH_CALL(ScriptFlags_Set40)();
    ++Sprite_Current[4];
}

// original 0x412060 (Area91_States 1): the leader's byte +0x137 0:
// Sound_PlayEffect(0x200), then Sprite_Current (read again for each) +0xA = 0,
// +0xB = 0, +4 + 1.
extern "C" void __cdecl Area91_State1Arm(void) {
    if (B(::bof3::Img(ObjTrio_addr) + 0x137) != 0) return;
    AH_CALL(Sound_PlayEffect)(0x200);
    Sprite_Current[0xA] = 0;
    Sprite_Current[0xB] = 0;
    ++Sprite_Current[4];
}

// original 0x4120A0 (Area91_States 2): +0xA + 1; at 0x1E: story flag 0x6B
// clear - set it and Party_DropIn(0), set - Party_DropIn(1); then
// Sprite_Current put back, Field_Kind2X / Z = (0x1F8000, 0x1A0000),
// MoveScript_F3Divisor 0x20, +4 + 1 and Sprite_Current read again. Always
// Area91_DrawGlow(+0xA * 10, 0xFF).
extern "C" void __cdecl Area91_State2Grow(void) {
    ++Sprite_Current[0xA];
    unsigned char* object = Sprite_Current;
    if (object[0xA] == 0x1E) {
        if (AH_CALL(Flags_Test)(StoryFlags(), 0x6B) == 0) {
            AH_CALL(Flags_Set)(StoryFlags(), 0x6B);
            AH_CALL(Party_DropIn)(0);
        } else {
            AH_CALL(Party_DropIn)(1);
        }
        Sprite_Current = object;
        Field_Kind2X = 0x1F8000;
        Field_Kind2Z = 0x1A0000;
        MoveScript_F3Divisor = 0x20;
        ++object[4];
        object = Sprite_Current;
    }
    AH_CALL(Area91_DrawGlow)(object[0xA] * 10u, 0xFF);
}

// original 0x412140 (called by Area91_States 2..5): a glow at (0x1F8000,
// 0x1A0000). Gte_PushMatrix; a stack MATRIX whose translation is
// Gte_RotTrans((0xCFC0, 0xCD00, 0x80)) and whose rotation is Gte_RotMatrix
// of (0, 0, 0), multiplied by Camera_Matrix (Gte_MulMatrix0) and set
// (Gte_SetRotMatrix, Gte_SetTransMatrix); a draw mode (tpage 0x3E) linked at
// the place (size 0xC). Then 32 semi-transparent POLY_G3s, a fan of radius
// r = the first argument's word: the centre (0, 0, 0) and the rim points
// ((cos a * r) >> 12, (sin a * r) >> 12, 0) of angles a and a + 0x80, through
// Prim_VertexScratch and Gte_RotTransPers3 into the primitive at
// Gfx_PacketNext (read again for each), Gte_PrimDepths3_10B; the centre's
// colour (c, c, 0) with c the second argument's byte, the rim's (0, 0, 0);
// each linked at the place (size 0x34). Gte_PopMatrix.
extern "C" void __cdecl Area91_DrawGlow(unsigned radius_word, unsigned colour_byte) {
    short vector[4] = {static_cast<short>(0xCFC0), static_cast<short>(0xCD00), 0x80, 0};
    short angles[4] = {0, 0, 0, 0};
    alignas(4) short matrix[16] = {};   // a PSX MATRIX: nine s16, padding, three s32 at +0x14
    long depth = 0;
    AH_CALL(Gte_PushMatrix)();
    AH_CALL(Gte_RotTrans)(vector, reinterpret_cast<long*>(matrix + 10));
    AH_CALL(Gte_RotMatrix)(angles, matrix);
    AH_CALL(Gte_MulMatrix0)(Camera_Matrix, matrix, matrix);
    AH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    AH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x3E, 0);
    AH_CALL(MapView_LinkPrimAt)(0x1F8000, 0x1A0000, 2, 0xC);
    const U radius = radius_word & 0xFFFF;
    const auto colour = static_cast<unsigned char>(colour_byte);
    for (int angle = 0; angle < 0x1000; angle += 0x80) {
        const int next = angle + 0x80;
        Prim_VertexScratch[0].x = 0;
        Prim_VertexScratch[0].y = 0;
        Prim_VertexScratch[0].z = 0;
        Prim_VertexScratch[1].x = static_cast<short>(MulSar(AH_CALL(Math_Cos)(angle), radius, 12));
        const std::int32_t sin_a = MulSar(AH_CALL(Math_Sin)(angle), radius, 12);
        Prim_VertexScratch[1].z = 0;
        Prim_VertexScratch[1].y = static_cast<short>(sin_a);
        Prim_VertexScratch[2].x = static_cast<short>(MulSar(AH_CALL(Math_Cos)(next), radius, 12));
        const std::int32_t sin_b = MulSar(AH_CALL(Math_Sin)(next), radius, 12);
        Prim_VertexScratch[2].z = 0;
        Prim_VertexScratch[2].y = static_cast<short>(sin_b);
        unsigned char* const prim = Gfx_PacketNext;
        AH_CALL(Gpu_SetPolyG3)(prim);
        AH_CALL(Gpu_SetSemiTrans)(prim, 1);
        AH_CALL(Gte_RotTransPers3)(reinterpret_cast<short*>(Prim_VertexScratch), reinterpret_cast<short*>(&Prim_VertexScratch[1]), reinterpret_cast<short*>(&Prim_VertexScratch[2]),
                                   reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
                                   reinterpret_cast<float*>(prim + 0x28), &depth);
        AH_CALL(Gte_PrimDepths3_10B)(prim);
        prim[4] = colour;
        prim[5] = colour;
        prim[6] = 0;
        prim[0x14] = 0;
        prim[0x15] = 0;
        prim[0x16] = 0;
        prim[0x24] = 0;
        prim[0x25] = 0;
        prim[0x26] = 0;
        AH_CALL(MapView_LinkPrimAt)(0x1F8000, 0x1A0000, 2, 0x34);
    }
    AH_CALL(Gte_PopMatrix)();
}

// original 0x412310 (Area91_States 3): Field_Kind2Hold 0: Sprite_Current (read
// again for each) +0x70 = 0, at (0x1F8000, 0x1A0000), word +0x3E 0xFF00, +0
// bit 6 cleared, +4 + 1; then with that object: the 0x20 words of its CLUT
// row (+0x27, read each pass) in Gfx_ClutStrip zeroed, Gfx_ClutStripDirty + 1,
// +0xB = 0, and Sprite_Current (read again) +0xA = 2. Always
// Area91_DrawGlow(0x12C, 0xFF).
extern "C" void __cdecl Area91_State3Place(void) {
    if (Field_Kind2Hold == 0) {
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->reach, 0);
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, 0x1F8000);
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, 0x1A0000);
        rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, 0xFF00);
        Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
        ++Sprite_Current[4];
        unsigned char* const object = Sprite_Current;
        for (std::int32_t i = 0; i < 0x20; ++i) SetStripAt((object[0x27] << 5) + i, 0);
        ++Gfx_ClutStripDirty;
        object[0xB] = 0;
        Sprite_Current[0xA] = 2;
    }
    AH_CALL(Area91_DrawGlow)(0x12C, 0xFF);
}

// original 0x4123D0 (Area91_States 4): +0xA - 1; with b = +0xB and the row
// R = +0x27 << 5: the words R + b and R - b + 0x1F of Gfx_ClutStripSource
// (halved, & 0x3DEF, while +0xA is not 0) to the same places of
// Gfx_ClutStrip - the second place computed again from the object's bytes.
// At +0xA 0: +0xA = 2 and +0xB + 1; at 0x10, +0xA = 0x1E and +4 + 1
// (Sprite_Current read again for each). Always Gfx_ClutStripDirty + 1 and
// Area91_DrawGlow(0x12C, 0xFF).
extern "C" void __cdecl Area91_State4RevealClut(void) {
    --Sprite_Current[0xA];
    unsigned char* const object = Sprite_Current;
    const std::int32_t row = object[0x27] << 5;
    const std::int32_t b = object[0xB];
    unsigned low = SourceAt(row + b), high = SourceAt(row - b + 0x1F);
    if (object[0xA] != 0) {
        low = (low >> 1) & 0x3DEF;
        high = (high >> 1) & 0x3DEF;
    }
    SetStripAt(row + b, low);
    SetStripAt((object[0x27] << 5) - object[0xB] + 0x1F, high);
    if (object[0xA] == 0) {
        object[0xA] = 2;
        ++Sprite_Current[0xB];
        if (Sprite_Current[0xB] == 0x10) {
            Sprite_Current[0xA] = 0x1E;
            ++Sprite_Current[4];
        }
    }
    ++Gfx_ClutStripDirty;
    AH_CALL(Area91_DrawGlow)(0x12C, 0xFF);
}

// original 0x412500 (Area91_States 5): +0xA - 1; at 0: counter 3 = 1, +4 = 0
// (state 0), the active member's +0x80 bit 0 cleared and its word +0x8A + 1
// (the member read again), ScriptFlags_Clear40, Sprite_Current read again.
// Always Area91_DrawGlow(+0xA * 10, 0xFF).
extern "C" void __cdecl Area91_State5Shrink(void) {
    --Sprite_Current[0xA];
    unsigned char* object = Sprite_Current;
    if (object[0xA] == 0) {
        B(::bof3::Img(at::kCounter3.v)) = 1;
        object[4] = 0;
        Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
        unsigned char* const member = Field_ActiveMember;
        auto* const tMember = rv::As<records::SpriteObject>(member);
        tMember->script_position = tMember->script_position + 1u;
        AH_CALL(ScriptFlags_Clear40)();
        object = Sprite_Current;
    }
    AH_CALL(Area91_DrawGlow)(object[0xA] * 10u, 0xFF);
}

// original 0x412570 (Field_ObjectTriggers id 49): ScriptFlags_Set40, tail kind
// 4 with sub-kind 0x10; al 0.
extern "C" unsigned char __cdecl Area91_Trigger49(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0x10;
    return 0;
}

// original 0x412590 (EffectKind18_States 71, 0x654188; Sprite_Current an
// effect record): while Cond_ByteFE is not 0. The record's place (+0x34 /
// +0x38 >> 9, less 0x4000; height 0x80) through Prim_VertexScratch and
// Gte_RotTransPers into MapView_ScreenXY; a draw mode (tpage 0xB5, dtd 1)
// linked (size 0xC); +9 + 4. Three rings k = 0, 1, 2 (+9 read again for
// each): radius r = max(+9 - 0x20k, 0), colour c = clamp(0x10k - +9 + 0xC8,
// 0, 0xFF). Each: 64 semi-transparent LINE_F2s of colour (c/2, c/2, c) around
// an ellipse (x = (sin a * r) >> 14, y = (cos a * r) >> 15 about the screen
// point, a in steps of 0x40), the depth by Gte_StoreDepthF to both ends
// (+0x10 copied to +0x1C), each linked (size 0x20); then 16 TILE_1s of colour
// (c, c, c) at x = (sin t * r) >> 13, y = (cos t * r) >> 14 less r / 2, with
// t = (Frame_Counter & 0xF) * 16 * Area91_RingTurns[k] + 0x100 i (the counter
// and the turn read again for the cosine), each linked (size 0x14). The
// sines and cosines are x87: fild, fadd the screen float, fstp. A draw mode
// (tpage 0xB5) linked last; the last ring's colour 0: Cond_ByteFE = 0.
extern "C" void __cdecl Area91_EffectRings(void) {
    if (Cond_ByteFE == 0) return;
    {
        unsigned char* const object = Sprite_Current;
        auto* const tObject = rv::As<records::EffectObject>(object);
        Prim_VertexScratch[0].x = static_cast<short>((tObject->x >> 9) - 0x4000);
        Prim_VertexScratch[0].y = static_cast<short>((tObject->z >> 9) - 0x4000);
        Prim_VertexScratch[0].z = 0x80;
    }
    long depth = 0;
    AH_CALL(Gte_RotTransPers)(reinterpret_cast<short*>(Prim_VertexScratch), reinterpret_cast<unsigned long*>(MapView_ScreenXY), &depth);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(rv::Current<records::EffectObject>()->x), static_cast<U>(rv::Current<records::EffectObject>()->z), 2, 0xC);
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] + 4);
    std::int32_t ring = 0, inner = 0, fade = 0, colour = 0;
    do {
        const std::int32_t b9 = Sprite_Current[9];
        const std::int32_t reach = b9 - inner;
        const std::int32_t radius = reach < 0 ? 0 : reach;
        std::int32_t c = fade - b9 + 0xC8;
        if (c > 0xFF) c = 0xFF;
        colour = c < 0 ? 0 : c;
        const std::int32_t half_colour = colour / 2;
        for (std::int32_t angle = 0; angle < 0x1000;) {
            unsigned char* const prim = Gfx_PacketNext;
            AH_CALL(Gpu_SetLineF2)(prim);
            AH_CALL(Gpu_SetSemiTrans)(prim, 1);
            prim[6] = static_cast<unsigned char>(colour);
            prim[4] = static_cast<unsigned char>(half_colour);
            prim[5] = static_cast<unsigned char>(half_colour);
            AH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x10));
            SetLong(prim + 0x1C, Long(prim + 0x10));
            const std::int32_t x0 = MulSar(AH_CALL(Math_Sin)(angle), static_cast<U>(radius), 14);
            StoreFloat(prim + 8, AddFloat(x0, MapView_ScreenXY[0]));
            const std::int32_t y0 = MulSar(AH_CALL(Math_Cos)(angle), static_cast<U>(radius), 15);
            angle += 0x40;
            StoreFloat(prim + 0xC, AddFloat(y0, MapView_ScreenXY[1]));
            const std::int32_t x1 = MulSar(AH_CALL(Math_Sin)(angle), static_cast<U>(radius), 14);
            StoreFloat(prim + 0x14, AddFloat(x1, MapView_ScreenXY[0]));
            const std::int32_t y1 = MulSar(AH_CALL(Math_Cos)(angle), static_cast<U>(radius), 15);
            StoreFloat(prim + 0x18, AddFloat(y1, MapView_ScreenXY[1]));
            AH_CALL(MapView_LinkPrimAt)(static_cast<U>(rv::Current<records::EffectObject>()->x), static_cast<U>(rv::Current<records::EffectObject>()->z), 2, 0x20);
        }
        const std::int32_t half_radius = radius / 2;
        const float lift = static_cast<float>(half_radius);
        for (std::int32_t angle = 0; angle < 0x1000; angle += 0x100) {
            unsigned char* const prim = Gfx_PacketNext;
            AH_CALL(Gpu_SetTile1)(prim);
            AH_CALL(Gpu_SetSemiTrans)(prim, 1);
            prim[4] = static_cast<unsigned char>(colour);
            prim[5] = static_cast<unsigned char>(colour);
            prim[6] = static_cast<unsigned char>(colour);
            AH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x10));
            const std::int32_t turn_x = static_cast<signed char>(B(::bof3::Img(Area91_RingTurns_addr) + static_cast<U>(ring)));
            const std::int32_t t_x = static_cast<std::int32_t>(((Frame_Counter & 0xFu) << 4) * static_cast<U>(turn_x) + static_cast<U>(angle));
            const std::int32_t x = MulSar(AH_CALL(Math_Sin)(t_x), static_cast<U>(radius), 13);
            StoreFloat(prim + 8, AddFloat(x, MapView_ScreenXY[0]));
            const std::int32_t turn_y = static_cast<signed char>(B(::bof3::Img(Area91_RingTurns_addr) + static_cast<U>(ring)));
            const std::int32_t t_y = static_cast<std::int32_t>(((Frame_Counter & 0xFu) << 4) * static_cast<U>(turn_y) + static_cast<U>(angle));
            const std::int32_t y = MulSar(AH_CALL(Math_Cos)(t_y), static_cast<U>(radius), 14);
            const double raised = static_cast<double>(y) + static_cast<double>(MapView_ScreenXY[1]);
            StoreFloat(prim + 0xC, static_cast<float>(raised - static_cast<double>(lift)));
            AH_CALL(MapView_LinkPrimAt)(static_cast<U>(rv::Current<records::EffectObject>()->x), static_cast<U>(rv::Current<records::EffectObject>()->z), 2, 0x14);
        }
        inner += 0x20;
        ++ring;
        fade += 0x10;
    } while (inner < 0x60);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(rv::Current<records::EffectObject>()->x), static_cast<U>(rv::Current<records::EffectObject>()->z), 2, 0xC);
    if (colour == 0) Cond_ByteFE = 0;
}

void Area91_Inject() {
    BOF3_INJECT(Area90_ChoiceFocusPair);
    BOF3_INJECT(Area91_ChoiceTakeItem59);
    BOF3_INJECT(Area91_ChoiceMessage7);
    BOF3_INJECT(Area91_ObjectRun);
    BOF3_INJECT(Area91_State0CheckItem57);
    BOF3_INJECT(Area91_State1Arm);
    BOF3_INJECT(Area91_State2Grow);
    BOF3_INJECT(Area91_DrawGlow);
    BOF3_INJECT(Area91_State3Place);
    BOF3_INJECT(Area91_State4RevealClut);
    BOF3_INJECT(Area91_State5Shrink);
    BOF3_INJECT(Area91_Trigger49);
    BOF3_INJECT(Area91_EffectRings);
}
