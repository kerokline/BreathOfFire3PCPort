// Effect kinds 0x45..0x49 - round thirteen, wave two, group E2D: the 54
// functions of the cut (analysis/round13_cut.tsv) in 0x4771B0..0x4789C1, each
// read with capstone to its last instruction (docs/effect_2d.md section 1).
// Effect_RunObjects (ours) makes each live record of Effect_Objects (20 of 0x80
// bytes) Sprite_Current and calls Effect_KindHandlers[+5]; each kind here is a
// dispatcher by +1 through its state table (none bounded by a compare) and the
// states it names:
//
//   kind 0x45  a 72 x 72 panel at (0x82, 0xA) - two filled squares, a border,
//              a grid - and a trace of 36 samples of a sine wave; state 1 slows
//              the wave and grows it while the flag 0x25 of the chapter's row is
//              set, state 2 reads the confirm button and sets or clears the flag
//              0x26 by where the wave stands (chapter 6's run 13 spawns it,
//              scena_sc6.cpp);
//   kind 0x46  a screen-wide tile whose colour steps up and down by 4 over 12
//              frames, five times, then the chapters' counter byte 0x903848 up;
//              every field sprite's screen place redrawn each frame;
//   kind 0x47  two free sprites put beside the leader (or sprite object 2 when
//              +0xB is 0), tinted red and blue, the leader (or object 2) hidden
//              for four frames (area 85's handlers 3 and 4, area_w2b.cpp);
//   kind 0x48  rings of four sparks every four frames, +0xB rings, then the
//              sparks run out (area 52's handlers 6 and 7, area_w1c.cpp);
//   kind 0x49  ten variants picked by +1 at the spawn (chapter 7,
//              Scena07_TakeEffect49): 0 puffs, 1 a CLUT fade, 2 a glow flown to a
//              point, 3 dust, 4 (and 5..9, E2E's) by +2.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, writes a sprite record past the pool on an index the
// callee never gives, or would loop forever, ours aborts with a message
// (docs/effect_2d.md section 2).
#include "game/effect_2d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
short SW(const unsigned char* p) { return static_cast<short>(Word(p)); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
void StoreFloat(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }
// The x87 at the game's 53-bit precision: a float plus or less a whole number
// rounded once to 53 bits, then to a float at the store.
void StoreSum(unsigned char* p, float a, int b) { StoreFloat(p, static_cast<float>(static_cast<double>(a) + static_cast<double>(b))); }
unsigned char* Row() { return *reinterpret_cast<unsigned char**>(At(at::kFlagRow)); }
unsigned char* Colour() { return At(at::kPanelColour); }   // [0] 0x6761C4, [1] 0x6761C5, [2] 0x6761C6

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + which]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through the
// dword after - the next table, or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned which) {
    const unsigned state = Sprite_Current[which];
    if (state >= entries)
        bof3::Fatal("%s: byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2d.md section 2)",
                    who, which, state, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// Sprite_Objects record `index`: a Fatal past the thirty, where the original
// writes past the pool (Sprite_FindFree answers 0..29 or 0xFF).
unsigned char* SpriteRec(const char* who, unsigned index) {
    if (index >= at::kSprites)
        bof3::Fatal("%s: sprite index %u is past Sprite_Objects' 30 records - the original writes past the pool "
                    "(docs/effect_2d.md section 2)",
                    who, index);
    return Sprite_Objects + index * at::kSpriteStride;
}

unsigned char* Spark(unsigned i) { return EffectKind30_Shards + i * at::kSparkStride; }

}  // namespace

// ===========================================================================
// Kind 0x45: Effect_KindHandlers[0x45] (0x655464), EffectKind45_States (four)
// ===========================================================================

// original 0x4771B0 (0x12 bytes): jmp [EffectKind45_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind45_Run(void) { Dispatch("EffectKind45_Run", AddressOf(EffectKind45_States), EffectKind45_States_count, 1); }

// original 0x4771D0 (state 0): the first 32 samples of the history cleared,
// Field_ScriptFlags bit 8 set (byte 0x9039A3 |= 1); +0x2E (the phase) 0, +0x30
// (the frequency) 0xA00, +0x32 (the amplitude) 0, +0x5D (the markers' shade)
// 0; +1 up. With the flag 0x25 of the chapter's row: Music_FadeOutStop(10) and
// Sound_PlayEffect(0x203).
extern "C" void __cdecl EffectKind45_Start(void) {
    for (unsigned i = 0; i < 0x20; ++i) {
        SetWord(EffectKind30_Shards + 4 * i, 0);
        SetWord(EffectKind30_Shards + 4 * i + 2, 0);
    }
    unsigned char* const flags_high = reinterpret_cast<unsigned char*>(&Field_ScriptFlags) + 1;   // byte 0x9039A3
    *flags_high = static_cast<unsigned char>(*flags_high | 1);
    SetWord(S() + 0x2E, 0);
    SetWord(S() + 0x30, 0xA00);
    SetWord(S() + 0x32, 0);
    S()[0x5D] = 0;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    if (SH_CALL(Flags_Test)(Row(), 0x25)) {
        SH_CALL(Music_FadeOutStop)(0xA);
        SH_CALL(Sound_PlayEffect)(0x203);
    }
}

// original 0x477250 (state 1): the panel; twice: the phase +0x2E up (0 at
// 0x48), and with the flag 0x25 the frequency +0x30 down 0x10 (not below
// 0x200), the amplitude +0x32 up 1 on a frame a multiple of 4 (not above 0x18)
// and the offset +0x36 0; a sample pushed. Then the trace. Frequency 0x200 and
// amplitude 0x18: +1 up; else a cancel or confirm button pressed: +1 = 3.
extern "C" void __cdecl EffectKind45_Tune(void) {
    SH_CALL(EffectKind45_DrawPanel)(0x82, 0xA);
    for (unsigned pass = 0; pass < 2; ++pass) {
        unsigned char* s = S();
        SetWord(s + 0x2E, Word(s + 0x2E) + 1u);
        if (SW(s + 0x2E) >= 0x48) SetWord(s + 0x2E, 0);
        if (SH_CALL(Flags_Test)(Row(), 0x25)) {
            s = S();
            SetWord(s + 0x30, Word(s + 0x30) + 0xFFF0u);
            if (SW(s + 0x30) < 0x200) SetWord(s + 0x30, 0x200);
            SetWord(s + 0x32, Word(s + 0x32) + ((Frame_Counter & 3u) == 0 ? 1u : 0u));
            if (SW(s + 0x32) > 0x18) SetWord(s + 0x32, 0x18);
            SetWord(s + 0x36, 0);
        }
        s = S();
        SH_CALL(EffectKind45_PushSample)(0x82, 0xA, Word(s + 0x2E), Word(s + 0x30), Word(s + 0x32), Word(s + 0x36));
    }
    SH_CALL(EffectKind45_DrawTrace)(0x82, 0xA);
    unsigned char* const s = S();
    if (Word(s + 0x32) == 0x18 && Word(s + 0x30) == 0x200) {
        s[1] = static_cast<unsigned char>(s[1] + 1);
        return;
    }
    const unsigned short pressed = Input_Pressed;
    if ((Field_CancelButtons & pressed) != 0 || (Field_ConfirmButtons & pressed) != 0) s[1] = 3;
}

// original 0x477370 (state 2): the markers lit (+0x5D = 0x80) when the offset
// +0x36 is 8 and the phase +0x2E 0x20; the panel; twice: the phase up, at 0x48
// back to 0 and the offset down 8 (five bits); a sample pushed; at phase 2
// Sound_PlayEffect(0x205). Then the trace. Confirm pressed: the flag 0x26 set
// when the offset is 8 and the phase within 0x1C..0x24, else cleared; +1 = 3
// either way. Cancel pressed: +1 = 3.
extern "C" void __cdecl EffectKind45_Aim(void) {
    {
        unsigned char* const s = S();
        if (Word(s + 0x36) == 8 && Word(s + 0x2E) == 0x20) s[0x5D] = 0x80;
    }
    SH_CALL(EffectKind45_DrawPanel)(0x82, 0xA);
    for (unsigned pass = 0; pass < 2; ++pass) {
        unsigned char* s = S();
        SetWord(s + 0x2E, Word(s + 0x2E) + 1u);
        if (SW(s + 0x2E) >= 0x48) {
            SetWord(s + 0x2E, 0);
            SetWord(s + 0x36, static_cast<unsigned char>(s[0x36] - 8) & 0x1Fu);
        }
        SH_CALL(EffectKind45_PushSample)(0x82, 0xA, Word(s + 0x2E), Word(s + 0x30), Word(s + 0x32), Word(s + 0x36));
        s = S();
        if (Word(s + 0x2E) == 2) SH_CALL(Sound_PlayEffect)(0x205);
    }
    SH_CALL(EffectKind45_DrawTrace)(0x82, 0xA);
    const unsigned short pressed = Input_Pressed;
    if ((Field_ConfirmButtons & pressed) != 0) {
        unsigned char* const s = S();
        if (Word(s + 0x36) == 8 && SW(s + 0x2E) >= 0x1C && SW(s + 0x2E) <= 0x24) {
            SH_CALL(Flags_Set)(Row(), 0x26);
            S()[1] = 3;
            return;
        }
        SH_CALL(Flags_Clear)(Row(), 0x26);
        S()[1] = 3;
        return;
    }
    if ((Field_CancelButtons & pressed) != 0) S()[1] = 3;
}

// original 0x4774A0 (state 3): with the flag 0x25, Sound_PlayEffect(0x206) when
// the flag 0x26 is set, else 0x204; the flag 0x2D cleared; Effect_Release (a
// tail jump). The row pointer read again for each call.
extern "C" void __cdecl EffectKind45_End(void) {
    if (SH_CALL(Flags_Test)(Row(), 0x25)) SH_CALL(Sound_PlayEffect)(SH_CALL(Flags_Test)(Row(), 0x26) ? 0x206 : 0x204);
    SH_CALL(Flags_Clear)(Row(), 0x2D);
    SH_CALL(Effect_Release)();
}

// original 0x477500: the panel at (x, y). Blend 2 and a grey 0x48 square; blend
// 1 and a green one over it; the outer border (x..x + 0x47) and an inner one
// (x + 3..x + 0x44), two sides of each in blend 1, two in blend 2; then in
// blend 1 seven grid lines each way at 0xC, 0x14, .. 0x3C (the vertical one at
// 0x24 red, the rest green); four marker ticks shaded by +0x5D (red 0x80), and
// +0x5D down 0x10 unless 0. The colour is the three bytes 0x6761C4.. the line
// and rectangle helpers commit.
extern "C" void __cdecl EffectKind45_DrawPanel(unsigned x, unsigned y) {
    unsigned char* const c = Colour();
    SH_CALL(EffectKind45_DrawMode)(2);
    c[0] = 0x40;
    c[1] = 0x40;
    c[2] = 0x40;
    SH_CALL(EffectKind45_FillRect)(x, y, 0x48, 0x48);
    SH_CALL(EffectKind45_DrawMode)(1);
    c[1] = 0x40;
    c[0] = 0;
    c[2] = 0;
    SH_CALL(EffectKind45_FillRect)(x, y, 0x48, 0x48);
    SH_CALL(EffectKind45_DrawMode)(1);
    c[1] = 0x40;
    c[0] = 0;
    c[2] = 0;
    SH_CALL(EffectKind45_DrawLine)(x, y, x + 0x47, y);
    SH_CALL(EffectKind45_DrawLine)(x, y, x, y + 0x47);
    SH_CALL(EffectKind45_DrawLine)(x + 3, y + 0x44, x + 0x44, y + 0x44);
    SH_CALL(EffectKind45_DrawLine)(x + 0x44, y + 3, x + 0x44, y + 0x44);
    SH_CALL(EffectKind45_DrawMode)(2);
    SH_CALL(EffectKind45_DrawLine)(x, y + 0x47, x + 0x47, y + 0x47);
    SH_CALL(EffectKind45_DrawLine)(x + 0x47, y, x + 0x47, y + 0x47);
    SH_CALL(EffectKind45_DrawLine)(x + 3, y + 3, x + 0x44, y + 3);
    SH_CALL(EffectKind45_DrawLine)(x + 3, y + 3, x + 3, y + 0x44);
    SH_CALL(EffectKind45_DrawMode)(1);
    for (U k = 0xC; static_cast<short>(k) < 0x44; k += 8) {
        const bool middle = static_cast<unsigned short>(k) == 0x24;
        const unsigned char green = middle ? 0 : 0x40, red = middle ? 0x40 : 0;
        c[2] = 0;
        c[1] = 0x40;
        SH_CALL(EffectKind45_DrawLine)(x + 4, y + k, x + 0x44, y + k);
        c[1] = green;
        c[2] = red;
        SH_CALL(EffectKind45_DrawLine)(x + k, y + 4, x + k, y + 0x44);
    }
    {
        const unsigned char shade = S()[0x5D];
        c[2] = 0x80;
        c[0] = shade;
        c[1] = shade;
    }
    SH_CALL(EffectKind45_DrawLine)(x + 4, y + 0x24, x + 0xA, y + 0x24);
    SH_CALL(EffectKind45_DrawLine)(x + 0x3E, y + 0x24, x + 0x44, y + 0x24);
    SH_CALL(EffectKind45_DrawLine)(x + 0x21, y + 0xC, x + 0x27, y + 0xC);
    SH_CALL(EffectKind45_DrawLine)(x + 0x21, y + 0x3C, x + 0x27, y + 0x3C);
    unsigned char* const s = S();
    if (s[0x5D] != 0) s[0x5D] = static_cast<unsigned char>(s[0x5D] - 0x10);
}

// original 0x4776E0: a LINE_F2 at Gfx_PacketNext (read once), semi-transparent
// (Gpu_SetSemiTrans 1); the ends (x0, y0), (x1, y1), each an s16 as a float at
// +8, +0xC, +0x14, +0x18; the colour 0x6761C6, 0x6761C5, 0x6761C4 at +4, +5,
// +6; Gfx_CommitPrim(1, 0x20).
extern "C" void __cdecl EffectKind45_DrawLine(unsigned x0, unsigned y0, unsigned x1, unsigned y1) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    StoreFloat(p + 0x08, static_cast<float>(static_cast<short>(x0)));
    StoreFloat(p + 0x0C, static_cast<float>(static_cast<short>(y0)));
    StoreFloat(p + 0x14, static_cast<float>(static_cast<short>(x1)));
    StoreFloat(p + 0x18, static_cast<float>(static_cast<short>(y1)));
    const unsigned char* const c = Colour();
    p[4] = c[2];
    p[5] = c[1];
    p[6] = c[0];
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// original 0x477760: Gpu_GetTPage(0, abr & 0xFF, 0x3C0, 0); Gpu_SetDrawMode at
// Gfx_PacketNext (read after that call) with it (its low word); committed
// Gfx_CommitPrim(1, 0xC).
extern "C" void __cdecl EffectKind45_DrawMode(unsigned abr) {
    const unsigned page = SH_CALL(Gpu_GetTPage)(0, abr & 0xFFu, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
}

// original 0x4777A0: the 36 dwords at EffectKind30_Shards moved up one (the
// oldest dropped); the first gets x word x + phase + 4 and y word y + 0x24 +
// (Math_Sin(((phase - offset - 0x20) * (frequency << 4) / 64) & 0xFFFF) *
// amplitude >> 12) - each argument its s16, the products 32-bit, the divide
// toward zero, the shift arithmetic.
extern "C" void __cdecl EffectKind45_PushSample(unsigned x, unsigned y, unsigned phase, unsigned frequency, unsigned amplitude,
                                               unsigned offset) {
    unsigned char* const h = EffectKind30_Shards;
    for (unsigned i = at::kHistory - 1; i >= 1; --i) SetUL(h + 4 * i, UL(h + 4 * (i - 1)));
    SetWord(h, x + phase + 4);
    const U a = static_cast<U>(static_cast<short>(phase)) - static_cast<U>(static_cast<short>(offset)) - 0x20u;
    const U f = Sar(static_cast<U>(static_cast<short>(frequency)) << 12, 8);
    const U product = a * f;
    const U quotient = Sar(product + (static_cast<std::int32_t>(product) < 0 ? 0x3Fu : 0u), 6);
    const U sine = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(quotient & 0xFFFFu)));
    const U wave = Sar(sine * static_cast<U>(static_cast<short>(amplitude)), 12);
    SetWord(h + 2, wave + y + 0x24);
}

// original 0x477820: the clip (x + 4, y + 4, 0x40, 0x40) on the stack; blend 1;
// the newest sample plotted shade 0x80 when its x lies in the clip's columns;
// then EffectKind45_DrawPoints(0x48, the history, the clip).
extern "C" void __cdecl EffectKind45_DrawTrace(unsigned x, unsigned y) {
    const short clip[4] = {static_cast<short>(x + 4), static_cast<short>(y + 4), 0x40, 0x40};
    SH_CALL(EffectKind45_DrawMode)(1);
    const short first = SW(EffectKind30_Shards);
    if (first >= clip[0] && static_cast<int>(first) < static_cast<int>(clip[2]) + static_cast<int>(clip[0]))
        SH_CALL(EffectKind45_Plot)(reinterpret_cast<const short*>(EffectKind30_Shards), 0x80);
    SH_CALL(EffectKind45_DrawPoints)(0x48, reinterpret_cast<const short*>(EffectKind30_Shards), clip);
}

// original 0x4778A0: for each of the (count & 0xFF) - 1 pairs of points (x, y
// s16 each) from `points`: when the second's x lies in the clip's columns, a
// pixel at it if the first's x is less, else EffectKind45_DrawSegment from the
// first to it; the shade 0x40 down 2 a point (not below 0). The clip's x and w
// read again for each pair.
extern "C" void __cdecl EffectKind45_DrawPoints(unsigned count, const short* points, const short* clip) {
    const int pairs = static_cast<int>(count & 0xFFu) - 1;
    if (pairs <= 0) return;
    unsigned char shade = 0x40;
    const short* from = points;
    const short* to = points + 2;
    for (unsigned char i = 0;;) {
        const short tx = to[0];
        const short left = clip[0];
        if (tx >= left && static_cast<int>(tx) < static_cast<int>(clip[2]) + static_cast<int>(left)) {
            if (from[0] < tx) {
                SH_CALL(EffectKind45_Plot)(to, shade);
            } else {
                U a, b;
                std::memcpy(&a, from, 4);
                std::memcpy(&b, to, 4);
                SH_CALL(EffectKind45_DrawSegment)(a, b, shade);
            }
        }
        if (shade != 0) shade = static_cast<unsigned char>(shade - 2);
        from += 2;
        to += 2;
        i = static_cast<unsigned char>(i + 1);
        if (static_cast<int>(i) >= pairs) break;
    }
}

// original 0x477940: the pixels between `from` and `to` (x the low s16, y the
// high), 16-bit arithmetic throughout: dx and dy their distances; along y when
// dy is not 0 and dx is not more than it (signed), starting at the point of
// the lower y, y up one a pixel and x stepping toward the other point on the
// error term; else along x from the point of the lower x. A pixel each step
// (EffectKind45_Plot at the stepping point, `shade`), the distance of the
// longer axis many (none when it is not above 0). The step count is a byte
// against the 16-bit distance: past 0xFF the original never returns - ours
// aborts there with a message.
extern "C" void __cdecl EffectKind45_DrawSegment(unsigned from, unsigned to, unsigned shade) {
    using W = unsigned short;
    const W x0 = static_cast<W>(from), y0 = static_cast<W>(from >> 16);
    const W x1 = static_cast<W>(to), y1 = static_cast<W>(to >> 16);
    const W dx = static_cast<short>(x1) > static_cast<short>(x0) ? static_cast<W>(x1 - x0) : static_cast<W>(x0 - x1);
    const W dy = static_cast<short>(y1) > static_cast<short>(y0) ? static_cast<W>(y1 - y0) : static_cast<W>(y0 - y1);
    short at[2];
    if (!(static_cast<short>(dx) > static_cast<short>(dy)) && dy != 0) {
        // along y: from the lower y
        const bool swap = static_cast<short>(y0) > static_cast<short>(y1);
        const W sx = swap ? x1 : x0, sy = swap ? y1 : y0, ex = swap ? x0 : x1;
        const W step = static_cast<short>(ex) <= static_cast<short>(sx) ? 0xFFFFu : 1u;
        if (static_cast<short>(dy) <= 0) return;
        if (static_cast<short>(dy) > 0xFF)
            bof3::Fatal("EffectKind45_DrawSegment: a distance of %d along y - the original's byte count never reaches it "
                        "and it never returns (docs/effect_2d.md section 2)",
                        static_cast<int>(static_cast<short>(dy)));
        at[0] = static_cast<short>(sx);
        at[1] = static_cast<short>(sy);
        W error = static_cast<W>(0u - dy);
        const W up = static_cast<W>(2u * dx), down = static_cast<W>(2u * dy);
        for (unsigned char n = 0;;) {
            SH_CALL(EffectKind45_Plot)(at, shade);
            at[1] = static_cast<short>(static_cast<W>(at[1]) + 1u);
            error = static_cast<W>(error + up);
            if (static_cast<short>(error) > 0) {
                at[0] = static_cast<short>(static_cast<W>(at[0]) + step);
                error = static_cast<W>(error - down);
            }
            n = static_cast<unsigned char>(n + 1);
            if (!(static_cast<short>(n) < static_cast<short>(dy))) break;
        }
        return;
    }
    // along x: from the lower x
    const bool swap = static_cast<short>(x0) > static_cast<short>(x1);
    const W sx = swap ? x1 : x0, sy = swap ? y1 : y0, ey = swap ? y0 : y1;
    const W step = static_cast<short>(ey) <= static_cast<short>(sy) ? 0xFFFFu : 1u;
    if (static_cast<short>(dx) <= 0) return;
    if (static_cast<short>(dx) > 0xFF)
        bof3::Fatal("EffectKind45_DrawSegment: a distance of %d along x - the original's byte count never reaches it "
                    "and it never returns (docs/effect_2d.md section 2)",
                    static_cast<int>(static_cast<short>(dx)));
    at[0] = static_cast<short>(sx);
    at[1] = static_cast<short>(sy);
    W error = static_cast<W>(0u - dx);
    const W up = static_cast<W>(2u * dy), down = static_cast<W>(2u * dx);
    for (unsigned char n = 0;;) {
        SH_CALL(EffectKind45_Plot)(at, shade);
        at[0] = static_cast<short>(static_cast<W>(at[0]) + 1u);
        error = static_cast<W>(error + up);
        if (static_cast<short>(error) > 0) {
            at[1] = static_cast<short>(static_cast<W>(at[1]) + step);
            error = static_cast<W>(error - down);
        }
        n = static_cast<unsigned char>(n + 1);
        if (!(static_cast<short>(n) < static_cast<short>(dx))) break;
    }
}

// original 0x477AC0: a TILE_1 at Gfx_PacketNext (read once), semi-transparent;
// the point's two s16 as floats at +8 and +0xC; the three colour bytes the
// shade's low byte; Gfx_CommitPrim(1, 0x14).
extern "C" void __cdecl EffectKind45_Plot(const short* point, unsigned shade) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    StoreFloat(p + 8, static_cast<float>(point[0]));
    const short y = point[1];
    p[4] = static_cast<unsigned char>(shade);
    p[5] = static_cast<unsigned char>(shade);
    p[6] = static_cast<unsigned char>(shade);
    StoreFloat(p + 0xC, static_cast<float>(y));
    SH_CALL(Gfx_CommitPrim)(1, 0x14);
}

// original 0x477B20: a TILE at Gfx_PacketNext (read once), semi-transparent; x,
// y, w, h each an s16 as a float at +8, +0xC, +0x14, +0x18; the colour as
// EffectKind45_DrawLine's; Gfx_CommitPrim(1, 0x1C).
extern "C" void __cdecl EffectKind45_FillRect(unsigned x, unsigned y, unsigned w, unsigned h) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    StoreFloat(p + 0x08, static_cast<float>(static_cast<short>(x)));
    StoreFloat(p + 0x0C, static_cast<float>(static_cast<short>(y)));
    StoreFloat(p + 0x14, static_cast<float>(static_cast<short>(w)));
    StoreFloat(p + 0x18, static_cast<float>(static_cast<short>(h)));
    const unsigned char* const c = Colour();
    p[4] = c[2];
    p[5] = c[1];
    p[6] = c[0];
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// ===========================================================================
// Kind 0x46: Effect_KindHandlers[0x46] (0x655468), EffectKind46_States (five)
// ===========================================================================

// original 0x477BA0 (0x12 bytes): jmp [EffectKind46_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind46_Run(void) { Dispatch("EffectKind46_Run", AddressOf(EffectKind46_States), EffectKind46_States_count, 1); }

// original 0x477BC0 (state 0): +9 = 0xC, +1 = 1, +2 = 0, the colour +0x5F /
// +0x5E / +0x5D 0; the flash drawn and the sprites redrawn (a tail jump).
extern "C" void __cdecl EffectKind46_Start(void) {
    S()[9] = 0xC;
    S()[1] = 1;
    S()[2] = 0;
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    SH_CALL(EffectKind46_DrawFlash)();
    SH_CALL(EffectKind46_RedrawSprites)();
}

namespace {
// States 1 and 2: +9 down; not 0: each colour byte +0x5D..+0x5F plus `step`;
// 0: +9 = 0xC, +1 = 3. Then the flash and the sprites.
void Fade(unsigned char step) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) {
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + step);
        s[0x5E] = static_cast<unsigned char>(s[0x5E] + step);
        s[0x5F] = static_cast<unsigned char>(s[0x5F] + step);
    } else {
        s[9] = 0xC;
        s[1] = 3;
    }
    SH_CALL(EffectKind46_DrawFlash)();
    SH_CALL(EffectKind46_RedrawSprites)();
}
}  // namespace

// original 0x477C10 (state 1): the colour up 4 a frame for 11 frames, then state 3.
extern "C" void __cdecl EffectKind46_FadeIn(void) { Fade(4); }

// original 0x477C70 (state 2): the colour down 4 a frame for 11 frames, then state 3.
extern "C" void __cdecl EffectKind46_FadeOut(void) { Fade(0xFC); }

// original 0x477CD0 (state 3): +2 (the count) up; at 5 the counter byte
// 0x903848 up and +1 = 4; else +1 = 1 + (+2 & 1) - a fade in after an even
// count, out after an odd. Then the flash and the sprites.
extern "C" void __cdecl EffectKind46_Count(void) {
    unsigned char* s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
    s = S();
    if (s[2] == 5) {
        At(at::kCounter0)[0] = static_cast<unsigned char>(At(at::kCounter0)[0] + 1);
        s[1] = 4;
    } else {
        s[1] = static_cast<unsigned char>((s[2] & 1) + 1);
    }
    SH_CALL(EffectKind46_DrawFlash)();
    SH_CALL(EffectKind46_RedrawSprites)();
}

// original 0x477D10 (state 4): the flash and the sprites, each frame (nothing
// here ends the record).
extern "C" void __cdecl EffectKind46_Hold(void) {
    SH_CALL(EffectKind46_DrawFlash)();
    SH_CALL(EffectKind46_RedrawSprites)();
}

// original 0x477D20: a draw mode at Gfx_PacketNext (Gpu_SetDrawMode(0, 1, 0x4F,
// 0)) committed at slot 5 (Gfx_CommitPrim(5, 0xC)); then a TILE at the new
// Gfx_PacketNext: the colour +0x5D, +0x5E, +0x5F at +4..+6, (0, 0) and 320 x
// 240 as floats, semi-transparent, untextured shading off (Gpu_SetShadeTex 0),
// Gfx_CommitPrim(5, 0x1C).
extern "C" void __cdecl EffectKind46_DrawFlash(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x4F, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    p[4] = S()[0x5D];
    p[5] = S()[0x5E];
    const unsigned char blue = S()[0x5F];
    SetLong(p + 8, 0);
    p[6] = blue;
    SetLong(p + 0xC, 0);
    SetUL(p + 0x14, 0x43A00000u);   // 320.0f
    SetUL(p + 0x18, 0x43700000u);   // 240.0f
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0x1C);
}

// original 0x477DB0 (a shared tail: the five states jump to it): for each of the
// 30 Sprite_Objects records, Sprite_Current = it and, with +0 bit 0, +0x29 = 5
// and Sprite_UpdateScreen; then Sprite_Current = the leader (ObjTrio), its
// +0x29 = 5, Sprite_UpdateScreen; Sprite_Current put back.
extern "C" void __cdecl EffectKind46_RedrawSprites(void) {
    unsigned char* const saved = Sprite_Current;
    for (unsigned i = 0; i < at::kSprites; ++i) {
        unsigned char* const o = Sprite_Objects + i * at::kSpriteStride;
        Sprite_Current = o;
        if ((o[0] & 1) != 0) {
            o[0x29] = 5;
            SH_CALL(Sprite_UpdateScreen)();
        }
    }
    Sprite_Current = ObjTrio;
    ObjTrio[0x29] = 5;
    SH_CALL(Sprite_UpdateScreen)();
    Sprite_Current = saved;
}

// ===========================================================================
// Kind 0x47: Effect_KindHandlers[0x47] (0x65546C), EffectKind47_States (three)
// ===========================================================================

// original 0x477E10 (0x12 bytes): jmp [EffectKind47_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind47_Run(void) { Dispatch("EffectKind47_Run", AddressOf(EffectKind47_States), EffectKind47_States_count, 1); }

// original 0x477E30 (state 0): two free sprites into +3 and +4 (none: nothing;
// only the second: the first given back), each marked in use. The model's
// animation byte +0x4B - the leader's, or sprite object 2's when +0xB is 0 -
// looked up among the five triples at 0x654564 (the sixth, past them, when none
// matches). For each sprite: DamageScratch's word = its index,
// EventOp_0x(0x654540 + 17 * +0xB), Sprite_SetAnimationAt(the triple's
// animation, its start). Sprite_Current put back; the first moved (-0x1000,
// +0x1000), the second (+0x1000, -0x1000), both at the leader's height, +0 |=
// 0x20, +0x5C = 0, tinted (0xF, 0, 0, 1) and (0, 0, 0xF, 1); +9 = 4, +1 = 1
// (Sprite_Current read again after the tints). The record is the one read
// after the second Sprite_FindFree throughout.
extern "C" void __cdecl EffectKind47_Start(void) {
    const char* const who = "EffectKind47_Start";
    S()[3] = SH_CALL(Sprite_FindFree)();
    if (S()[3] == 0xFF) return;
    SpriteRec(who, S()[3])[0] = 1;
    S()[4] = SH_CALL(Sprite_FindFree)();
    unsigned char* const self = S();
    if (self[4] == 0xFF) {
        SpriteRec(who, self[3])[0] = 0;
        return;
    }
    SpriteRec(who, self[4])[0] = 1;
    unsigned char model = ObjTrio[0x4B];
    if (self[0xB] == 0) model = At(at::kObject2)[0x4B];
    unsigned k = 0;
    while (k < 5 && model != At(at::kTwinAnims + 3 * k)[0]) ++k;
    const unsigned char* const triple = At(at::kTwinAnims + 3 * k);
    SetWord(At(bof3::addr::DamageScratch), self[3]);
    SH_CALL(EventOp_0x)(At(at::kTwinOps + 0x11u * self[0xB]));
    SH_CALL(Sprite_SetAnimationAt)(triple[1], triple[2]);
    SetWord(At(bof3::addr::DamageScratch), self[4]);
    SH_CALL(EventOp_0x)(At(at::kTwinOps + 0x11u * self[0xB]));
    SH_CALL(Sprite_SetAnimationAt)(triple[1], triple[2]);
    Sprite_Current = self;
    unsigned char* a = SpriteRec(who, self[3]);
    SetUL(a + 0x34, UL(a + 0x34) + 0xFFFFF000u);
    SetUL(a + 0x38, UL(a + 0x38) + 0x1000u);
    unsigned char* b = SpriteRec(who, self[4]);
    SetUL(b + 0x34, UL(b + 0x34) + 0x1000u);
    const U height = UL(ObjTrio + 0x3C);
    SetUL(b + 0x38, UL(b + 0x38) + 0xFFFFF000u);
    SetUL(b + 0x3C, height);
    SetUL(a + 0x3C, height);
    a = SpriteRec(who, self[3]);
    a[0] = static_cast<unsigned char>(a[0] | 0x20);
    b = SpriteRec(who, self[4]);
    b[0] = static_cast<unsigned char>(b[0] | 0x20);
    SpriteRec(who, self[4])[0x5C] = 0;
    SpriteRec(who, self[3])[0x5C] = 0;
    SH_CALL(Sprite_SetTint)(SpriteRec(who, self[3]), 0xF, 0, 0, 1);
    SH_CALL(Sprite_SetTint)(SpriteRec(who, self[4]), 0, 0, 0xF, 1);
    S()[9] = 4;
    S()[1] = 1;
}

// original 0x4780D0 (state 1): while +9 is not 0 the leader's +0 |= 0x40 (sprite
// object 2's when +0xB is 0) and +9 down; at 0 that bit cleared, both sprites'
// +0 = 0 and +1 = 2.
extern "C" void __cdecl EffectKind47_Blink(void) {
    unsigned char* const s = S();
    unsigned char* const model = s[0xB] != 0 ? ObjTrio : At(at::kObject2);
    if (s[9] != 0) {
        model[0] = static_cast<unsigned char>(model[0] | 0x40);
        s[9] = static_cast<unsigned char>(s[9] - 1);
        return;
    }
    model[0] = static_cast<unsigned char>(model[0] & 0xBF);
    SpriteRec("EffectKind47_Blink", s[3])[0] = 0;
    SpriteRec("EffectKind47_Blink", s[4])[0] = 0;
    s[1] = 2;
}

// original 0x478160 (EffectKind47_States[2], EffectKind14_States[2],
// EffectKind3C_States[2]): Sprite_ReleaseTint for Sprite_Objects[+3] and
// [+4] (Sprite_Current read for each; the addresses only - the callee compares,
// it does not read the record), then Effect_Release (a tail jump).
extern "C" void __cdecl EffectTwinSprites_Release(void) {
    SH_CALL(Sprite_ReleaseTint)(Sprite_Objects + S()[3] * at::kSpriteStride);
    SH_CALL(Sprite_ReleaseTint)(Sprite_Objects + S()[4] * at::kSpriteStride);
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x48: Effect_KindHandlers[0x48] (0x655470), EffectKind48_States (three)
// ===========================================================================

// original 0x4781B0 (0x12 bytes): jmp [EffectKind48_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind48_Run(void) { Dispatch("EffectKind48_Run", AddressOf(EffectKind48_States), EffectKind48_States_count, 1); }

// original 0x4781D0 (state 0): another live kind-0x48 record in the pool
// (Sprite_Current read once): this one released. Else +6 (the rings thrown) 0,
// +9 0, the sparks cleared, Sound_PlayEffect(0x203), +1 up.
extern "C" void __cdecl EffectKind48_Start(void) {
    unsigned char* const self = S();
    for (unsigned i = 0; i < at::kEffects; ++i) {
        const unsigned char* const e = Effect_Objects + i * at::kEffectStride;
        if (e[0] != 0 && e[5] == 0x48 && e != self) {
            SH_CALL(Effect_Release)();
            return;
        }
    }
    self[6] = 0;
    S()[9] = 0;
    SH_CALL(EffectKind48_ClearSparks)();
    SH_CALL(Sound_PlayEffect)(0x203);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x478230 (state 1): a ring spawned when +9 is 0; +9 up, at 4 back to
// 0 and +6 up; the sparks stepped; +6 at +0xB: +1 up.
extern "C" void __cdecl EffectKind48_Burst(void) {
    if (S()[9] == 0) SH_CALL(EffectKind48_SpawnRing)();
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (s[9] == 4) {
        s[9] = 0;
        s[6] = static_cast<unsigned char>(s[6] + 1);
    }
    SH_CALL(EffectKind48_StepSparks)();
    s = S();
    if (s[6] == s[0xB]) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x478280 (state 2): the sparks stepped; none live: Effect_Release (a
// tail jump).
extern "C" void __cdecl EffectKind48_Fade(void) {
    if (SH_CALL(EffectKind48_StepSparks)() == 0) SH_CALL(Effect_Release)();
}

// original 0x478290: four sparks at the angles 0x80, 0x180, 0x280, 0x380, each
// the first free (none: skipped): +0x15 = 1 (live), +0x16 = 8 (its life), x =
// Math_Cos(angle) << 4 + Sprite_Current's x, z = Math_Sin(angle) << 4 + its z
// (Sprite_Current read after each call), the height its height, +0x10 = 0x1800
// (the size), +0x14 = 0xC0 (the shade).
extern "C" void __cdecl EffectKind48_SpawnRing(void) {
    U angle = 0x80;
    for (unsigned k = 0; k < 4; ++k, angle += 0x100) {
        unsigned char* const spark = SH_CALL(EffectKind48_FindFreeSpark)();
        if (spark == nullptr) continue;
        spark[0x15] = 1;
        const U a = angle & 0xFFFFu;
        spark[0x16] = 8;
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        SetUL(spark, (c << 4) + UL(S() + 0x34));
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
        SetUL(spark + 4, (sn << 4) + UL(S() + 0x38));
        const U height = UL(S() + 0x3C);
        SetUL(spark + 0x10, 0x1800);
        SetUL(spark + 8, height);
        spark[0x14] = 0xC0;
    }
}

// original 0x478320: the first of the 16 sparks of 0x18 at EffectKind30_Shards
// whose +0x15 is 0, or null.
extern "C" unsigned char* __cdecl EffectKind48_FindFreeSpark(void) {
    for (unsigned i = 0; i < at::kSparks; ++i)
        if (Spark(i)[0x15] == 0) return Spark(i);
    return nullptr;
}

// original 0x478340: +0x15 of the 16 sparks 0.
extern "C" void __cdecl EffectKind48_ClearSparks(void) {
    for (unsigned i = 0; i < at::kSparks; ++i) Spark(i)[0x15] = 0;
}

// original 0x478360: the map camera loaded (EffectGte_LoadMapCamera); each live
// spark (+0x15) drawn, then its height +0x3000, its shade - 0x18, its life down
// and at 0 +0x15 = 0. Answers 1 in al when any spark was live, else 0.
extern "C" unsigned char __cdecl EffectKind48_StepSparks(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kSparks; ++i) {
        unsigned char* const spark = Spark(i);
        if (spark[0x15] == 0) continue;
        SH_CALL(EffectKind48_DrawSpark)(spark);
        const U height = UL(spark + 8) + 0x3000u;
        const unsigned char shade = static_cast<unsigned char>(spark[0x14] + 0xE8);
        const unsigned char life = static_cast<unsigned char>(spark[0x16] - 1);
        SetUL(spark + 8, height);
        spark[0x14] = shade;
        spark[0x16] = life;
        if (life == 0) spark[0x15] = 0;
        any = 1;
    }
    return any;
}

// original 0x4783C0: a draw mode (Gpu_GetTPage(0, 1, 0x2C0, 0x100), committed
// Gfx_CommitPrim(1, 0xC)); a POLY_FT4 at the new Gfx_PacketNext,
// semi-transparent. The spark's point (x, z, height << 8) projected
// (EffectGte_ProjectPoint: screen x, y, depth) and its size (+0x10 >> 6, the
// low word) scaled at that depth (EffectGte_ProjectSize: h); the corners x - h
// .. x + h, y - h .. y + h (the x87 at 53 bits, rounded once to floats), the
// depth at each; u 0xE0 / 0xFF, v 0x30 / 0x4F; the CLUT Gpu_GetClut(0xA0,
// 0x1E3) at +0x16, the page again at +0x26; the colour the shade +0x14;
// Gfx_CommitPrim(1, 0x48). The original hands EffectGte_ProjectSize a size
// whose second word is its uninitialised stack and never reads that quotient;
// ours passes 0 there.
extern "C" void __cdecl EffectKind48_DrawSpark(const unsigned char* spark) {
    const unsigned page = SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    short size[2] = {static_cast<short>(UL(spark + 0x10) >> 6), 0};
    const long point[3] = {Long(spark), Long(spark + 4), static_cast<long>(UL(spark + 8) << 8)};
    short scaled[2] = {0, 0};
    SH_CALL(EffectGte_ProjectSize)(point, size, scaled);
    float screen[3];
    SH_CALL(EffectGte_ProjectPoint)(point, screen);
    const int h = scaled[0];
    StoreSum(prim + 0x28, screen[0], -h);
    StoreSum(prim + 0x08, screen[0], -h);
    StoreSum(prim + 0x38, screen[0], h);
    StoreSum(prim + 0x18, screen[0], h);
    StoreSum(prim + 0x1C, screen[1], -h);
    StoreSum(prim + 0x0C, screen[1], -h);
    StoreSum(prim + 0x3C, screen[1], h);
    StoreSum(prim + 0x2C, screen[1], h);
    std::memcpy(prim + 0x40, &screen[2], 4);
    std::memcpy(prim + 0x30, &screen[2], 4);
    std::memcpy(prim + 0x20, &screen[2], 4);
    std::memcpy(prim + 0x10, &screen[2], 4);
    SetWord(prim + 0x16, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    SetWord(prim + 0x26, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    prim[0x15] = 0x30;
    prim[0x25] = 0x30;
    prim[0x14] = 0xE0;
    prim[0x24] = 0xFF;
    prim[0x34] = 0xE0;
    prim[0x35] = 0x4F;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x4F;
    prim[4] = spark[0x14];
    prim[5] = spark[0x14];
    prim[6] = spark[0x14];
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// ===========================================================================
// Kind 0x49: Effect_KindHandlers[0x49] (0x655474), EffectKind49_Variants (ten)
// ===========================================================================

// original 0x478550 (0x12 bytes): jmp [EffectKind49_Variants + +1 * 4],
// unbounded. +1 is the variant Scena07_TakeEffect49 stored; entries 5..9 are
// group E2E's.
extern "C" void __cdecl EffectKind49_Run(void) {
    Dispatch("EffectKind49_Run", AddressOf(EffectKind49_Variants), EffectKind49_Variants_count, 1);
}

// original 0x478570: variant 0, jmp [EffectKind49_V0States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind49_V0Run(void) {
    Dispatch("EffectKind49_V0Run", AddressOf(EffectKind49_V0States), EffectKind49_V0States_count, 2);
}

// original 0x478590 (variant 0, state 0): the puffs cleared (0x4790C0); the
// dword +0xC = 0x40; +2 up.
extern "C" void __cdecl EffectKind49_V0Start(void) {
    SH_AT(void (__cdecl*)(), at::kPuffsClear)();
    SetUL(S() + 0xC, 0x40);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4785B0 (variant 0, state 1): on a count +0xC with its low three
// bits 0, a free puff (0x47CF20) started (0x4790F0); the puffs stepped
// (0x479260); +0xC down, at 0 +2 up.
extern "C" void __cdecl EffectKind49_V0Emit(void) {
    if ((S()[0xC] & 7) == 0) {
        unsigned char* const puff = SH_AT(unsigned char* (__cdecl*)(), at::kPuffFindFree)();
        if (puff != nullptr) SH_AT(void (__cdecl*)(unsigned char*), at::kPuffStart)(puff);
    }
    SH_AT(unsigned char (__cdecl*)(), at::kPuffsStep)();
    unsigned char* s = S();
    SetUL(s + 0xC, UL(s + 0xC) - 1u);
    s = S();
    if (UL(s + 0xC) == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x4785F0 (variant 0, state 2): the puffs stepped; none live:
// Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind49_V0End(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kPuffsStep)() == 0) SH_CALL(Effect_Release)();
}

// original 0x478600: variant 1, jmp [EffectKind49_V1States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind49_V1Run(void) {
    Dispatch("EffectKind49_V1Run", AddressOf(EffectKind49_V1States), EffectKind49_V1States_count, 2);
}

// original 0x478620 (variant 1, state 0): the dword +0x10 = -8; +2 up.
extern "C" void __cdecl EffectKind49_V1Start(void) {
    SetUL(S() + 0x10, 0xFFFFFFF8u);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x478640 (variant 1, state 1): Area85_ClutShift(+0x10); +0x10 up 1 on
// a frame a multiple of 16; at 0 Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind49_V1Fade(void) {
    SH_CALL(Area85_ClutShift)(Long(S() + 0x10));
    const U up = (Frame_Counter & 0xFu) == 0 ? 1u : 0u;
    unsigned char* s = S();
    SetUL(s + 0x10, UL(s + 0x10) + up);
    s = S();
    if (UL(s + 0x10) == 0) SH_CALL(Effect_Release)();
}

// original 0x478680: variant 2, jmp [EffectKind49_V2States + +2 * 4],
// unbounded; entries 5 and 6 are WeretigerFx_Next (+2 on), 7 Effect_StateRelease.
extern "C" void __cdecl EffectKind49_V2Run(void) {
    Dispatch("EffectKind49_V2Run", AddressOf(EffectKind49_V2States), EffectKind49_V2States_count, 2);
}

// original 0x4786A0 (variant 2, state 0): the glow's word 0x92C49E = 0xC0; the
// dword +0x14 (a speed) 0, then 16 times: +0x14 up 0x800000, x +0x34 up
// 0x10000, the height +0x3C up +0x14 - the glow lifted along its arc at once;
// Sound_PlayEffect(0x202); +0x14 negated; +2 up.
extern "C" void __cdecl EffectKind49_V2Launch(void) {
    SetWord(At(at::kGlowHeight), 0xC0);
    SetUL(S() + 0x14, 0);
    for (unsigned i = 0; i < 16; ++i) {
        SetUL(S() + 0x14, UL(S() + 0x14) + 0x800000u);
        SetUL(S() + 0x34, UL(S() + 0x34) + 0x10000u);
        SetUL(S() + 0x3C, UL(S() + 0x3C) + UL(S() + 0x14));
    }
    SH_CALL(Sound_PlayEffect)(0x202);
    SetUL(S() + 0x14, 0u - UL(S() + 0x14));
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

namespace {
// Variant 2's glow record 0x92C060 stepped (0x4794D0) and drawn (0x4796B0).
void Glow() {
    SH_AT(void (__cdecl*)(unsigned char*), at::kGlowStep)(At(at::kGlow));
    SH_AT(void (__cdecl*)(unsigned char*), at::kGlowDraw)(At(at::kGlow));
}
}  // namespace

// original 0x478720 (variant 2, state 1): the glow; x +0x34 down 0x10000, the
// height +0x3C up the speed +0x14, a speed not 0 up 0x800000; at the target
// (+0x34 = +0xC and +0x38 = +0x10) +9 = 0x2F and +2 up.
extern "C" void __cdecl EffectKind49_V2Fly(void) {
    Glow();
    unsigned char* const s = S();
    SetUL(s + 0x34, UL(s + 0x34) + 0xFFFF0000u);
    SetUL(s + 0x3C, UL(s + 0x3C) + UL(s + 0x14));
    const U speed = UL(s + 0x14);
    if (speed != 0) SetUL(s + 0x14, speed + 0x800000u);
    if (UL(s + 0x34) == UL(s + 0xC) && UL(s + 0x38) == UL(s + 0x10)) {
        s[9] = 0x2F;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
}

// original 0x4787A0 (variant 2, state 2): the glow; +9 down; at 0
// Sound_PlayEffect(0x203), +9 = 0x20, +2 up.
extern "C" void __cdecl EffectKind49_V2Wait(void) {
    Glow();
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] != 0) return;
    SH_CALL(Sound_PlayEffect)(0x203);
    S()[9] = 0x20;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4787F0 (variant 2, state 3): the glow; its word 0x92C49E up 6; +9
// down; at 0 +9 = 0x20, +2 up.
extern "C" void __cdecl EffectKind49_V2Rise(void) {
    Glow();
    unsigned char* const s = S();
    SetWord(At(at::kGlowHeight), Word(At(at::kGlowHeight)) + 6u);
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] != 0) return;
    s[9] = 0x20;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478840 (variant 2, state 4): the glow; its word 0x92C49E down 0xC;
// +9 down; at 0 +2 up.
extern "C" void __cdecl EffectKind49_V2Sink(void) {
    Glow();
    unsigned char* const s = S();
    SetWord(At(at::kGlowHeight), Word(At(at::kGlowHeight)) + 0xFFF4u);
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478880: variant 3, jmp [EffectKind49_V3States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind49_V3Run(void) {
    Dispatch("EffectKind49_V3Run", AddressOf(EffectKind49_V3States), EffectKind49_V3States_count, 2);
}

// original 0x4788A0 (variant 3, state 0): the height +0x3C = the ground's
// elevation at (+0x34, +0x38) (AreaMap_Elevation, its low word signed) << 16;
// the dust cleared (0x47A110); +9 = 0, +0xC = 0x5F, +2 up;
// Sound_PlayEffect(0x205).
extern "C" void __cdecl EffectKind49_V3Start(void) {
    const unsigned char* const s = S();
    const long ground = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    SetUL(S() + 0x3C, static_cast<U>(static_cast<std::int32_t>(static_cast<short>(ground))) << 16);
    SH_AT(void (__cdecl*)(), at::kDustClear)();
    S()[9] = 0;
    SetUL(S() + 0xC, 0x5F);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(Sound_PlayEffect)(0x205);
}

// original 0x478900 (variant 3, state 1): with +9 the step: when the low byte of
// the count +0xC has none of the step's mask bits (0x6545EC[+9]), the word +0x10
// of what 0x6761D0 points at up the step's count (0x6545F4[+9]) and that many
// free dust records (0x47A130) started (0x47A150) - the count re-read by +9
// after each. Then a count with its low nibble 0 moves +9 on; the dust stepped
// (0x47A200); +0xC down, at 0 +2 up. The byte tables are read in place.
extern "C" void __cdecl EffectKind49_V3Burst(void) {
    unsigned char* s = S();
    const unsigned step = s[9];
    if ((s[0xC] & At(at::kBurstMask)[step]) == 0) {
        const unsigned char add = At(at::kBurstCount)[step];
        unsigned char* const anchor = *reinterpret_cast<unsigned char**>(At(at::kDustAnchor));
        SetWord(anchor + 0x10, Word(anchor + 0x10) + add);
        s = S();
        if (At(at::kBurstCount)[s[9]] != 0) {
            unsigned char n = 0;
            do {
                unsigned char* const dust = SH_AT(unsigned char* (__cdecl*)(), at::kDustFindFree)();
                if (dust != nullptr) SH_AT(void (__cdecl*)(unsigned char*), at::kDustStart)(dust);
                s = S();
                n = static_cast<unsigned char>(n + 1);
            } while (n < At(at::kBurstCount)[s[9]]);
        }
    }
    if ((s[0xC] & 0xF) == 0) s[9] = static_cast<unsigned char>(s[9] + 1);
    SH_AT(unsigned char (__cdecl*)(), at::kDustStep)();
    s = S();
    SetUL(s + 0xC, UL(s + 0xC) - 1u);
    s = S();
    if (UL(s + 0xC) == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x4789A0 (variant 3, state 2): the dust stepped; none live:
// Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind49_V3End(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kDustStep)() == 0) SH_CALL(Effect_Release)();
}

// original 0x4789B0: variant 4, jmp [EffectKind49_V4States + +2 * 4], unbounded
// (its four states are group E2E's).
extern "C" void __cdecl EffectKind49_V4Run(void) {
    Dispatch("EffectKind49_V4Run", AddressOf(EffectKind49_V4States), EffectKind49_V4States_count, 2);
}

void Effect2D_Inject() {
    if (bof3::WantsShadow("effect_2d")) effect_2d::SelfTest();
    BOF3_INJECT(EffectKind45_Run);
    BOF3_INJECT(EffectKind45_Start);
    BOF3_INJECT(EffectKind45_Tune);
    BOF3_INJECT(EffectKind45_Aim);
    BOF3_INJECT(EffectKind45_End);
    BOF3_INJECT(EffectKind45_DrawPanel);
    BOF3_INJECT(EffectKind45_DrawLine);
    BOF3_INJECT(EffectKind45_DrawMode);
    BOF3_INJECT(EffectKind45_PushSample);
    BOF3_INJECT(EffectKind45_DrawTrace);
    BOF3_INJECT(EffectKind45_DrawPoints);
    BOF3_INJECT(EffectKind45_DrawSegment);
    BOF3_INJECT(EffectKind45_Plot);
    BOF3_INJECT(EffectKind45_FillRect);
    BOF3_INJECT(EffectKind46_Run);
    BOF3_INJECT(EffectKind46_Start);
    BOF3_INJECT(EffectKind46_FadeIn);
    BOF3_INJECT(EffectKind46_FadeOut);
    BOF3_INJECT(EffectKind46_Count);
    BOF3_INJECT(EffectKind46_Hold);
    BOF3_INJECT(EffectKind46_DrawFlash);
    BOF3_INJECT(EffectKind46_RedrawSprites);
    BOF3_INJECT(EffectKind47_Run);
    BOF3_INJECT(EffectKind47_Start);
    BOF3_INJECT(EffectKind47_Blink);
    BOF3_INJECT(EffectTwinSprites_Release);
    BOF3_INJECT(EffectKind48_Run);
    BOF3_INJECT(EffectKind48_Start);
    BOF3_INJECT(EffectKind48_Burst);
    BOF3_INJECT(EffectKind48_Fade);
    BOF3_INJECT(EffectKind48_SpawnRing);
    BOF3_INJECT(EffectKind48_FindFreeSpark);
    BOF3_INJECT(EffectKind48_ClearSparks);
    BOF3_INJECT(EffectKind48_StepSparks);
    BOF3_INJECT(EffectKind48_DrawSpark);
    BOF3_INJECT(EffectKind49_Run);
    BOF3_INJECT(EffectKind49_V0Run);
    BOF3_INJECT(EffectKind49_V0Start);
    BOF3_INJECT(EffectKind49_V0Emit);
    BOF3_INJECT(EffectKind49_V0End);
    BOF3_INJECT(EffectKind49_V1Run);
    BOF3_INJECT(EffectKind49_V1Start);
    BOF3_INJECT(EffectKind49_V1Fade);
    BOF3_INJECT(EffectKind49_V2Run);
    BOF3_INJECT(EffectKind49_V2Launch);
    BOF3_INJECT(EffectKind49_V2Fly);
    BOF3_INJECT(EffectKind49_V2Wait);
    BOF3_INJECT(EffectKind49_V2Rise);
    BOF3_INJECT(EffectKind49_V2Sink);
    BOF3_INJECT(EffectKind49_V3Run);
    BOF3_INJECT(EffectKind49_V3Start);
    BOF3_INJECT(EffectKind49_V3Burst);
    BOF3_INJECT(EffectKind49_V3End);
    BOF3_INJECT(EffectKind49_V4Run);
}
