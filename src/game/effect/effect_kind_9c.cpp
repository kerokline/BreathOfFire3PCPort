// Moved here by tools/move_funcs.py from effect_4e.cpp, rest_3f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_9c.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_9c_callees.h"
#include "game/effect_4e.h"
#include "game/effect_4e_callees.h"
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

namespace at = effect_kind_9c::at;

}  // namespace

namespace {

using U = std::uint32_t;

using LD = long double;

using move_script::Long;

using move_script::SetLong;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul r32, r32 then sar: the product's low 32 bits, shifted.
U MulSar(int a, U b, unsigned n) { return Sar(static_cast<U>(a) * b, n); }

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

// The trail of EffectKind9C_DrawTrail that kind 0x9C's states 1..6 draw:
// (Sprite_Current +0x34, +0xC, the byte +0x5D).
void Trail9C() {
    unsigned char* const s = S();
    using Trail = void (__cdecl*)(const long*, const long*, unsigned);
    SH_AT(Trail, at::kTrail9C.v)(Point(s + 0x34), Point(s + 0xC), s[0x5D]);
}

// Kind 0x9C's glow: EffectKindA0_DrawGlow(+0x34, the word +0x2E, 2).
void Glow9C() {
    unsigned char* const s = S();
    SH_CALL(EffectKindA0_DrawGlow)(Point(s + 0x34), rv::As<records::EffectWork>(s)->work_2e, 2);
}

using move_script::At;

}  // namespace

// original 0x48E320 (hidden in 0x48E0A0): jmp [EffectKind9C_States + +1 * 4],
// unbounded.
extern "C" void __cdecl EffectKind9C_Run(void) {
    DispatchEffect4e("EffectKind9C_Run", EffectKind9C_States, EffectKind9C_States_count);
}

// original 0x48E340 (state 0): the point (0x308000, 0x740000), +0x3C its
// AreaMap_Elevation as an s16 << 16; the trail's other end +0xC..+0x14 =
// (0x308000, 0x718000, 0xFEC00000); the glow's size word +0x2E and the trail's
// shade +0x5D = 0; +9 = 8; +1 up.
extern "C" void __cdecl EffectKind9C_Start(void) {
    rv::Current<records::EffectObject>()->x = 0x308000;
    rv::Current<records::EffectObject>()->z = 0x740000;
    unsigned char* s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    const long elevation = SH_CALL(AreaMap_Elevation)(so->x, so->z);
    rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<U>(S16(static_cast<U>(elevation))) << 16);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x308000);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0x718000);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0xFEC00000u);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0);
    S()[0x5D] = 0;
    S()[9] = 8;
    NextState();
}

// original 0x48E3D0 (state 1): the size +0x2E up 0x10, the shade +0x5D up 8;
// the trail and the glow drawn; +9 down, at 0: +9 = 0x3C, +1 up,
// Sound_PlayEffect(0x20D).
extern "C" void __cdecl EffectKind9C_Grow(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_2e + 0x10u);
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
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e)) > 0) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_2e - 0x10u);
        s = S();
    }
    if (static_cast<signed char>(s[0x5D]) > 0) s[0x5D] = static_cast<unsigned char>(s[0x5D] - 8);
    Trail9C();
    Glow9C();
    SH_CALL(EffectKind9C_DrawBeams)(0x40);
    if (!CountDown()) return;
    S()[9] = 0x10;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0x40);
    NextState();
}

// original 0x48E6B0 (state 7): the word +0x2E down 4, the beams drawn that
// size; +9 down, at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKind9C_End(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_2e - 4u);
    SH_CALL(EffectKind9C_DrawBeams)(rv::As<records::EffectWork>(S())->work_2e);
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

// original 0x48ED80 (from, to, shade; PSX twin 0x801D1C1C): a draw mode
// (Gpu_GetTPage(0, 1, 0x2C0, 0x100), dtd 1) committed 0xC to slot 2; two
// semi-transparent POLY_G4 from `from` projected (a) to `to` projected (b),
// each committed 0x44 to slot 2: the first (a + (hx, hy), a, b - (hx, hy), b)
// shaded (black, (s, s, 0), black, (s, s, 0)), the second (a, a + (hx, hy), b,
// b - (hx, hy)) shaded ((0, s, 0), black, (0, s, 0), black) - hx / hy the
// floats 0x5C41B8 / 0x5C41C0, every vertex at its end's depth. Then up to
// eight TILE_1 dots (0, s, 0) from from + (Frame_Counter & 7) * d stepping 8 *
// d, d = (to - from) sar 6, while the dot's y is not below to's, each at its
// projection less hx in y, committed 0x14 to slot 2.
extern "C" void __cdecl EffectKind9C_DrawTrail(const long* from, const long* to, unsigned shade) {
    DrawMode(1, 0x2C0, 0x100, 1, 2);
    const unsigned char c = static_cast<unsigned char>(shade);
    const unsigned char* const hx = At(at::kTrailHalfX);
    const unsigned char* const hy = At(at::kTrailHalfY);
    alignas(4) unsigned char o[12];
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    StF(p + 8, F(o) + F(hx));
    StF(p + 0xC, F(o + 4) + F(hy));
    SetUL(p + 0x18, UL(o));
    SetUL(p + 0x1C, UL(o + 4));
    SetUL(p + 0x20, UL(o + 8));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    StF(p + 0x28, F(o) - F(hx));
    StF(p + 0x2C, F(o + 4) - F(hy));
    SetUL(p + 0x38, UL(o));
    SetUL(p + 0x3C, UL(o + 4));
    SetUL(p + 0x40, UL(o + 8));
    SetUL(p + 0x30, UL(o + 8));
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = c;
    p[0x15] = c;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0;
    p[0x34] = c;
    p[0x35] = c;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(from, Out(o));
    SetUL(p + 8, UL(o));
    SetUL(p + 0xC, UL(o + 4));
    StF(p + 0x18, F(o) + F(hx));
    StF(p + 0x1C, F(o + 4) + F(hy));
    SetUL(p + 0x20, UL(o + 8));
    SetUL(p + 0x10, UL(o + 8));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(o));
    SetUL(p + 0x28, UL(o));
    SetUL(p + 0x2C, UL(o + 4));
    StF(p + 0x38, F(o) - F(hx));
    StF(p + 0x3C, F(o + 4) - F(hy));
    SetUL(p + 0x40, UL(o + 8));
    SetUL(p + 0x30, UL(o + 8));
    p[4] = 0;
    p[5] = c;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0;
    p[0x25] = c;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
    // the dots
    const U fx = static_cast<U>(from[0]), fy = static_cast<U>(from[1]);
    const U dx = Sar(static_cast<U>(to[0]) - fx, 6);
    const U dy = Sar(static_cast<U>(to[1]) - fy, 6);
    const U fz = static_cast<U>(from[2]);
    const U dz = Sar(static_cast<U>(to[2]) - fz, 6);
    const U f = Frame_Counter & 7u;
    alignas(4) long q[3] = {static_cast<long>(f * dx + fx), static_cast<long>(f * dy + fy), static_cast<long>(f * dz + fz)};
    for (unsigned j = 0; j < 8; ++j) {
        if (q[1] < to[1]) break;
        SH_CALL(EffectGte_ProjectPoint)(q, Out(o));
        p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, UL(o));
        StF(p + 0xC, F(o + 4) - F(hx));
        SetUL(p + 0x10, UL(o + 8));
        p[4] = 0;
        p[5] = c;
        p[6] = 0;
        SH_CALL(Gfx_CommitPrim)(2, 0x14);
        q[0] = static_cast<long>(static_cast<U>(q[0]) + dx * 8);
        q[1] = static_cast<long>(static_cast<U>(q[1]) + dy * 8);
        q[2] = static_cast<long>(static_cast<U>(q[2]) + dz * 8);
    }
}

void EffectKind9cFromEffect4e_Inject() {
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
}

void EffectKind9cFromRest3f_Inject() {
    BOF3_INJECT(EffectKind9C_DrawTrail);
}
