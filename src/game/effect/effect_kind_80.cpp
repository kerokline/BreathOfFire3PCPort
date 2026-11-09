// Moved here by tools/move_funcs.py from effect_3d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_80.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_3d.h"
#include "game/effect_3d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3d::at;

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

U UL(U a) { return UL(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U W(const unsigned char* p) { return Word(p); }

U W(U a) { return Word(At(a)); }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

void Copy4(unsigned char* to, const unsigned char* from) { std::memcpy(to, from, 4); }   // mov reg, [from]; mov [to], reg

// fild dword v; fadd dword [f]; fstp dword [to]
void FildAdd(unsigned char* to, std::int32_t v, const unsigned char* f) {
    __asm__ volatile("fildl %1\n\tfadds %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(to))
                     : "m"(v), "m"(*reinterpret_cast<const float*>(f)));
}

// fld dword [from]; fst dword [a]; fstp dword [b]
void FldTwo(unsigned char* a, unsigned char* b, const unsigned char* from) {
    __asm__ volatile("flds %2\n\tfsts %0\n\tfstps %1"
                     : "=m"(*reinterpret_cast<float*>(a)), "=m"(*reinterpret_cast<float*>(b))
                     : "m"(*reinterpret_cast<const float*>(from)));
}

// fld dword [a]; fsub dword [b]; call _ftol (0x5B9550: the control word's
// rounding set to chop, fistp qword, restored) - the low dword of the answer.
U FldSubFtol(const unsigned char* a, const unsigned char* b) {
    std::int64_t r;
    std::uint16_t cw, chop;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    chop = static_cast<std::uint16_t>(cw | 0x0C00u);
    __asm__ volatile("fldcw %3\n\tflds %1\n\tfsubs %2\n\tfistpll %0\n\tfldcw %4"
                     : "=m"(r)
                     : "m"(*reinterpret_cast<const float*>(a)), "m"(*reinterpret_cast<const float*>(b)), "m"(chop),
                       "m"(cw));
    return static_cast<U>(static_cast<std::uint64_t>(r));
}

// fld dword [a]; fcomp dword [b]; fnstsw ax; test ah, 0x40: C3, set when the two
// are equal or unordered (a NaN).
bool FcompSame(const unsigned char* a, const unsigned char* b) {
    std::uint16_t sw;
    __asm__ volatile("flds %1\n\tfcomps %2\n\tfnstsw %0"
                     : "=a"(sw)
                     : "m"(*reinterpret_cast<const float*>(a)), "m"(*reinterpret_cast<const float*>(b)));
    return (sw & 0x4000u) != 0;
}

// fild dword v; fstp dword: a float argument (whole numbers only, exact).
float FildValue(std::int32_t v) {
    float f;
    __asm__ volatile("fildl %1\n\tfstps %0" : "=m"(f) : "m"(v));
    return f;
}

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, data or the next kind's states.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3d.md section 7)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

void NextState() { S()[1] = static_cast<unsigned char>(S()[1] + 1); }

// +9 down one; true when it reaches 0 (Sprite_Current read for each access).
bool CountDown() {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    return S()[9] == 0;
}

}  // namespace

// original 0x487270 (Effect_KindHandlers[0x80], hidden in 0x486FC0): jmp
// [EffectKind80_States + +1 * 4].
extern "C" void __cdecl EffectKind80_Run(void) {
    Dispatch("EffectKind80_Run", AddressOf(EffectKind80_States), EffectKind80_States_count, 1);
}

// original 0x487290 (state 0): the point (1, 0x25, 0x38 cells: 0x10000,
// 0x250000, 0x3800000), the trail's size 0x60, the trail stepped 32 times (all
// its points at the point); +9 0, +1 up.
extern "C" void __cdecl EffectKind80_Start(void) {
    rv::Current<records::EffectObject>()->x = 0x10000;
    rv::Current<records::EffectObject>()->z = 0x250000;
    rv::Current<records::EffectObject>()->height = 0x3800000;
    SetWord(At(::bof3::Img(at::kTrailSize.v)), 0x60);
    for (int n = 0x20; n != 0; --n) SH_CALL(EffectKind80_TrailStep)(P(::bof3::Img(EffectKind30_Shards_addr)));
    S()[9] = 0;
    NextState();
}

// original 0x4872F0 (state 1): +9 up (Sound_PlayEffect(0x200) on the first
// frame); the trail stepped and drawn; x a cell on; at x = 0x17 cells +9 0x20
// and +1 up.
extern "C" void __cdecl EffectKind80_Rise(void) {
    unsigned char* const s = S();
    const unsigned frames = s[9];
    s[9] = static_cast<unsigned char>(frames + 1);
    if (frames == 0) SH_CALL(Sound_PlayEffect)(0x200);
    SH_CALL(EffectKind80_TrailStep)(P(::bof3::Img(EffectKind30_Shards_addr)));
    SH_CALL(EffectKind80_DrawTrail)(P(::bof3::Img(EffectKind30_Shards_addr)));
    rv::Current<records::EffectObject>()->x = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x) + 0x10000;
    if (static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x) == 0x170000) {
        S()[9] = 0x20;
        NextState();
    }
}

// original 0x487360 (state 2): stepped and drawn; +9 down, at 0 +9 0x10 and +1 up.
extern "C" void __cdecl EffectKind80_Hold(void) {
    SH_CALL(EffectKind80_TrailStep)(P(::bof3::Img(EffectKind30_Shards_addr)));
    SH_CALL(EffectKind80_DrawTrail)(P(::bof3::Img(EffectKind30_Shards_addr)));
    if (CountDown()) {
        S()[9] = 0x10;
        NextState();
    }
}

// original 0x4873A0 (state 3): stepped and drawn, the size 6 narrower; +9 down,
// at 0 +1 up.
extern "C" void __cdecl EffectKind80_Fade(void) {
    SH_CALL(EffectKind80_TrailStep)(P(::bof3::Img(EffectKind30_Shards_addr)));
    SH_CALL(EffectKind80_DrawTrail)(P(::bof3::Img(EffectKind30_Shards_addr)));
    SetWord(At(::bof3::Img(at::kTrailSize.v)), W(::bof3::Img(at::kTrailSize.v)) - 6u);
    if (CountDown()) NextState();
}

// original 0x4873E0 (0x1D1 bytes, cdecl): the trail at `pool` - 32 points of
// 0x20 (+0 x, z, height; +0x10 the screen x, y, depth; +0x1C the angle and
// +0x1E the half width, words), 31 angle words at +0x400, the size word
// +0x43E. The points move down one (31 is dropped), point 0 is the record's
// point; each projected (EffectGte_ProjectPoint) and its half width the first
// word EffectGte_ProjectSize gives for (size, size). Each pair's screen
// direction (Math_Ratan2 of the differences, each _ftol'd to a word; 0x1000 for
// none), a 0x1000 filled from the next real one ahead or else behind (0 when
// there is none); each point's angle the mean of its two neighbours'
// (EffectAngle_Mean), the ends their own.
extern "C" void __cdecl EffectKind80_TrailStep(unsigned char* pool) {
    for (int i = static_cast<int>(at::kTrailPoints) - 2; i >= 0; --i)
        std::memcpy(pool + at::kTrailStride * (i + 1), pool + at::kTrailStride * i, at::kTrailStride);
    SetUL(pool, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    SetUL(pool + 4, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    SetUL(pool + 8, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height));
    SH_CALL(EffectGte_LoadMapCamera)();
    for (U i = 0; i < at::kTrailPoints; ++i) {
        unsigned char* const p = pool + at::kTrailStride * i;
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(p), reinterpret_cast<float*>(p + 0x10));
        const auto w = static_cast<short>(W(pool + 0x43E));
        short size[2] = {w, w};
        short out[2];
        SH_CALL(EffectGte_ProjectSize)(reinterpret_cast<const long*>(p), size, out);
        SetWord(p + 0x1E, static_cast<U>(static_cast<std::uint16_t>(out[0])));
    }
    unsigned char* const angles = pool + 0x400;
    for (U i = 0; i < at::kTrailPoints - 1; ++i) {
        const unsigned char* const a = pool + at::kTrailStride * i + 0x10;
        const unsigned char* const b = a + at::kTrailStride;
        const U dx = FldSubFtol(a, b) & 0xFFFFu;
        const U dy = FldSubFtol(a + 4, b + 4) & 0xFFFFu;
        if (dx == 0 && dy == 0) {
            SetWord(angles + 2 * i, 0x1000);
        } else {
            const int angle = SH_CALL(Math_Ratan2)(FildValue(S16(dy)), FildValue(S16(dx)));
            SetWord(angles + 2 * i, static_cast<U>(angle) & 0xFFFFu);
        }
    }
    const unsigned n = at::kTrailPoints - 1;   // 31 angles
    for (unsigned c = 0; c < n; ++c) {
        if (W(angles + 2 * c) != 0x1000) continue;
        unsigned found = n;
        for (unsigned a = c + 1; a < n; ++a)
            if (W(angles + 2 * a) != 0x1000) {
                found = a;
                break;
            }
        if (found == n)
            for (unsigned a = 0; a < c; ++a)
                if (W(angles + 2 * a) != 0x1000) {
                    found = a;
                    break;
                }
        SetWord(angles + 2 * c, found == n ? 0u : W(angles + 2 * found));
    }
    SetWord(pool + 0x1C, W(angles));
    for (U i = 0; i + 1 < n; ++i) {
        const long mean = SH_CALL(EffectAngle_Mean)(W(angles + 2 * i), W(angles + 2 * (i + 1)));
        SetWord(pool + at::kTrailStride * (i + 1) + 0x1C, static_cast<U>(mean) & 0xFFFFu);
    }
    SetWord(pool + at::kTrailStride * (at::kTrailPoints - 1) + 0x1C, W(angles + 2 * (n - 1)));
}

// original 0x4875C0 (0x2D0 bytes, cdecl): the trail at `pool` drawn - a draw mode
// (page Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1) committed (1, 0xC); point 0's end
// cap (EffectTrail_DrawCap, its angle - 0x400, shade 0x80); for each pair whose
// screen points differ, two Gouraud quads, each from the pair's points to their
// rims at the angles + 0x400 and - 0x400 (cos / sin of the half width >> 12),
// the shade at the first point and 4 less at the second, black at the rims,
// semi-transparent, committed (1, 0x44) - the second a copy of the first with
// its rims moved; the shade 4 less a pair drawn. Point 31's cap last.
extern "C" void __cdecl EffectKind80_DrawTrail(unsigned char* pool) {
    const U page = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char shade = 0x80;
    SH_CALL(EffectTrail_DrawCap)(pool + 0x10, W(pool + 0x1E), (W(pool + 0x1C) - 0x400u) & 0xFFFFu, 0x80);
    for (U i = 0; i < at::kTrailPoints - 1; ++i) {
        const unsigned char* const a = pool + at::kTrailStride * i + 0x10;
        const unsigned char* const b = a + at::kTrailStride;
        if (FcompSame(a, b) && FcompSame(a + 4, b + 4)) continue;
        unsigned char* const q = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(q);
        SH_CALL(Gpu_SetSemiTrans)(q, 1);
        Copy4(q + 8, a);
        Copy4(q + 0xC, a + 4);
        const U ca = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(W(a + 0xC) + 0x400u)));
        FildAdd(q + 0x18, static_cast<std::int32_t>(ca * W(a + 0xE)) >> 12, a);
        const U sa = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(W(a + 0xC) + 0x400u)));
        FildAdd(q + 0x1C, static_cast<std::int32_t>(sa * W(a + 0xE)) >> 12, a + 4);
        FldTwo(q + 0x20, q + 0x10, a + 8);
        Copy4(q + 0x28, b);
        Copy4(q + 0x2C, b + 4);
        const U cb = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(W(b + 0xC) + 0x400u)));
        FildAdd(q + 0x38, static_cast<std::int32_t>(cb * W(b + 0xE)) >> 12, b);
        const U sb = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(W(b + 0xC) + 0x400u)));
        FildAdd(q + 0x3C, static_cast<std::int32_t>(sb * W(b + 0xE)) >> 12, b + 4);
        const auto next = static_cast<unsigned char>(shade - 4);
        FldTwo(q + 0x40, q + 0x30, b + 8);
        q[6] = shade;
        q[5] = shade;
        q[4] = shade;
        q[0x16] = 0;
        q[0x15] = 0;
        q[0x14] = 0;
        q[0x26] = next;
        q[0x25] = next;
        q[0x24] = next;
        q[0x36] = 0;
        q[0x35] = 0;
        q[0x34] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        unsigned char* const r = Gfx_PacketNext;
        for (unsigned d = 0; d < 0x11; ++d) Copy4(r + 4 * d, r - 0x44 + 4 * d);   // rep movsd
        const U c2 = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(W(a + 0xC) - 0x400u)));
        FildAdd(r + 0x18, static_cast<std::int32_t>(c2 * W(a + 0xE)) >> 12, a);
        const U s2 = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(W(a + 0xC) - 0x400u)));
        FildAdd(r + 0x1C, static_cast<std::int32_t>(s2 * W(a + 0xE)) >> 12, a + 4);
        const U c3 = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(W(b + 0xC) - 0x400u)));
        FildAdd(r + 0x38, static_cast<std::int32_t>(c3 * W(b + 0xE)) >> 12, b);
        const U s3 = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(W(b + 0xC) - 0x400u)));
        FildAdd(r + 0x3C, static_cast<std::int32_t>(s3 * W(b + 0xE)) >> 12, b + 4);
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        shade = next;
    }
    SH_CALL(EffectTrail_DrawCap)(pool + 0x3F0, W(pool + 0x3FE), (W(pool + 0x3FC) + 0x400u) & 0xFFFFu, shade);
}

void EffectKind80_Inject() {
    BOF3_INJECT(EffectKind80_Run);
    BOF3_INJECT(EffectKind80_Start);
    BOF3_INJECT(EffectKind80_Rise);
    BOF3_INJECT(EffectKind80_Hold);
    BOF3_INJECT(EffectKind80_Fade);
    BOF3_INJECT(EffectKind80_TrailStep);
    BOF3_INJECT(EffectKind80_DrawTrail);
}
