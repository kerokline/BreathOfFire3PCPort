// World 2's areas 90, 91, 92 and 94: the PSX's BIN/WORLD02/AREA090..094.EMI
// compiled into the exe at 0x411F10..0x4135B0 (Area_Descriptors entries
// 90..94; area 93's descriptor names no code). Round ten, group AR2C: the
// band's 45 functions, none ours before, each read to its last instruction
// with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w2c.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept (they stay in
// .data); area 91's handler 0 aborts on a state past its six-entry table,
// where the original would jump to itself forever (state 6) or through the
// descriptor's words (docs/area_w2c.md section 6). Every call goes through
// the harness (AH_CALL), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies; the group's own callee (Area91_DrawGlow) is
// called the same way, so each function is fuzzed alone.
#include "game/area_w2c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2c_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2c::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
// The dword at `cell` as a pointer.
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(at::kFlagRow); }
unsigned char* StoryFlags() { return Mem(at::kStoryFlags); }
// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
// The choice's answer as the choices read it with movsx.
int Answer() { return static_cast<signed char>(B(at::kChoiceAnswer)); }
// A message word of a table indexed by the signed answer (unchecked).
unsigned MessageAt(U table, int answer) { return Word(Mem(table + static_cast<U>(answer) * 2u)); }
void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }

// The scene start the choices share: ScriptFlags_Set40, then the movement
// script's four counters 0, the run step byte after MoveScript_Var7 and
// MoveScript_Var7 itself.
void RunStep(unsigned char var7, unsigned char step) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kCounter0) = 0;
    B(at::kCounter1) = 0;
    B(at::kCounter2) = 0;
    B(at::kCounter3) = 0;
    B(at::kVar7Step) = step;
    B(at::kVar7) = var7;
}

// A dword of `v` shifted right as the originals' `sar` after an `imul`: the
// 32-bit product (wrapping), then an arithmetic shift.
std::int32_t MulSar(std::int32_t v, U by, unsigned n) {
    return static_cast<std::int32_t>(static_cast<U>(v) * by) >> n;
}
// fild the int, fadd the float, fstp to a float: at the game's 53-bit x87
// precision the IEEE double add, rounded once to a float (psx_gte_float.cpp).
float AddFloat(std::int32_t v, float f) { return static_cast<float>(static_cast<double>(v) + static_cast<double>(f)); }
void StoreFloat(unsigned char* at, float v) { std::memcpy(at, &v, sizeof v); }

// ---- area 90 ----

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

// Area 92's handlers 0..6: Effect_Spawn's body written out (the kind, 0, a
// signed byte of `args` by the party list byte at `list`, the record's words
// +0x2E / +0x30), for party record `record`, which Sprite_Current is made (and
// left). Unlike Effect_Spawn's callers, the slot goes to the record's +0xB
// before the test for none (0xFF). Every store reads +0xB again.
void SpawnInline(U record, unsigned char kind, U args, U list) {
    Sprite_Current = Mem(record);
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    unsigned char* const r = Sprite_Current;
    if (r[0xB] == 0xFF) return;
    EffectAt(r[0xB])[0] = 1;
    EffectAt(r[0xB])[5] = 6;
    EffectAt(r[0xB])[6] = kind;
    SetLong(EffectAt(r[0xB]) + 0xC, 0);
    SetLong(EffectAt(r[0xB]) + 0x10, static_cast<signed char>(B(args + B(list))));
    SetWord(EffectAt(r[0xB]) + 0x2E, Word(r + 0x2E));
    SetWord(EffectAt(r[0xB]) + 0x30, Word(r + 0x30));
}

// Area 94's spawns: Effect_Spawn(kind, 0, `args`[the list byte], the record's
// words +0x2E, +0x30) with Sprite_Current made the record (and left); a slot
// that is not 0xFF goes to its +0xB (Sprite_Current read again after the call).
void SpawnAt(U record, unsigned char kind, U args, U list) {
    const auto z = static_cast<short>(Word(Mem(record + 0x30)));
    const auto x = static_cast<short>(Word(Mem(record + 0x2E)));
    Sprite_Current = Mem(record);
    const auto b = static_cast<signed char>(B(args + B(list)));
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, b, x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// Area 91's CLUT strips as words, by an index that may run before a row.
unsigned SourceAt(std::int32_t i) { return Word(Mem(at::kClutStripSource + static_cast<U>(i) * 2u)); }
void SetStripAt(std::int32_t i, unsigned v) { SetWord(Mem(at::kClutStrip + static_cast<U>(i) * 2u), v); }

}  // namespace

// ===========================================================================
// Area 90 (descriptor 0x614588; PSX 0x801F40EC)
// ===========================================================================

// original 0x411F10 (area 90 +0x34[0], a choice): the message word 0xFFFF;
// the object whose talk opened the box (0x903804) gets +0x18 and +0x1C (dwords)
// from the byte pair of Area90_FocusPairs by the s8 answer, unchecked - the
// pointer and the answer read again for the second.
extern "C" void __cdecl Area90_ChoiceFocusPair(void) {
    unsigned char* focus = Ptr(at::kFocusObject);
    const unsigned first = B(at::kArea90FocusPairs + static_cast<U>(Answer()) * 2u);
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, static_cast<std::int32_t>(first));
    focus = Ptr(at::kFocusObject);
    SetLong(focus + 0x1C, B(at::kArea90FocusPairs + 1u + static_cast<U>(Answer()) * 2u));
}

// ===========================================================================
// Area 91 (descriptor 0x614748; PSX 0x801F3B60)
// ===========================================================================

// original 0x411F50 (area 91 +0x34[0]): answer (byte) not 0: message 7 and the
// mark 6. 0: Inventory_Count(0, 0x59, 0) as a word not 0: Inventory_Remove(0,
// 0x59, 1) and message 5; else message 6 and the mark 6.
extern "C" void __cdecl Area91_ChoiceTakeItem59(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(7);
        B(at::kAnswerMark) = 6;
        return;
    }
    if (AH_CALL(Inventory_Count)(0, 0x59, 0) != 0) {
        AH_CALL(Inventory_Remove)(0, 0x59, 1);
        SetMessage(5);
        return;
    }
    SetMessage(6);
    B(at::kAnswerMark) = 6;
}

// original 0x411FB0 (area 91 +0x34[3] and [4]): answer not 0: message 7 and
// the mark 6; 0: message 0xFFFF.
extern "C" void __cdecl Area91_ChoiceMessage7(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(7);
        B(at::kAnswerMark) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x411FE0 (area 91 +0x3C[0], PSX 0x801F2D6C): jmp [Area91_States +
// Sprite_Current[4] * 4], unchecked. The six states; a state of 6 names the
// descriptor's +0x3C array, whose one entry is this handler (the original
// loops forever), 7 and above the descriptor's other words: ours aborts.
extern "C" void __cdecl Area91_ObjectRun(void) {
    StateEntry("Area91_ObjectRun", at::kArea91States, at::kArea91StateCount, Sprite_Current[4])();
}

// original 0x412000 (Area91_States 0): the party list's first byte not 6 or 3:
// the active member's +0x80 bit 0 cleared. Else Inventory_Count(0, 0x57, 0) as
// a word 0: Cond_ByteFE = 1, the member's bit cleared (the member read before
// that store), Sound_PlayEffect(0x201); not 0: ScriptFlags_Set40 and
// Sprite_Current +4 (the state) + 1.
extern "C" void __cdecl Area91_State0CheckItem57(void) {
    const unsigned char list = B(at::kPartyList0);
    if (list != 6 && list != 3) {
        Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
        return;
    }
    if (AH_CALL(Inventory_Count)(0, 0x57, 0) == 0) {
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
    if (B(at::kLeader137) != 0) return;
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
        Prim_VertexScratch[0] = 0;
        Prim_VertexScratch[1] = 0;
        Prim_VertexScratch[2] = 0;
        Prim_VertexScratch[4] = static_cast<short>(MulSar(AH_CALL(Math_Cos)(angle), radius, 12));
        const std::int32_t sin_a = MulSar(AH_CALL(Math_Sin)(angle), radius, 12);
        Prim_VertexScratch[6] = 0;
        Prim_VertexScratch[5] = static_cast<short>(sin_a);
        Prim_VertexScratch[8] = static_cast<short>(MulSar(AH_CALL(Math_Cos)(next), radius, 12));
        const std::int32_t sin_b = MulSar(AH_CALL(Math_Sin)(next), radius, 12);
        Prim_VertexScratch[10] = 0;
        Prim_VertexScratch[9] = static_cast<short>(sin_b);
        unsigned char* const prim = Gfx_PacketNext;
        AH_CALL(Gpu_SetPolyG3)(prim);
        AH_CALL(Gpu_SetSemiTrans)(prim, 1);
        AH_CALL(Gte_RotTransPers3)(Prim_VertexScratch, Prim_VertexScratch + 4, Prim_VertexScratch + 8,
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
        SetLong(Sprite_Current + 0x70, 0);
        SetLong(Sprite_Current + 0x34, 0x1F8000);
        SetLong(Sprite_Current + 0x38, 0x1A0000);
        SetWord(Sprite_Current + 0x3E, 0xFF00);
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
        B(at::kCounter3) = 1;
        object[4] = 0;
        Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
        unsigned char* const member = Field_ActiveMember;
        SetWord(member + 0x8A, Word(member + 0x8A) + 1u);
        AH_CALL(ScriptFlags_Clear40)();
        object = Sprite_Current;
    }
    AH_CALL(Area91_DrawGlow)(object[0xA] * 10u, 0xFF);
}

// original 0x412570 (Field_ObjectTriggers id 49): ScriptFlags_Set40, tail kind
// 4 with sub-kind 0x10; al 0.
extern "C" unsigned char __cdecl Area91_Trigger49(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 4;
    B(at::kTailSub) = 0x10;
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
        Prim_VertexScratch[0] = static_cast<short>((Long(object + 0x34) >> 9) - 0x4000);
        Prim_VertexScratch[1] = static_cast<short>((Long(object + 0x38) >> 9) - 0x4000);
        Prim_VertexScratch[2] = 0x80;
    }
    long depth = 0;
    AH_CALL(Gte_RotTransPers)(Prim_VertexScratch, reinterpret_cast<unsigned long*>(MapView_ScreenXY), &depth);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(Long(Sprite_Current + 0x34)), static_cast<U>(Long(Sprite_Current + 0x38)), 2, 0xC);
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
            AH_CALL(MapView_LinkPrimAt)(static_cast<U>(Long(Sprite_Current + 0x34)), static_cast<U>(Long(Sprite_Current + 0x38)), 2, 0x20);
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
            const std::int32_t turn_x = static_cast<signed char>(B(at::kArea91RingTurns + static_cast<U>(ring)));
            const std::int32_t t_x = static_cast<std::int32_t>(((Frame_Counter & 0xFu) << 4) * static_cast<U>(turn_x) + static_cast<U>(angle));
            const std::int32_t x = MulSar(AH_CALL(Math_Sin)(t_x), static_cast<U>(radius), 13);
            StoreFloat(prim + 8, AddFloat(x, MapView_ScreenXY[0]));
            const std::int32_t turn_y = static_cast<signed char>(B(at::kArea91RingTurns + static_cast<U>(ring)));
            const std::int32_t t_y = static_cast<std::int32_t>(((Frame_Counter & 0xFu) << 4) * static_cast<U>(turn_y) + static_cast<U>(angle));
            const std::int32_t y = MulSar(AH_CALL(Math_Cos)(t_y), static_cast<U>(radius), 14);
            const double raised = static_cast<double>(y) + static_cast<double>(MapView_ScreenXY[1]);
            StoreFloat(prim + 0xC, static_cast<float>(raised - static_cast<double>(lift)));
            AH_CALL(MapView_LinkPrimAt)(static_cast<U>(Long(Sprite_Current + 0x34)), static_cast<U>(Long(Sprite_Current + 0x38)), 2, 0x14);
        }
        inner += 0x20;
        ++ring;
        fade += 0x10;
    } while (inner < 0x60);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(Long(Sprite_Current + 0x34)), static_cast<U>(Long(Sprite_Current + 0x38)), 2, 0xC);
    if (colour == 0) Cond_ByteFE = 0;
}

// ===========================================================================
// Area 92 (descriptor 0x614E78; PSX 0x801F416C)
// ===========================================================================

// original 0x4128A0 (area 92 +0x34[0]): the message word of
// Area92_ChoiceMessages by the s8 answer; answer 2, 3, 4: the scene start with
// MoveScript_Var7 8 and step 0xA, 0x1E, 0x14.
extern "C" void __cdecl Area92_ChoiceRunStep(void) {
    const int answer = Answer();
    SetMessage(MessageAt(at::kArea92Messages0, answer));
    switch (answer) {
    case 2: RunStep(8, 0xA); break;
    case 3: RunStep(8, 0x1E); break;
    case 4: RunStep(8, 0x14); break;
    default: break;
    }
}

// original 0x412940 (area 92 +0x34[1]): the message word by the s8 answer;
// answer (byte) 0: the scene start with MoveScript_Var7 8 and step 0.
extern "C" void __cdecl Area92_ChoiceRun0(void) {
    SetMessage(MessageAt(at::kArea92Messages1, Answer()));
    if (B(at::kChoiceAnswer) == 0) RunStep(8, 0);
}

// original 0x412990 (area 92 +0x34[2]): the same with step 0x28.
extern "C" void __cdecl Area92_ChoiceRun28(void) {
    SetMessage(MessageAt(at::kArea92Messages2, Answer()));
    if (B(at::kChoiceAnswer) == 0) RunStep(8, 0x28);
}

// original 0x4129E0 (area 92 +0x34[3]): the message word by the s8 answer;
// answer 0: the scene start with MoveScript_Var7 7, step 0x3C; 1: the
// chapter row's flag 0x1B clear - step 0x12, set - step 0x23.
extern "C" void __cdecl Area92_ChoiceRunByFlag1B(void) {
    const int answer = Answer();
    SetMessage(MessageAt(at::kArea92Messages3, answer));
    if (answer == 0) {
        RunStep(7, 0x3C);
    } else if (answer == 1) {
        if (AH_CALL(Flags_Test)(FlagRow(), 0x1B) == 0)
            RunStep(7, 0x12);
        else
            RunStep(7, 0x23);
    }
}

// original 0x412AA0 (area 92 +0x3C[0] = +0x34[4], PSX 0x801F2E9C): an effect
// of kind 4 at party record 0, its third byte Area92_EffectArgs4 by the first
// party list byte.
extern "C" void __cdecl Area92_SpawnKind4AtMember0(void) { SpawnInline(at::kParty, 4, at::kArea92EffectArgs4, at::kPartyList0); }

// original 0x412B50 (+0x3C[1], PSX 0x801F304C): kind 3 at record 0 by list 0,
// Area92_EffectArgs3.
extern "C" void __cdecl Area92_SpawnKind3AtMember0(void) { SpawnInline(at::kParty, 3, at::kArea92EffectArgs3, at::kPartyList0); }

// original 0x412C00 (+0x3C[2], PSX 0x801F31FC): kind 3 at record 1 by list 1.
extern "C" void __cdecl Area92_SpawnKind3AtMember1(void) {
    SpawnInline(at::kParty + at::kPartyStride, 3, at::kArea92EffectArgs3, at::kPartyList1);
}

// original 0x412CB0 (+0x3C[3], PSX 0x801F33AC): kind 3 at record 2 by list 2
// (read as a dword, masked to its byte).
extern "C" void __cdecl Area92_SpawnKind3AtMember2(void) {
    SpawnInline(at::kParty + 2 * at::kPartyStride, 3, at::kArea92EffectArgs3, at::kPartyList2);
}

// original 0x412D70 (+0x3C[4], PSX 0x801F355C): kind 1 at record 0 by list 0,
// Area92_EffectArgs1.
extern "C" void __cdecl Area92_SpawnKind1AtMember0(void) { SpawnInline(at::kParty, 1, at::kArea92EffectArgs1, at::kPartyList0); }

// original 0x412E20 (+0x3C[5], PSX 0x801F3708): kind 1 at record 1 by list 1.
extern "C" void __cdecl Area92_SpawnKind1AtMember1(void) {
    SpawnInline(at::kParty + at::kPartyStride, 1, at::kArea92EffectArgs1, at::kPartyList1);
}

// original 0x412ED0 (+0x3C[6], PSX 0x801F38B4): kind 1 at record 2 by list 2.
extern "C" void __cdecl Area92_SpawnKind1AtMember2(void) {
    SpawnInline(at::kParty + 2 * at::kPartyStride, 1, at::kArea92EffectArgs1, at::kPartyList2);
}

// original 0x412F90 (+0x3C[7] = +0x34[11], PSX 0x801F3A60): Music_Play(0x3D, 8).
extern "C" void __cdecl Area92_PlayMusic3D(void) { AH_CALL(Music_Play)(0x3D, 8); }

// original 0x412FA0 (Field_ObjectTriggers id 37): ScriptFlags_Set40, tail kind
// 0x2C with state 0 and sub-kind 0xB; al 0.
extern "C" unsigned char __cdecl Area92_Trigger37(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x2C;
    B(at::kTailState) = 0;
    B(at::kTailSub) = 0xB;
    return 0;
}

// ===========================================================================
// Area 94 (descriptor 0x616D70; PSX 0x801F52F8)
// ===========================================================================

// original 0x412FC0 (area 94 +0x34[0] = +0x3C[13], PSX 0x801F2C04): the
// message word of Area94_ChoiceMessages by the s8 answer.
extern "C" void __cdecl Area94_ChoiceMessage0(void) { SetMessage(MessageAt(at::kArea94Messages0, Answer())); }

// original 0x412FE0 (+0x34[1] = +0x3C[14], PSX 0x801F2C30): the same, its
// second list.
extern "C" void __cdecl Area94_ChoiceMessage1(void) { SetMessage(MessageAt(at::kArea94Messages1, Answer())); }

// original 0x413000 (+0x34[2] = +0x3C[15], PSX 0x801F2C5C): the message word;
// answer 1: the chapter row's flag 0x15 set.
extern "C" void __cdecl Area94_ChoiceSetFlag15(void) {
    const int answer = Answer();
    SetMessage(MessageAt(at::kArea94Messages2, answer));
    if (answer == 1) AH_CALL(Flags_Set)(FlagRow(), 0x15);
}

// original 0x413030 (+0x34[3] = +0x3C[16], PSX 0x801F2CB4): the message word;
// answer 1: the row's flag 0x16 clear - set it (the row read again), the scene
// start with MoveScript_Var7 7, step 0; set - flag 0x1A clear: step 0x19, set:
// step 0x32.
extern "C" void __cdecl Area94_ChoiceRunByFlags16(void) {
    const int answer = Answer();
    SetMessage(MessageAt(at::kArea94Messages3, answer));
    if (answer != 1) return;
    if (AH_CALL(Flags_Test)(FlagRow(), 0x16) == 0) {
        AH_CALL(Flags_Set)(FlagRow(), 0x16);
        RunStep(7, 0);
    } else if (AH_CALL(Flags_Test)(FlagRow(), 0x1A) == 0) {
        RunStep(7, 0x19);
    } else {
        RunStep(7, 0x32);
    }
}

// original 0x413110 (+0x34[4] = +0x3C[17], PSX 0x801F2DC4): the message word;
// answer 1: the scene start with MoveScript_Var7 7, step 0x32; 2: the row's
// flag 0x20 clear - MoveScript_Var7 9, step 0; set - step 2.
extern "C" void __cdecl Area94_ChoiceRunByFlag20(void) {
    const int answer = Answer();
    SetMessage(MessageAt(at::kArea94Messages4, answer));
    if (answer == 1) {
        RunStep(7, 0x32);
    } else if (answer == 2) {
        if (AH_CALL(Flags_Test)(FlagRow(), 0x20) == 0)
            RunStep(9, 0);
        else
            RunStep(9, 2);
    }
}

// original 0x4131D0 (+0x3C[0], PSX 0x801F2ED0): Effect_Spawn kind 2 at party
// record 0 by list 0, Area94_EffectArgsA.
extern "C" void __cdecl Area94_SpawnKind2AtMember0(void) { SpawnAt(at::kParty, 2, at::kArea94EffectArgsA, at::kPartyList0); }

// original 0x413220 (+0x3C[1], PSX 0x801F2F50): kind 3 at record 1, args A.
extern "C" void __cdecl Area94_SpawnKind3AtMember1(void) {
    SpawnAt(at::kParty + at::kPartyStride, 3, at::kArea94EffectArgsA, at::kPartyList1);
}

// original 0x413270 (+0x3C[2], PSX 0x801F2FD0): kind 3 at record 2, args A.
extern "C" void __cdecl Area94_SpawnKind3AtMember2(void) {
    SpawnAt(at::kParty + 2 * at::kPartyStride, 3, at::kArea94EffectArgsA, at::kPartyList2);
}

// original 0x4132C0 (+0x3C[3], PSX 0x801F3050): kind 4 at record 0, args B.
extern "C" void __cdecl Area94_SpawnKind4AtMember0(void) { SpawnAt(at::kParty, 4, at::kArea94EffectArgsB, at::kPartyList0); }

// original 0x413310 (+0x3C[4], PSX 0x801F30D0): the map cells (0x52, 0x32..0x34)
// zeroed.
extern "C" void __cdecl Area94_ClearCells(void) {
    AH_CALL(AreaMap_SetByte)(0x52, 0x32, 0);
    AH_CALL(AreaMap_SetByte)(0x52, 0x33, 0);
    AH_CALL(AreaMap_SetByte)(0x52, 0x34, 0);
}

// original 0x413340 (+0x3C[5], PSX 0x801F3118): kind 1 at record 1, args C.
extern "C" void __cdecl Area94_SpawnKind1AtMember1(void) {
    SpawnAt(at::kParty + at::kPartyStride, 1, at::kArea94EffectArgsC, at::kPartyList1);
}

// original 0x413390 (+0x3C[6], PSX 0x801F3198): kind 1 at record 2, args C.
extern "C" void __cdecl Area94_SpawnKind1AtMember2(void) {
    SpawnAt(at::kParty + 2 * at::kPartyStride, 1, at::kArea94EffectArgsC, at::kPartyList2);
}

// original 0x4133E0 (+0x3C[7], PSX 0x801F3218; also area 131 +0x3C[8] and area
// 133 +0x3C[3]): counter 1 = the leader's byte +8.
extern "C" void __cdecl Area94_Counter1FromLeaderPose(void) { B(at::kCounter1) = B(at::kLeaderPose); }

// original 0x4133F0 (+0x3C[8], PSX 0x801F3230): kind 3 at record 0, args D.
extern "C" void __cdecl Area94_SpawnKind3AtMember0(void) { SpawnAt(at::kParty, 3, at::kArea94EffectArgsD, at::kPartyList0); }

// original 0x413440 (+0x3C[9], PSX 0x801F32B0): kind 4 at record 1, args B.
extern "C" void __cdecl Area94_SpawnKind4AtMember1(void) {
    SpawnAt(at::kParty + at::kPartyStride, 4, at::kArea94EffectArgsB, at::kPartyList1);
}

// original 0x413490 (+0x3C[10], PSX 0x801F3330): kind 4 at record 2, args B.
extern "C" void __cdecl Area94_SpawnKind4AtMember2(void) {
    SpawnAt(at::kParty + 2 * at::kPartyStride, 4, at::kArea94EffectArgsB, at::kPartyList2);
}

// original 0x4134E0 (+0x3C[11], PSX 0x801F33B0): KeyItem_Add(7), then the
// chapter row (read after the call) flag 0x1E.
extern "C" void __cdecl Area94_GiveKeyItem7(void) {
    AH_CALL(KeyItem_Add)(7);
    AH_CALL(Flags_Set)(FlagRow(), 0x1E);
}

// original 0x413500 (+0x3C[12], PSX 0x801F33E0): kind 4 at record 0, args E.
extern "C" void __cdecl Area94_SpawnKind4AtMember0E(void) { SpawnAt(at::kParty, 4, at::kArea94EffectArgsE, at::kPartyList0); }

// original 0x413550 (+0x40, PSX 0x801F3460): the previous area (u16 0x802290)
// 0x79: story flag 0x42 set, 0x43 cleared, then the patch chain applied - from
// AreaMap_Header dword AreaMap_PatchBase (the dword at 0x8CB5A8 masked to its
// word), each entry not 0: AreaMap_ApplyPatch(entry), then on by (its dword,
// read again, >> 16) + 1 dwords; a zero dword ends it (none: the walk runs on).
extern "C" void __cdecl Area94_InitPatches(void) {
    if (Word(Mem(at::kLastArea)) != 0x79) return;
    AH_CALL(Flags_Set)(StoryFlags(), 0x42);
    AH_CALL(Flags_Clear)(StoryFlags(), 0x43);
    U entry = at::kMapHeader + (static_cast<U>(Long(Mem(at::kPatchBase))) & 0xFFFF) * 4u;
    while (Long(Mem(entry)) != 0) {
        AH_CALL(AreaMap_ApplyPatch)(Mem(entry));
        entry += (static_cast<U>(Long(Mem(entry))) >> 16) * 4u + 4u;
    }
}

void AreaW2c_Inject() {
    if (bof3::WantsShadow("area_w2c")) area_w2c::SelfTest();
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
    BOF3_INJECT(Area92_ChoiceRunStep);
    BOF3_INJECT(Area92_ChoiceRun0);
    BOF3_INJECT(Area92_ChoiceRun28);
    BOF3_INJECT(Area92_ChoiceRunByFlag1B);
    BOF3_INJECT(Area92_SpawnKind4AtMember0);
    BOF3_INJECT(Area92_SpawnKind3AtMember0);
    BOF3_INJECT(Area92_SpawnKind3AtMember1);
    BOF3_INJECT(Area92_SpawnKind3AtMember2);
    BOF3_INJECT(Area92_SpawnKind1AtMember0);
    BOF3_INJECT(Area92_SpawnKind1AtMember1);
    BOF3_INJECT(Area92_SpawnKind1AtMember2);
    BOF3_INJECT(Area92_PlayMusic3D);
    BOF3_INJECT(Area92_Trigger37);
    BOF3_INJECT(Area94_ChoiceMessage0);
    BOF3_INJECT(Area94_ChoiceMessage1);
    BOF3_INJECT(Area94_ChoiceSetFlag15);
    BOF3_INJECT(Area94_ChoiceRunByFlags16);
    BOF3_INJECT(Area94_ChoiceRunByFlag20);
    BOF3_INJECT(Area94_SpawnKind2AtMember0);
    BOF3_INJECT(Area94_SpawnKind3AtMember1);
    BOF3_INJECT(Area94_SpawnKind3AtMember2);
    BOF3_INJECT(Area94_SpawnKind4AtMember0);
    BOF3_INJECT(Area94_ClearCells);
    BOF3_INJECT(Area94_SpawnKind1AtMember1);
    BOF3_INJECT(Area94_SpawnKind1AtMember2);
    BOF3_INJECT(Area94_Counter1FromLeaderPose);
    BOF3_INJECT(Area94_SpawnKind3AtMember0);
    BOF3_INJECT(Area94_SpawnKind4AtMember1);
    BOF3_INJECT(Area94_SpawnKind4AtMember2);
    BOF3_INJECT(Area94_GiveKeyItem7);
    BOF3_INJECT(Area94_SpawnKind4AtMember0E);
    BOF3_INJECT(Area94_InitPatches);
}
