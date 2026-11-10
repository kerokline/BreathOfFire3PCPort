// Moved here by tools/move_funcs.py from effect_2d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_45.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect_2d.h"
#include "game/effect_2d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2d::at;

}  // namespace

namespace {

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

U AddressOf(const void* p) { return Low32(p); }

U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

void StoreFloat(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

unsigned char* Row() { return At(Chapter_FlagRow); }   // the slot's 32-bit word, a place

unsigned char* Colour() { return At(::bof3::Img(Effect2d_PanelColour_addr)); } // [0] 0x6761C4, [1] 0x6761C5, [2] 0x6761C6

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

}  // namespace

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
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0xA00);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32, 0);
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
// DIV-0188 (Cheats_MinigameNoFail): a confirm press here - too soon, before
// the aim - sets the chapter's flag 0x26 as a press in the window would, so
// the lighthouse's valve opens whenever the player presses.
extern "C" void __cdecl EffectKind45_Tune(void) {
    SH_CALL(EffectKind45_DrawPanel)(0x82, 0xA);
    for (unsigned pass = 0; pass < 2; ++pass) {
        unsigned char* s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_2e + 1u);
        if (static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e) >= 0x48) rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, 0);
        if (SH_CALL(Flags_Test)(Row(), 0x25)) {
            s = S();
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, Word(s + 0x30) + 0xFFF0u);
            if (static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_30) < 0x200) rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0x200);
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_32, rv::As<records::EffectWork>(s)->work_32 + ((Frame_Counter & 3u) == 0 ? 1u : 0u));
            if (static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_32) > 0x18) rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_32, 0x18);
            rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, 0);
        }
        s = S();
        SH_CALL(EffectKind45_PushSample)(0x82, 0xA, rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_32, rv::As<records::EffectObject>(s)->x_whole);
    }
    SH_CALL(EffectKind45_DrawTrace)(0x82, 0xA);
    unsigned char* const s = S();
    if (rv::As<records::EffectWork>(s)->work_32 == 0x18 && rv::As<records::EffectWork>(s)->work_30 == 0x200) {
        s[1] = static_cast<unsigned char>(s[1] + 1);
        return;
    }
    const unsigned short pressed = Input_Pressed;
    if ((Field_ConfirmButtons & pressed) != 0 && Cheats_MinigameNoFail()) SH_CALL(Flags_Set)(Row(), 0x26);
    if ((Field_CancelButtons & pressed) != 0 || (Field_ConfirmButtons & pressed) != 0) S()[1] = 3;
}

// original 0x477370 (state 2): the markers lit (+0x5D = 0x80) when the offset
// +0x36 is 8 and the phase +0x2E 0x20; the panel; twice: the phase up, at 0x48
// back to 0 and the offset down 8 (five bits); a sample pushed; at phase 2
// Sound_PlayEffect(0x205). Then the trace. Confirm pressed: the flag 0x26 set
// when the offset is 8 and the phase within 0x1C..0x24, else cleared; +1 = 3
// either way. Cancel pressed: +1 = 3. DIV-0188 (Cheats_MinigameNoFail): a
// confirm press sets the flag wherever the trace is.
extern "C" void __cdecl EffectKind45_Aim(void) {
    {
        unsigned char* const s = S();
        if (rv::As<records::EffectObject>(s)->x_whole == 8 && rv::As<records::EffectWork>(s)->work_2e == 0x20) s[0x5D] = 0x80;
    }
    SH_CALL(EffectKind45_DrawPanel)(0x82, 0xA);
    for (unsigned pass = 0; pass < 2; ++pass) {
        unsigned char* s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_2e + 1u);
        if (static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e) >= 0x48) {
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, 0);
            rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, static_cast<unsigned char>(s[0x36] - 8) & 0x1Fu);
        }
        SH_CALL(EffectKind45_PushSample)(0x82, 0xA, rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_32, rv::As<records::EffectObject>(s)->x_whole);
        s = S();
        if (rv::As<records::EffectWork>(s)->work_2e == 2) SH_CALL(Sound_PlayEffect)(0x205);
    }
    SH_CALL(EffectKind45_DrawTrace)(0x82, 0xA);
    const unsigned short pressed = Input_Pressed;
    if ((Field_ConfirmButtons & pressed) != 0) {
        unsigned char* const s = S();
        if (Cheats_MinigameNoFail() ||
            (rv::As<records::EffectObject>(s)->x_whole == 8 && static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e) >= 0x1C && static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e) <= 0x24)) {
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

void EffectKind45_Inject() {
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
    // DIV-0188 guards the lighthouse gauge's judgement in EffectKind45_Tune
    // and _Aim; Capcom's would judge as before and the cheat would be silently
    // half off.
    if (Cheats_MinigameNoFail() && (!bof3::IsEnabled(bof3::addr::EffectKind45_Tune) || !bof3::IsEnabled(bof3::addr::EffectKind45_Aim)))
        bof3::Fatal("BOF3X_NO_FAIL=1 needs EffectKind45_Tune and EffectKind45_Aim ours, and one is disabled");
}
