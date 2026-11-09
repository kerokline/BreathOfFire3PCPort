// Moved here by tools/move_funcs.py from effect_3c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_73.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3c.h"
#include "game/effect_3c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3c::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using Handler = scenario_harness::Handler;

using VoidFn = void (__cdecl*)();

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

U Shards() { return AddressOf(EffectKind30_Shards); }

// `fild dword [v]; fsubr dword [c]; fstp dword [o]`: c - v
void FildSubr(std::int32_t v, const void* c, void* o) {
    __asm__ volatile("fildl %2\n\tfsubrs (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(v) : "st", "memory");
}

// `fild dword [v]; fsubr dword [c]; fiadd dword [w]; fstp dword [o]`: c - v + w
void FildSubrAdd(std::int32_t v, const void* c, std::int32_t w, void* o) {
    __asm__ volatile("fildl %2\n\tfsubrs (%1)\n\tfiaddl %3\n\tfstps (%0)"
                     :
                     : "r"(o), "r"(c), "m"(v), "m"(w)
                     : "st", "memory");
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original jumps
// through the dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3c.md section 6)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 (or +2) up, Sprite_Current read afresh.
void Step(unsigned byte) {
    unsigned char* const s = S();
    s[byte] = static_cast<unsigned char>(s[byte] + 1);
}

// +9 down one; answers whether it is now 0 (Sprite_Current read for each access).
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

}  // namespace

// original 0x4852F0 (Effect_KindHandlers[0x73], hidden in 0x4851E0):
// jmp [EffectKind73_States + +1 * 4], unbounded. +1 is the variant area 132
// spawns (0 alone, 1 the pair).
extern "C" void __cdecl EffectKind73_Run(void) {
    Dispatch("EffectKind73_Run", AddressOf(EffectKind73_States), EffectKind73_States_count, 1);
}

// original 0x485310 (variant 0): jmp [EffectKind73A_States + +2 * 4].
extern "C" void __cdecl EffectKind73_RunA(void) {
    Dispatch("EffectKind73_RunA", AddressOf(EffectKind73A_States), EffectKind73A_States_count, 2);
}

// original 0x485330 (variant 0, sub-state 0): the sparks cleared; +9 = 0x40,
// +2 up, sound 0x20C.
extern "C" void __cdecl EffectKind73A_Start(void) {
    SH_CALL(EffectKind73_ClearSparks)();
    S()[9] = 0x40;
    Step(2);
    SH_CALL(Sound_PlayEffect)(0x20C);
}

// original 0x485360 (variant 0, sub-state 1): every fourth count (+9 & 3 0) a
// free spark set up at the record's point; the sparks moved and drawn
// (EffectKind73_MoveSparks, its answer unread); +9 down, at 0 +2 up.
extern "C" void __cdecl EffectKind73A_Emit(void) {
    if ((S()[9] & 3u) == 0) {
        unsigned char* const r = SH_CALL(EffectKind73_FindSpark)();
        if (r != nullptr) SH_CALL(EffectKind73_SparkInit)(r);
    }
    SH_CALL(EffectKind73_MoveSparks)();
    if (CountDown()) Step(2);
}

// original 0x4853A0 (variant 0, sub-state 2) and 0x485430 (variant 1,
// sub-state 1): EffectKind73_MoveSparks; none live (al 0): a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind73A_Fade(void) {
    if (SH_CALL(EffectKind73_MoveSparks)() == 0) SH_CALL(Effect_Release)();
}

// original 0x4853B0 (variant 1): jmp [EffectKind73B_States + +2 * 4].
extern "C" void __cdecl EffectKind73_RunB(void) {
    Dispatch("EffectKind73_RunB", AddressOf(EffectKind73B_States), EffectKind73B_States_count, 2);
}

// original 0x4853D0 (variant 1, sub-state 0): +0xB set - the sparks cleared
// first (the pair's first record); sixteen free sparks set up at the record's
// point, each +0x14 0x80 and its angle byte +0x17 its number 0..15 (a number
// is spent when none is free); sound 0x206, +2 up.
extern "C" void __cdecl EffectKind73B_Burst(void) {
    if (S()[0xB] != 0) SH_CALL(EffectKind73_ClearSparks)();
    for (unsigned n = 0; n < 0x10; ++n) {
        unsigned char* const r = SH_CALL(EffectKind73_FindSpark)();
        if (r == nullptr) continue;
        SH_CALL(EffectKind73_SparkInit)(r);
        SetWord(r + 0x14, 0x80);
        r[0x17] = static_cast<unsigned char>(n);
    }
    SH_CALL(Sound_PlayEffect)(0x206);
    Step(2);
}

extern "C" void __cdecl EffectKind73B_Fade(void) {
    if (SH_CALL(EffectKind73_MoveSparks)() == 0) SH_CALL(Effect_Release)();
}

// original 0x485440 (Effect_KindHandlers[0x74], hidden in 0x4851E0):
// jmp [EffectKind74_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind74_Run(void) {
    Dispatch("EffectKind74_Run", AddressOf(EffectKind74_States), EffectKind74_States_count, 1);
}

// original 0x485460 (state 0): the column's foot +0xC / +0x10 / +0x14 the
// record's point +0x34 / +0x38 / +0x3C, its top +0x1C the foot, its angle
// +0x20 (s16) 0, its width +0x24 0x1000000; +9 = 0x40, +1 up, sound 0x207.
extern "C" void __cdecl EffectKind74_Start(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y));
    SetWord(S() + 0x20, 0);
    SetUL(S() + 0x24, 0x1000000);
    S()[9] = 0x40;
    Step(1);
    SH_CALL(Sound_PlayEffect)(0x207);
}

// original 0x4854D0 (state 1): the top +0x1C up 0x1000000, no higher than the
// foot + 0x8000000; the angle +0x20 turned by 0xFE00; drawn
// (EffectKind74_Draw); +9 down, at 0 +9 = 0x10 and +1 up.
extern "C" void __cdecl EffectKind74_Rise(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x1000000u);
    s = S();
    const U cap = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y) + 0x8000000u;
    if (S32(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c)) > S32(cap)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, cap);
        s = S();
    }
    SetWord(s + 0x20, Word(s + 0x20) + 0xFE00u);
    SH_CALL(EffectKind74_Draw)();
    if (CountDown()) {
        S()[9] = 0x10;
        Step(1);
    }
}

// original 0x485530 (state 2): +9 down; at 0 a tail jump to Effect_Release;
// else the angle turned by 0xFE00, the width +0x24 down 0x100000 and a tail
// jump to EffectKind74_Draw.
extern "C" void __cdecl EffectKind74_Fade(void) {
    if (CountDown()) {
        SH_CALL(Effect_Release)();
        return;
    }
    unsigned char* s = S();
    SetWord(s + 0x20, Word(s + 0x20) + 0xFE00u);
    s = S();
    SetUL(s + 0x24, UL(s + 0x24) + 0xFFF00000u);
    SH_CALL(EffectKind74_Draw)();
}

// original 0x485570: EffectGte_LoadMapCamera; the column from the foot's
// height (+0x14) up to the top (+0x1C) + 0x1000000 in steps of 0x100000, its
// edge a point at the angle (+0x20, up 0x100 a step) 32 * (Math_Cos,
// Math_Sin) round the foot (+0xC, +0x10 - read once, at entry); each step's
// two heights (the step less the width +0x24, no lower than the foot; the
// step, no higher than the top - +0x14, +0x24, +0x1C read afresh) projected,
// and a POLY_FT4 (opaque, grey 0x80, u 0x1F / 0, v 0xFF / 0xE0, the CLUT
// Gpu_GetClut(0, 0x1E5), the page Gpu_GetTPage(1, 0, 0x1C0, 0x100)) from the
// previous step's two projections to this one's, linked at the edge point
// (dy 2, 0x48).
extern "C" void __cdecl EffectKind74_Draw(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* s = S();
    U angle = Word(s + 0x20);
    const U x = rv::As<records::EffectWork>(s)->work_0c, z = rv::As<records::EffectWork>(s)->work_10;
    U y = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y);
    long pt[3];
    float low[3], high[3];
    auto project = [&](U a) {
        pt[0] = static_cast<long>((static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a))) << 5) + x);
        pt[1] = static_cast<long>((static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a))) << 5) + z);
        unsigned char* c = S();
        U h = y - UL(c + 0x24);
        if (S32(h) < S32(static_cast<std::uint32_t>(rv::As<records::EffectObject>(c)->step_y))) h = static_cast<std::uint32_t>(rv::As<records::EffectObject>(c)->step_y);
        pt[2] = static_cast<long>(h);
        SH_CALL(EffectGte_ProjectPoint)(pt, low);
        c = S();
        h = y;
        if (S32(y) > S32(static_cast<std::uint32_t>(rv::As<records::EffectObject>(c)->work_1c))) h = static_cast<std::uint32_t>(rv::As<records::EffectObject>(c)->work_1c);
        pt[2] = static_cast<long>(h);
        SH_CALL(EffectGte_ProjectPoint)(pt, high);
    };
    project(angle);
    s = S();
    if (S32(y) >= S32(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x1000000u)) return;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 0);
        angle += 0x100;
        y += 0x100000;
        std::memcpy(prim + 0x08, low, 12);
        std::memcpy(prim + 0x18, high, 12);
        project(angle & 0xFFFFu);
        std::memcpy(prim + 0x28, low, 12);
        std::memcpy(prim + 0x38, high, 12);
        prim[0x14] = 0x1F;
        prim[0x15] = 0xFF;
        prim[0x24] = 0x1F;
        prim[0x25] = 0xE0;
        prim[0x34] = 0;
        prim[0x35] = 0xFF;
        prim[0x44] = 0;
        prim[0x45] = 0xE0;
        prim[4] = prim[5] = prim[6] = 0x80;
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->tpage, SH_CALL(Gpu_GetTPage)(1, 0, 0x1C0, 0x100));
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->clut, SH_CALL(Gpu_GetClut)(0, 0x1E5));
        SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(pt[0]), static_cast<unsigned long>(pt[1]), 2, 0x48);
        s = S();
    } while (S32(y) < S32(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x1000000u));
}

// original 0x4857C0 (a 0x18-byte spark): at Sprite_Current's point (+4 / +8 /
// +0xC its +0x34 / +0x38 / +0x3C); +0 1, +1 0, +0x14 (s16) 0x80, +3 0x40,
// +0x16 7 (all three colour bits), +2 0x20 (the life).
extern "C" void __cdecl EffectKind73_SparkInit(unsigned char* r) {
    SetUL(r + 4, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    SetUL(r + 8, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    const U y = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height);
    r[0] = 1;
    SetUL(r + 0xC, y);
    r[1] = 0;
    SetWord(r + 0x14, 0x80);
    r[3] = 0x40;
    r[0x16] = 7;
    r[2] = 0x20;
}

// original 0x485810: the first of the 32 sparks of 0x18 at EffectKind30_Shards
// whose +0 is 0, or null; the cursor kSparkCursor left at it (or past the
// last).
extern "C" unsigned char* __cdecl EffectKind73_FindSpark(void) {
    U p = Shards();
    Effect3c_SparkCursor = p;
    for (unsigned i = 0; i < at::kSparkCount; ++i) {
        if (At(p)[0] == 0) return At(p);
        p += at::kSparkStride;
        Effect3c_SparkCursor = p;
    }
    return nullptr;
}

// original 0x485840: +0 = 0 in the 32 sparks, through the cursor.
extern "C" void __cdecl EffectKind73_ClearSparks(void) {
    Effect3c_SparkCursor = Shards();
    for (unsigned n = at::kSparkCount; n != 0; --n) {
        At(Effect3c_SparkCursor)[0] = 0;
        Effect3c_SparkCursor = Effect3c_SparkCursor + at::kSparkStride;
    }
}

// original 0x485870: EffectGte_LoadMapCamera; the 32 sparks stepped with the
// cursor: each live one +4 += 0x2000; with the record's +1 0 grown (+0x14 +=
// 8), else spread by its angle byte (+4 += Math_Cos(+0x17 << 8), +0xC +=
// Math_Sin(..) << 8); its shade +3 -= 2, its life +2 down (0 frees it) and drawn
// (EffectKind73_DrawSpark). al 1 when any was live.
extern "C" unsigned char __cdecl EffectKind73_MoveSparks(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    U c = Shards();
    Effect3c_SparkCursor = c;
    unsigned char any = 0;
    for (unsigned n = at::kSparkCount; n != 0; --n) {
        unsigned char* const r = At(c);
        if (r[0] != 0) {
            SetUL(r + 4, UL(r + 4) + 0x2000u);
            if (S()[1] == 0) {
                unsigned char* const cur = At(Effect3c_SparkCursor);
                SetWord(cur + 0x14, Word(cur + 0x14) + 8u);
            } else {
                unsigned char* cur = At(Effect3c_SparkCursor);
                unsigned char* p = cur + 4;
                U v = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(static_cast<U>(cur[0x17]) << 8)));
                SetUL(p, UL(p) + v);
                cur = At(Effect3c_SparkCursor);
                p = cur + 0xC;
                v = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(static_cast<U>(cur[0x17]) << 8)));
                SetUL(p, UL(p) + (v << 8));
            }
            unsigned char* cur = At(Effect3c_SparkCursor);
            cur[3] = static_cast<unsigned char>(cur[3] + 0xFE);
            cur = At(Effect3c_SparkCursor);
            cur[2] = static_cast<unsigned char>(cur[2] - 1);
            cur = At(Effect3c_SparkCursor);
            if (cur[2] == 0) {
                cur[0] = 0;
                cur = At(Effect3c_SparkCursor);
            }
            SH_CALL(EffectKind73_DrawSpark)(cur);
            c = Effect3c_SparkCursor;
            any = 1;
        }
        c += at::kSparkStride;
        Effect3c_SparkCursor = c;
    }
    return any;
}

// original 0x485960 (a 0x18-byte spark): a draw mode (page (0x3C0, 0) abr 2,
// dtd 0) linked at the spark's x, z (+4, +8; dy 3, 0xC); a POLY_FT4 at the
// cursor, semi-transparent: its centre the point +4 projected, its size +0x14
// (both axes) scaled at its depth in place (EffectGte_ProjectSize, size and
// out one local); the corners x - (w >> 1) and + w, y - (h >> 1) and + h on
// the x87 (fild, fsubr, fiadd); the depth to +0x10..+0x40 by mov; u 0xE0 /
// 0xFF, v 0x30 / 0x4F; the CLUT Gpu_GetClut(0xA0, 0x1E3), the page
// Gpu_GetTPage(0, 2, 0x2C0, 0x100); colour +3 in each channel whose bit of
// +0x16 is set (4 red, 2 green, 1 blue); linked (+4, +8, 3, 0x48).
extern "C" void __cdecl EffectKind73_DrawSpark(unsigned char* r) {
    DrawMode(2, 0x3C0, 0, 0);
    SH_CALL(MapView_LinkPrimAt)(UL(r + 4), UL(r + 8), 3, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const long* const point = reinterpret_cast<const long*>(r + 4);
    float screen[3];
    SH_CALL(EffectGte_ProjectPoint)(point, screen);
    short wh[2] = {static_cast<short>(Word(r + 0x14)), static_cast<short>(Word(r + 0x14))};
    SH_CALL(EffectGte_ProjectSize)(point, wh, wh);
    const std::int32_t w = wh[0], h = wh[1];
    const std::int32_t hw = static_cast<short>(wh[0] >> 1), hh = static_cast<short>(wh[1] >> 1);
    FildSubr(hw, &screen[0], prim + 8);
    FildSubr(hh, &screen[1], prim + 0xC);
    FildSubrAdd(hw, &screen[0], w, prim + 0x18);
    FildSubr(hh, &screen[1], prim + 0x1C);
    FildSubr(hw, &screen[0], prim + 0x28);
    FildSubrAdd(hh, &screen[1], h, prim + 0x2C);
    FildSubrAdd(hw, &screen[0], w, prim + 0x38);
    FildSubrAdd(hh, &screen[1], h, prim + 0x3C);
    prim[0x15] = 0x30;
    prim[0x25] = 0x30;
    prim[0x14] = 0xE0;
    prim[0x24] = 0xFF;
    prim[0x34] = 0xE0;
    prim[0x35] = 0x4F;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x4F;
    U depth;
    std::memcpy(&depth, &screen[2], 4);
    SetUL(prim + 0x40, depth);
    SetUL(prim + 0x30, depth);
    SetUL(prim + 0x20, depth);
    SetUL(prim + 0x10, depth);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->clut, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(prim)->tpage, SH_CALL(Gpu_GetTPage)(0, 2, 0x2C0, 0x100));
    const unsigned char bits = r[0x16];
    const unsigned char red = (bits & 4) ? r[3] : 0;
    const unsigned char green = (bits & 2) ? r[3] : 0;
    const unsigned char blue = (bits & 1) ? r[3] : 0;
    prim[4] = red;
    prim[5] = green;
    prim[6] = blue;
    SH_CALL(MapView_LinkPrimAt)(UL(r + 4), UL(r + 8), 3, 0x48);
}

// original 0x485BA0 (Effect_KindHandlers[0x75], hidden in 0x485960):
// jmp [EffectKind75_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind75_Run(void) {
    Dispatch("EffectKind75_Run", AddressOf(EffectKind75_States), EffectKind75_States_count, 1);
}

// original 0x485BC0 (state 0): the tile's colour +0x5D..+0x5F 0, +9 = 0xF,
// +1 = 1.
extern "C" void __cdecl EffectKind75_Start(void) {
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    S()[9] = 0xF;
    S()[1] = 1;
}

// original 0x485C00 (state 1): +9 down; not 0: each colour byte up 7; at 0,
// +1 = 2. Then the screen tile (E4D's 0x48CA90) and a tail jump to
// EffectKind75_MarkSprites.
extern "C" void __cdecl EffectKind75_Brighten(void) {
    if (!CountDown()) {
        unsigned char* s = S();
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + 7);
        s = S();
        s[0x5E] = static_cast<unsigned char>(s[0x5E] + 7);
        s = S();
        s[0x5F] = static_cast<unsigned char>(s[0x5F] + 7);
    } else {
        S()[1] = 2;
    }
    SH_AT(VoidFn, at::kScreenTile.v)();
    SH_CALL(EffectKind75_MarkSprites)();
}

// original 0x485C50 (state 2): the screen tile and a tail jump to
// EffectKind75_MarkSprites.
extern "C" void __cdecl EffectKind75_Hold(void) {
    SH_AT(VoidFn, at::kScreenTile.v)();
    SH_CALL(EffectKind75_MarkSprites)();
}

// original 0x485C60 (a start the cut does not list: kind 0x75's two states
// jump to it; its own frame and ret): Sprite_Current kept; each of the thirty
// Sprite_Objects records made current in turn, and one in use (+0 bit 0) at
// pose 6 (+6) given +0x29 = 5 and drawn again (Sprite_UpdateScreen) - over the
// tile; Sprite_Current put back.
extern "C" void __cdecl EffectKind75_MarkSprites(void) {
    unsigned char* const saved = Sprite_Current;
    unsigned char* o = rv::Bytes(Sprite_Objects);
    for (unsigned n = at::kSpriteCount; n != 0; --n, o += at::kSpriteStride) {
        Sprite_Current = o;
        if ((o[0] & 1) != 0 && o[6] == 6) {
            o[0x29] = 5;
            SH_CALL(Sprite_UpdateScreen)();
        }
    }
    Sprite_Current = saved;
}

void EffectKind73_Inject() {
    BOF3_INJECT(EffectKind73_Run);
    BOF3_INJECT(EffectKind73_RunA);
    BOF3_INJECT(EffectKind73A_Start);
    BOF3_INJECT(EffectKind73A_Emit);
    BOF3_INJECT(EffectKind73A_Fade);
    BOF3_INJECT(EffectKind73_RunB);
    BOF3_INJECT(EffectKind73B_Burst);
    BOF3_INJECT(EffectKind73B_Fade);
    BOF3_INJECT(EffectKind74_Run);
    BOF3_INJECT(EffectKind74_Start);
    BOF3_INJECT(EffectKind74_Rise);
    BOF3_INJECT(EffectKind74_Fade);
    BOF3_INJECT(EffectKind74_Draw);
    BOF3_INJECT(EffectKind73_SparkInit);
    BOF3_INJECT(EffectKind73_FindSpark);
    BOF3_INJECT(EffectKind73_ClearSparks);
    BOF3_INJECT(EffectKind73_MoveSparks);
    BOF3_INJECT(EffectKind73_DrawSpark);
    BOF3_INJECT(EffectKind75_Run);
    BOF3_INJECT(EffectKind75_Start);
    BOF3_INJECT(EffectKind75_Brighten);
    BOF3_INJECT(EffectKind75_Hold);
    BOF3_INJECT(EffectKind75_MarkSprites);
}
