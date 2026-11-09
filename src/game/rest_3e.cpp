// Round fourteen group R3E (docs/rest_3e.md): the 50 functions of
// analysis/round14_cut.tsv's group R3E, 0x46A320..0x4801ED, each read with
// capstone to its last instruction (2026-10-04). All effect code: three kinds'
// dispatchers, the helpers five kinds of round twelve and thirteen call, the
// pools kinds 0x48 / 0x49 / 0x52 draw with, and the states of kinds 0x5D, 0x5E
// and 0x5F.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies, with one
// exception: EffectGlowTrail_Update is EffectKind52_TrailUpdate (E2F's)
// instruction for instruction but the call targets, so it calls that function
// directly (its callees go through the harness inside it). Sprite_Current is
// read again wherever the original reads [0x937F88] again after a call. No
// divergence: each is a faithful replacement. Where the original jumps through
// a state table past its end, or indexes the marks past their room, ours
// aborts with a message (docs/rest_3e.md section 7).
//
// One divergence rides on EffectKind5E_DrawPanel, its own switch
// (DockLegend, DIV-0137 of docs/button-unification-round.md): the dock's
// legend shows the buttons the Config has on its three tests now (section
// "the dock's legend" below). A second on EffectKind5E_DrawList (DIV-0198):
// the list's first line follows the release the mapped image's list box is
// in - the Western PSX twin's y + 4 over the West's own box, Japan's y + 0x14
// over Japan's.
#include "game/rest_3e.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/rest_3e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/exe_image.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_3e::at;
using U = std::uint32_t;
using LD = long double;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U ULRest3e(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
U ULRest3e(U a) { return ULRest3e(move_script::At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U AddressOfRest3e(const void* p) { return Low32(p); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul r32, r32: the product's low 32 bits.
U Mul(U a, U b) { return a * b; }
// (cos or sin answered first) * the width word +0xE, zero-extended, sar 12:
// the width read after the call, as the original reads it.
U Turn(int answer, const unsigned char* point) { return Sar(Mul(static_cast<U>(answer), Word(point + 0xE)), 12); }
void Copy12Rest3e(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, ULRest3e(from + i));
}

// --- x87 as the original has it (effect_3a.cpp's form): `fld dword` through
// inline assembly, every operation in long double at the game's control word,
// `fstp dword` out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}
LD I(std::int32_t v) { return static_cast<LD>(v); }
void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
// fcomp; fnstsw; test ah, 0x40: C3 set - equal, or unordered.
bool EqualOrUnordered(const unsigned char* a, const unsigned char* b) {
    const LD x = F(a), y = F(b);
    return !(x < y) && !(x > y);
}

const long* Point(const unsigned char* p) { return reinterpret_cast<const long*>(p); }
float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - the next table.
void Dispatch(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3e.md section 7)",
                    who, state, entries, (unsigned)::bof3::VaOf(AddressOfRest3e(table)));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(ULRest3e(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// The draw-mode packet the draws open with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawModeRest3e(U abr, int x, int y, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

}  // namespace

// ===========================================================================
// Three kinds' dispatchers (Effect_KindHandlers[0x37], [0x17], [0x1B]; their
// state tables FC1's, docs/field_c1.md)
// ===========================================================================

// ===========================================================================
// Helpers of kinds 0x41, 0x30, 0x1D, 0x21, 0x24
// ===========================================================================

// ===========================================================================
// The glow sparks: 8 records of 0x1C at EffectKind30_Shards (+0 in use, +1 the
// state, +2 the count, +3 the shade, +4 the reload, +8 the rise speed, +0xC
// the point). Kinds 0x48 and 0x49 (E2D, E2E) and kind 0x52 (E2F) use them.
// ===========================================================================

unsigned char* Spark(unsigned i) { return EffectKind30_Shards + i * rest_3e::at::kSparkStride; }

// original 0x4790C0: +0 of each of the 8 sparks = 0; the two sound flags
// 0x6761C9 and 0x6761C8 = 0.
extern "C" void __cdecl EffectGlowSparks_Clear(void) {
    for (unsigned i = 0; i < rest_3e::at::kSparks; ++i) Spark(i)[0] = 0;
    Rest3e_GlowSounded = 0;
    Rest3e_RiseSounded = 0;
}

// original 0x4790F0: a spark set going near the record: +0 = 1, +1 = 0, +2 =
// 8, +3 = 0, +8 = 0, +4 = 0x40; x = ((Rand & 0xFF) - 0x80) << 11 + the
// record's x, z likewise (Sprite_Current read after each Rand), the height the
// record's +0x3C.
extern "C" void __cdecl EffectGlowSparks_StartRise(unsigned char* spark) {
    spark[0] = 1;
    spark[1] = 0;
    spark[2] = 8;
    spark[3] = 0;
    SetUL(spark + 8, 0);
    spark[4] = 0x40;
    U r = static_cast<U>(SH_CALL(Rand)());
    SetUL(spark + 0xC, (((r & 0xFFu) - 0x80u) << 11) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->x));
    r = static_cast<U>(SH_CALL(Rand)());
    SetUL(spark + 0x10, (((r & 0xFFu) - 0x80u) << 11) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->z));
    SetUL(spark + 0x14, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->height));
}

// original 0x479160: a spark set on a sphere round the record: +0 = 1, +1 =
// 0, +2 = 8, +3 = 0, +8 = 0, +4 = 1; a = Rand & 0x7FF, b = Rand & 0xFFF, the
// radius r = the low word, signed, of ((Rand % 2, signed) << 12 | Rand &
// 0xFFF); x = (sin a cos b sar 12) r sar 8 + the record's x, z = (sin a sin b
// sar 12) r sar 8 + its z, the height cos a * r + its +0x3C + 0x2000000
// (Sprite_Current read for each).
extern "C" void __cdecl EffectGlowSparks_StartBurst(unsigned char* spark) {
    spark[0] = 1;
    spark[1] = 0;
    spark[2] = 8;
    spark[3] = 0;
    SetUL(spark + 8, 0);
    spark[4] = 1;
    const U a = static_cast<U>(SH_CALL(Rand)()) & 0x7FFu;
    const U b = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const std::int32_t half = static_cast<std::int32_t>(SH_CALL(Rand)()) % 2;
    const U low = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U r = static_cast<U>(static_cast<std::int16_t>((static_cast<U>(half) << 12) | low));
    const U sa = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
    const U cb = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(b)));
    U v = Sar(Mul(sa, cb), 12);
    SetUL(spark + 0xC, Sar(Mul(v, r), 8) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->x));
    const U sa2 = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
    const U sb = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(b)));
    v = Sar(Mul(sa2, sb), 12);
    SetUL(spark + 0x10, Sar(Mul(v, r), 8) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->z));
    const U ca = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
    SetUL(spark + 0x14, Mul(ca, r) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->height) + 0x2000000u);
}

// original 0x479260 (E2F's EffectKind52_MoveSparks instruction for
// instruction but its table): a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd
// 1, slot 1); EffectGte_LoadMapCamera; each spark in use called through
// EffectGlowSparks_States by its +1 (unbounded; each handed the spark) and
// drawn (EffectGlowSparks_Draw). Answers 1 when a spark was in use, else 0.
extern "C" unsigned char __cdecl EffectGlowSparks_Run(void) {
    DrawModeRest3e(1, 0x3C0, 0, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    for (unsigned i = 0; i < rest_3e::at::kSparks; ++i) {
        unsigned char* const k = Spark(i);
        if (k[0] == 0) continue;
        const unsigned state = k[1];
        if (state >= EffectGlowSparks_States_count)
            bof3::Fatal("EffectGlowSparks_Run: spark %u's state +1 is %u, past the %u entries of EffectGlowSparks_States - "
                        "the original calls through the dword after (docs/rest_3e.md section 7)",
                        i, state, EffectGlowSparks_States_count);
        reinterpret_cast<void (__cdecl*)(unsigned char*)>(
            static_cast<std::uintptr_t>(ULRest3e(reinterpret_cast<const unsigned char*>(EffectGlowSparks_States) + 4 * state)))(k);
        SH_CALL(EffectGlowSparks_Draw)(k);
        any = 1;
    }
    return any;
}

// original 0x4792E0 (E3A's EffectKind68_DrawMote but its size 0x20 and slot
// 1): the spark's point +0xC projected (o) and its size {0x20, a word the
// original never writes} projected (EffectGte_ProjectSize reads the first); r
// = that size's first word + (Frame_Counter & 1), its low word signed; a disc
// of 16 semi-transparent POLY_G3 round o, the centre in the shade +3, the rim
// black at (cos, sin) * r sar 12, 0x100 apart; each committed to slot 1
// (0x34).
extern "C" void __cdecl EffectGlowSparks_Draw(unsigned char* spark) {
    alignas(4) short sz[2] = {0x20, 0};
    alignas(4) short r[2];
    SH_CALL(EffectGte_ProjectSize)(Point(spark + 0xC), sz, r);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(spark + 0xC), Out(o));
    const U radius = static_cast<U>(static_cast<std::int16_t>(static_cast<U>(static_cast<unsigned short>(r[0])) + (Frame_Counter & 1u)));
    U angle = 0;
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, ULRest3e(o));
        SetUL(p + 0xC, ULRest3e(o + 4));
        U d = Sar(Mul(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))), radius), 12);
        StF(p + 0x18, I(static_cast<std::int32_t>(d)) + F(o));
        d = Sar(Mul(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))), radius), 12);
        angle += 0x100;
        StF(p + 0x1C, I(static_cast<std::int32_t>(d)) + F(o + 4));
        d = Sar(Mul(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))), radius), 12);
        StF(p + 0x28, I(static_cast<std::int32_t>(d)) + F(o));
        d = Sar(Mul(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))), radius), 12);
        StF(p + 0x2C, I(static_cast<std::int32_t>(d)) + F(o + 4));
        SetUL(p + 0x30, ULRest3e(o + 8));
        SetUL(p + 0x20, ULRest3e(o + 8));
        SetUL(p + 0x10, ULRest3e(o + 8));
        const unsigned char shade = spark[3];
        p[6] = shade;
        p[5] = shade;
        p[4] = shade;
        p[0x26] = p[0x25] = p[0x24] = 0;
        p[0x16] = p[0x15] = p[0x14] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// original 0x479420 (EffectGlowSparks_States[0]): the shade +3 up 6; the count
// +2 down, at 0 the count its reload +4, +1 up, and sound 0x200 the first time
// (the flag 0x6761C9).
extern "C" void __cdecl EffectGlowSparks_Glow(unsigned char* spark) {
    spark[3] = static_cast<unsigned char>(spark[3] + 6);
    spark[2] = static_cast<unsigned char>(spark[2] - 1);
    if (spark[2] != 0) return;
    spark[2] = spark[4];
    spark[1] = static_cast<unsigned char>(spark[1] + 1);
    if (Rest3e_GlowSounded != 0) return;
    SH_CALL(Sound_PlayEffect)(at::kSoundGlow);
    Rest3e_GlowSounded = 1;
}

// original 0x479470 (EffectGlowSparks_States[2]): the first time (the flag
// 0x6761C8) sound 0x201 unless the record's +6 is set, the flag set either way;
// then the rise speed +8 up 0x40000, the height +0x14 up by it, the shade +3
// up 6, the count +2 down, at 0 the spark freed (+0 = 0).
extern "C" void __cdecl EffectGlowSparks_Rise(unsigned char* spark) {
    if (Rest3e_RiseSounded == 0) {
        if (S()[6] == 0) SH_CALL(Sound_PlayEffect)(at::kSoundRise);
        Rest3e_RiseSounded = 1;
    }
    const U speed = ULRest3e(spark + 8) + 0x40000u;
    SetUL(spark + 8, speed);
    SetUL(spark + 0x14, ULRest3e(spark + 0x14) + speed);
    spark[3] = static_cast<unsigned char>(spark[3] + 6);
    spark[2] = static_cast<unsigned char>(spark[2] - 1);
    if (spark[2] == 0) spark[0] = 0;
}

// ===========================================================================
// The glow trail (0x92C060), the spiral (0x92C4A4), the ring
// ===========================================================================

// original 0x4794D0 (0x1D1 bytes): E2F's EffectKind52_TrailUpdate (0x47D040)
// and E3D's EffectKind80_TrailStep (0x4873E0) instruction for instruction but
// the call targets, which are the same callees: called directly (its callees
// go through the harness inside it). docs/effect_2f.md has the body.
extern "C" void __cdecl EffectGlowTrail_Update(unsigned char* trail) { EffectKind52_TrailUpdate(trail); }

// original 0x4796B0 (0x2BE bytes): E2F's EffectKind52_TrailDraw without the
// depths - a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1, slot 1); the cap
// at point 0 (EffectTrail_DrawCap at its angle - 0x400, shade 0x80); for each
// pair of points k, k + 1 (screen floats at +0x10) whose x or y differ (equal
// or unordered both = skipped), two Gouraud quads at the packet cursor,
// semi-transparent: the first from point k's screen x, y and x + (cos(a +
// 0x400) w sar 12), y + (sin ..) to point k + 1's likewise (a, w each point's
// angle +0x1C and width +0x1E, zero-extended), shaded the running shade at
// point k's and it - 4 at k + 1's (black at the offset ones), committed to slot
// 1 (0x44); the second a copy of the first (0x44 bytes from the cursor back)
// with the offset vertices at a - 0x400, committed; the running shade down 4.
// The depth words of the quads are not written. The cap at point 31 at its
// angle + 0x400 and the running shade.
extern "C" void __cdecl EffectGlowTrail_Draw(unsigned char* trail) {
    DrawModeRest3e(1, 0x3C0, 0, 1, 1);
    unsigned char shade = 0x80;
    SH_CALL(EffectTrail_DrawCap)(trail + 0x10, Word(trail + 0x1E), (Word(trail + 0x1C) - 0x400u) & 0xFFFFu, 0x80);
    for (unsigned k = 0; k < 0x1F; ++k) {
        const unsigned char* const p = trail + 0x10 + 0x20 * k;
        const unsigned char* const q = p + 0x20;
        if (EqualOrUnordered(p, q) && EqualOrUnordered(p + 4, q + 4)) continue;
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SetUL(prim + 8, ULRest3e(p));
        SetUL(prim + 0xC, ULRest3e(p + 4));
        U v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(p + 0xC) + 0x400u)), p);
        StF(prim + 0x18, I(static_cast<std::int32_t>(v)) + F(p));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(p + 0xC) + 0x400u)), p);
        StF(prim + 0x1C, I(static_cast<std::int32_t>(v)) + F(p + 4));
        SetUL(prim + 0x28, ULRest3e(q));
        SetUL(prim + 0x2C, ULRest3e(q + 4));
        v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(q + 0xC) + 0x400u)), q);
        StF(prim + 0x38, I(static_cast<std::int32_t>(v)) + F(q));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(q + 0xC) + 0x400u)), q);
        const auto next = static_cast<unsigned char>(shade - 4);
        prim[6] = prim[5] = prim[4] = shade;
        prim[0x16] = prim[0x15] = prim[0x14] = 0;
        StF(prim + 0x3C, I(static_cast<std::int32_t>(v)) + F(q + 4));
        prim[0x26] = prim[0x25] = prim[0x24] = next;
        prim[0x36] = prim[0x35] = prim[0x34] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        unsigned char* const copy = Gfx_PacketNext;
        std::memmove(copy, copy - 0x44, 0x44);
        v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(p + 0xC) - 0x400u)), p);
        StF(copy + 0x18, I(static_cast<std::int32_t>(v)) + F(p));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(p + 0xC) - 0x400u)), p);
        StF(copy + 0x1C, I(static_cast<std::int32_t>(v)) + F(p + 4));
        v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(q + 0xC) - 0x400u)), q);
        StF(copy + 0x38, I(static_cast<std::int32_t>(v)) + F(q));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(q + 0xC) - 0x400u)), q);
        StF(copy + 0x3C, I(static_cast<std::int32_t>(v)) + F(q + 4));
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        shade = next;
    }
    SH_CALL(EffectTrail_DrawCap)(trail + 0x3F0, Word(trail + 0x3FE), (Word(trail + 0x3FC) + 0x400u) & 0xFFFFu, shade);
}

// The spiral record (0xD20 bytes): +0..+8 its centre; eight rings of 32 screen
// points (12 bytes each) from +0x10, 0x180 a ring; eight rings of 32 shade
// bytes from +0xC10; the tilt word +0xD10; a light vector of three longs at
// +0xD14.
namespace {
constexpr U kRing = 0x180;
// Ring 0's point i: angle i * 0x80; with the tilt t: a = sin t cos i sar 12,
// b = sin t sin i sar 12, c = cos t; the shade from the light (a * l0 + b * l1
// + c * l2, summed as the original: l1 b + c l2, then + a l0): at most 0 the
// `dark` value, else ((sqrt, at most 0xFFF) sar 5 & 0x78) + 7; the point (a <<
// 5 + x, b << 5 + z, c << 13 + y) projected.
void SpiralPoint(unsigned char* spiral, unsigned i, unsigned char dark, unsigned char* shade, unsigned char* screen) {
    const int angle = static_cast<int>((i * 0x80u) & 0xFFFFu);
    U a = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(Word(spiral + 0xD10))));
    a = Sar(Mul(a, static_cast<U>(SH_CALL(Math_Cos)(angle))), 12);
    U b = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(Word(spiral + 0xD10))));
    b = Sar(Mul(b, static_cast<U>(SH_CALL(Math_Sin)(angle))), 12);
    const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(Word(spiral + 0xD10))));
    const U dot = Mul(ULRest3e(spiral + 0xD18), b) + Mul(c, ULRest3e(spiral + 0xD1C)) + Mul(a, ULRest3e(spiral + 0xD14));
    if (static_cast<std::int32_t>(dot) <= 0) {
        *shade = dark;
    } else {
        std::int32_t root = static_cast<std::int32_t>(SH_AT(long (__cdecl*)(long), rest_3e::at::kSqrt.v)(static_cast<long>(dot)));
        if (root > 0xFFF) root = 0xFFF;
        *shade = static_cast<unsigned char>(((static_cast<U>(root) >> 5) & 0x78u) + 7u);
    }
    alignas(4) unsigned char pt[12];
    SetUL(pt, (a << 5) + ULRest3e(spiral));
    SetUL(pt + 4, (b << 5) + ULRest3e(spiral + 4));
    SetUL(pt + 8, (c << 13) + ULRest3e(spiral + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(screen));
}
}  // namespace

// original 0x4799C0 (E2E's kind 0x48 states 8 and 10 call it): set up.
// EffectGte_LoadMapCamera; the light +0xD14 = (0, 0x1000, 0x1000) normalised in
// place (Gte_VectorNormal); ring 0's 32 points and shades (SpiralPoint, the
// dark shade 0x70), each copied to rings 1..7 at once (the point, then the
// shade).
extern "C" void __cdecl EffectSpiral_Init(unsigned char* spiral) {
    SH_CALL(EffectGte_LoadMapCamera)();
    SetUL(spiral + 0xD18, 0x1000);
    SetUL(spiral + 0xD14, 0);
    SetUL(spiral + 0xD1C, 0x1000);
    SH_CALL(Gte_VectorNormal)(Point(spiral + 0xD14), reinterpret_cast<long*>(spiral + 0xD14));
    for (unsigned i = 0; i < 0x20; ++i) {
        unsigned char* const shade = spiral + 0xC10 + i;
        unsigned char* const screen = spiral + 0x10 + 12 * i;
        SpiralPoint(spiral, i, 0x70, shade, screen);
        for (unsigned r = 1; r < 8; ++r) {
            Copy12Rest3e(screen + kRing * r, screen);
            shade[0x20 * r] = *shade;
        }
    }
}

// original 0x479B70 (E2E's kind 0x48 states 8, 9 and 10 call it; PSX twin
// 0x801F9330): the rings' points moved out one (ring 6 to 7 .. 0 to 1; the
// shades stay); EffectGte_LoadMapCamera; ring 0 again (the dark shade 7); a
// draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1, slot 1); for ring k = 1..7
// and point i of 32 (n = i + 1 mod 32): a POLY_G4 at the packet cursor,
// semi-transparent - ring k - 1's points i and n, ring k's points i and n -
// shaded ring k - 1's shades i and n times (8 - k) / 7 and ring k's shade i
// (twice: the original reads i for the fourth vertex too) times (7 - k) / 7;
// committed to slot 1 (0x44).
extern "C" void __cdecl EffectSpiral_StepDraw(unsigned char* spiral) {
    for (unsigned r = 7; r != 0; --r)
        for (unsigned i = 0; i < 0x20; ++i) Copy12Rest3e(spiral + 0x10 + kRing * r + 12 * i, spiral + 0x10 + kRing * (r - 1) + 12 * i);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < 0x20; ++i) SpiralPoint(spiral, i, 7, spiral + 0xC10 + i, spiral + 0x10 + 12 * i);
    DrawModeRest3e(1, 0x3C0, 0, 1, 1);
    for (unsigned k = 1; k < 8; ++k) {
        const std::int32_t outer = static_cast<std::int32_t>(8 - k), inner = static_cast<std::int32_t>(7 - k);
        for (unsigned i = 0; i < 0x20; ++i) {
            const unsigned n = (i + 1) & 0x1F;
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            Copy12Rest3e(p + 8, spiral + 0x10 + kRing * (k - 1) + 12 * i);
            Copy12Rest3e(p + 0x18, spiral + 0x10 + kRing * (k - 1) + 12 * n);
            p[4] = p[5] = p[6] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * (k - 1) + i] * outer / 7);
            p[0x14] = p[0x15] = p[0x16] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * (k - 1) + n] * outer / 7);
            Copy12Rest3e(p + 0x28, spiral + 0x10 + kRing * k + 12 * i);
            Copy12Rest3e(p + 0x38, spiral + 0x10 + kRing * k + 12 * n);
            p[0x24] = p[0x25] = p[0x26] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * k + i] * inner / 7);
            p[0x34] = p[0x35] = p[0x36] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * k + i] * inner / 7);
            SH_CALL(Gfx_CommitPrim)(1, 0x44);
        }
    }
}

// original 0x479EE0 (E2E's kind 0x48 state 7 calls it): a ring of 32
// semi-transparent POLY_G4 round the ring record's point (+0, +4, +8): the
// rim at angle t is (x + cos t << 5, z + sin t << 5) at the height and at the
// height + 0x7000000, each projected; quad i joins the rim at t = 0x80 i to t
// = 0x80 (i + 1). Before each quad a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 1) linked at the new rim point (MapView_LinkPrimAt(x, z, 0, 0xC)); the
// quad linked there too (0x44). The shade: the old pair's the last quad's new
// one (the first: min(+0x10, 0xFF)); the new pair's min((s * +0x10, 16 bits)
// >> 7, 0xFF), s a running byte from 0x80 down 4 for quads 4..19, else up 4.
namespace {
// The ring's rim at angle t: x + cos t << 5, z + sin t << 5, the height (each
// read after its call).
void RimPoint(const unsigned char* ring, int t, unsigned char* pt) {
    const U cos = static_cast<U>(SH_CALL(Math_Cos)(t));
    SetUL(pt, (cos << 5) + ULRest3e(ring));
    const U sin = static_cast<U>(SH_CALL(Math_Sin)(t));
    SetUL(pt + 4, (sin << 5) + ULRest3e(ring + 4));
    SetUL(pt + 8, ULRest3e(ring + 8));
}
}  // namespace

extern "C" void __cdecl EffectRing_Draw(const unsigned char* ring) {
    alignas(4) unsigned char pt[12];
    alignas(4) unsigned char bottom[12], top[12];
    int angle = 0;
    RimPoint(ring, angle, pt);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(bottom));
    SetUL(pt + 8, ULRest3e(ring + 8) + 0x7000000u);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(top));
    unsigned char s = 0x80;
    unsigned shade = Word(ring + 0x10);
    if (shade > 0xFF) shade = 0xFF;
    for (unsigned i = 0; i < 0x20; ++i) {
        angle += 0x80;
        const int t = angle & 0xFFFF;
        RimPoint(ring, t, pt);
        const U tp = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tp & 0xFFFFu, 0);
        SH_CALL(MapView_LinkPrimAt)(ULRest3e(pt), ULRest3e(pt + 4), 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12Rest3e(p + 8, bottom);
        Copy12Rest3e(p + 0x18, top);
        p[4] = p[5] = p[6] = static_cast<unsigned char>(shade);
        p[0x14] = p[0x15] = p[0x16] = static_cast<unsigned char>(shade);
        SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(bottom));
        SetUL(pt + 8, ULRest3e(ring + 8) + 0x7000000u);
        SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(top));
        Copy12Rest3e(p + 0x28, bottom);
        Copy12Rest3e(p + 0x38, top);
        if (i >= 4 && i < 0x14)
            s = static_cast<unsigned char>(s - 4);
        else
            s = static_cast<unsigned char>(s + 4);
        shade = ((static_cast<unsigned>(s) * Word(ring + 0x10)) & 0xFFFFu) >> 7;
        if (shade > 0xFF) shade = 0xFF;
        p[0x24] = p[0x25] = p[0x26] = static_cast<unsigned char>(shade);
        p[0x34] = p[0x35] = p[0x36] = static_cast<unsigned char>(shade);
        SH_CALL(MapView_LinkPrimAt)(ULRest3e(pt), ULRest3e(pt + 4), 0, 0x44);
    }
}

// ===========================================================================
// The dust: 64 records of 0x20 at 0x92D1DC (+0 in use, +2 the count, +4 x, +8
// z, +0xC the height, +0x14 the spread, +0x18 the rise, +0x1C the ground).
// Kind 0x49's variant 3 (E2D) uses them.
// ===========================================================================

unsigned char* Dust(unsigned i) { return At(::bof3::Img(rest_3e::at::kDust.v) + i * at::kDustStride); }

// original 0x47A110: +0 of each of the 64 = 0.
extern "C" void __cdecl EffectDust_Clear(void) {
    for (unsigned i = 0; i < at::kDustCount; ++i) Dust(i)[0] = 0;
}

// original 0x47A130: the first of the 64 whose +0 is 0, or null.
extern "C" unsigned char* __cdecl EffectDust_FindFree(void) {
    for (unsigned i = 0; i < at::kDustCount; ++i)
        if (Dust(i)[0] == 0) return Dust(i);
    return nullptr;
}

// original 0x47A150: a dust record set round Sprite_Current's point: +0 = 1,
// +1 = 0, +2 = 0x10, +0x18 = +0x14 = 0; the radius ((Rand % 2, signed) << 8 |
// Rand & 0xFF) << 8, the angle Rand & 0xFC0; x = cos * radius sar 12 + the
// record's x, z = sin * radius sar 12 + its z (Sprite_Current read for each);
// the height and the ground AreaMap_Elevation(x, z) << 16 (its low word,
// signed).
extern "C" void __cdecl EffectDust_Start(unsigned char* dust) {
    dust[0] = 1;
    dust[1] = 0;
    dust[2] = 0x10;
    SetUL(dust + 0x18, 0);
    SetUL(dust + 0x14, 0);
    const std::int32_t half = static_cast<std::int32_t>(SH_CALL(Rand)()) % 2;
    const U low = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
    const U radius = ((static_cast<U>(half) << 8) | low) << 8;
    const int angle = static_cast<int>(static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    const U cos = static_cast<U>(SH_CALL(Math_Cos)(angle));
    SetUL(dust + 4, Sar(Mul(cos, radius), 12) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->x));
    const U sin = static_cast<U>(SH_CALL(Math_Sin)(angle));
    const U z = Sar(Mul(sin, radius), 12) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->z);
    SetUL(dust + 8, z);
    const U ground = static_cast<U>(static_cast<std::int16_t>(SH_CALL(AreaMap_Elevation)(static_cast<long>(ULRest3e(dust + 4)), static_cast<long>(z)))) << 16;
    SetUL(dust + 0x1C, ground);
    SetUL(dust + 0xC, ground);
}

// original 0x47A200 (E2D's kind 0x49 variant 3 calls it; PSX twin 0x801F9EFC):
// a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1, slot 1);
// EffectGte_LoadMapCamera; each dust record in use: its column drawn
// (EffectDust_DrawColumn), its fan when the column answered 1; then the rise
// +0x18 up 0x100000, the height +0xC up by it, the spread +0x14 up 0x2000, the
// count +2 down, at 0 the record freed. Answers 1 when one was in use.
extern "C" unsigned char __cdecl EffectDust_Run(void) {
    DrawModeRest3e(1, 0x3C0, 0, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    for (unsigned i = 0; i < at::kDustCount; ++i) {
        unsigned char* const d = Dust(i);
        if (d[0] == 0) continue;
        if (SH_CALL(EffectDust_DrawColumn)(d) != 0) SH_CALL(EffectDust_DrawFan)(d);
        const U rise = ULRest3e(d + 0x18) + 0x100000u;
        SetUL(d + 0x14, ULRest3e(d + 0x14) + 0x2000u);
        SetUL(d + 0x18, rise);
        SetUL(d + 0xC, ULRest3e(d + 0xC) + rise);
        d[2] = static_cast<unsigned char>(d[2] - 1);
        if (d[2] == 0) d[0] = 0;
        any = 1;
    }
    return any;
}

// original 0x47A2B0 (PSX twin 0x801F9A80): a POLY_F4 at the packet cursor,
// semi-transparent, colour 0x40: the top point (x, z, height + spread << 8)
// and the bottom (x, z, height - spread << 8, raised to the ground +0x1C when
// below it - the answer 1 then, else 0), each projected; each end's two
// vertices its screen x -/+ the float 0x5C41B8, its y and depth; committed to
// slot 1 (0x38).
extern "C" unsigned char __cdecl EffectDust_DrawColumn(const unsigned char* dust) {
    unsigned char clamped = 0;
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    alignas(4) unsigned char pt[12], sc[12];
    SetUL(pt, ULRest3e(dust + 4));
    SetUL(pt + 4, ULRest3e(dust + 8));
    SetUL(pt + 8, (ULRest3e(dust + 0x14) << 8) + ULRest3e(dust + 0xC));
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(sc));
    const unsigned char* const k = At(at::kHalfWidth);
    StF(p + 8, F(sc) - F(k));
    SetUL(p + 0xC, ULRest3e(sc + 4));
    StF(p + 0x14, F(sc) + F(k));
    SetUL(p + 0x18, ULRest3e(sc + 4));
    SetUL(p + 0x1C, ULRest3e(sc + 8));
    SetUL(p + 0x10, ULRest3e(sc + 8));
    SetUL(pt, ULRest3e(dust + 4));
    SetUL(pt + 4, ULRest3e(dust + 8));
    U low = ULRest3e(dust + 0xC) - (ULRest3e(dust + 0x14) << 8);
    const U ground = ULRest3e(dust + 0x1C);
    if (static_cast<std::int32_t>(low) < static_cast<std::int32_t>(ground)) {
        low = ground;
        clamped = 1;
    }
    SetUL(pt + 8, low);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(sc));
    StF(p + 0x20, F(sc) - F(k));
    SetUL(p + 0x24, ULRest3e(sc + 4));
    StF(p + 0x2C, F(sc) + F(k));
    SetUL(p + 0x30, ULRest3e(sc + 4));
    SetUL(p + 0x34, ULRest3e(sc + 8));
    SetUL(p + 0x28, ULRest3e(sc + 8));
    p[4] = p[5] = p[6] = 0x40;
    SH_CALL(Gfx_CommitPrim)(1, 0x38);
    return clamped;
}

// original 0x47A3D0 (PSX twin 0x801F9BDC): a fan of 16 semi-transparent
// POLY_G3 on the ground round the dust's point: the centre (x, z, the ground
// AreaMap_Elevation(x, z) << 16) projected, shade 0x40; the rim at angle t
// (x + cos t * 4, z + sin t * 4) at the height AreaMap_Elevation(x, z) << 16 -
// read again at the dust's own point for each, not at the rim - projected,
// black; triangle j joins the rim at t = 0x100 j and 0x100 (j + 1); each
// committed to slot 1 (0x34).
extern "C" void __cdecl EffectDust_DrawFan(const unsigned char* dust) {
    alignas(4) unsigned char pt[12], centre[12], rim[12];
    SetUL(pt, ULRest3e(dust + 4));
    SetUL(pt + 4, ULRest3e(dust + 8));
    SetUL(pt + 8, static_cast<U>(static_cast<std::int16_t>(
                      SH_CALL(AreaMap_Elevation)(static_cast<long>(ULRest3e(dust + 4)), static_cast<long>(ULRest3e(dust + 8)))))
                      << 16);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(centre));
    U angle = 0;
    // the rim point at angle t: x, z, then the height
    auto rimAt = [&](int t) {
        const U cos = static_cast<U>(SH_CALL(Math_Cos)(t));
        SetUL(pt, ULRest3e(dust + 4) + (cos << 2));
        const U sin = static_cast<U>(SH_CALL(Math_Sin)(t));
        SetUL(pt + 4, ULRest3e(dust + 8) + (sin << 2));
        SetUL(pt + 8, static_cast<U>(static_cast<std::int16_t>(
                          SH_CALL(AreaMap_Elevation)(static_cast<long>(ULRest3e(dust + 4)), static_cast<long>(ULRest3e(dust + 8)))))
                          << 16);
        SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(rim));
    };
    rimAt(0);
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12Rest3e(p + 8, centre);
        angle += 0x100;
        Copy12Rest3e(p + 0x18, rim);
        rimAt(static_cast<int>(angle & 0xFFFFu));
        Copy12Rest3e(p + 0x28, rim);
        p[4] = p[5] = p[6] = 0x40;
        p[0x14] = p[0x15] = p[0x16] = 0;
        p[0x24] = p[0x25] = p[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// ===========================================================================
// Kinds 0x5D and 0x5E (EffectKind5D_States 0..3, EffectKind5E_States 0..2;
// their dispatchers E2G's). Three windows (+7 bit j hides window j) open,
// show the item +6 and its count and a board of marks, and close; kind 0x5E
// picks one of the eight items 0x4E..0x55 (+2) and adds it to the counts at
// 0x903A10.
// ===========================================================================

// ===========================================================================
// The dock's legend (DIVERGENCE: DIV-0137, docs/button-unification-
// round.md section 3 "IC-D"; docs/rest_3e.md section 11)
//
// The panel EffectKind5E_DrawPanel draws is baked art of the area page (tag
// 0x0A081000, AREA128.DAT): three rows, each a button icon and its word -
// place the part (EffectKind5E_Pick 0x47F610 tests circle 0x20), quit (cross
// 0x40), the memo board (triangle 0x10). Each release bakes its own default
// buttons there: Japan's page (the PC's, the JP disc's, PSP-JP's, the same
// bytes) circle / cross / triangle; the PlayStation West's (US, EU, FR, DE)
// cross / triangle / square, P of Japan's; PSP-EU's cross / circle / square,
// S of Japan's (read on all seven builds' pages, 2026-10-17, section 11).
//
// With DockLegend on, each row shows the button the Config has its test on now:
// Sigma of the row's bit (input/sigma.h; Japan's terms, so the identity on a
// JP-based install at its defaults, P on a West install at the West's, S on
// PSP-EU's). Where that is the page's own icon nothing changes: one quad, the
// original's. Where it is not, that row's icon cell is left out of the panel
// (the panel drawn in pieces, the cell's 11 x 10 texels not drawn) and the
// right icon drawn there instead:
//
//   1. the page's own icon of that button, when another row of the page has
//      it (the disc's shapes, or Capcom's on the PC's own page) - an 11 x 10
//      FT4 of that row's cell;
//   2. else, under a Latin overlay, DIV-0051's icon of the button (the disc's
//      12 x 12 set, loc_build.py's ICONS_AT), as the Config panel draws it;
//   3. else the port's own stand-in, the glyph the Config panel's original
//      0x461C00 draws for the button (0x66A2FC's strings, circled numerals
//      and an X), in the same colour.
//
// The page's icons are read from the VRAM shadow, not assumed from the
// install: a cell is told by a rule on its bright texels (index 8 and up -
// the icons' ink; the words and the shadows are below 8), so a page from any
// source, or a later mod's, says itself what it shows. A page whose cells do
// not read as three of the four shapes is drawn as it is (logged once).
// ===========================================================================

namespace {

namespace dock {

constexpr unsigned kColU = 4, kColW = 11;          // u offsets 4..14 of the panel
constexpr unsigned kBandV = 5, kBandH = 10;        // row r's cell: v offsets 5 + 10 r ..
constexpr unsigned kRows = 3;

// The rows' buttons in Japan's terms: EffectKind5E_Pick's three tests.
constexpr std::uint16_t kRowBit[kRows] = {0x20, 0x40, 0x10};

// The six buttons in DIV-0051's order (config_text.cpp's kIcons), each with
// the colour 0x461C00 draws its stand-in in and the Config panel its icon.
enum Shape : unsigned char { kCircle, kCross, kTriangle, kSquare, kL1, kR1, kUnknown };
constexpr std::uint16_t kShapeBit[6] = {0x20, 0x40, 0x10, 0x80, 0x04, 0x08};

Shape ShapeOfBit(std::uint16_t bit) {
    for (unsigned k = 0; k < 6; ++k)
        if (kShapeBit[k] == bit) return static_cast<Shape>(k);
    return kUnknown;
}

// One cell's ink, row-major: true where the texel's index is 8 or more.
struct Cell {
    bool ink[kBandH][kColW];
};

// The rule (section 11 of docs/rest_3e.md gives it with the pages it was read
// on). The bright texels' bounding box, 5..11 wide and 5..10 high, then its top
// and bottom rows: both full and the centre empty - a square; both with ink at
// the two ends, the centre inked and the top not full - a cross; neither at the ends, each more than half the width -
// a circle; the top a third of the width at most and the bottom full - a
// triangle. Anything else is not one of the four.
Shape Classify(const Cell& c) {
    int x0 = kColW, x1 = -1, y0 = kBandH, y1 = -1;
    for (int r = 0; r < static_cast<int>(kBandH); ++r)
        for (int u = 0; u < static_cast<int>(kColW); ++u)
            if (c.ink[r][u]) {
                if (u < x0) x0 = u;
                if (u > x1) x1 = u;
                if (r < y0) y0 = r;
                if (r > y1) y1 = r;
            }
    if (x1 < 0) return kUnknown;
    const int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if (w < 5 || h < 5) return kUnknown;
    int top = 0, bottom = 0;
    for (int u = x0; u <= x1; ++u) {
        top += c.ink[y0][u];
        bottom += c.ink[y1][u];
    }
    const bool top_ends = c.ink[y0][x0] && c.ink[y0][x1], bottom_ends = c.ink[y1][x0] && c.ink[y1][x1];
    const bool centre = c.ink[(y0 + y1) / 2][(x0 + x1) / 2] || c.ink[(y0 + y1 + 1) / 2][(x0 + x1 + 1) / 2];
    if (top == w && bottom == w && !centre) return kSquare;
    if (top_ends && bottom_ends && centre && top < w) return kCross;
    if (!top_ends && !bottom_ends && 2 * top > w && 2 * bottom > w) return kCircle;
    if (3 * top <= w && bottom == w) return kTriangle;
    return kUnknown;
}

// What a row shows: the page's cell as it is, another row's cell, or a glyph.
enum How : unsigned char { kKeep, kCellOf, kGlyph };
struct Row {
    How how;
    unsigned char from;   // kCellOf: the row whose cell; kGlyph: the Shape
};

// The plan for the three rows: `baked` the page's icons, `wanted` the buttons
// the rows' tests are on now. False when the page's icons are not three of
// the four shapes: then nothing is changed.
bool Plan(const Shape baked[kRows], const std::uint16_t wanted[kRows], Row out[kRows]) {
    for (unsigned r = 0; r < kRows; ++r)
        if (baked[r] == kUnknown) return false;
    for (unsigned r = 0; r < kRows; ++r) {
        const Shape want = ShapeOfBit(wanted[r]);
        if (want == kUnknown)
            bof3::Fatal("DockLegend: row %u's button is 0x%04X, not one of the six Sigma maps (docs/button-sigma.md 2)", r,
                        static_cast<unsigned>(wanted[r]));
        out[r] = {kKeep, 0};
        if (baked[r] == want) continue;
        out[r] = {kGlyph, static_cast<unsigned char>(want)};
        for (unsigned k = 0; k < kRows; ++k)
            if (baked[k] == want) {
                out[r] = {kCellOf, static_cast<unsigned char>(k)};
                break;
            }
    }
    return true;
}

// DIVERGENCE: DIV-0137 - 1 after this module's self-test (PatchBytes
// "DockLegend", BOF3X_ORIGINAL=DockLegend for the page as it is).
}  }  namespace rest_3e::dock { unsigned char g_on = 0; }  namespace { namespace dock {

// The self-test (BOF3X_SHADOW=rest_3e, after the fuzz): the rule on shapes
// drawn here by rule - no page's texels are copied - at every place in the
// cell; then the plan on each release's page order (docs/rest_3e.md section
// 11) with stand-in Configs built by sigma::Build from sigma::DefaultWords: at
// each release's defaults every row kept (the original's one quad), under the
// stand-in swap (Speak <-> Action, View <-> Change) rows 0 and 1 trading cells,
// and over all 720 assignments of the six lines every row showing its test's
// button.
unsigned g_bad = 0;
void Check(bool ok, const char* what, unsigned a = 0, unsigned b = 0) {
    if (ok) return;
    if (g_bad < 20) bof3::Log("shadow    rest_3e DockLegend MISMATCH: %s (%u, %u)", what, a, b);
    ++g_bad;
}

// A 7 x 7 shape by rule: the square's ring, the circle's ring with its four
// corners cut, the cross's two diagonals, the triangle's rows widening by one
// texel each side every second row.
bool Drawn(Shape s, int x, int y) {
    const bool edge = x == 0 || x == 6 || y == 0 || y == 6;
    switch (s) {
    case kSquare: return edge;
    case kCircle: return edge && !((x == 0 || x == 6) && (y == 0 || y == 6));
    case kCross: return x == y || x == 6 - y;
    case kTriangle: return y == 6 || x == 3 - y / 2 || x == 3 + y / 2;
    default: return false;
    }
}

void SelfTest() {
    namespace sg = bof3x::input::sigma;
    g_bad = 0;
    unsigned cells = 0, plans = 0;
    // 1. The rule.
    for (unsigned s = kCircle; s <= kSquare; ++s)
        for (unsigned ox = 0; ox + 7 <= kColW; ++ox)
            for (unsigned oy = 0; oy + 7 <= kBandH; ++oy) {
                Cell c = {};
                for (int y = 0; y < 7; ++y)
                    for (int x = 0; x < 7; ++x) c.ink[oy + y][ox + x] = Drawn(static_cast<Shape>(s), x, y);
                Check(Classify(c) == s, "a drawn shape read as another", s, Classify(c));
                ++cells;
            }
    {
        Cell empty = {}, block = {}, line = {}, small = {};
        for (unsigned y = 0; y < 7; ++y)
            for (unsigned x = 0; x < 7; ++x) block.ink[y][x] = true;
        for (unsigned x = 0; x < kColW; ++x) line.ink[4][x] = true;
        for (unsigned y = 0; y < 3; ++y)
            for (unsigned x = 0; x < 3; ++x) small.ink[y][x] = x != 1 || y != 1;
        Check(Classify(empty) == kUnknown, "an empty cell read as a shape");
        Check(Classify(block) == kUnknown, "a filled block read as a shape", Classify(block));
        Check(Classify(line) == kUnknown, "a line read as a shape", Classify(line));
        Check(Classify(small) == kUnknown, "a 3 x 3 ring read as a shape", Classify(small));
        cells += 4;
    }
    // 2. The plan. Each release's page order: its defaults' buttons on the
    // three rows (M_R of circle, cross, triangle).
    const sg::Release releases[3] = {sg::Release::kJapan, sg::Release::kWest, sg::Release::kPspEu};
    for (const sg::Release rel : releases) {
        const unsigned rn = static_cast<unsigned>(rel);
        Shape page[kRows];
        for (unsigned r = 0; r < kRows; ++r) page[r] = ShapeOfBit(sg::ReleaseMap(rel, kRowBit[r]));
        std::uint16_t words[9];
        sg::DefaultWords(rel, false, words);
        std::string why;
        const sg::Table t = sg::Build(words, why);
        Check(t.valid, "a release's defaults refused", rn);
        std::uint16_t wanted[kRows];
        Row rows[kRows];
        for (unsigned r = 0; r < kRows; ++r) wanted[r] = sg::Apply(t, kRowBit[r]);
        Check(Plan(page, wanted, rows), "a release's page unreadable", rn);
        for (unsigned r = 0; r < kRows; ++r) Check(rows[r].how == kKeep, "a row changed at the release's defaults", rn, r);
        ++plans;
        // The stand-in swap: rows 0 and 1 trade cells, row 2 kept, on all three.
        std::uint16_t swapped[9];
        std::memcpy(swapped, words, sizeof swapped);
        const std::uint16_t l2r2 = static_cast<std::uint16_t>(words[0] & sg::kL2R2);
        swapped[0] = static_cast<std::uint16_t>(words[6] | l2r2);
        swapped[6] = static_cast<std::uint16_t>(words[0] & ~sg::kL2R2);
        std::swap(swapped[3], swapped[4]);
        const sg::Table ts = sg::Build(swapped, why);
        Check(ts.valid, "the stand-in swap refused", rn);
        for (unsigned r = 0; r < kRows; ++r) wanted[r] = sg::Apply(ts, kRowBit[r]);
        Check(Plan(page, wanted, rows), "a release's page unreadable", rn);
        Check(rows[0].how == kCellOf && rows[0].from == 1, "swap: row 0 not row 1's cell", rn, rows[0].how);
        Check(rows[1].how == kCellOf && rows[1].from == 0, "swap: row 1 not row 0's cell", rn, rows[1].how);
        Check(rows[2].how == kKeep, "swap: row 2 changed", rn, rows[2].how);
        ++plans;
        // Every assignment of the six lines to the six buttons: each row shows
        // its test's button - kept only where the page has it there, a cell
        // only where the page has it in another row, a glyph only where the
        // page has it nowhere.
        unsigned perm[6] = {0, 1, 2, 3, 4, 5};
        const unsigned char line_word[6] = {0, 1, 6, 2, 3, 4};
        do {
            std::uint16_t w[9];
            std::memcpy(w, words, sizeof w);
            for (unsigned l = 0; l < 6; ++l) w[line_word[l]] = kShapeBit[perm[l]];
            const sg::Table tp = sg::Build(w, why);
            Check(tp.valid, "a permutation refused", perm[0], perm[1]);
            for (unsigned r = 0; r < kRows; ++r) wanted[r] = sg::Apply(tp, kRowBit[r]);
            Check(Plan(page, wanted, rows), "a release's page unreadable", rn);
            for (unsigned r = 0; r < kRows; ++r) {
                const Shape want = ShapeOfBit(wanted[r]);
                bool on_page = false;
                for (unsigned k = 0; k < kRows; ++k) on_page = on_page || page[k] == want;
                const bool ok = rows[r].how == kKeep     ? page[r] == want
                                : rows[r].how == kCellOf ? page[r] != want && page[rows[r].from] == want
                                                         : !on_page && rows[r].from == want;
                Check(ok, "a row not showing its button", r, want);
            }
            ++plans;
        } while (std::next_permutation(perm, perm + 6));
    }
    {
        const Shape page[kRows] = {kCircle, kUnknown, kTriangle};
        const std::uint16_t wanted[kRows] = {0x40, 0x20, 0x10};
        Row rows[kRows];
        Check(!Plan(page, wanted, rows), "an unreadable page planned");
        ++plans;
    }
    bof3::Log("shadow    rest_3e DockLegend self-test: %u cells read by the rule, %u plans (3 page orders at their "
              "defaults, the stand-in swap and 720 assignments each, an unreadable page); %u MISMATCHES",
              cells, plans, g_bad);
    if (g_bad) bof3::Fatal("rest_3e DockLegend: %u mismatches against docs/rest_3e.md section 11", g_bad);
    if (cells != 4 * 5 * 4 + 4 || plans != 3 * (2 + 720) + 1)
        bof3::Fatal("rest_3e DockLegend self-test: %u cells, %u plans (84, 2167 wanted)", cells, plans);
}

}  // namespace dock

}  // namespace

// ===========================================================================
// Kind 0x5F's states 1..9 (EffectKind5F_States; state 0 is Task_StartHold60).
// The record's point +0x34 / +0x38 / +0x3C, its velocity +0xC / +0x10 / +0x14.
// ===========================================================================

void Rest3E_Inject() {
    if (bof3::WantsShadow("rest_3e")) {
        rest_3e::SelfTest();
        dock::SelfTest();
    }
    BOF3_INJECT(EffectGlowSparks_Clear);
    BOF3_INJECT(EffectGlowSparks_StartRise);
    BOF3_INJECT(EffectGlowSparks_StartBurst);
    BOF3_INJECT(EffectGlowSparks_Run);
    BOF3_INJECT(EffectGlowSparks_Draw);
    BOF3_INJECT(EffectGlowSparks_Glow);
    BOF3_INJECT(EffectGlowSparks_Rise);
    BOF3_INJECT(EffectGlowTrail_Update);
    BOF3_INJECT(EffectGlowTrail_Draw);
    BOF3_INJECT(EffectSpiral_Init);
    BOF3_INJECT(EffectSpiral_StepDraw);
    BOF3_INJECT(EffectRing_Draw);
    BOF3_INJECT(EffectDust_Clear);
    BOF3_INJECT(EffectDust_FindFree);
    BOF3_INJECT(EffectDust_Start);
    BOF3_INJECT(EffectDust_Run);
    BOF3_INJECT(EffectDust_DrawColumn);
    BOF3_INJECT(EffectDust_DrawFan);
    // DIVERGENCE: DIV-0137, the dock's legend from the Config (after the
    // fuzz above, which compares Capcom's one quad).
    static const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("DockLegend", Low32(&rest_3e::dock::g_on), &was, &is, 1);
    bof3::Log("DockLegend the dock's legend shows the buttons the Config has its three tests on: %s",
              rest_3e::dock::g_on ? "on" : "OFF (BOF3X_ORIGINAL=DockLegend)");
}
