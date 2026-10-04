// Round thirteen group E4E (docs/effect_4e.md): the 48 functions of
// analysis/round13_cut.tsv's group E4E, 0x48DF90..0x491C96, and kind 0xA3's
// dispatcher 0x4912F0 that no list held, each read with capstone to its last
// instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says (the spawners: area 100's tail and
// chapter 15's runs, area_w2d.cpp and scena_sc15.cpp):
//
//   kind 0x9B  a ring of 16 semi-transparent POLY_G4 round the record's
//              point, linked into the map, that rises (its height +0x14
//              accelerating) to 0x8000000 and stays
//   kind 0x9C  at a fixed place (0x308000, 0x740000): a glow and a trail
//              (catalog part 6's 0x48ED80) that grow and hold; eight panels
//              that flicker in; five beams (LINE_F2, two fans and two quads
//              each); the glow shrinking; the beams fading; released
//   kind 0x9E  a dispatcher here (its seven states are catalog part 6 rows)
//              and four quads those states draw round the record's point:
//              a white plate, a textured plate, a wall and a shaded plate
//   kind 0xA0  a glow at the leader's point that grows, holds and rises;
//              thirty-two shards and sixteen sparks of its own pools
//              (0x6762B0, 0x676430); then a trail flown from the glow to
//              (0x2F8000, 0x278000, 0xF000000) until the chapter's count
//              reaches 0x17; then a fade
//   kinds 0xA1, 0xA2, 0xA3, 0xA7, 0xA8, 0xA9  dispatchers only (their
//              states are catalog part 6 rows); 0xA7's and 0xA9's call
//              through their tables and then draw a glow / a disc while +1
//              is not 0
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Each is a
// faithful replacement; where the original jumps through a state table past
// its end, or its trail's dot loop would run until the coordinate wraps, ours
// aborts with a message (docs/effect_4e.md section 6). The x87 arithmetic is
// done in long double, which the compiler keeps on the x87 at the control
// word's precision, as the originals' fild / fadd / fsub / fstp chains are.
#include "game/effect_4e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4e_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4e::at;
using U = std::uint32_t;
using LD = long double;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SL(const unsigned char* p) { return Long(p); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S16(U v) { return static_cast<std::int16_t>(v); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul r32, r32 then sar: the product's low 32 bits, shifted.
U MulSar(int a, U b, unsigned n) { return Sar(static_cast<U>(a) * b, n); }
// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

// --- x87 as the original has it: `fld dword` through inline assembly, so the
// optimizer cannot narrow an operation of two floats to SSE; every operation
// in long double on the x87 at the game's control word, `fstp dword` out.
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
float AsFloat(LD v) { return static_cast<float>(v); }
// The CRT's _ftol 0x5B9550 on st(0): truncation to 64 bits in a copy of the
// control word, the low dword kept; NaN and out-of-range answer the integer
// indefinite 0x8000000000000000, whose low dword is 0 (effect_2f.cpp Ftol).
U Ftol(LD v) {
    if (!(v > -9223372036854775809.0L && v < 9223372036854775808.0L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }
float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }
const unsigned char* Bytes(const long* p) { return reinterpret_cast<const unsigned char*>(p); }
unsigned char* Bytes(long* p) { return reinterpret_cast<unsigned char*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original
// jumps through the dword after - the next kind's table or data.
void Dispatch(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4e.md section 6)",
                    who, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(static_cast<const unsigned char*>(table) + 4 * state)))();
}

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
// The +6 tick of kind 0xA0's states 4..7: +6 up, true when it was a multiple
// of four before.
bool Tick() {
    unsigned char* const s = S();
    const unsigned char was = s[6];
    s[6] = static_cast<unsigned char>(was + 1);
    return (was & 3) == 0;
}

unsigned char* Spark(unsigned i) { return EffectKindA0_Sparks + i * at::kSparkStride; }
unsigned char* Shard(unsigned i) { return EffectKindA0_Shards + i * at::kShardStride; }

// The trail of catalog part 6's 0x48ED80 that kind 0x9C's states 1..6 draw:
// (Sprite_Current +0x34, +0xC, the byte +0x5D).
void Trail9C() {
    unsigned char* const s = S();
    using Trail = void (__cdecl*)(const long*, const long*, unsigned);
    SH_AT(Trail, at::kTrail9C)(Point(s + 0x34), Point(s + 0xC), s[0x5D]);
}
// Kind 0x9C's glow: EffectKindA0_DrawGlow(+0x34, the word +0x2E, 2).
void Glow9C() {
    unsigned char* const s = S();
    SH_CALL(EffectKindA0_DrawGlow)(Point(s + 0x34), Word(s + 0x2E), 2);
}
// Kind 0xA0's glow: EffectKindA0_DrawGlow(+0x64, the word +0x2E, 6).
void GlowA0() {
    unsigned char* const s = S();
    SH_CALL(EffectKindA0_DrawGlow)(Point(s + 0x64), Word(s + 0x2E), 6);
}
// Kind 0xA0's trail from +0x18 to +0x34.
void TrailA0(unsigned shade) {
    unsigned char* const s = S();
    SH_CALL(EffectKindA0_DrawTrail)(Point(s + 0x18), Point(s + 0x34), shade);
}
// The three swaps of kind 0xA0's states 6 and 7: +0x34..+0x3C with +0x18..+0x20.
void SwapPoints() {
    for (unsigned i = 0; i < 12; i += 4) {
        unsigned char* const s = S();
        SH_CALL(EffectKindA0_SwapLong)(Point(s + 0x34 + i), Point(s + 0x18 + i));
    }
}

}  // namespace

// ===========================================================================
// Kind 0x9B: Effect_KindHandlers[0x9B] (0x6555BC), EffectKind9B_States (three)
// ===========================================================================

// original 0x48DF90 (hidden in E4D's 0x48DC40): jmp [EffectKind9B_States +
// +1 * 4], unbounded.
extern "C" void __cdecl EffectKind9B_Run(void) {
    Dispatch("EffectKind9B_Run", EffectKind9B_States, EffectKind9B_States_count);
}

// original 0x48DFB0 (state 0): +0x3C = AreaMap_Elevation(+0x34, +0x38) as an
// s16 << 16; the height +0x14 and its speed +0x20 = 0; +1 up.
extern "C" void __cdecl EffectKind9B_Start(void) {
    unsigned char* s = S();
    const long elevation = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    s = S();
    SetUL(s + 0x3C, static_cast<U>(S16(static_cast<U>(elevation))) << 16);
    SetUL(S() + 0x14, 0);
    SetUL(S() + 0x20, 0);
    NextState();
}

// original 0x48E000 (state 1): the speed +0x20 up 0x100000, the height +0x14
// up by it; the ring drawn at (+0x34, +0x38, +0x3C) that high; at 0x8000000
// or more (signed) +1 up.
extern "C" void __cdecl EffectKind9B_Rise(void) {
    unsigned char* s = S();
    SetUL(s + 0x20, UL(s + 0x20) + 0x100000u);
    s = S();
    SetUL(s + 0x14, UL(s + 0x14) + UL(s + 0x20));
    s = S();
    alignas(4) unsigned char q[12];
    Copy12(q, s + 0x34);
    SH_CALL(EffectKind9B_DrawRing)(Point(q), Long(s + 0x14));
    if (SL(S() + 0x14) >= 0x8000000) NextState();
}

// original 0x48E070 (state 2): the ring drawn at (+0x34, +0x38, +0x3C),
// 0x8000000 high. Never leaves.
extern "C" void __cdecl EffectKind9B_Hold(void) {
    alignas(4) unsigned char q[12];
    Copy12(q, S() + 0x34);
    SH_CALL(EffectKind9B_DrawRing)(Point(q), 0x8000000);
}

// original 0x48E0A0: EffectGte_LoadMapCamera; sixteen semi-transparent
// POLY_G4 round `point`, radius (cos << 9) sar 4 on x and (sin << 9) sar 4 on
// z (angles 0x100 apart): each quad's bottom edge at the point's height, its
// top `height` above, both projected (EffectGte_ProjectPoint) for the angle
// before and this one. Each quad: a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 1) linked 0xC at the corner (MapView_LinkPrimAt), the quad linked 0x44,
// a second draw mode (dtd 0) linked 0xC. The shade starts at ((Frame_Counter
// & 1) + 8) << 2, the first edge's; each quad's second edge 4 more for quads
// 0..3 and 12..15, 4 less for 4..11, carried on.
extern "C" void __cdecl EffectKind9B_DrawRing(const long* point, long height) {
    SH_CALL(EffectGte_LoadMapCamera)();
    const unsigned char* const o = Bytes(point);
    alignas(4) unsigned char a[12], p0[12], q0[12], p1[12], q1[12];
    // the corner at angle t: a = (x + cos * 32, z + sin * 32, y); p0 its bottom
    // projected, q0 its top
    const auto corner = [&](int t) {
        const int c = SH_CALL(Math_Cos)(t);
        SetUL(a, Sar(static_cast<U>(c) << 9, 4) + UL(o));
        const int sn = SH_CALL(Math_Sin)(t);
        SetUL(a + 4, Sar(static_cast<U>(sn) << 9, 4) + UL(o + 4));
        SetUL(a + 8, UL(o + 8));
        SH_CALL(EffectGte_ProjectPoint)(Point(a), Out(p0));
        SetUL(a + 8, static_cast<U>(height) + UL(o + 8));
        SH_CALL(EffectGte_ProjectPoint)(Point(a), Out(q0));
    };
    corner(0);
    unsigned char shade = static_cast<unsigned char>(((Frame_Counter & 1u) + 8u) << 2);
    U angle = 0;
    for (unsigned i = 0; i < 0x10; ++i) {
        Copy12(p1, p0);
        Copy12(q1, q0);
        angle += 0x100;
        corner(static_cast<int>(angle & 0xFFFFu));
        SetMode(1, 0x3C0, 0, 1);
        SH_CALL(MapView_LinkPrimAt)(UL(a), UL(a + 4), 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, p1);
        Copy12(p + 0x18, q1);
        Copy12(p + 0x28, p0);
        Copy12(p + 0x38, q0);
        p[4] = p[5] = p[6] = shade;
        p[0x14] = p[0x15] = p[0x16] = shade;
        shade = static_cast<unsigned char>(i < 4 || i >= 0xC ? shade + 4 : shade - 4);
        p[0x24] = p[0x25] = p[0x26] = shade;
        p[0x34] = p[0x35] = p[0x36] = shade;
        SH_CALL(MapView_LinkPrimAt)(UL(a), UL(a + 4), 0, 0x44);
        SetMode(1, 0x3C0, 0, 0);
        SH_CALL(MapView_LinkPrimAt)(UL(a), UL(a + 4), 0, 0xC);
    }
}

// ===========================================================================
// Kind 0x9C: Effect_KindHandlers[0x9C] (0x6555C0), EffectKind9C_States (eight)
// ===========================================================================

// original 0x48E320 (hidden in 0x48E0A0): jmp [EffectKind9C_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKind9C_Run(void) {
    Dispatch("EffectKind9C_Run", EffectKind9C_States, EffectKind9C_States_count);
}

// original 0x48E340 (state 0): the point (0x308000, 0x740000), +0x3C its
// AreaMap_Elevation as an s16 << 16; the trail's other end +0xC..+0x14 =
// (0x308000, 0x718000, 0xFEC00000); the glow's size word +0x2E and the trail's
// shade +0x5D = 0; +9 = 8; +1 up.
extern "C" void __cdecl EffectKind9C_Start(void) {
    SetUL(S() + 0x34, 0x308000);
    SetUL(S() + 0x38, 0x740000);
    unsigned char* s = S();
    const long elevation = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    SetUL(S() + 0x3C, static_cast<U>(S16(static_cast<U>(elevation))) << 16);
    SetUL(S() + 0xC, 0x308000);
    SetUL(S() + 0x10, 0x718000);
    SetUL(S() + 0x14, 0xFEC00000u);
    SetWord(S() + 0x2E, 0);
    S()[0x5D] = 0;
    S()[9] = 8;
    NextState();
}

// original 0x48E3D0 (state 1): the size +0x2E up 0x10, the shade +0x5D up 8;
// the trail and the glow drawn; +9 down, at 0: +9 = 0x3C, +1 up,
// Sound_PlayEffect(0x20D).
extern "C" void __cdecl EffectKind9C_Grow(void) {
    unsigned char* s = S();
    SetWord(s + 0x2E, Word(s + 0x2E) + 0x10u);
    s = S();
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + 8);
    Trail9C();
    Glow9C();
    if (!CountDown()) return;
    S()[9] = 0x3C;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x20D);
}

// original 0x48E450 (state 2): the trail and the glow; +9 down, at 0: the
// panels' shade +0x5E = 0, +9 = 4, +1 up.
extern "C" void __cdecl EffectKind9C_Hold(void) {
    Trail9C();
    Glow9C();
    if (!CountDown()) return;
    S()[0x5E] = 0;
    S()[9] = 4;
    NextState();
}

// original 0x48E4B0 (state 3): the shade +0x5E up 0x40, the panels drawn in
// it (0xFF when it wrapped to 0); the trail and the glow; +9 down, at 0: +9 =
// 4, +1 up.
extern "C" void __cdecl EffectKind9C_PanelsIn(void) {
    unsigned char* s = S();
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0x40);
    const unsigned char shade = S()[0x5E];
    SH_CALL(EffectKind9C_DrawPanels)(shade != 0 ? shade : 0xFFu);
    Trail9C();
    Glow9C();
    if (!CountDown()) return;
    S()[9] = 4;
    NextState();
}

// original 0x48E530 (state 4): the shade +0x5E down 0x40 (+0xC0), the panels
// in it; the trail, the glow, the beams (size 0x40); +9 down, at 0: +9 =
// 0x3C, +1 up.
extern "C" void __cdecl EffectKind9C_PanelsOut(void) {
    unsigned char* s = S();
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xC0);
    SH_CALL(EffectKind9C_DrawPanels)(S()[0x5E]);
    Trail9C();
    Glow9C();
    SH_CALL(EffectKind9C_DrawBeams)(0x40);
    if (!CountDown()) return;
    S()[9] = 0x3C;
    NextState();
}

// original 0x48E5B0 (state 5): the trail, the glow, the beams (0x40); +9 down,
// at 0: +9 = 0x3C, +1 up, Sound_PlayEffect(0x20E).
extern "C" void __cdecl EffectKind9C_Beams(void) {
    Trail9C();
    Glow9C();
    SH_CALL(EffectKind9C_DrawBeams)(0x40);
    if (!CountDown()) return;
    S()[9] = 0x3C;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x20E);
}

// original 0x48E620 (state 6): the size +0x2E down 0x10 while above 0 (s16),
// the shade +0x5D down 8 while above 0 (s8); the trail, the glow, the beams
// (0x40); +9 down, at 0: +9 = 0x10, +0x2E = 0x40, +1 up.
extern "C" void __cdecl EffectKind9C_Shrink(void) {
    unsigned char* s = S();
    if (SW(s + 0x2E) > 0) {
        SetWord(s + 0x2E, Word(s + 0x2E) - 0x10u);
        s = S();
    }
    if (static_cast<signed char>(s[0x5D]) > 0) s[0x5D] = static_cast<unsigned char>(s[0x5D] - 8);
    Trail9C();
    Glow9C();
    SH_CALL(EffectKind9C_DrawBeams)(0x40);
    if (!CountDown()) return;
    S()[9] = 0x10;
    SetWord(S() + 0x2E, 0x40);
    NextState();
}

// original 0x48E6B0 (state 7): the word +0x2E down 4, the beams drawn that
// size; +9 down, at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKind9C_End(void) {
    unsigned char* const s = S();
    SetWord(s + 0x2E, Word(s + 0x2E) - 4u);
    SH_CALL(EffectKind9C_DrawBeams)(Word(S() + 0x2E));
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x48E6F0: eight semi-transparent POLY_F4 panels side by side, x
// from 0x2C8000 by 0x10000, z 0x718000, from the ground (0) to 0xFD000000
// high: each a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1) linked 0xC at
// its left edge, the four corners projected, the shade (shade, shade, shade),
// committed 0x38 to slot 1.
extern "C" void __cdecl EffectKind9C_DrawPanels(unsigned shade) {
    const unsigned char k = static_cast<unsigned char>(shade);
    alignas(4) unsigned char low[12], high[12];
    SetUL(low, 0x2C8000);
    SetUL(low + 4, 0x718000);
    SetUL(low + 8, 0);
    SetUL(high, 0x2C8000);
    SetUL(high + 4, 0x718000);
    SetUL(high + 8, 0xFD000000u);
    for (unsigned n = 8; n != 0; --n) {
        SetMode(1, 0x3C0, 0, 1);
        SH_CALL(MapView_LinkPrimAt)(UL(low), UL(low + 4), 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyF4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SH_CALL(EffectGte_ProjectPoint)(Point(low), Out(p + 8));
        SH_CALL(EffectGte_ProjectPoint)(Point(high), Out(p + 0x14));
        SetUL(low, UL(low) + 0x10000u);
        SetUL(high, UL(high) + 0x10000u);
        SH_CALL(EffectGte_ProjectPoint)(Point(low), Out(p + 0x20));
        SH_CALL(EffectGte_ProjectPoint)(Point(high), Out(p + 0x2C));
        p[4] = k;
        p[5] = k;
        p[6] = k;
        SH_CALL(Gfx_CommitPrim)(1, 0x38);
    }
}

// original 0x48E800: five beams (EffectKind9C_DrawBeam, `size` passed on)
// between six heights of the column at z 0x718000: x 0x308000 at 0xFD000000
// to 0xFE000000, then x 0x300000 at 0xFE800000, 0xFF000000, x 0x308000 at
// 0xFF800000 and at 0 - a zigzag down, each beam from the newer point.
extern "C" void __cdecl EffectKind9C_DrawBeams(unsigned size) {
    alignas(4) unsigned char a[12], b[12];
    const auto set = [](unsigned char* p, U x, U y) {
        SetUL(p, x);
        SetUL(p + 4, 0x718000);
        SetUL(p + 8, y);
    };
    set(a, 0x308000, 0xFD000000u);
    set(b, 0x308000, 0xFE000000u);
    SH_CALL(EffectKind9C_DrawBeam)(Point(a), Point(b), size);
    set(a, 0x300000, 0xFE800000u);
    SH_CALL(EffectKind9C_DrawBeam)(Point(a), Point(b), size);
    set(b, 0x300000, 0xFF000000u);
    SH_CALL(EffectKind9C_DrawBeam)(Point(a), Point(b), size);
    set(a, 0x308000, 0xFF800000u);
    SH_CALL(EffectKind9C_DrawBeam)(Point(a), Point(b), size);
    set(b, 0x308000, 0);
    SH_CALL(EffectKind9C_DrawBeam)(Point(a), Point(b), size);
}

// original 0x48E8E0: a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1)
// committed to slot 3; EffectGte_LoadMapCamera; an opaque LINE_F2 (0x40, 0,
// 0) from `from` projected to `to` projected, committed 0x20 to slot 3; its
// angle Math_Ratan2(dy, dx), each the _ftol of the line's screen difference
// as a float; then at each end a fan (EffectKind9C_DrawBeamEnd) of radius
// EffectGte_ProjectSize(end, {size, size}) + (Frame_Counter & 1), at angles
// +0x400 and +0xC00; then the two side quads between them
// (EffectKind9C_DrawBeamSides). The ends' screen points are the _ftol of the
// line's packet floats, read again before each use as the original does.
extern "C" void __cdecl EffectKind9C_DrawBeam(const long* from, const long* to, unsigned size) {
    DrawMode(1, 0x3C0, 0, 1, 3);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SH_CALL(EffectGte_ProjectPoint)(from, Out(p + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(p + 0x14));
    p[4] = 0x40;
    p[5] = 0;
    p[6] = 0;
    SH_CALL(Gfx_CommitPrim)(3, 0x20);
    const U dx = Ftol(F(p + 0x14) - F(p + 8));
    const float fdx = AsFloat(I(dx));
    const U dy = Ftol(F(p + 0x18) - F(p + 0xC));
    const float fdy = AsFloat(I(dy));
    const U angle = static_cast<U>(SH_CALL(Math_Ratan2)(fdy, fdx));
    alignas(4) short sz[2] = {static_cast<short>(size), static_cast<short>(size)};
    alignas(4) short r[2];
    SH_CALL(EffectGte_ProjectSize)(from, sz, r);
    const U r1 = (Frame_Counter & 1u) + (static_cast<U>(static_cast<unsigned short>(r[0])) | (static_cast<U>(static_cast<unsigned short>(r[1])) << 16));
    U y = Ftol(F(p + 0xC));
    U x = Ftol(F(p + 8));
    SH_CALL(EffectKind9C_DrawBeamEnd)(x, y, r1, angle + 0x400u);
    SH_CALL(EffectGte_ProjectSize)(to, sz, r);
    const U r2 = (Frame_Counter & 1u) + (static_cast<U>(static_cast<unsigned short>(r[0])) | (static_cast<U>(static_cast<unsigned short>(r[1])) << 16));
    y = Ftol(F(p + 0x18));
    x = Ftol(F(p + 0x14));
    SH_CALL(EffectKind9C_DrawBeamEnd)(x, y, r2, angle + 0xC00u);
    const U y2 = Ftol(F(p + 0x18));
    const U x2 = Ftol(F(p + 0x14));
    const U y1 = Ftol(F(p + 0xC));
    const U x1 = Ftol(F(p + 8));
    SH_CALL(EffectKind9C_DrawBeamSides)(x1, y1, r1, angle + 0x400u, x2, y2, r2, angle + 0xC00u);
}

// original 0x48EA80: eight semi-transparent POLY_G3 fanned round the screen
// point (x, y) (s16 each), the centre (0x40, 0, 0), the rim black at (x, y) +
// (cos, sin) * r sar 12 (r an s16) for the angles a, a + 0x100, ... (each
// & 0xFFFF), each triangle the next 0x100; committed 0x34 to slot 3.
extern "C" void __cdecl EffectKind9C_DrawBeamEnd(unsigned x, unsigned y, unsigned radius, unsigned angle) {
    const U cx = static_cast<U>(S16(x));
    const U cy = static_cast<U>(S16(y));
    const U r = static_cast<U>(S16(radius));
    alignas(4) unsigned char centre[8];
    StF(centre, I(cx));
    StF(centre + 4, I(cy));
    U a = angle;
    int t = static_cast<int>(a & 0xFFFFu);
    for (unsigned n = 8; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, UL(centre));
        SetUL(p + 0xC, UL(centre + 4));
        int c = SH_CALL(Math_Cos)(t);
        StF(p + 0x18, I(MulSar(c, r, 12) + cx));
        int s = SH_CALL(Math_Sin)(t);
        a += 0x100;
        t = static_cast<int>(a & 0xFFFFu);
        StF(p + 0x1C, I(MulSar(s, r, 12) + cy));
        c = SH_CALL(Math_Cos)(t);
        StF(p + 0x28, I(MulSar(c, r, 12) + cx));
        s = SH_CALL(Math_Sin)(t);
        p[4] = 0x40;
        p[5] = 0;
        p[6] = 0;
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x16] = 0;
        StF(p + 0x2C, I(MulSar(s, r, 12) + cy));
        p[0x24] = 0;
        p[0x25] = 0;
        p[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(3, 0x34);
    }
}

// original 0x48EBB0: two semi-transparent POLY_G4 along a beam between the
// screen points (x1, y1) and (x2, y2) (s16 each): the first (p1, p2, p1 + (cos,
// sin)(a1) * r1, p2 + (cos, sin)(a2 + 0x800) * r2), the second, copied from
// the first 0x44 bytes on (rep movsd: the copy lies where the first's commit
// moved the cursor to), its outer corners at a1 + 0x800 and a2 - one each
// side; the beam (0x40, 0, 0), the outer corners black; each committed 0x44
// to slot 3. The angles are & 0xFFFF, the radii s16, the products sar 12.
extern "C" void __cdecl EffectKind9C_DrawBeamSides(unsigned x1, unsigned y1, unsigned r1, unsigned a1, unsigned x2,
                                                   unsigned y2, unsigned r2, unsigned a2) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const U ax = static_cast<U>(S16(x1)), ay = static_cast<U>(S16(y1));
    const U bx = static_cast<U>(S16(x2)), by = static_cast<U>(S16(y2));
    const U ra = static_cast<U>(S16(r1)), rb = static_cast<U>(S16(r2));
    StF(p + 8, I(ax));
    StF(p + 0xC, I(ay));
    StF(p + 0x18, I(bx));
    StF(p + 0x1C, I(by));
    const U ea = a1 & 0xFFFFu;
    int c = SH_CALL(Math_Cos)(static_cast<int>(ea));
    StF(p + 0x28, I(MulSar(c, ra, 12) + ax));
    int s = SH_CALL(Math_Sin)(static_cast<int>(ea));
    StF(p + 0x2C, I(MulSar(s, ra, 12) + ay));
    const U eb = a2 & 0xFFFFu;
    c = SH_CALL(Math_Cos)(static_cast<int>(eb + 0x800u));
    StF(p + 0x38, I(MulSar(c, rb, 12) + bx));
    s = SH_CALL(Math_Sin)(static_cast<int>(eb + 0x800u));
    p[4] = 0x40;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = 0x40;
    p[0x15] = 0;
    StF(p + 0x3C, I(MulSar(s, rb, 12) + by));
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(3, 0x44);
    unsigned char* const q = p + 0x44;
    std::memmove(q, p, 0x44);
    c = SH_CALL(Math_Cos)(static_cast<int>(ea + 0x800u));
    StF(q + 0x28, I(MulSar(c, ra, 12) + ax));
    s = SH_CALL(Math_Sin)(static_cast<int>(ea + 0x800u));
    StF(q + 0x2C, I(MulSar(s, ra, 12) + ay));
    c = SH_CALL(Math_Cos)(static_cast<int>(eb));
    StF(q + 0x38, I(MulSar(c, rb, 12) + bx));
    s = SH_CALL(Math_Sin)(static_cast<int>(eb));
    StF(q + 0x3C, I(MulSar(s, rb, 12) + by));
    SH_CALL(Gfx_CommitPrim)(3, 0x44);
}

// ===========================================================================
// Kind 0x9E: Effect_KindHandlers[0x9E] (0x6555C8), EffectKind9E_States
// (seven, catalog part 6 rows and Effect_StateRelease), and the four quads
// its states 1..4 draw round the record's point `a` (+0x34) with the
// half-extent `b` (+0xC, or the cells 0x6762A0): +6 bit 0 says which of x / z
// the second and third corners take a's sum on.
// ===========================================================================

// original 0x48F070 (hidden in catalog part 6's 0x48ED80): jmp
// [EffectKind9E_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind9E_Run(void) {
    Dispatch("EffectKind9E_Run", EffectKind9E_States, EffectKind9E_States_count);
}

namespace {

// The corners the four quads share (Sprite_Current +6 read afresh for each):
// "plus-minus" is (a.x + b.x, a.z - b.z) when +6 bit 0 is set, else (a.x -
// b.x, a.z + b.z); "minus-plus" the other.
void PlusMinus(unsigned char* v, const unsigned char* a, const unsigned char* b, bool set) {
    if (set) {
        SetUL(v, UL(a) + UL(b));
        SetUL(v + 4, UL(a + 4) - UL(b + 4));
    } else {
        SetUL(v, UL(a) - UL(b));
        SetUL(v + 4, UL(b + 4) + UL(a + 4));
    }
}
bool Bit0() { return (S()[6] & 1) != 0; }
void Sum(unsigned char* v, const unsigned char* a, const unsigned char* b, U lift) {
    SetUL(v, UL(a) + UL(b));
    SetUL(v + 4, UL(b + 4) + UL(a + 4));
    SetUL(v + 8, UL(b + 8) + UL(a + 8) + lift);
}
void Difference(unsigned char* v, const unsigned char* a, const unsigned char* b) {
    SetUL(v, UL(a) - UL(b));
    SetUL(v + 4, UL(a + 4) - UL(b + 4));
    SetUL(v + 8, UL(a + 8) - UL(b + 8));
}
// The corners of a flat quad into p + 8, + 0x14, + 0x20, + 0x2C: a + b; the
// one of +6 bit 0 at a.y + b.y; the other at a.y - b.y; a - b.
void FlatCorners(unsigned char* p, const unsigned char* a, const unsigned char* b) {
    alignas(4) unsigned char v[12];
    Sum(v, a, b, 0);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 8));
    PlusMinus(v, a, b, Bit0());
    SetUL(v + 8, UL(b + 8) + UL(a + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x14));
    PlusMinus(v, a, b, !Bit0());
    SetUL(v + 8, UL(a + 8) - UL(b + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x20));
    Difference(v, a, b);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x2C));
}
// The shade of the two shaded quads: ((+6 & 0xFE) << 6, (~+6 & 0xFE) << 6, 0).
void ShadeBy6(unsigned char* p) {
    p[4] = static_cast<unsigned char>((S()[6] & 0xFE) << 6);
    p[6] = 0;
    p[5] = static_cast<unsigned char>((~S()[6] & 0xFE) << 6);
}

}  // namespace

// original 0x48F5D0: an opaque white POLY_F4 (0xFF, 0xFF, 0xFF), its corners
// FlatCorners(a, b) projected; linked 0x38 at (a.x, a.z) (MapView_LinkPrimAt).
extern "C" void __cdecl EffectKind9E_DrawPlate(const long* a, const long* b) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    FlatCorners(p, Bytes(a), Bytes(b));
    p[4] = 0xFF;
    p[5] = 0xFF;
    p[6] = 0xFF;
    SH_CALL(MapView_LinkPrimAt)(UL(Bytes(a)), UL(Bytes(a) + 4), 0, 0x38);
}

// original 0x48F720: a semi-transparent textured POLY_FT4 (0x20, 0x20, 0x20):
// corners (the one of +6 bit 0 at a.y + b.y, a + b, a - b, the other at a.y -
// b.y) projected into +8, +0x18, +0x28, +0x38; the texture from
// EffectKind9E_Pages' record 0 or (+6 bit 2) record 1 - (s16 x, s16 y, u, v):
// u / v and u + 0x18 / v + 0x10 at the corners, the page Gpu_GetTPage(1, 1, x,
// y), the CLUT Gpu_GetClut(0xA0, 0x1F0); linked 0x48 at (a.x, a.z).
extern "C" void __cdecl EffectKind9E_DrawTexPlate(const long* a, const long* b) {
    const unsigned char* const page = EffectKind9E_Pages + ((S()[6] & 4) != 0 ? 6 : 0);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned char* const pa = Bytes(a);
    const unsigned char* const pb = Bytes(b);
    alignas(4) unsigned char v[12];
    PlusMinus(v, pa, pb, Bit0());
    SetUL(v + 8, UL(pa + 8) + UL(pb + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 8));
    Sum(v, pa, pb, 0);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x18));
    Difference(v, pa, pb);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x28));
    PlusMinus(v, pa, pb, !Bit0());
    SetUL(v + 8, UL(pa + 8) - UL(pb + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x38));
    p[0x14] = page[4];
    p[0x15] = page[5];
    p[0x24] = static_cast<unsigned char>(page[4] + 0x18);
    p[0x25] = page[5];
    p[0x34] = page[4];
    p[0x35] = static_cast<unsigned char>(page[5] + 0x10);
    p[0x44] = static_cast<unsigned char>(page[4] + 0x18);
    p[0x45] = static_cast<unsigned char>(page[5] + 0x10);
    const U tp = SH_CALL(Gpu_GetTPage)(1, 1, SW(page), SW(page + 2));
    SetWord(p + 0x26, tp);
    const U clut = SH_CALL(Gpu_GetClut)(0xA0, 0x1F0);
    SetWord(p + 0x16, clut);
    p[4] = 0x20;
    p[5] = 0x20;
    p[6] = 0x20;
    SH_CALL(MapView_LinkPrimAt)(UL(pa), UL(pa + 4), 0, 0x48);
}

// original 0x48F8F0: an opaque POLY_F4 wall: its bottom corners a + b and the
// one of +6 bit 0 (at a.y + b.y), its top the same two 0x100000 higher; the
// shade ShadeBy6; linked 0x38 at (a.x, a.z).
extern "C" void __cdecl EffectKind9E_DrawWall(const long* a, const long* b) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    const unsigned char* const pa = Bytes(a);
    const unsigned char* const pb = Bytes(b);
    alignas(4) unsigned char v[12];
    Sum(v, pa, pb, 0);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 8));
    PlusMinus(v, pa, pb, Bit0());
    SetUL(v + 8, UL(pb + 8) + UL(pa + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x14));
    Sum(v, pa, pb, 0x100000);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x20));
    PlusMinus(v, pa, pb, Bit0());
    SetUL(v + 8, UL(pb + 8) + UL(pa + 8) + 0x100000u);
    SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x2C));
    ShadeBy6(p);
    SH_CALL(MapView_LinkPrimAt)(UL(pa), UL(pa + 4), 0, 0x38);
}

// original 0x48FA80: a draw mode (Gpu_GetTPage(0, 0, 0x3C0, 0), dtd 1) linked
// 0xC at (a.x, a.z); a semi-transparent POLY_F4, its corners FlatCorners(a,
// b), the shade ShadeBy6; linked 0x38 at (a.x, a.z).
extern "C" void __cdecl EffectKind9E_DrawShadePlate(const long* a, const long* b) {
    SetMode(0, 0x3C0, 0, 1);
    const unsigned char* const pa = Bytes(a);
    SH_CALL(MapView_LinkPrimAt)(UL(pa), UL(pa + 4), 0, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    FlatCorners(p, pa, Bytes(b));
    ShadeBy6(p);
    SH_CALL(MapView_LinkPrimAt)(UL(pa), UL(pa + 4), 0, 0x38);
}

// ===========================================================================
// Kind 0xA0: Effect_KindHandlers[0xA0] (0x6555D0), EffectKindA0_States (nine)
// ===========================================================================

// original 0x48FC20 (hidden in 0x48FA80): jmp [EffectKindA0_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKindA0_Run(void) {
    Dispatch("EffectKindA0_Run", EffectKindA0_States, EffectKindA0_States_count);
}

// original 0x48FC40 (state 0): the glow's point +0x64..+0x6C the leader's
// (ObjTrio +0x34..), its height 0x800000 more; the size +0x2E = 0; +9 = 8; +1
// up; Sound_PlayEffect(0x20B).
extern "C" void __cdecl EffectKindA0_Start(void) {
    SetUL(S() + 0x64, UL(At(at::kLeaderPoint)));
    SetUL(S() + 0x68, UL(At(at::kLeaderPoint + 4)));
    SetUL(S() + 0x6C, UL(At(at::kLeaderPoint + 8)) + 0x800000u);
    SetWord(S() + 0x2E, 0);
    S()[9] = 8;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x20B);
}

// original 0x48FCA0 (state 1): the size +0x2E up 0x20, the glow; +9 down, at
// 0: +9 = 0x40, +1 up.
extern "C" void __cdecl EffectKindA0_Grow(void) {
    unsigned char* const s = S();
    SetWord(s + 0x2E, Word(s + 0x2E) + 0x20u);
    GlowA0();
    if (!CountDown()) return;
    S()[9] = 0x40;
    NextState();
}

// original 0x48FCF0 (state 2): the glow; +9 down, at 0: +9 = 0x20, +1 up.
extern "C" void __cdecl EffectKindA0_Hold(void) {
    GlowA0();
    if (!CountDown()) return;
    S()[9] = 0x20;
    NextState();
}

// original 0x48FD30 (state 3): the glow's height +0x6C up 0xC0000, its size
// down 6, the glow; +9 down, at 0: +9 = 0x20, +6 = 0, +1 up, the sparks
// cleared, the point +0x34..+0x3C = +0x64..+0x6C, the first sixteen shards
// set up there (EffectKindA0_InitShard), the second sixteen's speed and shade
// words (+0x28, +0x2A) 0.
extern "C" void __cdecl EffectKindA0_Rise(void) {
    unsigned char* s = S();
    SetUL(s + 0x6C, UL(s + 0x6C) + 0xC0000u);
    s = S();
    SetWord(s + 0x2E, Word(s + 0x2E) - 6u);
    GlowA0();
    if (!CountDown()) return;
    S()[9] = 0x20;
    S()[6] = 0;
    NextState();
    SH_CALL(EffectKindA0_ClearSparks)();
    SetUL(S() + 0x34, UL(S() + 0x64));
    SetUL(S() + 0x38, UL(S() + 0x68));
    SetUL(S() + 0x3C, UL(S() + 0x6C));
    for (unsigned i = 0; i < at::kShardRun; ++i) SH_CALL(EffectKindA0_InitShard)(Shard(i));
    for (unsigned i = at::kShardRun; i < at::kShardCount; ++i) {
        SetWord(Shard(i) + 0x28, 0);
        SetWord(Shard(i) + 0x2A, 0);
    }
}

// original 0x48FDF0 (state 4): the +6 tick (a spark spawned every fourth),
// the shards drawn, the sparks stepped; once the chapter's count 0x903848 is
// 0x14: the trail's head +0x18..+0x20 = the glow's point, its speed +0xC..
// +0x14 = 0, its pull +0x64..+0x6C = ((0x2F8000, 0x278000, 0xF000000) - the
// point) sar 7; +1 up.
extern "C" void __cdecl EffectKindA0_Sparkle(void) {
    if (Tick()) SH_CALL(EffectKindA0_SpawnSpark)();
    SH_CALL(EffectKindA0_DrawShards)();
    SH_CALL(EffectKindA0_StepSparks)();
    if (At(at::kCounter)[0] != 0x14) return;
    SetUL(S() + 0x18, UL(S() + 0x64));
    SetUL(S() + 0x1C, UL(S() + 0x68));
    SetUL(S() + 0x20, UL(S() + 0x6C));
    SetUL(S() + 0xC, 0);
    SetUL(S() + 0x10, 0);
    SetUL(S() + 0x14, 0);
    unsigned char* s = S();
    SetUL(s + 0x64, Sar(0x2F8000u - UL(s + 0x64), 7));
    s = S();
    SetUL(s + 0x68, Sar(0x278000u - UL(s + 0x68), 7));
    s = S();
    SetUL(s + 0x6C, Sar(0xF000000u - UL(s + 0x6C), 7));
    NextState();
}

// original 0x48FEB0 (state 5): the tick, the shards, the sparks; +9 = 0x20,
// +1 up, Sound_PlayEffect(0x209).
extern "C" void __cdecl EffectKindA0_Launch(void) {
    if (Tick()) SH_CALL(EffectKindA0_SpawnSpark)();
    SH_CALL(EffectKindA0_DrawShards)();
    SH_CALL(EffectKindA0_StepSparks)();
    S()[9] = 0x20;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x209);
}

// original 0x48FF00 (state 6): the tick, the sparks, the shards, the trail
// (shade 0x40); the speed +0xC.. up by the pull +0x64.., the head +0x18.. up
// by the speed, held at (0x2F8000, 0x278000, 0xF000000) once its x is below
// 0x2F8000; the shade +3 down 4; +9 down, at 0: the second sixteen shards
// set up at the head (the points swapped round EffectKindA0_InitShard), +9 =
// 0x80, +1 up.
extern "C" void __cdecl EffectKindA0_Fly(void) {
    if (Tick()) SH_CALL(EffectKindA0_SpawnSpark)();
    SH_CALL(EffectKindA0_StepSparks)();
    SH_CALL(EffectKindA0_DrawShards)();
    TrailA0(0x40);
    unsigned char* s = S();
    SetUL(s + 0xC, UL(s + 0xC) + UL(s + 0x64));
    s = S();
    SetUL(s + 0x10, UL(s + 0x10) + UL(s + 0x68));
    s = S();
    SetUL(s + 0x14, UL(s + 0x14) + UL(s + 0x6C));
    s = S();
    SetUL(s + 0x18, UL(s + 0x18) + UL(s + 0xC));
    s = S();
    SetUL(s + 0x1C, UL(s + 0x1C) + UL(s + 0x10));
    s = S();
    SetUL(s + 0x20, UL(s + 0x20) + UL(s + 0x14));
    s = S();
    if (SL(s + 0x18) < 0x2F8000) {
        SetUL(s + 0x18, 0x2F8000);
        SetUL(S() + 0x1C, 0x278000);
        SetUL(S() + 0x20, 0xF000000);
        s = S();
    }
    s[3] = static_cast<unsigned char>(s[3] + 0xFC);
    if (!CountDown()) return;
    SwapPoints();
    for (unsigned i = at::kShardRun; i < at::kShardCount; ++i) SH_CALL(EffectKindA0_InitShard)(Shard(i));
    SwapPoints();
    S()[9] = 0x80;
    NextState();
}

// original 0x490090 (state 7): the tick - every fourth a spark at the point
// and one at the head (the points swapped round the second spawn); the
// sparks, the shards, the trail (0x40); once the chapter's count is 0x17:
// the shade +3 = 0x80, +9 = 8, +1 up, Sound_PlayEffect(0x20A).
extern "C" void __cdecl EffectKindA0_FlyWait(void) {
    if (Tick()) {
        SH_CALL(EffectKindA0_SpawnSpark)();
        SwapPoints();
        SH_CALL(EffectKindA0_SpawnSpark)();
        SwapPoints();
    }
    SH_CALL(EffectKindA0_StepSparks)();
    SH_CALL(EffectKindA0_DrawShards)();
    TrailA0(0x40);
    if (At(at::kCounter)[0] != 0x17) return;
    S()[3] = 0x80;
    S()[9] = 8;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x20A);
}

// original 0x490180 (state 8): the size +0x2E down 0x10, the shade +3 down 8;
// the sparks; the trail in that shade; +9 down, at 0 Effect_Release (a tail
// jmp).
extern "C" void __cdecl EffectKindA0_Fade(void) {
    unsigned char* s = S();
    SetWord(s + 0x2E, Word(s + 0x2E) - 0x10u);
    s = S();
    s[3] = static_cast<unsigned char>(s[3] + 0xF8);
    SH_CALL(EffectKindA0_StepSparks)();
    TrailA0(S()[3]);
    if (CountDown()) SH_CALL(Effect_Release)();
}

// original 0x4901D0: a draw mode (Gpu_GetTPage(0, 1, 0x2C0, 0x100), dtd 1)
// committed to slot 2; EffectGte_LoadMapCamera; o = `point` projected; r =
// EffectGte_ProjectSize(point, {size, size})'s first word + (Frame_Counter &
// 1), a word; a fan of 32 semi-transparent POLY_G3 round o - the centre in
// the colour bits' shades ((c & 0xFC) << 5, (c & 0xFE) << 6, c << 7, each a
// byte), the rim black at o + (cos, sin) * r sar 12 from the angle 0x80 by
// 0x80 (the first rim point o.x + r) - every vertex at o's depth; each
// committed 0x34 to slot 2. EffectKind64_DrawGlow's form, without its
// unwritten rim depth (DIV-0068's site is that function, not this one).
extern "C" void __cdecl EffectKindA0_DrawGlow(const long* point, unsigned size, unsigned colour) {
    DrawMode(1, 0x2C0, 0x100, 1, 2);
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
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, UL(o));
        SetUL(p + 0xC, UL(o + 4));
        SetUL(p + 0x18, UL(rim));
        SetUL(p + 0x1C, UL(rim + 4));
        const U dx = MulSar(SH_CALL(Math_Cos)(t), static_cast<U>(radius), 12);
        StF(rim, I(dx) + F(o));
        const U dy = MulSar(SH_CALL(Math_Sin)(t), static_cast<U>(radius), 12);
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
        SH_CALL(Gfx_CommitPrim)(2, 0x34);
    }
}

// The most dots EffectKindA0_DrawTrail draws before ours gives up: ordinary
// play draws about nine (the head is held at x 0x2F8000, the pull is a
// 128th of the way); the original runs on until the coordinate wraps when
// the pull's x is not negative (docs/effect_4e.md section 7).
constexpr unsigned kMostDots = 0x400;

// original 0x490390: two semi-transparent POLY_G4 along the trail from `from`
// projected (a) to `to` projected (b) - the first (a + (h, -h), a, b + (h,
// -h), b) with a and b shaded (shade, shade, 0), the second (a, a + (-h, h),
// b, b + (-h, h)) with the offset corners black and a, b shaded, h the float at
// 0x5C41B8 - each committed 0x44 to slot 2 (EffectKind64_DrawTrail's two
// quads); then TILE_1 dots (shade, shade, 0) from q = `to` + (Frame_Counter &
// 0xF) * the pull (+0x64..+0x6C, Sprite_Current read afresh), each step the
// pull << 4, while q.x is above from's (signed) - no count, unlike kind
// 0x64's eight.
extern "C" void __cdecl EffectKindA0_DrawTrail(const long* from, const long* to, unsigned shade) {
    const unsigned char* const a = Bytes(from);
    const unsigned char* const b = Bytes(to);
    const LD half = F(At(at::kHalf));
    const unsigned char k = static_cast<unsigned char>(shade);
    alignas(4) unsigned char o[12];
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    StF(p + 8, F(o) + half);
    StF(p + 0xC, F(o + 4) - half);
    SetUL(p + 0x18, UL(o));
    SetUL(p + 0x1C, UL(o + 4));
    SetUL(p + 0x20, UL(o + 8));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    StF(p + 0x28, F(o) + half);
    StF(p + 0x2C, F(o + 4) - half);
    SetUL(p + 0x38, UL(o));
    SetUL(p + 0x3C, UL(o + 4));
    SetUL(p + 0x40, UL(o + 8));
    SetUL(p + 0x30, UL(o + 8));
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = k;
    p[0x15] = k;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = k;
    p[0x35] = k;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    SetUL(p + 8, UL(o));
    SetUL(p + 0xC, UL(o + 4));
    StF(p + 0x18, F(o) - half);
    StF(p + 0x1C, F(o + 4) + half);
    SetUL(p + 0x20, UL(o + 8));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    SetUL(p + 0x28, UL(o));
    SetUL(p + 0x2C, UL(o + 4));
    StF(p + 0x38, F(o) - half);
    StF(p + 0x3C, F(o + 4) + half);
    SetUL(p + 0x40, UL(o + 8));
    SetUL(p + 0x30, UL(o + 8));
    p[4] = k;
    p[5] = k;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = k;
    p[0x25] = k;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    const unsigned char* s = S();
    const U f = Frame_Counter & 0xFu;
    alignas(4) unsigned char q[12];
    SetUL(q, f * UL(s + 0x64) + UL(b));
    SetUL(q + 4, f * UL(s + 0x68) + UL(b + 4));
    SetUL(q + 8, f * UL(s + 0x6C) + UL(b + 8));
    for (unsigned dots = 0; SL(q) > SL(a); ++dots) {
        if (dots == kMostDots)
            bof3::Fatal("EffectKindA0_DrawTrail: %u dots and x 0x%X still above 0x%X (pull x 0x%X) - the original "
                        "draws on until the coordinate wraps (docs/effect_4e.md section 7)",
                        dots, (unsigned)UL(q), (unsigned)UL(a), (unsigned)UL(S() + 0x64));
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o));
        p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, o);
        p[4] = k;
        p[5] = k;
        p[6] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x14);
        s = S();
        SetUL(q, UL(q) + (UL(s + 0x64) << 4));
        SetUL(q + 4, UL(q + 4) + (UL(s + 0x68) << 4));
        SetUL(q + 8, UL(q + 8) + (UL(s + 0x6C) << 4));
    }
}

// original 0x490630: the sixteen sparks of EffectKindA0_Sparks out of use.
extern "C" void __cdecl EffectKindA0_ClearSparks(void) {
    for (unsigned i = 0; i < at::kSparkCount; ++i) Spark(i)[0] = 0;
}

// original 0x490650: the first spark out of use (none: nothing) put in use at
// Sprite_Current's point +0x34..: +1 = 0, life +2 = 0x10, size +0x14 = 0x100,
// shades +0x16 = 0x60, +0x17 = 0.
extern "C" void __cdecl EffectKindA0_SpawnSpark(void) {
    for (unsigned i = 0; i < at::kSparkCount; ++i) {
        unsigned char* const k = Spark(i);
        if (k[0] != 0) continue;
        const unsigned char* const s = S();
        k[0] = 1;
        k[1] = 0;
        k[2] = 0x10;
        SetUL(k + 4, UL(s + 0x34));
        SetUL(k + 8, UL(s + 0x38));
        SetUL(k + 0xC, UL(s + 0x3C));
        SetWord(k + 0x14, 0x100);
        k[0x16] = 0x60;
        k[0x17] = 0;
        return;
    }
}

// original 0x4906A0: EffectGte_LoadMapCamera; each spark in use: its size
// +0x14 down 0x10, its life +2 down (at 0 out of use), drawn
// (EffectKind64_DrawSpark, group E3A's). Answers 1 when a spark was in use,
// else 0 (EffectKind64_StepSparks' form over the sixteen).
extern "C" unsigned char __cdecl EffectKindA0_StepSparks(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kSparkCount; ++i) {
        unsigned char* const k = Spark(i);
        if (k[0] == 0) continue;
        const unsigned char life = k[2];
        SetWord(k + 0x14, Word(k + 0x14) + 0xFFF0u);
        any = 1;
        k[2] = static_cast<unsigned char>(life - 1);
        if (k[2] == 0) k[0] = 0;
        SH_CALL(EffectKind64_DrawSpark)(k);
    }
    return any;
}

// original 0x4906F0: the shard's point +0..+8 the record's (+0x34..); three
// Rand angles x = & 0xFFF, y = (& 0x7FF) - 0x400, z = & 0xFFF; its two edge
// vectors +0x10 = (cos 0x10, sin 0x10, 0) and +0x18 = (cos -0x10, sin -0x10,
// 0) as words, turned by the matrix EffectGte_SetDiagonalOne, Gte_RotMatrixX
// (x), Gte_RotMatrixY (-y), Gte_RotMatrixZ (z) make (0x5A7C70, in place); its
// shade +0x2A = 0x40, its speed +0x28 = (Rand & 2) + 3, its angles
// +0x20..+0x25 = 0 (EffectKind64_InitShard's form; that one's shade is 0x20).
extern "C" void __cdecl EffectKindA0_InitShard(unsigned char* shard) {
    SetUL(shard, UL(S() + 0x34));
    SetUL(shard + 4, UL(S() + 0x38));
    SetUL(shard + 8, UL(S() + 0x3C));
    const U x = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U y = (static_cast<U>(SH_CALL(Rand)()) & 0x7FFu) - 0x400u;
    const U z = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    SetWord(shard + 0x10, static_cast<U>(SH_CALL(Math_Cos)(0x10)));
    SetWord(shard + 0x12, static_cast<U>(SH_CALL(Math_Sin)(0x10)));
    SetWord(shard + 0x14, 0);
    SetWord(shard + 0x18, static_cast<U>(SH_CALL(Math_Cos)(-0x10)));
    SetWord(shard + 0x1A, static_cast<U>(SH_CALL(Math_Sin)(-0x10)));
    SetWord(shard + 0x1C, 0);
    alignas(4) short m[16];
    SH_CALL(EffectGte_SetDiagonalOne)(m);
    SH_CALL(Gte_RotMatrixX)(S16(x), m);
    SH_CALL(Gte_RotMatrixY)(-S16(y), m);
    SH_CALL(Gte_RotMatrixZ)(S16(z), m);
    using Turn = void (__cdecl*)(const short*, short*, short*);
    SH_AT(Turn, at::kMatrixVector)(m, reinterpret_cast<short*>(shard + 0x10), reinterpret_cast<short*>(shard + 0x10));
    SH_AT(Turn, at::kMatrixVector)(m, reinterpret_cast<short*>(shard + 0x18), reinterpret_cast<short*>(shard + 0x18));
    const U speed = (static_cast<U>(SH_CALL(Rand)()) & 2u) + 3u;
    SetWord(shard + 0x2A, 0x40);
    SetWord(shard + 0x28, speed);
    SetWord(shard + 0x20, 0);
    SetWord(shard + 0x22, 0);
    SetWord(shard + 0x24, 0);
}

// original 0x490810: a draw mode (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1)
// committed to slot 2; EffectGte_LoadMapCamera; each of the 32 shards of
// EffectKindA0_Shards drawn (EffectKindA0_DrawShard) and its angle +0x24 up
// 0x10.
extern "C" void __cdecl EffectKindA0_DrawShards(void) {
    DrawMode(1, 0x380, 0x100, 1, 2);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kShardCount; ++i) {
        unsigned char* const shard = Shard(i);
        SH_CALL(EffectKindA0_DrawShard)(shard);
        SetWord(shard + 0x24, Word(shard + 0x24) + 0x10u);
    }
}

// original 0x490880: a semi-transparent POLY_G3: the shard's point projected,
// and the point plus each edge vector (+0x10, +0x18: three s16) turned by
// the angle +0x24 in its first two components - ((cos e0 - sin e1) sar 8,
// (sin e0 + cos e1) sar 8) - each times the speed +0x28, the third << 12
// times the speed; the shade +0x2A held to 0..0xFF on the first two
// channels of the first vertex, the rest black; committed 0x34 to slot 2.
extern "C" void __cdecl EffectKindA0_DrawShard(unsigned char* shard) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG3)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(Point(shard), Out(p + 8));
    alignas(4) unsigned char v[12];
    for (unsigned e = 0; e < 2; ++e) {
        const unsigned char* const edge = shard + 0x10 + 8 * e;
        int c = SH_CALL(Math_Cos)(SW(shard + 0x24));
        U x = static_cast<U>(c) * static_cast<U>(SW(edge));
        int s = SH_CALL(Math_Sin)(SW(shard + 0x24));
        x = Sar(x - static_cast<U>(s) * static_cast<U>(SW(edge + 2)), 8) * static_cast<U>(SW(shard + 0x28));
        s = SH_CALL(Math_Sin)(SW(shard + 0x24));
        U y = static_cast<U>(s) * static_cast<U>(SW(edge));
        c = SH_CALL(Math_Cos)(SW(shard + 0x24));
        y += static_cast<U>(c) * static_cast<U>(SW(edge + 2));
        const U speed = static_cast<U>(SW(shard + 0x28));
        const U z = (static_cast<U>(SW(edge + 4)) << 12) * speed + UL(shard + 8);
        y = Sar(y, 8) * speed + UL(shard + 4);
        SetUL(v, x + UL(shard));
        SetUL(v + 4, y);
        SetUL(v + 8, z);
        SH_CALL(EffectGte_ProjectPoint)(Point(v), Out(p + 0x18 + 0x10 * e));
    }
    const std::int32_t w = SW(shard + 0x2A);
    const unsigned char k = w < 0 ? 0 : w > 0xFF ? 0xFF : shard[0x2A];
    p[4] = k;
    p[5] = k;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x34);
}

// original 0x490A30: the dwords at `a` and `b` swapped by three xors (a and b
// the same: both 0, as the xors make it).
extern "C" void __cdecl EffectKindA0_SwapLong(long* a, long* b) {
    unsigned char* const pa = Bytes(a);
    unsigned char* const pb = Bytes(b);
    SetUL(pa, UL(pa) ^ UL(pb));
    SetUL(pb, UL(pb) ^ UL(pa));
    SetUL(pa, UL(pa) ^ UL(pb));
}

// ===========================================================================
// Kinds 0xA1, 0xA2, 0xA3, 0xA7, 0xA8, 0xA9: dispatchers whose states are
// catalog part 6 rows (in no group of round thirteen)
// ===========================================================================

// original 0x490A60 (hidden in 0x490A30): jmp [EffectKindA1_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKindA1_Run(void) {
    Dispatch("EffectKindA1_Run", EffectKindA1_States, EffectKindA1_States_count);
}

// original 0x4910D0 (hidden in 0x490A30): jmp [EffectKindA2_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKindA2_Run(void) {
    Dispatch("EffectKindA2_Run", EffectKindA2_States, EffectKindA2_States_count);
}

// original 0x4912F0 (in no list; between catalog part 6 rows): jmp
// [EffectKindA3_States + +1 * 4], unbounded. Its first five states are kind
// 0xA1's.
extern "C" void __cdecl EffectKindA3_Run(void) {
    Dispatch("EffectKindA3_Run", EffectKindA3_States, EffectKindA3_States_count);
}

// original 0x491AA0 (hidden in catalog part 6's 0x4918B0): call
// [EffectKindA7_States + +1 * 4], unbounded; then, Sprite_Current read again,
// while +1 is not 0 the glow 0x491E30(+0x34, the word +0xC, 7).
extern "C" void __cdecl EffectKindA7_Run(void) {
    Dispatch("EffectKindA7_Run", EffectKindA7_States, EffectKindA7_States_count);
    unsigned char* const s = S();
    if (s[1] == 0) return;
    using Glow = void (__cdecl*)(const long*, unsigned, unsigned);
    SH_AT(Glow, at::kGlowA7)(Point(s + 0x34), Word(s + 0xC), 7);
}

// original 0x491BA0 (hidden in 0x4918B0): jmp [EffectKindA8_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKindA8_Run(void) {
    Dispatch("EffectKindA8_Run", EffectKindA8_States, EffectKindA8_States_count);
}

// original 0x491C60 (hidden in 0x4918B0): call [EffectKindA9_States + +1 *
// 4], unbounded; then, while +1 is not 0, the disc 0x492260(the word +0x2E,
// the word +0x30, 0x80).
extern "C" void __cdecl EffectKindA9_Run(void) {
    Dispatch("EffectKindA9_Run", EffectKindA9_States, EffectKindA9_States_count);
    unsigned char* const s = S();
    if (s[1] == 0) return;
    using Disc = void (__cdecl*)(unsigned, unsigned, unsigned);
    SH_AT(Disc, at::kDiscA9)(Word(s + 0x2E), Word(s + 0x30), 0x80);
}

void Effect4E_Inject() {
    if (bof3::WantsShadow("effect_4e")) effect_4e::SelfTest();
    BOF3_INJECT(EffectKind9B_Run);
    BOF3_INJECT(EffectKind9B_Start);
    BOF3_INJECT(EffectKind9B_Rise);
    BOF3_INJECT(EffectKind9B_Hold);
    BOF3_INJECT(EffectKind9B_DrawRing);
    BOF3_INJECT(EffectKind9C_Run);
    BOF3_INJECT(EffectKind9C_Start);
    BOF3_INJECT(EffectKind9C_Grow);
    BOF3_INJECT(EffectKind9C_Hold);
    BOF3_INJECT(EffectKind9C_PanelsIn);
    BOF3_INJECT(EffectKind9C_PanelsOut);
    BOF3_INJECT(EffectKind9C_Beams);
    BOF3_INJECT(EffectKind9C_Shrink);
    BOF3_INJECT(EffectKind9C_End);
    BOF3_INJECT(EffectKind9C_DrawPanels);
    BOF3_INJECT(EffectKind9C_DrawBeams);
    BOF3_INJECT(EffectKind9C_DrawBeam);
    BOF3_INJECT(EffectKind9C_DrawBeamEnd);
    BOF3_INJECT(EffectKind9C_DrawBeamSides);
    BOF3_INJECT(EffectKind9E_Run);
    BOF3_INJECT(EffectKind9E_DrawPlate);
    BOF3_INJECT(EffectKind9E_DrawTexPlate);
    BOF3_INJECT(EffectKind9E_DrawWall);
    BOF3_INJECT(EffectKind9E_DrawShadePlate);
    BOF3_INJECT(EffectKindA0_Run);
    BOF3_INJECT(EffectKindA0_Start);
    BOF3_INJECT(EffectKindA0_Grow);
    BOF3_INJECT(EffectKindA0_Hold);
    BOF3_INJECT(EffectKindA0_Rise);
    BOF3_INJECT(EffectKindA0_Sparkle);
    BOF3_INJECT(EffectKindA0_Launch);
    BOF3_INJECT(EffectKindA0_Fly);
    BOF3_INJECT(EffectKindA0_FlyWait);
    BOF3_INJECT(EffectKindA0_Fade);
    BOF3_INJECT(EffectKindA0_DrawGlow);
    BOF3_INJECT(EffectKindA0_DrawTrail);
    BOF3_INJECT(EffectKindA0_ClearSparks);
    BOF3_INJECT(EffectKindA0_SpawnSpark);
    BOF3_INJECT(EffectKindA0_StepSparks);
    BOF3_INJECT(EffectKindA0_InitShard);
    BOF3_INJECT(EffectKindA0_DrawShards);
    BOF3_INJECT(EffectKindA0_DrawShard);
    BOF3_INJECT(EffectKindA0_SwapLong);
    BOF3_INJECT(EffectKindA1_Run);
    BOF3_INJECT(EffectKindA2_Run);
    BOF3_INJECT(EffectKindA3_Run);
    BOF3_INJECT(EffectKindA7_Run);
    BOF3_INJECT(EffectKindA8_Run);
    BOF3_INJECT(EffectKindA9_Run);
}
