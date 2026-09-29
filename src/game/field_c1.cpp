// Round twelve group FC1 (docs/field_c1.md): the field core's first half -
// analysis/round12_cut.tsv's 41 rows for FC1, each read to its last
// instruction with capstone (2026-09-29) and fuzzed through the scenario
// harness's field mode (scenario_harness.h, docs/scenario_harness.md section 7).
//
//   Config_DrawRowLabel        0x461800  the Config screen's row: box, label, hand, separator
//   EffectKind06_PlayOnce      0x469D10  EffectKind06_Ticks[5] / EffectKind19_Ticks[5]
//   EffectKind06_Fade          0x469D40  EffectKind06_Ticks[3] / EffectKind19_Ticks[3]
//   EffectKind19_Run           0x469DE0  Effect_KindHandlers[0x19]
//   EffectKind19_Tick          0x469E00  EffectKind19_States[1]
//   EffectKind04_HoldTick      0x469FB0  Effect_KindHandlers[4]
//   EffectKind31_Run           0x46A020  Effect_KindHandlers[0x31]
//   CameraZoom_Start / _Step / _End  0x46A040 / 0x46A070 / 0x46A0C0  CameraZoom_States 0..2
//   EffectKind32_Run           0x46A0E0  Effect_KindHandlers[0x32]
//   EffectKind32_Throw / _Arc / _Settle  0x46A100 / 0x46A1E0 / 0x46A290  EffectKind32_States 0..2
//   Effect_StateRelease        0x46A310  49 state-table cells: a tail jump to Effect_Release
//   EffectKind37_Start / _Play 0x46A340 / 0x46A390  EffectKind37_States 0, 1
//   EffectKind14_Start / _Hold 0x46A600 / 0x46A7F0  EffectKind14_States 0, 1
//   EffectKind3C_Start / _Hold 0x46A950 / 0x46AB30  EffectKind3C_States 0, 1
//   EffectKind17_Start .. _Rest 0x46ABD0 .. 0x46B340  EffectKind17_States 0..5 (a pushed block)
//   EffectKind17_TakeCell .. _CellBlocked 0x46B380 .. 0x46B6D0  their helpers (E8)
//   EffectKind1B_Start .. _Trail 0x46B7C0 .. 0x46B9A0  EffectKind1B_States 0..3
//   EffectKind1B_Hit           0x46BA90  EffectKind1B_Fly's test (E8)
//   EffectKind30_Run / _Start  0x46BB30 / 0x46BB50  Effect_KindHandlers[0x30], EffectKind30_States[0]
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call, and
// kept where it keeps it in a register. No divergence: each is a faithful
// replacement. Where the original would jump through a state table to what is
// not code, write through an index past Sprite_Objects / Sprite_ObjectsExtra /
// Effect_Objects / ObjTrio, divide by 0, or read its own stack frame, ours
// aborts with a message (docs/field_c1.md section 7); reads of the image's
// tables past their ends are kept (they read what the original reads).
#include "game/field_c1.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_c1_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_c1::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;
using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U L(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }
short S16(U v) { return static_cast<short>(v & 0xFFFFu); }
// cdq / xor / sub: the absolute value, 0x80000000 staying itself.
std::int32_t Abs32(U v) { return static_cast<std::int32_t>(S32(v) < 0 ? 0u - v : v); }

// A state table's entry, read in place (the fuzz swaps the entries for
// recorders); past the table's code the original jumps to what is not code.
constexpr U kTextLo = 0x401000, kTextHi = 0x5C3000;
Handler Entry(U table, unsigned index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = L(At(cell));
    if (!scenario_harness::g_active && (entry < kTextLo || entry >= kTextHi))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X, not code - past its table (the original jumps there)", who, index,
                    (unsigned)entry, (unsigned)cell);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry));
}

// The records the handlers write by an index byte the original never checks.
unsigned char* SpriteRec(int i, const char* who) {
    if (i < 0 || i >= static_cast<int>(at::kSpriteCount))
        bof3::Fatal("%s: Sprite_Objects index %d - the original writes outside the records", who, i);
    return Sprite_Objects + static_cast<U>(i) * at::kRecordStride;
}
unsigned char* ExtraRec(int i, const char* who) {
    if (i < 0 || i >= static_cast<int>(at::kExtraCount))
        bof3::Fatal("%s: Sprite_ObjectsExtra index %d - the original writes outside the records", who, i);
    return Sprite_ObjectsExtra + static_cast<U>(i) * at::kRecordStride;
}
unsigned char* EffectRec(unsigned i, const char* who) {
    if (i >= at::kEffectCount)
        bof3::Fatal("%s: Effect_Objects index %u - the original writes outside the records", who, i);
    return Effect_Objects + i * at::kEffectStride;
}
unsigned char* Member(int i, const char* who) {
    if (i < 0 || i >= static_cast<int>(at::kMemberCount))
        bof3::Fatal("%s: ObjTrio index %d - the original writes outside the records", who, i);
    return ObjTrio + static_cast<U>(i) * at::kMemberStride;
}
// A Sprite_Objects record the original only reads, by an unchecked byte: read
// in place wherever it lands, as the original reads it.
unsigned char* SpriteRead(unsigned i) { return Sprite_Objects + i * at::kRecordStride; }

void Tick() {
    SH_CALL(Sprite_ScriptTick)();
    SH_CALL(Sprite_UpdateScreenSlot)();
}

// A byte of the area block's cell words (AreaMap_Header's word +2 the offset,
// in pairs of words): 0 marks a cell with no floor. The original indexes with
// the signed cell coordinates, unchecked; read in place.
U CellWord(short cx, short cz) {
    const int width = static_cast<int>(AreaMap_Header[0]);
    const int index = width * cz + static_cast<int>(Word(At(at::kAreaCells))) * 2 + cx;
    return Word(At(AddressOf(AreaMap_Header) + static_cast<U>(index) * 2u));
}

// The object at the effect's point (Sprite_ObjectAt's index): its +0x80 bit 0
// set - Sprite_Objects below 0x1E, Sprite_ObjectsExtra from there (the index
// a signed byte, as the original's movsx).
void MarkObject(unsigned char found, const char* who) {
    const int i = static_cast<signed char>(found);
    unsigned char* const rec = i < 0x1E ? SpriteRec(i, who) : ExtraRec(i - 0x1E, who);
    rec[0x80] = static_cast<unsigned char>(rec[0x80] | 1);
}

}  // namespace

// ============================================================================
// The Config screen's row label (START.EMI's resident draw)
// ============================================================================

// original 0x461800 (called by the panel draw 0x461710 for each of the six
// rows; docs/config-screen.md): the row's box Menu_DrawBox(x, y - s, 0xF9,
// 2 s + 0xB, 0, the style byte 0x903A5A), s the fourth argument's byte. Its
// label is one of six strings the original puts on its stack as immediates,
// by the row byte (unchecked: past six it reads its own frame - ours aborts).
// s == 3 (the row under the cursor): the label large, right-aligned -
// Text_DrawAt(x - len * 6 + 0x3A, y - 1, 0, 0xFF, label) - and, unless the
// menu step 0x929F02 is 1, Menu_DrawHand(x + 4, y, 0); else small,
// Text_DrawSmall(x - len * 4 + 0x3A, y + 1, 0, 0xFF, label). Then a line
// primitive at Gfx_PacketNext, read before Gpu_SetLineF2: grey 0x80, from
// (x16 + 0x74, y16 - s + 1) to (x16 + 0x74, y16 + s + 9) as floats, x16 and y16
// the arguments' low words; Gfx_CommitPrim(1, 0x20).
//
// DIV-0015 and DIV-0017 (config_text.cpp) rewrite this function's operands in
// the original's code: the six label immediates, the two 0x3A anchors, the
// large branch's `len * 6` and its call. Ours reads each of them from the
// original's bytes at every call, so the patches hold for ours unchanged. The
// box's y and height take the fourth argument's low byte where the original
// takes eax's low word with its caller's upper half (the caller's x): the box
// reads the low words only (Menu_DrawBox, menu_windows.cpp).
extern "C" void __cdecl Config_DrawRowLabel(int x, int y, unsigned row, unsigned state) {
    const int s = static_cast<int>(state & 0xFF);
    const unsigned char style = At(0x903A5A)[0];
    SH_CALL(Menu_DrawBox)(x, y - s, 0xF9, 2 * s + 0xB, 0, style);
    const unsigned r = row & 0xFF;
    if (r >= 6)
        bof3::Fatal("Config_DrawRowLabel: row %u - the original reads its own stack frame past the six labels", r);
    const auto* const label =
        reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(L(At(at::kRowLabelOperands + 8u * r))));
    const int len = static_cast<int>(std::strlen(reinterpret_cast<const char*>(label)));
    if (s == 3) {
        static const unsigned char kSix[] = {0x8D, 0x0C, 0x49, 0xD1, 0xE1};   // lea ecx,[ecx+ecx*2] / shl ecx,1
        static const unsigned char kFour[] = {0xC1, 0xE1, 0x02, 0x90, 0x90};  // DIV-0017: shl ecx,2 / nop / nop
        int width;
        if (std::memcmp(At(at::kRowLabelWidth), kSix, sizeof kSix) == 0) width = len * 6;
        else if (std::memcmp(At(at::kRowLabelWidth), kFour, sizeof kFour) == 0) width = len * 4;
        else bof3::Fatal("Config_DrawRowLabel: the width code at 0x%X is neither the original's nor DIV-0017's",
                         (unsigned)at::kRowLabelWidth);
        const int anchor = static_cast<signed char>(At(at::kRowAnchorBig)[0]);
        const U big = at::kRowBigCall + 5u + L(At(at::kRowBigCall + 1));
        using Draw = const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*);
        SH_AT(Draw, big)(x - width + anchor, y - 1, 0, 0xFF, label);
        if (At(0x929F02)[0] != 1) SH_CALL(Menu_DrawHand)(x + 4, y, 0);
    } else {
        const int anchor = static_cast<signed char>(At(at::kRowAnchorSmall)[0]);
        SH_CALL(Text_DrawSmall)(x - len * 4 + anchor, y + 1, 0, 0xFF, label);
    }
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    const int xs = static_cast<int>((static_cast<U>(x) & 0xFFFFu) + 0x74);
    const int ys = static_cast<int>(static_cast<U>(y) & 0xFFFFu);
    const float fx = static_cast<float>(xs);
    const float y0 = static_cast<float>(ys - s + 1);
    const float y1 = static_cast<float>(s + ys + 9);
    std::memcpy(prim + 0x14, &fx, 4);
    std::memcpy(prim + 8, &fx, 4);
    prim[4] = prim[5] = 0x80;
    std::memcpy(prim + 0xC, &y0, 4);
    prim[6] = 0x80;
    std::memcpy(prim + 0x18, &y1, 4);
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// ============================================================================
// Effect kinds 6 and 0x19: two ticks of kind 6's, kind 0x19's run and tick
// ============================================================================

// original 0x469D10 (EffectKind06_Ticks[5], EffectKind19_Ticks[5]): the
// script stepped once; +1 up when it ended (al not 0); then a tail jump to
// Sprite_QueueOverlay for the kind +5 == 6, else Sprite_UpdateScreen.
extern "C" void __cdecl EffectKind06_PlayOnce(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)()) Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
    if (Cur()[5] == 6) SH_CALL(Sprite_QueueOverlay)();
    else SH_CALL(Sprite_UpdateScreen)();
}

// original 0x469D40 (EffectKind06_Ticks[3], EffectKind19_Ticks[3]): +9 down;
// at 0 +1 up and nothing more. Else the tint bytes +0x5D, +0x5E, +0x5F each
// plus 0xFA (six down, in 8 bits), Sprite_ScriptTick, then by the kind +5 ==
// 6: on odd +9 the word +0x30 up, tail Sprite_QueueOverlay; else on odd +9
// the dword +0x10 down, tail Sprite_UpdateScreen (+5 and +9 read after the
// tick).
extern "C" void __cdecl EffectKind06_Fade(void) {
    Cur()[9] = static_cast<unsigned char>(Cur()[9] - 1);
    unsigned char* o = Cur();
    if (o[9] == 0) {
        o[1] = static_cast<unsigned char>(o[1] + 1);
        return;
    }
    o[0x5D] = static_cast<unsigned char>(o[0x5D] + 0xFA);
    Cur()[0x5E] = static_cast<unsigned char>(Cur()[0x5E] + 0xFA);
    Cur()[0x5F] = static_cast<unsigned char>(Cur()[0x5F] + 0xFA);
    SH_CALL(Sprite_ScriptTick)();
    o = Cur();
    const unsigned char kind = o[5], left = o[9];
    if (kind == 6) {
        if (left & 1) SetWord(o + 0x30, Word(o + 0x30) + 1u);
        SH_CALL(Sprite_QueueOverlay)();
    } else {
        if (left & 1) SetL(o + 0x10, L(o + 0x10) - 1u);
        SH_CALL(Sprite_UpdateScreen)();
    }
}

// original 0x469DE0 (Effect_KindHandlers[0x19]): a tail jump through
// EffectKind19_States 0x653EF4 by +1 - EffectKind06_Start, EffectKind19_Tick,
// EffectKind06_End.
extern "C" void __cdecl EffectKind19_Run(void) { Entry(AddressOf(EffectKind19_States), Cur()[1], "EffectKind19_Run")(); }

// original 0x469E00 (EffectKind19_States[1]): EffectKind19_Ticks 0x653F00 by
// the variant +6 (kind 6's ticks again: _Blink for 0, 1, 2, 4, _Fade for 3,
// _PlayOnce for 5), then the words +0x2E += +0xC and +0x30 -= +0x10 - kind
// 6's tick with a drift.
extern "C" void __cdecl EffectKind19_Tick(void) {
    Entry(AddressOf(EffectKind19_Ticks), Cur()[6], "EffectKind19_Tick")();
    unsigned char* o = Cur();
    SetWord(o + 0x2E, Word(o + 0x2E) + Word(o + 0xC));
    o = Cur();
    SetWord(o + 0x30, Word(o + 0x30) - Word(o + 0x10));
}

// ============================================================================
// Kind 4: Effect_HoldFlag1C's countdown
// ============================================================================

// original 0x469FB0 (Effect_KindHandlers[4]; Effect_HoldFlag1C spawns it with
// +9 its frames): +9 down while not 0; at 0 Flags_Clear(0x904030, 0x1C) and a
// tail jump to Effect_Release.
extern "C" void __cdecl EffectKind04_HoldTick(void) {
    unsigned char* const o = Cur();
    const unsigned char left = o[9];
    if (left == 0) {
        SH_CALL(Flags_Clear)(At(at::kStoryFlags), 0x1C);
        SH_CALL(Effect_Release)();
        return;
    }
    o[9] = static_cast<unsigned char>(left - 1);
}

// ============================================================================
// Kind 0x31: the camera turn with a zoom
// ============================================================================

// original 0x46A020 (Effect_KindHandlers[0x31]): CameraZoom_States 0x653F24
// by +1, then MapView_Redraw = 2 (as Effect_CameraTurn's).
extern "C" void __cdecl EffectKind31_Run(void) {
    Entry(AddressOf(CameraZoom_States), Cur()[1], "EffectKind31_Run")();
    MapView_Redraw = 2;
}

// original 0x46A040 (CameraZoom_States[0]): CameraTurn_Start, then the
// distance's step +0x10 = ((dword +0xC - s16 Camera_Distance) << 16) / +9
// (idiv by the unsigned byte; a +9 of 0 divides by zero - CameraTurn_Start
// divides by it first; ours aborts).
extern "C" void __cdecl CameraZoom_Start(void) {
    SH_CALL(CameraTurn_Start)();
    unsigned char* const o = Cur();
    const U num = (L(o + 0xC) - static_cast<U>(static_cast<int>(Camera_Distance))) << 16;
    const unsigned frames = o[9];
    if (frames == 0) bof3::Fatal("CameraZoom_Start: +9 is 0 - the original divides by zero");
    SetLong(o + 0x10, S32(num) / static_cast<std::int32_t>(frames));
}

// original 0x46A070 (CameraZoom_States[1]): CameraTurn_Step, then
// Camera_Distance = ((s16 distance << 16) + +0x10) >> 16 (arithmetic), held
// at the target +0xC once past it - below it for a falling step, above it for
// a rising one (the new distance sign-extended against the dword).
extern "C" void __cdecl CameraZoom_Step(void) {
    SH_CALL(CameraTurn_Step)();
    unsigned char* const o = Cur();
    const U sum = (static_cast<U>(static_cast<int>(Camera_Distance)) << 16) + L(o + 0x10);
    const short now = static_cast<short>(S32(sum) >> 16);
    Camera_Distance = now;
    const std::int32_t target = S32(L(o + 0xC));
    if (S32(L(o + 0x10)) < 0) {
        if (now < target) Camera_Distance = static_cast<short>(target);
    } else {
        if (now > target) Camera_Distance = static_cast<short>(target);
    }
}

// original 0x46A0C0 (CameraZoom_States[2]): Camera_Distance = the word +0xC,
// then a tail jump to CameraTurn_End.
extern "C" void __cdecl CameraZoom_End(void) {
    Camera_Distance = static_cast<short>(Word(Cur() + 0xC));
    SH_CALL(CameraTurn_End)();
}

// ============================================================================
// Kind 0x32: a thing thrown from a sprite, bouncing
// ============================================================================

// original 0x46A0E0 (Effect_KindHandlers[0x32]): a tail jump through
// EffectKind32_States 0x653F30 by +1.
extern "C" void __cdecl EffectKind32_Run(void) { Entry(AddressOf(EffectKind32_States), Cur()[1], "EffectKind32_Run")(); }

// original 0x46A100 (EffectKind32_States[0]): +8 = the leader's facing
// (ObjTrio +8) & 7; Sprite_SetAnimationBank(0x18); +0x48, +0x24, +0x2A = 0;
// Sprite_SetAnimation(8); the position +0x34 / +0x38 / +0x3C that of
// Sprite_Objects[+6] (the byte unchecked, read in place); the tint bytes
// +0x5C..+0x5F 0; MoveCmd_Move of a stack object whose byte +0 is 0 and +4
// is 3 (the only bytes it reads: speed 3, no bit 7), in the direction +8;
// then EffectKind32_Arc.
extern "C" void __cdecl EffectKind32_Throw(void) {
    Cur()[8] = static_cast<unsigned char>(ObjTrio[8] & 7);
    SH_CALL(Sprite_SetAnimationBank)(0x18);
    Cur()[0x48] = 0;
    Cur()[0x24] = 0;
    Cur()[0x2A] = 0;
    SH_CALL(Sprite_SetAnimation)(8);
    SetLong(Cur() + 0x34, Long(SpriteRead(Cur()[6]) + 0x34));
    SetLong(Cur() + 0x38, Long(SpriteRead(Cur()[6]) + 0x38));
    SetLong(Cur() + 0x3C, Long(SpriteRead(Cur()[6]) + 0x3C));
    Cur()[0x5C] = 0;
    Cur()[0x5F] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5D] = 0;
    unsigned char mover[8] = {0, 0, 0, 0, 3, 0, 0, 0};
    SH_CALL(MoveCmd_Move)(mover, Cur()[8]);
    SH_CALL(EffectKind32_Arc)();
}

// original 0x46A1E0 (EffectKind32_States[1]; also called by _Throw): the
// ground AreaMap_Elevation(+0x34, +0x38); at or above the height word +0x3E
// (signed), or with +0 bit 7, or in state 0, a hop - the rise +0x14 and fall
// +0x20 from EffectKind32_Hops[+2] (the byte unchecked, read in place), +1
// up. Then the position +0x34 += +0xC, +0x38 += +0x10, +0x3C += +0x14 and
// +0x14 -= +0x20; Sprite_ScriptTick, tail Sprite_UpdateScreenSlot.
extern "C" void __cdecl EffectKind32_Arc(void) {
    unsigned char* o = Cur();
    const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(Long(o + 0x34), Long(o + 0x38))));
    o = Cur();
    if (ground >= static_cast<short>(Word(o + 0x3E)) || (o[0] & 0x80) || o[1] == 0) {
        SetL(o + 0x14, L(At(AddressOf(EffectKind32_Hops) + 8u * o[2])));
        o = Cur();
        SetL(o + 0x20, L(At(AddressOf(EffectKind32_Hops) + 4 + 8u * o[2])));
        o = Cur();
        o[1] = static_cast<unsigned char>(o[1] + 1);
        o = Cur();
    }
    SetL(o + 0x34, L(o + 0x34) + L(o + 0xC));
    o = Cur();
    SetL(o + 0x38, L(o + 0x38) + L(o + 0x10));
    o = Cur();
    SetL(o + 0x3C, L(o + 0x3C) + L(o + 0x14));
    o = Cur();
    SetL(o + 0x14, L(o + 0x14) - L(o + 0x20));
    Tick();
}

// original 0x46A290 (EffectKind32_States[2]): the ground under +0x34 / +0x38;
// unless it lies within 0x80 below the height word +0x3E (ground <= height <
// ground + 0x80) with +0 bit 7 clear, +1 up and Sound_PlayEffect(0x106). Then
// +0x3C += +0x14, +0x14 -= +0x20 (no step across), Sprite_ScriptTick,
// Sprite_UpdateScreenSlot.
extern "C" void __cdecl EffectKind32_Settle(void) {
    unsigned char* o = Cur();
    const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(Long(o + 0x34), Long(o + 0x38))));
    o = Cur();
    const short height = static_cast<short>(Word(o + 0x3E));
    if (ground + 0x80 <= height || ground > height || (o[0] & 0x80)) {
        o[1] = static_cast<unsigned char>(o[1] + 1);
        SH_CALL(Sound_PlayEffect)(0x106);
        o = Cur();
    }
    SetL(o + 0x3C, L(o + 0x3C) + L(o + 0x14));
    o = Cur();
    SetL(o + 0x14, L(o + 0x14) - L(o + 0x20));
    Tick();
}

// original 0x46A310 (49 cells of the effect kinds' state tables, among them
// EffectKind32_States[3], EffectKind37_States[2], EffectKind18_States'):
// a tail jump to Effect_Release.
extern "C" void __cdecl Effect_StateRelease(void) { SH_CALL(Effect_Release)(); }

// ============================================================================
// Kind 0x37: an animation played through
// ============================================================================

// original 0x46A340 (EffectKind37_States[0]): Sprite_SetAnimationBank(the word
// +0x2C); +0x48 = 1, +0x24 = 0, +0x2A = +6 >> 7; Sprite_SetAnimation(+6 &
// 0x7F); +1 = 1.
extern "C" void __cdecl EffectKind37_Start(void) {
    SH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(Word(Cur() + 0x2C)));
    Cur()[0x48] = 1;
    Cur()[0x24] = 0;
    Cur()[0x2A] = static_cast<unsigned char>(Cur()[6] >> 7);
    SH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Cur()[6] & 0x7F));
    Cur()[1] = 1;
}

// original 0x46A390 (EffectKind37_States[1]): Sprite_ScriptTickOnce; when it
// ended a pass, +9 down and at 0 +1 = 2. Also +1 = 2 when +7 is not 0 and
// equals the frame word +0x58. Tail Sprite_UpdateScreen.
extern "C" void __cdecl EffectKind37_Play(void) {
    const unsigned char ended = SH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* o = Cur();
    if (ended) {
        o[9] = static_cast<unsigned char>(o[9] - 1);
        o = Cur();
        if (o[9] == 0) {
            o[1] = 2;
            o = Cur();
        }
    }
    if (o[7] != 0 && static_cast<U>(o[7]) == Word(o + 0x58)) o[1] = 2;
    SH_CALL(Sprite_UpdateScreen)();
}

// ============================================================================
// Kinds 0x14 and 0x3C: two tinted sprites for a while
// ============================================================================

namespace {

// The start both kinds share: two free sprites into +3 and +4 (none: nothing,
// the first given back if only the second fails), each marked in use; for
// each, DamageScratch's word = its index and the op run (EventOp_6x with
// Sprite_SetAnimationAt(0x61, 0xC) for 0x14, EventOp_0x for 0x3C);
// Sprite_Current put back; the first moved by (dx, -dx), the second by (-dx,
// dx), both +0 |= 0x20, +0x5C = 0, tinted (0xF, 0, 0, 1) and (0, 0, 0xF, 1);
// the effect's +9 = 4 and +1 = 1 (Sprite_Current read again after the tints).
template <bool kKind14>
void TwinStart(const char* who) {
    Cur()[3] = SH_CALL(Sprite_FindFree)();
    if (Cur()[3] == 0xFF) return;
    SpriteRec(Cur()[3], who)[0] = 1;
    Cur()[4] = SH_CALL(Sprite_FindFree)();
    unsigned char* const self = Cur();
    if (self[4] == 0xFF) {
        SpriteRec(self[3], who)[0] = 0;
        return;
    }
    SpriteRec(self[4], who)[0] = 1;
    const auto* const op = At(kKind14 ? AddressOf(EffectKind14_Op) : AddressOf(EffectKind3C_Op));
    SetWord(At(bof3::addr::DamageScratch), self[3]);
    if (kKind14) {
        SH_CALL(EventOp_6x)(op);
        SH_CALL(Sprite_SetAnimationAt)(0x61, 0xC);
    } else {
        SH_CALL(EventOp_0x)(op);
    }
    SetWord(At(bof3::addr::DamageScratch), self[4]);
    if (kKind14) {
        SH_CALL(EventOp_6x)(op);
        SH_CALL(Sprite_SetAnimationAt)(0x61, 0xC);
    } else {
        SH_CALL(EventOp_0x)(op);
    }
    Sprite_Current = self;
    const U d = kKind14 ? 0x1000u : 0xFFFFF000u;
    unsigned char* a = SpriteRec(self[3], who);
    SetL(a + 0x34, L(a + 0x34) + d);
    SetL(a + 0x38, L(a + 0x38) - d);
    unsigned char* b = SpriteRec(self[4], who);
    SetL(b + 0x34, L(b + 0x34) - d);
    SetL(b + 0x38, L(b + 0x38) + d);
    a = SpriteRec(self[3], who);
    a[0] = static_cast<unsigned char>(a[0] | 0x20);
    b = SpriteRec(self[4], who);
    b[0] = static_cast<unsigned char>(b[0] | 0x20);
    SpriteRec(self[4], who)[0x5C] = 0;
    SpriteRec(self[3], who)[0x5C] = 0;
    SH_CALL(Sprite_SetTint)(SpriteRec(self[3], who), 0xF, 0, 0, 1);
    SH_CALL(Sprite_SetTint)(SpriteRec(self[4], who), 0, 0, 0xF, 1);
    Cur()[9] = 4;
    Cur()[1] = 1;
}

}  // namespace

// original 0x46A600 (EffectKind14_States[0]): the shared start with
// EventOp_6x(EffectKind14_Op) and Sprite_SetAnimationAt(0x61, 0xC) for each,
// the first sprite moved by (+0x1000, -0x1000).
extern "C" void __cdecl EffectKind14_Start(void) { TwinStart<true>("EffectKind14_Start"); }

// original 0x46A7F0 (EffectKind14_States[1]): while +9 is not 0 the leader's
// +0 |= 0x40 and +9 down; at 0 the leader's bit 0x40 cleared, both sprites'
// +0 = 0 and +1 = 2.
extern "C" void __cdecl EffectKind14_Hold(void) {
    unsigned char* const o = Cur();
    if (o[9] != 0) {
        ObjTrio[0] = static_cast<unsigned char>(ObjTrio[0] | 0x40);
        o[9] = static_cast<unsigned char>(o[9] - 1);
        return;
    }
    ObjTrio[0] = static_cast<unsigned char>(ObjTrio[0] & 0xBF);
    SpriteRec(o[3], "EffectKind14_Hold")[0] = 0;
    SpriteRec(o[4], "EffectKind14_Hold")[0] = 0;
    o[1] = 2;
}

// original 0x46A950 (EffectKind3C_States[0]): the shared start with
// EventOp_0x(EffectKind3C_Op) for each, the first sprite moved by (-0x1000,
// +0x1000).
extern "C" void __cdecl EffectKind3C_Start(void) { TwinStart<false>("EffectKind3C_Start"); }

// original 0x46AB30 (EffectKind3C_States[1]; also 0x65461C[35]): as
// EffectKind14_Hold with Sprite_Objects[+0xB] in the leader's place.
extern "C" void __cdecl EffectKind3C_Hold(void) {
    unsigned char* const o = Cur();
    if (o[9] != 0) {
        unsigned char* const s = SpriteRec(o[0xB], "EffectKind3C_Hold");
        s[0] = static_cast<unsigned char>(s[0] | 0x40);
        o[9] = static_cast<unsigned char>(o[9] - 1);
        return;
    }
    unsigned char* const s = SpriteRec(o[0xB], "EffectKind3C_Hold");
    s[0] = static_cast<unsigned char>(s[0] & 0xBF);
    SpriteRec(o[3], "EffectKind3C_Hold")[0] = 0;
    SpriteRec(o[4], "EffectKind3C_Hold")[0] = 0;
    o[1] = 2;
}

// ============================================================================
// Kind 0x17: a block pushed along the map
// ============================================================================

// original 0x46ABD0 (EffectKind17_States[0]): Sprite_SetAnimationBank(+0xB);
// EffectKind17_TakeCell (+0xB becomes the map byte under it); +0x29 = 6,
// +0x48 = 1, +0x24, +0x2A, +0x5D..+0x5F 0; the dwords +0xC..+0x20 0; +7 and
// +0xA 0; Sprite_SetAnimation(0); +1 up.
extern "C" void __cdecl EffectKind17_Start(void) {
    SH_CALL(Sprite_SetAnimationBank)(Cur()[0xB]);
    SH_CALL(EffectKind17_TakeCell)();
    unsigned char* const o = Cur();
    o[0x29] = 6;
    o[0x48] = 1;
    o[0x24] = 0;
    o[0x2A] = 0;
    o[0x5D] = 0;
    o[0x5E] = 0;
    o[0x5F] = 0;
    for (U off = 0xC; off <= 0x20; off += 4) SetL(o + off, 0);
    o[7] = 0;
    o[0xA] = 0;
    SH_CALL(Sprite_SetAnimation)(0);
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
}

// original 0x46AC90 (EffectKind17_States[1]): with Game_Mode 1 only
// Sprite_UpdateScreenSlot. With no push (+0xA 0): the saved map byte +0xB
// put back at the cell (+0x36, +0x3A) when Field_Request is 5; then the
// update. Pushed: EffectKind17_BlockedAhead - blocked, the byte put back, +9
// = 0, +1 = 4; free, the byte put back, the push +0xA = (+0xA + 1) >> 1, the
// speeds +0xC / +0x10 = the direction's unit (0x6696DC[+8], unchecked) << 2,
// each moved away from 0 by 3 * 0x400 * +0xA - 0xC00; +0x14 and +0x20 0; the
// brakes +0x18 / +0x1C -0x400, 0x400 or 0 against each speed's sign;
// Sprite_SetAnimation(1), +9 = 0, +1 up; then the update.
extern "C" void __cdecl EffectKind17_Push(void) {
    if (Game_Mode == 1) {
        SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    unsigned char* o = Cur();
    if (o[0xA] == 0) {
        if (Field_Request == 5) SH_CALL(AreaMap_SetByte)(Word(o + 0x36), Word(o + 0x3A), o[0xB]);
        SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    const unsigned char blocked = SH_CALL(EffectKind17_BlockedAhead)();
    o = Cur();
    if (blocked) {
        SH_CALL(AreaMap_SetByte)(Word(o + 0x36), Word(o + 0x3A), o[0xB]);
        Cur()[9] = 0;
        Cur()[1] = 4;
        SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    SH_CALL(AreaMap_SetByte)(Word(o + 0x36), Word(o + 0x3A), o[0xB]);
    o = Cur();
    o[0xA] = static_cast<unsigned char>((static_cast<int>(o[0xA]) + 1) >> 1);
    const U push = static_cast<U>(o[0xA]) * 3u * 0x400u - 0xC00u;
    SetL(o + 0xC, L(At(at::kWalkDelta + 8u * o[8])) << 2);
    U v = L(o + 0xC);
    if (S32(v) > 0) SetL(o + 0xC, v + push);
    else if (S32(v) < 0) SetL(o + 0xC, v - push);
    SetL(o + 0x10, L(At(at::kWalkDelta + 4 + 8u * o[8])) << 2);
    v = L(o + 0x10);
    if (S32(v) > 0) SetL(o + 0x10, v + push);
    else if (S32(v) < 0) SetL(o + 0x10, v - push);
    SetL(o + 0x20, 0);
    SetL(o + 0x14, 0);
    v = L(o + 0xC);
    SetL(o + 0x18, S32(v) > 0 ? 0xFFFFFC00u : S32(v) < 0 ? 0x400u : 0u);
    v = L(o + 0x10);
    SetL(o + 0x1C, S32(v) > 0 ? 0xFFFFFC00u : S32(v) < 0 ? 0x400u : 0u);
    SetL(o + 0x20, 0);
    SH_CALL(Sprite_SetAnimation)(1);
    Cur()[9] = 0;
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
    SH_CALL(Sprite_UpdateScreenSlot)();
}

// original 0x46AE30 (EffectKind17_States[2]): the slide. +6 = +7 = 0. Along x
// when +0xC is not 0 (else along z with +0x10 / +0x1C / +0x38): the speed
// plus its brake +0x18, back to the old speed and the brake 0 once it is
// below 0x1000 in size; x += the speed; the distance to the leader along that
// axis. Both brakes 0: +6 = 1, +7 = 0. The fall +0x14 += +0x20, the height
// word +0x3E += the word +0x14. With Field_Kind2Hold 0, the distance above
// 0x40000 and the view's kind-2 point at the leader: that point's whole part
// 5 along the push, MoveScript_FAWord 0 or (ground there - MapView_Elevation)
// / 20 when the view is higher, MoveScript_F3Divisor 0x40, +9 = 0x14. The
// ground under the block: below the height the fall -8, else rest on it.
// EffectKind17_Bump found something: done. Else EffectKind17_BlockedAt(x, z)
// (+6 1, +7 2) or EffectKind17_BlockedAhead (+6 1, +7 1). Stopped (+6): by +7
// - 0 on a whole cell (words +0x34 and +0x38 0) the rest (+7, +0xA 0,
// Sprite_SetAnimation(0), +1 = 5, then the tick), else and for 1
// EffectKind17_AlignTarget(0), 2 _AlignTarget(1). Sprite_ScriptTick,
// Sprite_UpdateScreenSlot.
extern "C" void __cdecl EffectKind17_Slide(void) {
    Cur()[6] = 0;
    Cur()[7] = 0;
    unsigned char* o = Cur();
    U dist;
    const U speed_x = L(o + 0xC);
    if (speed_x != 0) {
        SetL(o + 0xC, L(o + 0x18) + speed_x);
        o = Cur();
        if (Abs32(L(o + 0xC)) < 0x1000) {
            SetL(o + 0xC, speed_x);
            SetL(Cur() + 0x18, 0);
            o = Cur();
        }
        SetL(o + 0x34, L(o + 0x34) + L(o + 0xC));
        o = Cur();
        dist = L(o + 0x34) - L(ObjTrio + 0x34);
    } else {
        const U speed_z = L(o + 0x10);
        SetL(o + 0x10, L(o + 0x1C) + speed_z);
        o = Cur();
        if (Abs32(L(o + 0x10)) < 0x1000) {
            SetL(o + 0x10, speed_z);
            SetL(Cur() + 0x1C, 0);
            o = Cur();
        }
        SetL(o + 0x38, L(o + 0x38) + L(o + 0x10));
        o = Cur();
        dist = L(o + 0x38) - L(ObjTrio + 0x38);
    }
    const std::int32_t far = Abs32(dist);
    if (L(o + 0x18) == 0 && L(o + 0x1C) == 0) {
        o[6] = 1;
        Cur()[7] = 0;
        o = Cur();
    }
    SetL(o + 0x14, L(o + 0x14) + L(o + 0x20));
    o = Cur();
    SetWord(o + 0x3E, Word(o + 0x3E) + Word(o + 0x14));
    if (Field_Kind2Hold == 0 && far > 0x40000 && static_cast<U>(Field_Kind2X) == L(ObjTrio + 0x34) &&
        static_cast<U>(Field_Kind2Z) == L(ObjTrio + 0x38)) {
        o = Cur();
        if (S32(L(o + 0xC)) > 0) SetWord(At(at::kKind2XHigh), Word(At(at::kKind2XHigh)) + 5u);
        else if (S32(L(o + 0xC)) < 0) SetWord(At(at::kKind2XHigh), Word(At(at::kKind2XHigh)) - 5u);
        else if (S32(L(o + 0x10)) > 0) SetWord(At(at::kKind2ZHigh), Word(At(at::kKind2ZHigh)) + 5u);
        else if (S32(L(o + 0x10)) < 0) SetWord(At(at::kKind2ZHigh), Word(At(at::kKind2ZHigh)) - 5u);
        const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(Field_Kind2X, Field_Kind2Z)));
        const short view = S16(static_cast<U>(MapView_Elevation));
        MoveScript_FAWord = 0;
        if (view > ground) MoveScript_FAWord = static_cast<unsigned short>((static_cast<int>(ground) - view) / 20);
        MoveScript_F3Divisor = 0x40;
        Cur()[9] = 0x14;
    }
    o = Cur();
    const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(Long(o + 0x34), Long(o + 0x38))));
    o = Cur();
    if (static_cast<short>(Word(o + 0x3E)) > ground) {
        SetL(o + 0x20, 0xFFFFFFF8u);
    } else {
        SetL(o + 0x14, 0);
        SetL(Cur() + 0x20, 0);
        SetWord(Cur() + 0x3E, static_cast<unsigned short>(ground));
    }
    if (SH_CALL(EffectKind17_Bump)()) return;
    o = Cur();
    if (SH_CALL(EffectKind17_BlockedAt)(Long(o + 0x34), Long(o + 0x38))) {
        Cur()[6] = 1;
        Cur()[7] = 2;
    } else if (SH_CALL(EffectKind17_BlockedAhead)()) {
        Cur()[6] = 1;
        Cur()[7] = 1;
    }
    o = Cur();
    if (o[6] != 0) {
        switch (o[7]) {
        case 0:
            if (Word(o + 0x34) == 0 && Word(o + 0x38) == 0) {
                o[7] = 0;
                Cur()[0xA] = 0;
                SH_CALL(Sprite_SetAnimation)(0);
                Cur()[1] = 5;
                Tick();
                return;
            }
            SH_CALL(EffectKind17_AlignTarget)(0);
            break;
        case 1: SH_CALL(EffectKind17_AlignTarget)(0); break;
        case 2: SH_CALL(EffectKind17_AlignTarget)(1); break;
        default: break;
        }
    }
    Tick();
}

// original 0x46B100 (EffectKind17_States[3]): the last stretch to a cell
// edge. The start's x, z kept. With a target +0x18: along x when +0xC is not
// 0 (else z with +0x10 / +0x38) the position plus the speed, held at the
// target (and the target 0) once past it in the speed's direction. The
// ground there: below the height word the fall -8, else the fall and its
// speed 0; the height word += the word +0x14. EffectKind17_Bump found
// something: done. Sunk below the ground: the height the ground at the
// start's point, the fall 0. Target and fall both 0: both speeds within
// 0x4000 and EffectKind17_CellBlocked(the cell) clear - +7 and +0xA 0,
// EffectKind17_TakeCell, Sprite_SetAnimation(0), +1 = 5, the tick; else +1 =
// 4. Sprite_ScriptTick, Sprite_UpdateScreenSlot.
extern "C" void __cdecl EffectKind17_Settle(void) {
    unsigned char* o = Cur();
    const U start_z = L(o + 0x38), start_x = L(o + 0x34);
    if (L(o + 0x18) != 0) {
        if (L(o + 0xC) != 0) {
            SetL(o + 0x34, L(o + 0xC) + start_x);
            o = Cur();
            const std::int32_t pos = S32(L(o + 0x34)), target = S32(L(o + 0x18));
            if (S32(L(o + 0xC)) > 0 ? target < pos : target > pos) {
                SetL(o + 0x34, static_cast<U>(target));
                SetL(Cur() + 0x18, 0);
            }
        } else {
            SetL(o + 0x38, L(o + 0x10) + start_z);
            o = Cur();
            const std::int32_t pos = S32(L(o + 0x38)), target = S32(L(o + 0x18));
            if (S32(L(o + 0x10)) > 0 ? target < pos : target > pos) {
                SetL(o + 0x38, static_cast<U>(target));
                SetL(Cur() + 0x18, 0);
            }
        }
        o = Cur();
    }
    const int ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(Long(o + 0x34), Long(o + 0x38))));
    o = Cur();
    if (static_cast<short>(Word(o + 0x3E)) > ground) {
        SetL(o + 0x20, 0xFFFFFFF8u);
    } else {
        SetL(o + 0x20, 0);
        SetL(Cur() + 0x14, 0);
    }
    o = Cur();
    SetL(o + 0x14, L(o + 0x14) + L(o + 0x20));
    o = Cur();
    SetWord(o + 0x3E, Word(o + 0x3E) + Word(o + 0x14));
    if (SH_CALL(EffectKind17_Bump)()) return;
    o = Cur();
    if (static_cast<short>(Word(o + 0x3E)) < ground) {
        const long back = SH_CALL(AreaMap_Elevation)(S32(start_x), S32(start_z));
        SetWord(Cur() + 0x3E, static_cast<U>(back));
        SetL(Cur() + 0x14, 0);
        SetL(Cur() + 0x20, 0);
        o = Cur();
    }
    if (L(o + 0x18) != 0 || L(o + 0x14) != 0) {
        Tick();
        return;
    }
    if (Abs32(L(o + 0xC)) > 0x4000 || Abs32(L(o + 0x10)) > 0x4000) {
        o[1] = 4;
        Tick();
        return;
    }
    const unsigned char blocked = SH_CALL(EffectKind17_CellBlocked)(Word(o + 0x36), Word(o + 0x3A));
    o = Cur();
    if (blocked) {
        o[1] = 4;
        Tick();
        return;
    }
    o[7] = 0;
    Cur()[0xA] = 0;
    SH_CALL(EffectKind17_TakeCell)();
    SH_CALL(Sprite_SetAnimation)(0);
    Cur()[1] = 5;
    Tick();
}

// original 0x46B2C0 (EffectKind17_States[4]): +0x48 = 2, the scales +0x40 and
// +0x44 up 0x4000; +9 down while not 0 and Field_Kind2Hold is 0. Below a
// scale of 2.0 (+0x40 < 0x20000) a tail jump to Sprite_UpdateScreenSlot; at
// it, with the hold 0 and +9 0, EffectKind17_CameraBack and a tail jump to
// Effect_Release; else nothing.
extern "C" void __cdecl EffectKind17_Grow(void) {
    Cur()[0x48] = 2;
    unsigned char* o = Cur();
    SetL(o + 0x40, L(o + 0x40) + 0x4000u);
    o = Cur();
    SetL(o + 0x44, L(o + 0x44) + 0x4000u);
    o = Cur();
    if (o[9] != 0 && Field_Kind2Hold == 0) {
        o[9] = static_cast<unsigned char>(o[9] - 1);
        o = Cur();
    }
    if (S32(L(o + 0x40)) < 0x20000) {
        SH_CALL(Sprite_UpdateScreenSlot)();
        return;
    }
    if (Field_Kind2Hold != 0 || o[9] != 0) return;
    SH_CALL(EffectKind17_CameraBack)();
    SH_CALL(Effect_Release)();
}

// original 0x46B340 (EffectKind17_States[5]): +9 down while not 0; with the
// hold 0 and +9 0, +1 = 1 and EffectKind17_CameraBack. Sprite_ScriptTick,
// tail Sprite_UpdateScreenSlot.
extern "C" void __cdecl EffectKind17_Rest(void) {
    unsigned char* o = Cur();
    if (o[9] != 0) {
        o[9] = static_cast<unsigned char>(o[9] - 1);
        o = Cur();
    }
    if (Field_Kind2Hold == 0 && o[9] == 0) {
        o[1] = 1;
        SH_CALL(EffectKind17_CameraBack)();
    }
    Tick();
}

// original 0x46B380 (E8 from _Start and _Settle): +0xB = AreaMap_ByteAt(the
// cell +0x36, +0x3A), then AreaMap_SetByte(the cell, 0x10) - the block's
// cell marked, what was there kept.
extern "C" void __cdecl EffectKind17_TakeCell(void) {
    unsigned char* o = Cur();
    const unsigned char under = SH_CALL(AreaMap_ByteAt)(S16(Word(o + 0x36)), S16(Word(o + 0x3A)));
    Cur()[0xB] = under;
    o = Cur();
    SH_CALL(AreaMap_SetByte)(Word(o + 0x36), Word(o + 0x3A), 0x10);
}

// original 0x46B3C0 (E8 from _Grow and _Rest): the view's kind-2 point back
// on the leader (ObjTrio +0x34 / +0x38 into Field_Kind2X / Z),
// MoveScript_FAWord negated, MoveScript_F3Divisor 0x40, Field_Kind2Hold 1.
extern "C" void __cdecl EffectKind17_CameraBack(void) {
    const unsigned short fa = MoveScript_FAWord;
    Field_Kind2X = Long(ObjTrio + 0x34);
    MoveScript_FAWord = static_cast<unsigned short>(0u - fa);
    Field_Kind2Z = Long(ObjTrio + 0x38);
    MoveScript_F3Divisor = 0x40;
    Field_Kind2Hold = 1;
}

// original 0x46B400 (E8 from _Slide and _Settle): Sprite_ObjectAt(+0x34,
// +0x38, 0) found one - its +0x80 bit 0 set - or Party_MemberAt found a
// member other than the leader - its +2 = 1 (the leader, 0, nothing set):
// +1 = 4, Sprite_ScriptTick, Sprite_UpdateScreenSlot, al 1. Neither: al 0.
extern "C" unsigned char __cdecl EffectKind17_Bump(void) {
    unsigned char* o = Cur();
    const unsigned char found = SH_CALL(Sprite_ObjectAt)(Long(o + 0x34), Long(o + 0x38), 0);
    if (found != 0xFF) {
        MarkObject(found, "EffectKind17_Bump");
    } else {
        o = Cur();
        const unsigned char member = SH_CALL(Party_MemberAt)(Long(o + 0x34), Long(o + 0x38), 0);
        if (member == 0xFF) return 0;
        if (member != 0) Member(static_cast<signed char>(member), "EffectKind17_Bump")[2] = 1;
    }
    Cur()[1] = 4;
    Tick();
    return 1;
}

// original 0x46B4B0 (E8 from _Slide, the byte 0 or 1): off a whole cell in x
// (the word +0x34 not 0): moving up (+0xC > 0) the target +0x18 the next
// cell's edge - x + 0x10000 rounded down, or x rounded down for a flag -,
// moving down x rounded down; +1 = 3 (also with no speed). Else the same
// along z with +0x10 / +0x38; on a whole cell both ways, nothing.
extern "C" void __cdecl EffectKind17_AlignTarget(unsigned flag) {
    unsigned char* const o = Cur();
    U pos_off, speed_off;
    if (Word(o + 0x34) != 0) {
        pos_off = 0x34;
        speed_off = 0xC;
    } else if (Word(o + 0x38) != 0) {
        pos_off = 0x38;
        speed_off = 0x10;
    } else {
        return;
    }
    const std::int32_t speed = S32(L(o + speed_off));
    if (speed > 0) {
        const U pos = L(o + pos_off);
        SetL(o + 0x18, ((flag & 0xFF) == 0 ? pos + 0x10000u : pos) & 0xFFFF0000u);
        Cur()[1] = 3;
        return;
    }
    if (speed < 0) {
        SetL(o + 0x18, L(o + pos_off) & 0xFFFF0000u);
        Cur()[1] = 3;
        return;
    }
    o[1] = 3;
}

// original 0x46B580 (E8 from _Push and _Slide): EffectKind17_BlockedAt two
// steps ahead - +0x34 / +0x38 plus twice Field_DirectionSteps[+8] (the byte
// unchecked, read in place); its al.
extern "C" unsigned char __cdecl EffectKind17_BlockedAhead(void) {
    unsigned char* const o = Cur();
    const U d = o[8];
    const U x = L(o + 0x34), z = L(o + 0x38);
    const U dx = L(At(AddressOf(Field_DirectionSteps) + 8u * d));
    const U dz = L(At(AddressOf(Field_DirectionSteps) + 4 + 8u * d));
    return SH_CALL(EffectKind17_BlockedAt)(S32(x + dx * 2u), S32(z + dz * 2u));
}

// original 0x46B580's callee 0x46B5C0 (E8 from _Slide and _BlockedAhead):
// whether the block may not stand at (x, z), 16.16. Its own cell blocked
// (EffectKind17_CellBlocked): 1. Moving in x (+0xC not 0): on a whole cell
// (x's low word 0) 0; else the slope at the cell's middle in x
// (AreaMap_Slope(cell x.8000, z, +8)), and where it slopes (DamageScratch) the
// ground there above the height word +0x3E: 1; moving up in x, the next cell
// in x blocked: 1; else 0. Moving in z the same with z. The original reads
// its arguments' high words through its own frame at +2, so the cells passed
// to _CellBlocked carry the next word above - _CellBlocked reads the low
// words only.
extern "C" unsigned char __cdecl EffectKind17_BlockedAt(long x, long z) {
    const U ux = static_cast<U>(x), uz = static_cast<U>(z);
    const U cell_x = ux >> 16, cell_z = uz >> 16;
    if (SH_CALL(EffectKind17_CellBlocked)(cell_x, cell_z)) return 1;
    unsigned char* o = Cur();
    if (L(o + 0xC) != 0) {
        if ((ux & 0xFFFF) == 0) return 0;
        const U mid = (cell_x << 16) | 0x8000u;
        SH_CALL(AreaMap_Slope)(S32(mid), z, o[8]);
        if (At(bof3::addr::DamageScratch)[0] != 0) {
            const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(S32(mid), z)));
            if (static_cast<short>(Word(Cur() + 0x3E)) < ground) return 1;
        }
        if (S32(L(Cur() + 0xC)) <= 0) return 0;
        return SH_CALL(EffectKind17_CellBlocked)(cell_x + 1, cell_z) ? 1 : 0;
    }
    if ((uz & 0xFFFF) == 0) return 0;
    const U mid = (cell_z << 16) | 0x8000u;
    SH_CALL(AreaMap_Slope)(x, S32(mid), o[8]);
    if (At(bof3::addr::DamageScratch)[0] != 0) {
        const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(x, S32(mid))));
        if (static_cast<short>(Word(Cur() + 0x3E)) < ground) return 1;
    }
    if (S32(L(Cur() + 0x10)) <= 0) return 0;
    return SH_CALL(EffectKind17_CellBlocked)(cell_x, cell_z + 1) ? 1 : 0;
}

// original 0x46B6D0 (E8 from _Settle and _BlockedAt): whether cell (x, z)
// (signed words) takes no block: no floor word there (the area block's cell
// word 0): 1. Else by the ground at the cell's corner and its map byte: the
// height word +0x3E at most 0x100 above the ground on a byte 0xFx: 1; level
// with the ground on a byte 0x10, 0x2x or 0xAx: 1; below it: 1; else 0.
extern "C" unsigned char __cdecl EffectKind17_CellBlocked(unsigned cell_x, unsigned cell_z) {
    const short cx = S16(cell_x), cz = S16(cell_z);
    if (CellWord(cx, cz) == 0) return 1;
    const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(S32(static_cast<U>(cx) << 16), S32(static_cast<U>(cz) << 16))));
    const unsigned char b = SH_CALL(AreaMap_ByteAt)(cx, cz);
    const short height = static_cast<short>(Word(Cur() + 0x3E));
    if (height - ground <= 0x100 && (b & 0xF0) == 0xF0) return 1;
    if (height == ground) return b == 0x10 || (b & 0xF0) == 0x20 || (b & 0xF0) == 0xA0 ? 1 : 0;
    return height < ground ? 1 : 0;
}

// ============================================================================
// Kind 0x1B: a shot along the leader's facing, with a trail
// ============================================================================

// original 0x46B7C0 (EffectKind1B_States[0]): +8 = the leader's facing; 0x46D0E0
// (FC2's); +0x27 = 0x7B; Sprite_SetAnimation((+8 >> 1) + 0x4A, a byte);
// Sprite_LoadPalette(0x80D440, 1); at the leader's point, 0xC0 above its
// height word; the speeds +0xC / +0x10 the direction's unit (0x6696DC[+8],
// unchecked) << 4; the range +0xA 0xA when the leader's member id (ObjTrio
// +0x89) is 1, else 0x12; +0 |= 2; +1 up.
extern "C" void __cdecl EffectKind1B_Start(void) {
    Cur()[8] = ObjTrio[8];
    SH_AT(void (__cdecl*)(), at::kFc2Aim)();
    Cur()[0x27] = 0x7B;
    SH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>((Cur()[8] >> 1) + 0x4A));
    SH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(At(at::kKind1BPalette)), 1);
    SetL(Cur() + 0x34, L(ObjTrio + 0x34));
    SetL(Cur() + 0x38, L(ObjTrio + 0x38));
    SetWord(Cur() + 0x3E, Word(ObjTrio + 0x3E) + 0xC0u);
    unsigned char* o = Cur();
    SetL(o + 0xC, L(At(at::kWalkDelta + 8u * o[8])) << 4);
    o = Cur();
    SetL(o + 0x10, L(At(at::kWalkDelta + 4 + 8u * o[8])) << 4);
    Cur()[0xA] = ObjTrio[0x89] == 1 ? 0xA : 0x12;
    Cur()[0] = static_cast<unsigned char>(Cur()[0] | 2);
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
}

// original 0x46B8A0 (EffectKind1B_States[1]): the point plus the speeds; the
// range +0xA down. At 0, or over a cell with no floor word: animation 0x4E
// and +1 up. EffectKind1B_Hit: animation 0x4F and +1 up. Else on an even
// range a free effect (Effect_FindFree) becomes a trail - +0 = 1, kind 0x1B,
// state 3. Sprite_ScriptTick, Sprite_UpdateScreenSlot.
extern "C" void __cdecl EffectKind1B_Fly(void) {
    unsigned char* o = Cur();
    SetL(o + 0x34, L(o + 0x34) + L(o + 0xC));
    o = Cur();
    SetL(o + 0x38, L(o + 0x38) + L(o + 0x10));
    o = Cur();
    o[0xA] = static_cast<unsigned char>(o[0xA] - 1);
    o = Cur();
    unsigned char animation = 0x4E;
    if (o[0xA] != 0 && CellWord(static_cast<short>(Word(o + 0x36)), static_cast<short>(Word(o + 0x3A))) != 0) {
        if (!SH_CALL(EffectKind1B_Hit)()) {
            if ((Cur()[0xA] & 1) == 0) {
                const unsigned char slot = SH_CALL(Effect_FindFree)();
                if (slot != 0xFF) {
                    unsigned char* const trail = EffectRec(slot, "EffectKind1B_Fly");
                    trail[0] = 1;
                    trail[5] = 0x1B;
                    trail[1] = 3;
                }
            }
            Tick();
            return;
        }
        animation = 0x4F;
    }
    SH_CALL(Sprite_SetAnimation)(animation);
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
    Tick();
}

// original 0x46B980 (EffectKind1B_States[2]): Sprite_ScriptTickOnce; ended, a
// tail jump to Effect_Release, else to Sprite_UpdateScreen.
extern "C" void __cdecl EffectKind1B_End(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)()) SH_CALL(Effect_Release)();
    else SH_CALL(Sprite_UpdateScreen)();
}

// original 0x46B9A0 (EffectKind1B_States[3], a trail _Fly spawns): the source
// Effect_Objects[the leader's +0xB] (unchecked, read in place); +0 |= 2, +0x24
// = 1, its word +0x2C, +0x27 = 0x7B, +0x28 = 2, +0x25 = 0x1D, +0x26 = 0, its
// +0x4C, +0x2B = 1, its +0x29, +0x48 = 0, its point +0x34 / +0x38 and height
// word +0x3E, the tint bytes 0; Sprite_SetAnimation(0x4E); +1 = 2.
extern "C" void __cdecl EffectKind1B_Trail(void) {
    const unsigned char* const src = Effect_Objects + static_cast<U>(ObjTrio[0xB]) * at::kEffectStride;
    Cur()[0] = static_cast<unsigned char>(Cur()[0] | 2);
    Cur()[0x24] = 1;
    SetWord(Cur() + 0x2C, Word(src + 0x2C));
    Cur()[0x27] = 0x7B;
    Cur()[0x28] = 2;
    Cur()[0x25] = 0x1D;
    Cur()[0x26] = 0;
    SetL(Cur() + 0x4C, L(src + 0x4C));
    Cur()[0x2B] = 1;
    Cur()[0x29] = src[0x29];
    Cur()[0x48] = 0;
    SetL(Cur() + 0x34, L(src + 0x34));
    SetL(Cur() + 0x38, L(src + 0x38));
    SetWord(Cur() + 0x3E, Word(src + 0x3E));
    Cur()[0x5D] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5F] = 0;
    Cur()[0x5C] = 0;
    SH_CALL(Sprite_SetAnimation)(0x4E);
    Cur()[1] = 2;
}

// original 0x46BA90 (E8 from _Fly): an object at the point (Sprite_ObjectAt,
// its +0x80 bit 0 set): 1. The ground there more than 0x40 above the
// leader's height word: 1. Else whether the map byte of the cell is 0x11.
extern "C" unsigned char __cdecl EffectKind1B_Hit(void) {
    unsigned char* o = Cur();
    const unsigned char found = SH_CALL(Sprite_ObjectAt)(Long(o + 0x34), Long(o + 0x38), 0);
    if (found != 0xFF) {
        MarkObject(found, "EffectKind1B_Hit");
        return 1;
    }
    o = Cur();
    const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(Long(o + 0x34), Long(o + 0x38))));
    if (static_cast<int>(static_cast<short>(Word(ObjTrio + 0x3E))) + 0x40 < ground) return 1;
    o = Cur();
    return SH_CALL(AreaMap_ByteAt)(S16(Word(o + 0x36)), S16(Word(o + 0x3A))) == 0x11 ? 1 : 0;
}

// ============================================================================
// Kind 0x30
// ============================================================================

// original 0x46BB30 (Effect_KindHandlers[0x30]): a tail jump through
// EffectKind30_States 0x653FE4 by +1.
extern "C" void __cdecl EffectKind30_Run(void) { Entry(AddressOf(EffectKind30_States), Cur()[1], "EffectKind30_Run")(); }

// original 0x46BB50 (EffectKind30_States[0]): +8 = 1;
// Sprite_InitFromEntry(the area's entry list (Area_Descriptors[Game_AreaNumber]
// +8, unchecked, read in place) + 8 * +0xB); +0x70 = 1; the tint bytes
// +0x5D..+0x5F 0x80; +0xC, +0x10, +0x14, +9 0; 0x46BF80 (FC2's); +0xB = 0, +1
// up; Sprite_UpdateScreenA.
extern "C" void __cdecl EffectKind30_Start(void) {
    Cur()[8] = 1;
    const unsigned char* const desc = Area_Descriptors[Game_AreaNumber];
    const U list = L(desc + 8);
    unsigned char* const entry = At(list + 8u * Cur()[0xB]);
    SH_CALL(Sprite_InitFromEntry)(entry);
    SetL(Cur() + 0x70, 1);
    Cur()[0x5D] = 0x80;
    Cur()[0x5E] = 0x80;
    Cur()[0x5F] = 0x80;
    SetL(Cur() + 0xC, 0);
    SetL(Cur() + 0x10, 0);
    SetL(Cur() + 0x14, 0);
    Cur()[9] = 0;
    SH_AT(void (__cdecl*)(), at::kFc2Place)();
    Cur()[0xB] = 0;
    Cur()[1] = static_cast<unsigned char>(Cur()[1] + 1);
    SH_CALL(Sprite_UpdateScreenA)();
}

void FieldC1_Inject() {
    if (bof3::WantsShadow("field_c1")) field_c1::SelfTest();
    BOF3_INJECT(Config_DrawRowLabel);
    BOF3_INJECT(EffectKind06_PlayOnce);
    BOF3_INJECT(EffectKind06_Fade);
    BOF3_INJECT(EffectKind19_Run);
    BOF3_INJECT(EffectKind19_Tick);
    BOF3_INJECT(EffectKind04_HoldTick);
    BOF3_INJECT(EffectKind31_Run);
    BOF3_INJECT(CameraZoom_Start);
    BOF3_INJECT(CameraZoom_Step);
    BOF3_INJECT(CameraZoom_End);
    BOF3_INJECT(EffectKind32_Run);
    BOF3_INJECT(EffectKind32_Throw);
    BOF3_INJECT(EffectKind32_Arc);
    BOF3_INJECT(EffectKind32_Settle);
    BOF3_INJECT(Effect_StateRelease);
    BOF3_INJECT(EffectKind37_Start);
    BOF3_INJECT(EffectKind37_Play);
    BOF3_INJECT(EffectKind14_Start);
    BOF3_INJECT(EffectKind14_Hold);
    BOF3_INJECT(EffectKind3C_Start);
    BOF3_INJECT(EffectKind3C_Hold);
    BOF3_INJECT(EffectKind17_Start);
    BOF3_INJECT(EffectKind17_Push);
    BOF3_INJECT(EffectKind17_Slide);
    BOF3_INJECT(EffectKind17_Settle);
    BOF3_INJECT(EffectKind17_Grow);
    BOF3_INJECT(EffectKind17_Rest);
    BOF3_INJECT(EffectKind17_TakeCell);
    BOF3_INJECT(EffectKind17_CameraBack);
    BOF3_INJECT(EffectKind17_Bump);
    BOF3_INJECT(EffectKind17_AlignTarget);
    BOF3_INJECT(EffectKind17_BlockedAhead);
    BOF3_INJECT(EffectKind17_BlockedAt);
    BOF3_INJECT(EffectKind17_CellBlocked);
    BOF3_INJECT(EffectKind1B_Start);
    BOF3_INJECT(EffectKind1B_Fly);
    BOF3_INJECT(EffectKind1B_End);
    BOF3_INJECT(EffectKind1B_Trail);
    BOF3_INJECT(EffectKind1B_Hit);
    BOF3_INJECT(EffectKind30_Run);
    BOF3_INJECT(EffectKind30_Start);
}
