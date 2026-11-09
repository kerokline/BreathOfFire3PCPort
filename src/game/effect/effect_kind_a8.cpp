// Moved here by tools/move_funcs.py from effect_4e.cpp, effect_4f.cpp, rest_3f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_a8.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_a8_callees.h"
#include "game/effect_4e.h"
#include "game/effect_4e_callees.h"
#include "game/effect_4f.h"
#include "game/effect_4f_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3f.h"
#include "game/rest_3f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_a8::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::Long;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U AddressOf(const void* p) { return Low32(p); }

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original
// jumps through the dword after - the next kind's table or data.
void DispatchEffect4e(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4e.md section 6)",
                    who, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(static_cast<const unsigned char*>(table) + 4 * state)))();
}

using move_script::At;

U ULEffect4f(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect4f(U a) { return ULEffect4f(At(a)); }

using Handler = scenario_harness::Handler;

// The entry `state` of a kind's table (read in place: the fuzz swaps the cells
// for recorders); a Fatal past the table's own length, where the original jumps
// (or calls) through the dword after - the next kind's table.
Handler Entry(const char* who, U table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4f.md section 6)",
                    who, state, entries, (unsigned)table);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect4f(table + 4 * state)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]
void DispatchEffect4f(const char* who, U table, unsigned entries) { Entry(who, table, entries)(); }

using LD = long double;

using move_script::SetLong;

using move_script::SetWord;

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// --- x87 as the original has it (effect_4e.cpp's form): `fld dword` through
// inline assembly, every operation in long double, `fstp dword` out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}

LD I(U v) { return static_cast<LD>(static_cast<std::int32_t>(v)); }

void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

// Gpu_GetTPage(0, abr, x, y), then Gpu_SetDrawMode(cursor, 0, dtd, the page's
// low word, 0 - the fifth word the leftover of GetTPage's five pushes).
void SetMode(U abr, int x, int y, int dtd) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
}

// The same, committed 0xC to `slot`.
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    SetMode(abr, x, y, dtd);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// +9 down one; true when it reached 0 (Sprite_Current read for each access,
// as the originals).
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

unsigned char* Bar(unsigned i) { return EffectKind30_Shards + i * rest_3f::at::kBarStride; }

}  // namespace

// original 0x491AA0 (hidden in catalog part 6's 0x4918B0): call
// [EffectKindA7_States + +1 * 4], unbounded; then, Sprite_Current read again,
// while +1 is not 0 the glow EffectKindA7_DrawGlow(+0x34, the word +0xC, 7).
extern "C" void __cdecl EffectKindA7_Run(void) {
    DispatchEffect4e("EffectKindA7_Run", EffectKindA7_States, EffectKindA7_States_count);
    unsigned char* const s = S();
    if (s[1] == 0) return;
    using Glow = void (__cdecl*)(const long*, unsigned, unsigned);
    SH_AT(Glow, at::kGlowA7.v)(Point(s + 0x34), Word(s + 0xC), 7);
}

// original 0x491AE0 (state 0, hidden in 0x4918B0): the size +0xC 0, +9 = 0x10,
// +1 up.
extern "C" void __cdecl EffectKindA7_Start(void) {
    unsigned char* const s = S();
    rv::As<records::EffectWork>(s)->work_0c = 0;
    s[9] = 0x10;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x491B00 (state 1, hidden in 0x4918B0): the size up 0x10; +9 down,
// at 0 +9 = 0x20 and +1 up.
extern "C" void __cdecl EffectKindA7_Grow(void) {
    unsigned char* const s = S();
    rv::As<records::EffectWork>(s)->work_0c = rv::As<records::EffectWork>(s)->work_0c + 0x10u;
    if (CountDown()) {
        S()[9] = 0x20;
        NextState();
    }
}

// original 0x491B40 (state 2, hidden in 0x4918B0): +9 down, at 0 +9 = 0x10 and
// +1 up.
extern "C" void __cdecl EffectKindA7_Hold(void) {
    if (CountDown()) {
        S()[9] = 0x10;
        NextState();
    }
}

// original 0x491B70 (state 3, hidden in 0x4918B0): the size down 0x10; +9 down,
// at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindA7_Shrink(void) {
    unsigned char* const s = S();
    rv::As<records::EffectWork>(s)->work_0c = rv::As<records::EffectWork>(s)->work_0c + 0xFFFFFFF0u;
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x491BA0 (hidden in 0x4918B0): jmp [EffectKindA8_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKindA8_Run(void) {
    DispatchEffect4e("EffectKindA8_Run", EffectKindA8_States, EffectKindA8_States_count);
}

// original 0x491BC0 (state 0, hidden in 0x4918B0): the word +0x2E = 0x12C,
// +9 = 0; the bars cleared; +1 up.
extern "C" void __cdecl EffectKindA8_Start(void) {
    unsigned char* s = S();
    rv::As<records::EffectObject>(s)->screen_x = 0x12C;
    s = S();
    s[9] = 0;
    SH_CALL(EffectKindA8_ClearBars)();
    NextState();
}

// original 0x491BF0 (state 1, hidden in 0x4918B0): +9 0 - a bar started and +9
// = (Rand & 0xF) + 0x18; else +9 down; the bars stepped (0xF0); +1 up once the
// chapter's count is 0xE.
extern "C" void __cdecl EffectKindA8_Bars(void) {
    unsigned char* s = S();
    unsigned char next;
    if (s[9] == 0) {
        SH_CALL(EffectKindA8_StartBar)();
        const int r = SH_CALL(Rand)();
        s = S();
        next = static_cast<unsigned char>((r & 0xF) + 0x18);
    } else {
        next = static_cast<unsigned char>(s[9] - 1);
    }
    s[9] = next;
    SH_CALL(EffectKindA8_StepBars)(0xF0);
    if (At(::bof3::Img(rest_3f::at::kCounter.v))[0] == 0xE) NextState();
}

// original 0x491C40 (state 2, hidden in 0x4918B0): the bars stepped (0xF0);
// none left in use, Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindA8_End(void) {
    if (SH_CALL(EffectKindA8_StepBars)(0xF0) == 0) SH_CALL(Effect_Release)();
}

// original 0x491C60 (hidden in 0x4918B0): call [EffectKindA9_States + +1 *
// 4], unbounded; then, while +1 is not 0, the disc EffectKindA9_DrawDisc(the word +0x2E,
// the word +0x30, 0x80).
extern "C" void __cdecl EffectKindA9_Run(void) {
    DispatchEffect4e("EffectKindA9_Run", EffectKindA9_States, EffectKindA9_States_count);
    unsigned char* const s = S();
    if (s[1] == 0) return;
    using Disc = void (__cdecl*)(unsigned, unsigned, unsigned);
    SH_AT(Disc, at::kDiscA9.v)(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_30, 0x80);
}

// original 0x491CA0 (state 0, hidden in 0x4918B0): the radii words +0x2E, +0x30
// 0, +9 = 0xA, +1 up.
extern "C" void __cdecl EffectKindA9_Start(void) {
    unsigned char* const s = S();
    rv::As<records::EffectObject>(s)->screen_x = 0;
    rv::As<records::EffectObject>(s)->screen_y = 0;
    s[9] = 0xA;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x491CD0 (state 1, hidden in 0x4918B0): the radii up 0x15 / 9; +9
// down, at 0 +9 = 0x5A and +1 up.
extern "C" void __cdecl EffectKindA9_Grow(void) {
    unsigned char* const s = S();
    rv::As<records::EffectObject>(s)->screen_x = static_cast<std::uint16_t>(rv::As<records::EffectObject>(s)->screen_x) + 0x15u;
    rv::As<records::EffectObject>(s)->screen_y = static_cast<std::uint16_t>(rv::As<records::EffectObject>(s)->screen_y) + 9u;
    if (CountDown()) {
        S()[9] = 0x5A;
        NextState();
    }
}

// original 0x491D10 (state 2, hidden in 0x4918B0): once the chapter's count is
// 9, +9 = 0x1E and +1 up.
extern "C" void __cdecl EffectKindA9_Wait(void) {
    if (At(::bof3::Img(rest_3f::at::kCounter.v))[0] != 9) return;
    S()[9] = 0x1E;
    NextState();
}

// original 0x491D30 (state 3, hidden in 0x4918B0): the radii down 7 / 3; +9
// down, at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKindA9_Shrink(void) {
    unsigned char* const s = S();
    rv::As<records::EffectObject>(s)->screen_x = static_cast<std::uint16_t>(rv::As<records::EffectObject>(s)->screen_x) + 0xFFF9u;
    rv::As<records::EffectObject>(s)->screen_y = static_cast<std::uint16_t>(rv::As<records::EffectObject>(s)->screen_y) + 0xFFFDu;
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x491D70 (hidden in 0x4918B0): jmp [EffectKindAA_States + +1 * 4],
// unbounded. Its three states are catalog part 6 rows (EffectKindAA_Start, _FadeIn,
// _FadeOut).
extern "C" void __cdecl EffectKindAA_Run(void) {
    DispatchEffect4f("EffectKindAA_Run", AddressOf(EffectKindAA_States), EffectKindAA_States_count);
}

// original 0x491D90 (state 0, hidden in 0x4918B0): +9 = 0x10, +1 up.
extern "C" void __cdecl EffectKindAA_Start(void) {
    unsigned char* const s = S();
    s[9] = 0x10;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x491DB0 (state 1, hidden in 0x4918B0): the fill in the shade +9 *
// 0xF0 (its low byte: 0 at 0x10 up to 0xF0 at 1); +9 down, at 0 +9 = 0x10 and
// +1 up.
extern "C" void __cdecl EffectKindAA_FadeIn(void) {
    const unsigned char shade = static_cast<unsigned char>(S()[9] * 0xF0u);
    SH_CALL(EffectKindAA_DrawFill)(shade);
    if (CountDown()) {
        S()[9] = 0x10;
        NextState();
    }
}

// original 0x491DF0 (state 2, hidden in 0x4918B0): the fill in the shade 0xFF
// when +9 is 0x10, else +9 << 4 (a byte); +9 down, at 0 Effect_Release (a tail
// jmp).
extern "C" void __cdecl EffectKindAA_FadeOut(void) {
    const unsigned char n = S()[9];
    SH_CALL(EffectKindAA_DrawFill)(n == 0x10 ? 0xFFu : static_cast<unsigned char>(n << 4));
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x491E30 (point, size, colour): EffectKindA0_DrawGlow's code with
// the draw mode committed to slot 7 and every POLY_G3 opaque (SemiTrans 0) and
// committed 0x34 to slot 7: a draw mode (Gpu_GetTPage(0, 1, 0x2C0, 0x100), dtd
// 1); EffectGte_LoadMapCamera; o the point projected, r
// EffectGte_ProjectSize(point, {size, size})'s first word + (Frame_Counter &
// 1); a fan of 32 round o, the centre ((c & 0xFC) << 5, (c & 0xFE) << 6, c <<
// 7), the rim black at o + (cos, sin) * r sar 12 from the angle 0x80 by 0x80,
// every vertex at o's depth.
extern "C" void __cdecl EffectKindA7_DrawGlow(const long* point, unsigned size, unsigned colour) {
    DrawMode(1, 0x2C0, 0x100, 1, 7);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(point, Out(o));
    alignas(4) short sz[2] = {static_cast<short>(size), static_cast<short>(size)};
    alignas(4) short r[2];
    SH_CALL(EffectGte_ProjectSize)(point, sz, r);
    const std::int32_t radius = static_cast<std::int16_t>(static_cast<U>(static_cast<unsigned short>(r[0])) + (Frame_Counter & 1u));
    const unsigned char c = static_cast<unsigned char>(colour);
    const unsigned char red = static_cast<unsigned char>((c & 0xFC) << 5);
    const unsigned char green = static_cast<unsigned char>((c & 0xFE) << 6);
    const unsigned char blue = static_cast<unsigned char>(c << 7);
    alignas(4) unsigned char rim[8];
    StF(rim, I(static_cast<U>(radius)) + F(o));
    SetUL(rim + 4, UL(o + 4));
    U angle = 0;
    for (unsigned n = 0x20; n != 0; --n) {
        angle += 0x80;
        const int t = static_cast<int>(angle & 0xFFFFu);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 0);
        SetUL(p + 8, UL(o));
        SetUL(p + 0xC, UL(o + 4));
        SetUL(p + 0x18, UL(rim));
        SetUL(p + 0x1C, UL(rim + 4));
        const U dx = Sar(static_cast<U>(SH_CALL(Math_Cos)(t)) * static_cast<U>(radius), 12);
        StF(rim, I(dx) + F(o));
        const U dy = Sar(static_cast<U>(SH_CALL(Math_Sin)(t)) * static_cast<U>(radius), 12);
        SetUL(p + 0x28, UL(rim));
        StF(rim + 4, I(dy) + F(o + 4));
        SetUL(p + 0x2C, UL(rim + 4));
        SetUL(p + 0x30, UL(o + 8));
        SetUL(p + 0x20, UL(o + 8));
        SetUL(p + 0x10, UL(o + 8));
        p[4] = red;
        p[5] = green;
        p[6] = blue;
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = 0;
        p[0x25] = 0;
        p[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(7, 0x34);
    }
}

// original 0x491FF0: the sixteen bars out of use (+0 = 0).
extern "C" void __cdecl EffectKindA8_ClearBars(void) {
    for (unsigned i = 0; i < rest_3f::at::kBarCount; ++i) Bar(i)[0] = 0;
}

// original 0x492010: the first bar out of use put in use (+0 = 1, +2 = 0);
// none free, nothing.
extern "C" void __cdecl EffectKindA8_StartBar(void) {
    for (unsigned i = 0; i < rest_3f::at::kBarCount; ++i) {
        unsigned char* const b = Bar(i);
        if (b[0] == 0) {
            b[0] = 1;
            b[2] = 0;
            return;
        }
    }
}

// original 0x492030 (length, its low word read): a draw mode
// (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1) committed 0xC to slot 7;
// EffectGte_LoadMapCamera; each bar in use by its step +2: 0 - +1 = +3 = 4,
// +4 = length, +2 = 1; 1 - +2 = 2; 2 - +4 down 8, below 0 the bar out of use;
// then drawn unless +2 is 0. al 1 when any was in use.
extern "C" unsigned char __cdecl EffectKindA8_StepBars(unsigned length) {
    DrawMode(1, 0x380, 0x100, 1, 7);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    for (unsigned i = 0; i < rest_3f::at::kBarCount; ++i) {
        unsigned char* const b = Bar(i);
        if (b[0] == 0) continue;
        any = 1;
        switch (b[2]) {
        case 0: {
            const unsigned char next = static_cast<unsigned char>(b[2] + 1);
            b[1] = 4;
            b[3] = 4;
            SetWord(b + 4, length & 0xFFFFu);
            b[2] = next;
            break;
        }
        case 1: b[2] = static_cast<unsigned char>(b[2] + 1); break;
        case 2:
            SetWord(b + 4, Word(b + 4) - 8u);
            if (static_cast<std::int16_t>(Word(b + 4)) < 0) b[0] = 0;
            break;
        default: break;
        }
        if (b[2] != 0) SH_CALL(EffectKindA8_DrawBar)(b);
    }
    return any;
}

// original 0x4920F0 (bar): two opaque POLY_G4 across the screen, x 0..320
// (0x43A00000) - from y (+4 - +3 - (Frame_Counter & 1)) black to y +4 red
// (0x80, 0, 0), then from y +4 red to y (+4 + (Frame_Counter & 1) + +3) black;
// each committed 0x44 to slot 7. The depths are Gpu_SetPolyG4's (0.01). Also called by
// E4F's EffectKindB0_StepBars.
//
// DIV-0041: the bars run from Widescreen_FillX() to 320 + the columns - the
// PSP's -32..352 (its twin, SCE15EF2 0x801D1414 / SCE15EF3 0x801D2424, paired
// by shape in psp-widescreen-a.md section 2) - the original's 0..320 while
// the fills are unarmed, as the fuzz compares. This takes the owner's hold of
// 2026-10-04 ("to be looked at in game") under the answer of 2026-10-12 that
// the full-frame fills the PSP widened are stretched.
extern "C" void __cdecl EffectKindA8_DrawBar(const unsigned char* bar) {
    const U f = Frame_Counter & 1u;
    const U left = std::bit_cast<U>(Widescreen_FillX());
    const U right = std::bit_cast<U>(Widescreen_FillRight(320.0f));   // 0x43A00000 unarmed
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SetUL(p + 8, left);
    StF(p + 0xC, I(static_cast<U>(SW(bar + 4)) - bar[3] - f));
    SetUL(p + 0x18, right);
    SetUL(p + 0x28, left);
    StF(p + 0x1C, I(static_cast<U>(SW(bar + 4)) - bar[3] - f));
    SetUL(p + 0x38, right);
    StF(p + 0x2C, I(static_cast<U>(SW(bar + 4))));
    StF(p + 0x3C, I(static_cast<U>(SW(bar + 4))));
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0x80;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = 0x80;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(7, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SetUL(p + 8, left);
    SetUL(p + 0x18, right);
    StF(p + 0xC, I(static_cast<U>(SW(bar + 4))));
    SetUL(p + 0x28, left);
    StF(p + 0x1C, I(static_cast<U>(SW(bar + 4))));
    SetUL(p + 0x38, right);
    StF(p + 0x2C, I(bar[3] + f + static_cast<U>(SW(bar + 4))));
    p[4] = 0x80;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = 0x80;
    p[0x15] = 0;
    p[0x16] = 0;
    StF(p + 0x3C, I(bar[3] + f + static_cast<U>(SW(bar + 4))));
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(7, 0x44);
}

// original 0x492260 (rx, ry, shade - two s16 and a byte): a draw mode
// (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1) committed 0xC to slot 7; a fan of
// 32 semi-transparent POLY_G3 from the screen point (160, 240), its rim from
// the angle 0x800 by 0x40 to 0x1000 at (160 + f + cos * rx sar 12, 240 + f +
// sin * ry sar 12) - each a word, f = (Frame_Counter & 1) << 2 - the centre
// (shade, shade >> 2, shade >> 2), the rim black; each committed 0x34 to slot
// 7. The depths are Gpu_SetPolyG3's (0.01).
extern "C" void __cdecl EffectKindA9_DrawDisc(unsigned rx, unsigned ry, unsigned shade) {
    DrawMode(1, 0x380, 0x100, 1, 7);
    const U f = (Frame_Counter & 1u) << 2;
    const U r1 = static_cast<U>(static_cast<std::int16_t>(rx));
    const U r2 = static_cast<U>(static_cast<std::int16_t>(ry));
    U angle = 0x800;
    alignas(4) unsigned char prev[8];
    {
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
        const U x = static_cast<U>(static_cast<std::int16_t>(Sar(c * r1, 12) + f + 0xA0u));
        const U s = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
        const U y = static_cast<U>(static_cast<std::int16_t>(Sar(s * r2, 12) + f + 0xF0u));
        StF(prev, I(x));
        StF(prev + 4, I(y));
    }
    const unsigned char dim = static_cast<unsigned char>(static_cast<unsigned char>(shade) >> 2);
    for (unsigned n = 0x20; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        angle += 0x40;
        SetUL(p + 8, 0x43200000u);
        SetUL(p + 0xC, 0x43700000u);
        SetUL(p + 0x18, UL(prev));
        SetUL(p + 0x1C, UL(prev + 4));
        const int t = static_cast<int>(angle & 0xFFFFu);
        const U c = static_cast<U>(SH_CALL(Math_Cos)(t));
        const U x = static_cast<U>(static_cast<std::int16_t>(Sar(c * r1, 12) + f + 0xA0u));
        const U s = static_cast<U>(SH_CALL(Math_Sin)(t));
        const U y = static_cast<U>(static_cast<std::int16_t>(Sar(s * r2, 12) + f + 0xF0u));
        StF(prev, I(x));
        SetUL(p + 0x28, UL(prev));
        StF(prev + 4, I(y));
        SetUL(p + 0x2C, UL(prev + 4));
        p[4] = static_cast<unsigned char>(shade);
        p[5] = dim;
        p[6] = dim;
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = 0;
        p[0x25] = 0;
        p[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(7, 0x34);
    }
}

// original 0x492400 (shade, a byte): a draw mode (Gpu_GetTPage(0, 1, 0x380,
// 0x100), dtd 1) committed 0xC to slot 7; two opaque POLY_G4 over the frame -
// rows 0..120 from black to (shade, 0, 0), rows 120..240 back to black - each
// committed 0x44 to slot 7; the depths Gpu_SetPolyG4's. A full-frame fill
// (0, 0) 320 x 240: DIV-0041 section 3c's, drawn from Widescreen_FillX() to
// 320 + Widescreen_Fill() (the original's 0 .. 320 narrow or unarmed: every
// self-test compares those).
extern "C" void __cdecl EffectKindAA_DrawFill(unsigned shade) {
    DrawMode(1, 0x380, 0x100, 1, 7);
    const U left = std::bit_cast<U>(Widescreen_FillX());                                 // DIV-0041: -53 wide, 0 narrow
    const U right = std::bit_cast<U>(320.0f + static_cast<float>(Widescreen_Fill()));    // 0x43A00000, 320.0f narrow
    const unsigned char c = static_cast<unsigned char>(shade);
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SetUL(p + 8, left);
    SetUL(p + 0xC, 0);
    SetUL(p + 0x18, right);
    SetUL(p + 0x1C, 0);
    SetUL(p + 0x28, left);
    SetUL(p + 0x2C, 0x42F00000u);   // 120.0f
    SetUL(p + 0x38, right);
    SetUL(p + 0x3C, 0x42F00000u);
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = c;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = c;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(7, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SetUL(p + 8, left);
    SetUL(p + 0x2C, 0x43700000u);   // 240.0f
    SetUL(p + 0x3C, 0x43700000u);
    SetUL(p + 0xC, 0x42F00000u);
    SetUL(p + 0x18, right);
    SetUL(p + 0x1C, 0x42F00000u);
    SetUL(p + 0x28, left);
    SetUL(p + 0x38, right);
    p[4] = c;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = c;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(7, 0x44);
}

void EffectKindA8_Inject() {
    BOF3_INJECT(EffectKindA7_Run);
    BOF3_INJECT(EffectKindA7_Start);
    BOF3_INJECT(EffectKindA7_Grow);
    BOF3_INJECT(EffectKindA7_Hold);
    BOF3_INJECT(EffectKindA7_Shrink);
    BOF3_INJECT(EffectKindA8_Run);
    BOF3_INJECT(EffectKindA8_Start);
    BOF3_INJECT(EffectKindA8_Bars);
    BOF3_INJECT(EffectKindA8_End);
    BOF3_INJECT(EffectKindA9_Run);
    BOF3_INJECT(EffectKindA9_Start);
    BOF3_INJECT(EffectKindA9_Grow);
    BOF3_INJECT(EffectKindA9_Wait);
    BOF3_INJECT(EffectKindA9_Shrink);
    BOF3_INJECT(EffectKindAA_Run);
    BOF3_INJECT(EffectKindAA_Start);
    BOF3_INJECT(EffectKindAA_FadeIn);
    BOF3_INJECT(EffectKindAA_FadeOut);
    BOF3_INJECT(EffectKindA7_DrawGlow);
    BOF3_INJECT(EffectKindA8_ClearBars);
    BOF3_INJECT(EffectKindA8_StartBar);
    BOF3_INJECT(EffectKindA8_StepBars);
    BOF3_INJECT(EffectKindA8_DrawBar);
    BOF3_INJECT(EffectKindA9_DrawDisc);
    BOF3_INJECT(EffectKindAA_DrawFill);
}
