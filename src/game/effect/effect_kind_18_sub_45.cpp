// Moved here by tools/move_funcs.py from effect_6c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_45.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_6c.h"
#include "game/effect_6c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6c::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U ULEffect6c(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect6c(U a) { return ULEffect6c(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetUL(U a, U v) { SetUL(At(a), v); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// cdq; and edx, 2^n - 1; add eax, edx; sar eax, n: a signed divide by 2^n,
// toward zero.
U DivP2(U v, unsigned n) { return Sar(v + (I(v) < 0 ? (1u << n) - 1u : 0u), n); }

float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}

float F(U a) { return F(At(a)); }

double D(U a) { return static_cast<double>(F(a)); }

// fild dword: a whole number, exact in double.
double Fi(U v) { return static_cast<double>(I(v)); }

// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000; the callers keep eax's low word.
U Ftol(double v) {
    if (!(v > -9.2233720368547758e18 && v < 9.2233720368547758e18)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

void SetV(unsigned offset, U v) { SetWord(Vertices() + offset, v); }

const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

float* Fp(unsigned char* p) { return reinterpret_cast<float*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6c.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect6c(At(table + 4 * sub))))();
}

#define E6C_TABLE(name) AddressOf(name), name##_count

// v - back, at least 0 and (where `cap`) at most 0x100 past 0xFF.
U Ring(U v, U back, bool cap) {
    const U r = v - back;
    if (I(r) < 0) return 0;
    if (cap && I(r) > 0xFF) return 0x100;
    return r;
}

}  // namespace

// original 0x511DD0 (EffectKind18_States[0x45], hidden in 0x511D50): jmp
// [EffectKind18Sub45_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub45_Run(void) {
    Dispatch("EffectKind18Sub45_Run", E6C_TABLE(EffectKind18Sub45_States));
}

// original 0x511DF0 (sub-state 0): once Cond_ByteFE is 1, +0xB 0, the radius
// +0x3C 0, +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub45_Wait(void) {
    if (Cond_ByteFE != 1) return;
    S()[0xB] = 0;
    rv::Current<records::EffectObject>()->height = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x511E20 (sub-state 1): the radius +0x3C up 8; one ring from it (at
// most 0x100) in to it - 0x20 (at least 0), the half 1; once the inner edge is
// past 0xFF the radius 0 and +2 up.
extern "C" void __cdecl EffectKind18Sub45_Open(void) {
    rv::Current<records::EffectObject>()->height = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height) + 8u;
    const U v = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height);
    const U outer = I(v) > 0xFF ? 0x100u : v;
    const U inner = Ring(v, 0x20, false);
    SH_CALL(EffectKind18Sub45_DrawRing)(I(outer), I(inner), 1);
    if (I(inner) <= 0xFF) return;
    rv::Current<records::EffectObject>()->height = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x511E80 (sub-state 2): the radius up 8; eight rings, the radii v,
// v - 0x20, .. v - 0x100 each kept to 0..0x100, the halves 1, 0, 1, ...; once
// the first is past 0xFF (and +0xB is 0) MoveCmd_TestFB(0xB, 0x22) and +0xB 1;
// once the last is, Cond_ByteFE 2 and the record released.
extern "C" void __cdecl EffectKind18Sub45_Spread(void) {
    rv::Current<records::EffectObject>()->height = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height) + 8u;
    const U v = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height);
    U r[9];
    r[0] = I(v) > 0xFF ? 0x100u : v;
    for (unsigned k = 1; k < 9; ++k) r[k] = Ring(v, 0x20 * k, true);
    for (unsigned k = 0; k < 8; ++k) SH_CALL(EffectKind18Sub45_DrawRing)(I(r[k]), I(r[k + 1]), (k & 1) ? 0 : 1);
    if (I(r[0]) > 0xFF && S()[0xB] == 0) {
        SH_CALL(MoveCmd_TestFB)(0xB, 0x22);
        S()[0xB] = 1;
    }
    if (I(r[8]) > 0xFF) {
        Cond_ByteFE = 2;
        SH_CALL(Effect_Release)();
    }
}

// original 0x512040 (called by sub-kind 0x45's two drawing states): a ring of
// 32 textured quads round the fixed point (-14912, -12096, -960) - written into
// MapView_ScreenXY and the cell after it first - between the radius `outer`
// (vertices 0, 1) and `inner` (2, 3), each quad 0x1000 of angle (/32 for
// Math_Sin / Math_Cos, the products >> 13); projected, semi-transparency 0, a
// page by the half (((half << 7) + 0x140) >> 6 & 0xF | 0x90), grey 0x50, the u
// and v the same angles' products >> 16 plus the half's pair of kRingUv, linked
// at the record's point (dy 0, 0x48).
extern "C" void __cdecl EffectKind18Sub45_DrawRing(int outer_word, int inner_word, int half_word) {
    const U outer = static_cast<U>(outer_word), inner = static_cast<U>(inner_word), half = static_cast<U>(half_word);
    SetUL(AddressOf(MapView_ScreenXY), 0xC6690000u);   // -14912.0f
    SetUL(::bof3::Img(at::kScreenY.v), 0xC63D0000u);                   // -12096.0f
    SetUL(::bof3::Img(at::kScreenZ.v), 0xC4700000u);                   // -960.0f
    if (half >= at::kRingHalves)
        bof3::Fatal("EffectKind18Sub45_DrawRing: the half %u, past the %u pairs of 0x%X - the original reads sub-kind "
                    "0x51's rectangles (docs/effect_6c.md section 7)",
                    (unsigned)half, at::kRingHalves, at::kRingUv.v);
    for (U angle = 0; I(angle) < 0x20000; angle += 0x1000) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        for (unsigned v = 0; v < 4; ++v) {
            const U t = angle + ((v & 1) ? 0x1000u : 0u);
            const U radius = v < 2 ? outer : inner;
            SetV(8 * v, Ftol(D(AddressOf(MapView_ScreenXY))));
            const U q = DivP2(t, 5);
            const U sine = Sar(static_cast<U>(SH_CALL(Math_Sin)(I(q))) * radius, 13);
            SetV(8 * v + 2, Ftol(D(::bof3::Img(at::kScreenY.v)) - Fi(sine)));
            const U cosine = Sar(static_cast<U>(SH_CALL(Math_Cos)(I(q))) * radius, 13);
            SetV(8 * v + 4, Ftol(Fi(cosine) + D(::bof3::Img(at::kScreenZ.v))));
        }
        long depth;
        SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 0);
        p[6] = 0x50;
        p[5] = 0x50;
        p[4] = 0x50;
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, (Sar((half << 7) + 0x140u, 6) & 0xF) | 0x90u);
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, 0x7940);
        const U q0 = DivP2(angle, 5), q1 = DivP2(angle + 0x1000u, 5);
        const unsigned char* const uv = At(::bof3::Img(at::kRingUv.v) + 2 * half);
        struct Corner { unsigned at; U q; U radius; };
        const Corner corners[] = {{0x14, q0, outer}, {0x24, q1, outer}, {0x34, q0, inner}, {0x44, q1, inner}};
        for (const Corner& c : corners) {
            const U sine = static_cast<U>(SH_CALL(Math_Sin)(I(c.q)));
            p[c.at] = static_cast<unsigned char>(Sar(sine * c.radius, 16) + uv[0]);
            const U cosine = static_cast<U>(SH_CALL(Math_Cos)(I(c.q)));
            p[c.at + 1] = static_cast<unsigned char>(Sar(cosine * c.radius, 16) + uv[1]);
        }
        unsigned char* const s = S();
        auto* const so = rv::As<records::EffectObject>(s);
        SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(so->x), static_cast<std::uint32_t>(so->z), 0, 0x48);
    }
}

void EffectKind18Sub45_Inject() {
    BOF3_INJECT(EffectKind18Sub45_Run);
    BOF3_INJECT(EffectKind18Sub45_Wait);
    BOF3_INJECT(EffectKind18Sub45_Open);
    BOF3_INJECT(EffectKind18Sub45_Spread);
    BOF3_INJECT(EffectKind18Sub45_DrawRing);
}
