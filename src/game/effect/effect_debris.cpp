// Moved here by tools/move_funcs.py from effect_3c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_debris.h"

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

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul of two whole registers: the low 32 bits.
U Mul(U a, std::int32_t b) { return a * static_cast<U>(b); }

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32 (the original's 0x20-byte local).
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};

}  // namespace

// original 0x485030 (a 0x2C-byte debris record; kinds 0x1E, 0x1F, 0x4B and 0x72
// draw with it): a POLY_G3 at the cursor (read once), semi-transparent; its
// first corner the record's point (+0..+0xB) projected; the other two the
// edges +0x10 / +0x18 (three s16 each) turned in x / z by the angle +0x24
// (Math_Cos / Math_Sin, >> 8) and scaled by +0x28, the height (+0x14 / +0x1C)
// << 12 times the scale, added to the point and projected. Colour (v, v, v >>
// 1) at the first corner, v the shade +0x2A clamped to 0..0xFF; black at the
// others. Gfx_CommitPrim(1, 0x34).
extern "C" void __cdecl EffectDebris_Draw(unsigned char* d) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG3)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(d), reinterpret_cast<float*>(prim + 8));
    for (U edge = 0x10, out = 0x18; edge <= 0x18; edge += 8, out += 0x10) {
        const U c1 = static_cast<U>(SH_CALL(Math_Cos)(SW(d + 0x24)));
        U e = Mul(c1, SW(d + edge));
        const U s1 = static_cast<U>(SH_CALL(Math_Sin)(SW(d + 0x24)));
        const std::int32_t scale = SW(d + 0x28);
        e = Mul(Sar(e - Mul(s1, SW(d + edge + 2)), 8), scale);
        const U s2 = static_cast<U>(SH_CALL(Math_Sin)(SW(d + 0x24)));
        U f = Mul(s2, SW(d + edge));
        const U c2 = static_cast<U>(SH_CALL(Math_Cos)(SW(d + 0x24)));
        f += Mul(c2, SW(d + edge + 2));
        long p[3];
        p[0] = static_cast<long>(e + UL(d + 0));
        p[2] = static_cast<long>(Mul(static_cast<U>(SW(d + edge + 4)) << 12, scale) + UL(d + 8));
        p[1] = static_cast<long>(Mul(Sar(f, 8), scale) + UL(d + 4));
        SH_CALL(EffectGte_ProjectPoint)(p, reinterpret_cast<float*>(prim + out));
    }
    const std::int32_t shade = SW(d + 0x2A);
    const unsigned char v = shade < 0 ? 0 : shade > 0xFF ? 0xFF : d[0x2A];
    prim[4] = v;
    prim[5] = v;
    prim[6] = static_cast<unsigned char>(v >> 1);
    prim[0x14] = prim[0x15] = prim[0x16] = 0;
    prim[0x24] = prim[0x25] = prim[0x26] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x34);
}

// original 0x4851E0 (a 0x2C-byte debris record; kinds 0x1F and 0x72 set theirs
// up with it - EffectKind1E_DebrisInitOne's twin with 0x20 and 8 + Rand % 8):
// its point +0 / +4 / +8 Sprite_Current's +0x34 / +0x38 / +0x3C; three angles
// Rand & 0xFFF, Rand & 0x3FF, Rand & 0xFFF; two edges (Math_Cos, Math_Sin of
// 0x20; of -0x20; z 0) at +0x10 and +0x18 turned in place by a matrix of the
// three angles (EffectGte_SetDiagonalOne, Gte_RotMatrixX, _Y by the second
// negated, _Z; 0x5A7C70 for each edge - no push or pop of the GTE's matrix,
// unlike kind 0x1E's); the scale +0x28 8 + Rand & 7; +0x20, +0x22, +0x24 (its
// angle), +0x2A (its shade) 0.
extern "C" void __cdecl EffectDebris_InitOne(unsigned char* d) {
    SetUL(d + 0, UL(S() + 0x34));
    SetUL(d + 4, UL(S() + 0x38));
    SetUL(d + 8, UL(S() + 0x3C));
    const U ax = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U ay = static_cast<U>(SH_CALL(Rand)()) & 0x3FFu;
    const U az = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    SetWord(d + 0x10, static_cast<U>(SH_CALL(Math_Cos)(0x20)));
    SetWord(d + 0x12, static_cast<U>(SH_CALL(Math_Sin)(0x20)));
    SetWord(d + 0x14, 0);
    SetWord(d + 0x18, static_cast<U>(SH_CALL(Math_Cos)(-0x20)));
    SetWord(d + 0x1A, static_cast<U>(SH_CALL(Math_Sin)(-0x20)));
    Matrix m;
    SetWord(d + 0x1C, 0);
    SH_CALL(EffectGte_SetDiagonalOne)(m.m);
    SH_CALL(Gte_RotMatrixX)(static_cast<short>(ax), m.m);
    SH_CALL(Gte_RotMatrixY)(-static_cast<int>(static_cast<short>(ay)), m.m);
    SH_CALL(Gte_RotMatrixZ)(static_cast<short>(az), m.m);
    using Turn = void (__cdecl*)(const short*, const short*, short*);
    SH_AT(Turn, at::kMatrixVector.v)(m.m, reinterpret_cast<const short*>(d + 0x10), reinterpret_cast<short*>(d + 0x10));
    SH_AT(Turn, at::kMatrixVector.v)(m.m, reinterpret_cast<const short*>(d + 0x18), reinterpret_cast<short*>(d + 0x18));
    SetWord(d + 0x28, (static_cast<U>(SH_CALL(Rand)()) & 7u) + 8u);
    SetWord(d + 0x20, 0);
    SetWord(d + 0x22, 0);
    SetWord(d + 0x24, 0);
    SetWord(d + 0x2A, 0);
}

void EffectDebris_Inject() {
    BOF3_INJECT(EffectDebris_Draw);
    BOF3_INJECT(EffectDebris_InitOne);
}
