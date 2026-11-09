// Moved here by tools/move_funcs.py from effect_2c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_44.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2c.h"
#include "game/effect_2c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2c::at;

}  // namespace

namespace {

using U = std::uint32_t;

using LD = long double;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U ULEffect2c(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect2c(U a) { return ULEffect2c(At(a)); }

void SetULEffect2c(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetULEffect2c(U a, U v) { SetULEffect2c(At(a), v); }

std::int32_t SWEffect2c(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

std::int32_t SWEffect2c(U a) { return SWEffect2c(At(a)); }

U AddressOfEffect2c(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul r32, r32 then sar: the product's low 32 bits, shifted.
U MulSarEffect2c(std::int32_t a, std::int32_t b, unsigned n) { return Sar(static_cast<U>(a) * static_cast<U>(b), n); }

// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetULEffect2c(to + i, ULEffect2c(from + i));
}

// --- x87 as the original has it: `fld dword` through inline assembly, so the
// optimizer cannot narrow an operation of two floats to SSE (whose NaN rule
// differs); every operation in long double on the x87 at the game's control
// word, `fstp dword` on the way out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}

LD FAt(U a) { return F(At(a)); }

LD I(std::int32_t v) { return static_cast<LD>(v); }

void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// The CRT's _ftol 0x5B9550: truncation to 64 bits, the low dword; NaN and
// values out of range give the integer indefinite 0x8000000000000000, whose
// low dword is 0.
U Ftol(LD v) {
    if (!(v > -9223372036854775808.0L && v < 9223372036854775808.0L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + k]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, const void* table, unsigned entries, unsigned state) {
    if (state >= entries)
        bof3::Fatal("%s: state byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2c.md section 6)",
                    who, state, entries, (unsigned)AddressOfEffect2c(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(ULEffect2c(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// The draw-mode packet every draw here opens with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawMode(U abr, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// +9 down one (Sprite_Current read for each access, as the originals); at 0,
// +9 = reload and +1 up.
void CountDown(unsigned reload) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) return;
    s[9] = static_cast<unsigned char>(reload);
    s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }

const long* Point(const unsigned char* p) { return reinterpret_cast<const long*>(p); }

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

unsigned char* Ring() { return EffectKind44_RingCell; }

}  // namespace

// original 0x476680 (in the span of 0x4762D0, in no list of the cut; catalog
// part 2): jmp [EffectKind44_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind44_Run(void) {
    Dispatch("EffectKind44_Run", EffectKind44_States, EffectKind44_States_count, S()[1]);
}

// original 0x4766A0 (state 0): the record's point Sprite_Objects record 1's;
// the ring cell 0x6761C0 = 0x92BF80, whose top point is the record's point
// 0x800 up (+8 + 0x8000000) and centre the record's point, radius 0, shades
// 0x20 and 0; the sparks cleared (EffectKind44_SparksClear); +9 = 0x10, +1 up.
extern "C" void __cdecl EffectKind44_Start(void) {
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(Sprite_Objects[1].x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(Sprite_Objects[1].z));
    rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<std::uint32_t>(Sprite_Objects[1].height));
    const unsigned char* const s = S();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    SetULEffect2c(::bof3::Img(at::kRingCell.v), ::bof3::Img(EffectKind30_Shards_addr));
    SetULEffect2c(::bof3::Img(at::kShards.v), static_cast<std::uint32_t>(so->x));
    SetULEffect2c(::bof3::Img(at::kShards.v) + 4, static_cast<std::uint32_t>(so->z));
    SetULEffect2c(::bof3::Img(at::kShards.v) + 8, static_cast<std::uint32_t>(so->height) + 0x8000000u);
    SetULEffect2c(::bof3::Img(at::kShards.v) + 0x10, static_cast<std::uint32_t>(so->x));
    SetULEffect2c(::bof3::Img(at::kShards.v) + 0x14, static_cast<std::uint32_t>(so->z));
    SetULEffect2c(::bof3::Img(at::kShards.v) + 0x18, static_cast<std::uint32_t>(so->height));
    EffectKind30_Shards[0x22] = 0x20;
    EffectKind30_Shards[0x23] = 0;
    SetWord(At(::bof3::Img(EffectKind30_Shards_addr) + 0x20), 0);
    SH_CALL(EffectKind44_SparksClear)();
    S()[9] = 0x10;
    NextState();
}

// original 0x476750 (state 1): the ring's shade +0x23 up 3; projected and
// drawn; +9 down, at 0 the sparks cleared, +9 = 0x78 and +1 up.
extern "C" void __cdecl EffectKind44_FadeIn(void) {
    Ring()[0x23] = static_cast<unsigned char>(Ring()[0x23] + 3);
    SH_CALL(EffectKind44_RingProject)(Ring());
    SH_CALL(EffectKind44_RingDraw)(Ring());
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] != 0) return;
    SH_CALL(EffectKind44_SparksClear)();
    S()[9] = 0x78;
    NextState();
}

// original 0x4767B0 (state 2): the ring projected and drawn, the sparks
// stepped and one emitted (EffectKind44_SparkEmit); +9 down, at 0 +9 = 0x1E,
// +1 up.
extern "C" void __cdecl EffectKind44_Sparks(void) {
    SH_CALL(EffectKind44_RingProject)(Ring());
    SH_CALL(EffectKind44_RingDraw)(Ring());
    SH_CALL(EffectKind44_SparksStep)();
    SH_CALL(EffectKind44_SparkEmit)();
    CountDown(0x1E);
}

// original 0x476800 (state 3): the ring's radius word +0x20 up 0x30, at most
// 0x180 (signed); projected, drawn, its cone drawn (EffectKind44_RingCone), the
// sparks stepped; +9 down, at 0 +9 = 0x78, +1 up.
extern "C" void __cdecl EffectKind44_Widen(void) {
    SetWord(Ring() + 0x20, Word(Ring() + 0x20) + 0x30u);
    unsigned char* ring = Ring();
    if (SWEffect2c(ring + 0x20) > 0x180) {
        SetWord(ring + 0x20, 0x180);
        ring = Ring();
    }
    SH_CALL(EffectKind44_RingProject)(ring);
    SH_CALL(EffectKind44_RingDraw)(Ring());
    SH_CALL(EffectKind44_RingCone)(Ring());
    SH_CALL(EffectKind44_SparksStep)();
    CountDown(0x78);
}

// original 0x476870 (state 4): the ring's centre Sprite_Objects record 1's
// point; projected, drawn, its cone, the sparks; +9 down, at 0 +9 = 0x10, +1 up.
extern "C" void __cdecl EffectKind44_Follow(void) {
    SetULEffect2c(Ring() + 0x10, static_cast<std::uint32_t>(Sprite_Objects[1].x));
    SetULEffect2c(Ring() + 0x14, static_cast<std::uint32_t>(Sprite_Objects[1].z));
    SetULEffect2c(Ring() + 0x18, static_cast<std::uint32_t>(Sprite_Objects[1].height));
    SH_CALL(EffectKind44_RingProject)(Ring());
    SH_CALL(EffectKind44_RingDraw)(Ring());
    SH_CALL(EffectKind44_RingCone)(Ring());
    SH_CALL(EffectKind44_SparksStep)();
    CountDown(0x10);
}

// original 0x4768F0 (state 5): the ring's shade +0x23 down 3; projected,
// drawn, its cone, the sparks; +9 down, at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKind44_FadeOut(void) {
    Ring()[0x23] = static_cast<unsigned char>(Ring()[0x23] - 3);
    SH_CALL(EffectKind44_RingProject)(Ring());
    SH_CALL(EffectKind44_RingDraw)(Ring());
    SH_CALL(EffectKind44_RingCone)(Ring());
    SH_CALL(EffectKind44_SparksStep)();
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x476950 (0xA2 bytes): EffectGte_LoadMapCamera; the top point
// projected to +0x24, the centre to +0x30; sixteen points of the circle of
// radius +0x20 (s16) round the centre at the angles 0, 0x100, .. ((cos, sin) *
// radius >> 4) projected to +0x3C + 0xC i.
extern "C" void __cdecl EffectKind44_RingProject(unsigned char* ring) {
    SH_CALL(EffectGte_LoadMapCamera)();
    SH_CALL(EffectGte_ProjectPoint)(Point(ring), Out(ring + 0x24));
    SH_CALL(EffectGte_ProjectPoint)(Point(ring + 0x10), Out(ring + 0x30));
    U angle = 0;
    unsigned char* out = ring + 0x3C;
    for (unsigned n = 0x10; n != 0; --n) {
        const U t = angle & 0xFFFFu;
        alignas(4) unsigned char q[12];
        U v = MulSarEffect2c(SH_CALL(Math_Cos)(static_cast<int>(t)), SWEffect2c(ring + 0x20), 4);
        SetULEffect2c(q, v + ULEffect2c(ring + 0x10));
        v = MulSarEffect2c(SH_CALL(Math_Sin)(static_cast<int>(t)), SWEffect2c(ring + 0x20), 4);
        SetULEffect2c(q + 4, v + ULEffect2c(ring + 0x14));
        SetULEffect2c(q + 8, ULEffect2c(ring + 0x18));
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(out));
        out += 0xC;
        angle += 0x100;
    }
}

// original 0x476A00 (0x1F2 bytes): a draw mode (abr 2, slot 1); for each
// ring point i and the next j, a semi-transparent POLY_F4 of the edge i-j and
// its run to the screen's side (x 320.0 when the edge's middle is right of the
// centre's projection, else 0), shaded +0x23, committed 0x38 - the lowest and
// highest screen y met kept (_ftol, from 0 and 0xF0); then two TILEs of
// 320 across, above the highest (from 0) and below the lowest (to 0xF0), the
// same shade, committed 0x1C each (their +0x10 not written). DIV-0041: the
// runs go to the wide frame's sides (Widescreen_FillX() or 320 + the
// columns) and the two tiles run from Widescreen_FillX() over
// Widescreen_FillWidth(), as the PSP's -32 / 352 / 384; the original's 0 /
// 320 while the fills are unarmed, as the fuzz compares.
extern "C" void __cdecl EffectKind44_RingDraw(const unsigned char* ring) {
    DrawMode(2, 1, 1);
    const U left = std::bit_cast<U>(Widescreen_FillX());
    const U right = std::bit_cast<U>(Widescreen_FillRight(320.0f));
    const U width = std::bit_cast<U>(Widescreen_FillWidth());
    U lowest = 0;
    U highest = 0xF0;
    const unsigned char* pi = ring + 0x3C;
    unsigned char i = 0;
    do {
        const unsigned j = (i + 1u) & 0xFu;
        if (!(I(static_cast<std::int16_t>(lowest)) >= F(pi + 4))) lowest = Ftol(F(pi + 4));
        if (I(static_cast<std::int16_t>(highest)) > F(pi + 4)) highest = Ftol(F(pi + 4));
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyF4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        const unsigned char* const pj = ring + 0x3C + 12u * j;
        SetULEffect2c(prim + 8, ULEffect2c(pi));
        LD y = F(pi + 4);
        StF(prim + 0x18, y);
        StF(prim + 0xC, y);
        SetULEffect2c(prim + 0x20, ULEffect2c(pj));
        y = F(pj + 4);
        StF(prim + 0x30, y);
        StF(prim + 0x24, y);
        const LD middle = (F(pi) + F(pj)) * FAt(at::kHalf);
        const U side = middle > F(ring + 0x30) ? right : left;   // 320.0 or 0 unarmed
        SetULEffect2c(prim + 0x2C, side);
        SetULEffect2c(prim + 0x14, side);
        LD z = F(pi + 8);
        StF(prim + 0x1C, z);
        StF(prim + 0x10, z);
        z = F(pj + 8);
        StF(prim + 0x34, z);
        StF(prim + 0x28, z);
        prim[6] = prim[5] = prim[4] = ring[0x23];
        SH_CALL(Gfx_CommitPrim)(1, 0x38);
        ++i;
        pi += 0xC;
    } while (i < 0x10);
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SetULEffect2c(prim + 8, left);
    SetULEffect2c(prim + 0xC, 0);
    SetULEffect2c(prim + 0x14, width);
    StF(prim + 0x18, I(static_cast<std::int16_t>(highest)));
    prim[6] = prim[5] = prim[4] = ring[0x23];
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
    prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const std::int32_t below = static_cast<std::int16_t>(lowest);
    SetULEffect2c(prim + 8, left);
    SetULEffect2c(prim + 0x14, width);
    StF(prim + 0xC, I(below));
    StF(prim + 0x18, I(static_cast<std::int32_t>(0xF0u - static_cast<U>(below))));
    prim[6] = prim[5] = prim[4] = ring[0x23];
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x476C00 (0xFE bytes): a draw mode (abr 1, slot 1); for each ring
// point i and the next j, the triangle top-i-j where it faces the eye (0x4941B0's
// ax above 0): a semi-transparent POLY_F3 (0x5A7570) shaded +0x22, committed 0x2C.
extern "C" void __cdecl EffectKind44_RingCone(const unsigned char* ring) {
    DrawMode(1, 1, 1);
    const unsigned char* pi = ring + 0x3C;
    unsigned char step = 0;
    do {
        ++step;
        const unsigned char* const pj = ring + 0x3C + 12u * (step & 0xFu);
        const U facing = SH_AT(U (__cdecl*)(const unsigned char*, const unsigned char*, const unsigned char*),
                               at::kWinding.v)(ring + 0x24, pi, pj);
        if (static_cast<std::int16_t>(facing) > 0) {
            unsigned char* const prim = Gfx_PacketNext;
            SH_AT(void (__cdecl*)(unsigned char*), at::kSetPolyF3.v)(prim);
            SH_CALL(Gpu_SetSemiTrans)(prim, 1);
            Copy12(prim + 8, ring + 0x24);
            Copy12(prim + 0x14, pi);
            Copy12(prim + 0x20, pj);
            prim[6] = prim[5] = prim[4] = ring[0x22];
            SH_CALL(Gfx_CommitPrim)(1, 0x2C);
        }
        pi += 0xC;
    } while (step < 0x10);
}

// original 0x476D00 (0x1C bytes): the spark cursor 0x6761B8 = 0x92C07C, then
// eight times the in-use byte it points at cleared - the cursor never moves,
// so only the first of the eight records is (docs/effect_2c.md section 7).
extern "C" void __cdecl EffectKind44_SparksClear(void) {
    SetULEffect2c(::bof3::Img(at::kSparkCursor.v), ::bof3::Img(EffectKind30_Shards_addr) + 0xfc);
    for (unsigned n = at::kSparkCount44; n != 0; --n) EffectKind44_SparkCursor[0] = 0;
}

// original 0x476D20 (0x90 bytes): each live spark of the eight (the cursor on
// it): its ground point +0x28 / +0x2C moved by +8 / +0xC, its height +0x30 the
// ground's (AreaMap_Elevation's s16 << 16); its trail drawn
// (EffectKind44_SparkTrail); its life +1 down, at 0 out of use.
extern "C" void __cdecl EffectKind44_SparksStep(void) {
    U p = ::bof3::Img(EffectKind30_Shards_addr) + 0xfc;
    SetULEffect2c(::bof3::Img(at::kSparkCursor.v), p);
    for (unsigned n = at::kSparkCount44; n != 0; --n) {
        if (At(p)[0] != 0) {
            SetULEffect2c(At(p) + 0x28, ULEffect2c(At(p) + 0x28) + ULEffect2c(At(p) + 8));
            unsigned char* q = EffectKind44_SparkCursor;
            SetULEffect2c(q + 0x2C, ULEffect2c(q + 0x2C) + ULEffect2c(q + 0xC));
            q = EffectKind44_SparkCursor;
            const long ground = SH_CALL(AreaMap_Elevation)(Long(q + 0x28), Long(q + 0x2C));
            SetULEffect2c(EffectKind44_SparkCursor + 0x30, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16);
            SH_CALL(EffectKind44_SparkTrail)(EffectKind44_SparkCursor);
            q = EffectKind44_SparkCursor;
            q[1] = static_cast<unsigned char>(q[1] - 1);
            q = EffectKind44_SparkCursor;
            if (q[1] == 0) q[0] = 0;
            p = ULEffect2c(::bof3::Img(at::kSparkCursor.v));
        }
        p += at::kSparkStride44;
        SetULEffect2c(::bof3::Img(at::kSparkCursor.v), p);
    }
}

// original 0x476DB0 (0x206 bytes): a draw mode (abr 1, slot 1);
// EffectGte_LoadMapCamera; the spark's two histories of four screen points
// (+0x40 the high end's, +0x70 the ground end's) shifted one on and the newest
// projected (+0x18, +0x28); a semi-transparent LINE_F2 between the newest two
// in the spark's colour +4, committed 0x20; three semi-transparent POLY_G4
// between the histories' successive pairs, the colour down a quarter of itself
// (bytes, wrapping) at each step, committed 0x44 each.
extern "C" void __cdecl EffectKind44_SparkTrail(unsigned char* spark) {
    DrawMode(1, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned k = 3; k != 0; --k) {
        Copy12(spark + 0x40 + 12 * k, spark + 0x40 + 12 * (k - 1));
        Copy12(spark + 0x70 + 12 * k, spark + 0x70 + 12 * (k - 1));
    }
    SH_CALL(EffectGte_ProjectPoint)(Point(spark + 0x18), Out(spark + 0x40));
    SH_CALL(EffectGte_ProjectPoint)(Point(spark + 0x28), Out(spark + 0x70));
    const U colour = ULEffect2c(spark + 4);
    auto red = static_cast<unsigned char>(colour);
    auto green = static_cast<unsigned char>(colour >> 8);
    auto blue = static_cast<unsigned char>(colour >> 16);
    const auto dr = static_cast<unsigned char>(red >> 2);
    const auto dg = static_cast<unsigned char>(green >> 2);
    const auto db = static_cast<unsigned char>(blue >> 2);
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    Copy12(prim + 8, spark + 0x40);
    Copy12(prim + 0x14, spark + 0x70);
    prim[4] = red;
    prim[5] = green;
    prim[6] = blue;
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
    const unsigned char* a = spark + 0x40;
    const unsigned char* b = spark + 0x70;
    for (unsigned n = 3; n != 0; --n) {
        prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        Copy12(prim + 8, a);
        Copy12(prim + 0x18, b);
        prim[4] = red;
        prim[0x14] = red;
        red = static_cast<unsigned char>(red - dr);
        prim[5] = green;
        prim[0x15] = green;
        green = static_cast<unsigned char>(green - dg);
        prim[6] = blue;
        prim[0x16] = blue;
        blue = static_cast<unsigned char>(blue - db);
        Copy12(prim + 0x28, a + 0xC);
        Copy12(prim + 0x38, b + 0xC);
        prim[0x24] = red;
        prim[0x25] = green;
        prim[0x26] = blue;
        prim[0x34] = red;
        prim[0x35] = green;
        prim[0x36] = blue;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        a += 0xC;
        b += 0xC;
    }
}

// original 0x476FC0 (0x1B5 bytes): a free spark (EffectKind44_SparkFind, the
// cursor its answer); on every fourth frame (Frame_Counter & 3 = 0) and a
// record: its high end the record's point 0x600 up (+0x20 + 0x6000000), its
// ground end at the angle e = Rand & 0xFFF ((cos, sin) << 8 from the record's
// point) on the ground, its speed (cos, sin)(e + 0x800 + (Rand & 0x1FF) -
// 0x100) << 4, +0x10 = 0, its colour EffectKind44_SparkColours[Rand & 7], in
// use, life 0x20; EffectGte_LoadMapCamera; both ends projected into the first
// history points and copied to the other three.
extern "C" void __cdecl EffectKind44_SparkEmit(void) {
    unsigned char* const found = SH_CALL(EffectKind44_SparkFind)();
    const unsigned frame = Frame_Counter & 0xFFu;
    SetULEffect2c(::bof3::Img(at::kSparkCursor.v), AddressOfEffect2c(found));
    if ((frame & 3u) != 0 || found == nullptr) return;
    SetULEffect2c(found + 0x18, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    SetULEffect2c(EffectKind44_SparkCursor + 0x1C, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    SetULEffect2c(EffectKind44_SparkCursor + 0x20, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height) + 0x6000000u);
    U e = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    U spread = static_cast<U>(SH_CALL(Rand)()) & 0x1FFu;
    e &= 0xFFFFu;
    spread -= 0x100u;
    U v = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(e))) << 8;
    SetULEffect2c(EffectKind44_SparkCursor + 0x28, v + static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    v = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(e))) << 8;
    SetULEffect2c(EffectKind44_SparkCursor + 0x2C, v + static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    const unsigned char* q = EffectKind44_SparkCursor;
    const long ground = SH_CALL(AreaMap_Elevation)(Long(q + 0x28), Long(q + 0x2C));
    const U angle = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(spread))) + e + 0x800u;
    SetULEffect2c(EffectKind44_SparkCursor + 0x30, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16);
    v = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))) << 4;
    SetULEffect2c(EffectKind44_SparkCursor + 8, v);
    v = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))) << 4;
    SetULEffect2c(EffectKind44_SparkCursor + 0xC, v);
    SetULEffect2c(EffectKind44_SparkCursor + 0x10, 0);
    const U pick = static_cast<U>(SH_CALL(Rand)()) & 7u;
    SetULEffect2c(EffectKind44_SparkCursor + 4, ULEffect2c(::bof3::Img(at::kSparkColours.v) + 4 * pick));
    EffectKind44_SparkCursor[0] = 1;
    EffectKind44_SparkCursor[1] = 0x20;
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* sp = EffectKind44_SparkCursor;
    SH_CALL(EffectGte_ProjectPoint)(Point(sp + 0x18), Out(sp + 0x40));
    sp = EffectKind44_SparkCursor;
    SH_CALL(EffectGte_ProjectPoint)(Point(sp + 0x28), Out(sp + 0x70));
    for (unsigned k = 1; k <= 3; ++k) {
        sp = EffectKind44_SparkCursor;
        Copy12(sp + 0x40 + 12 * k, sp + 0x40);
        sp = EffectKind44_SparkCursor;
        Copy12(sp + 0x70 + 12 * k, sp + 0x70);
    }
}

// original 0x477180 (0x25 bytes): the first spark record of the eight not in
// use, the cursor 0x6761B8 left on it (past the last, and 0 answered, when none).
extern "C" unsigned char* __cdecl EffectKind44_SparkFind(void) {
    U p = ::bof3::Img(EffectKind30_Shards_addr) + 0xfc;
    SetULEffect2c(::bof3::Img(at::kSparkCursor.v), p);
    for (unsigned char n = 0;;) {
        if (At(p)[0] == 0) return At(p);
        p += at::kSparkStride44;
        ++n;
        SetULEffect2c(::bof3::Img(at::kSparkCursor.v), p);
        if (n >= at::kSparkCount44) return nullptr;
    }
}

void EffectKind44_Inject() {
    BOF3_INJECT(EffectKind44_Run);
    BOF3_INJECT(EffectKind44_Start);
    BOF3_INJECT(EffectKind44_FadeIn);
    BOF3_INJECT(EffectKind44_Sparks);
    BOF3_INJECT(EffectKind44_Widen);
    BOF3_INJECT(EffectKind44_Follow);
    BOF3_INJECT(EffectKind44_FadeOut);
    BOF3_INJECT(EffectKind44_RingProject);
    BOF3_INJECT(EffectKind44_RingDraw);
    BOF3_INJECT(EffectKind44_RingCone);
    BOF3_INJECT(EffectKind44_SparksClear);
    BOF3_INJECT(EffectKind44_SparksStep);
    BOF3_INJECT(EffectKind44_SparkTrail);
    BOF3_INJECT(EffectKind44_SparkEmit);
    BOF3_INJECT(EffectKind44_SparkFind);
}
