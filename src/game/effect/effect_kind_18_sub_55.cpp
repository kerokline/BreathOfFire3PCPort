// Moved here by tools/move_funcs.py from effect_6c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_55.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
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

using move_script::SetWord;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U ULEffect6c(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect6c(U a) { return ULEffect6c(At(a)); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// cdq; and edx, 2^n - 1; add eax, edx; sar eax, n: a signed divide by 2^n,
// toward zero.
U DivP2(U v, unsigned n) { return Sar(v + (I(v) < 0 ? (1u << n) - 1u : 0u), n); }

// cdq; xor eax, edx; sub eax, edx.
U Abs(U v) { return I(v) < 0 ? 0u - v : v; }

float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}

float F(U a) { return F(At(a)); }

double D(U a) { return static_cast<double>(F(a)); }

void SetF(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

void SetD(unsigned char* p, double d) { SetF(p, static_cast<float>(d)); }

void SetD(U a, double d) { SetD(At(a), d); }

// fild dword: a whole number, exact in double.
double Fi(U v) { return static_cast<double>(I(v)); }

unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

void SetV(unsigned offset, U v) { SetWord(Vertices() + offset, v); }

const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

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

}  // namespace

// original 0x5128B0 (EffectKind18_States[0x55], hidden in 0x512040): jmp
// [EffectKind18Sub55_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub55_Run(void) {
    Dispatch("EffectKind18Sub55_Run", E6C_TABLE(EffectKind18Sub55_States));
}

// original 0x5128D0 (sub-state 1): once flag 0xE of 0x904000 is set, +9 0 and
// +2 up. The squares drawn (0x80, 0).
extern "C" void __cdecl EffectKind18Sub55_Wait(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x70), 0xE) != 0) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub55_Draw)(0x80, 0);
}

// original 0x512910 (sub-state 2): +9 up; past 0xF the record released; the
// squares drawn ((0x10 - +9) * 8, +9 * 8), +9 read again after the release.
extern "C" void __cdecl EffectKind18Sub55_Fade(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] > 0xF) SH_CALL(Effect_Release)();
    const U n = S()[9];
    SH_CALL(EffectKind18Sub55_Draw)(I((0x10u - n) << 3), I(n * 8));
}

// original 0x512960 (called by sub-kind 0x55's two states): over the cells x
// 3..13, z 0x71..0x76 where x + z is even, a textured square: the cell's point
// ((x - 0x80) << 7, ((z - 0x80) << 6 - lift) << 1, (z - 0x77) << 4) projected
// into MapView_ScreenXY, half a side of |3 - (Frame_Counter / 3 + x + z & 7)|
// + 0x18; shaded (shade / 2, shade, shade / 2) (each << 6 or << 7, / 128
// toward zero); page 0x3B, CLUT 0x78CA; linked at (x << 16, z << 16) (dy 1,
// 0x48).
extern "C" void __cdecl EffectKind18Sub55_Draw(int shade_word, int lift_word) {
    const U shade = static_cast<U>(shade_word), lift = static_cast<U>(lift_word);
    for (U z = 0x71; I(z) < 0x77; ++z) {
        for (U x = 3; I(x) < 0xE; ++x) {
            if (((x + z) & 1) != 0) continue;
            SetV(0, (x - 0x80u) << 7);
            SetV(2, (((z - 0x80u) << 6) - lift) << 1);
            SetV(4, (z - 0x77u) << 4);
            long a;
            SH_CALL(Gte_RotTransPers)(Vx(0), reinterpret_cast<unsigned long*>(MapView_ScreenXY), &a);
            const U half = Abs(3u - (((Frame_Counter / 3) + x + z) & 7)) + 0x18;
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gte_PrimDepthFlat4_10)(p);
            const double h = Fi(half), side = Fi(half * 2);
            const double sx = D(AddressOf(MapView_ScreenXY)), sy = D(::bof3::Img(at::kScreenY.v));
            SetD(p + 8, sx - h);
            SetD(p + 0xC, sy - h);
            SetD(p + 0x18, (sx - h) + side);
            SetD(p + 0x1C, sy - h);
            SetD(p + 0x28, sx - h);
            SetD(p + 0x2C, (sy - h) + side);
            SetD(p + 0x38, (sx - h) + side);
            SetD(p + 0x3C, (sy - h) + side);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[0x14] = 0xE0;
            p[0x15] = 0x30;
            const unsigned char outer = static_cast<unsigned char>(DivP2(shade << 6, 7));
            p[4] = outer;
            p[6] = outer;
            p[0x24] = 0xFF;
            p[0x25] = 0x30;
            p[0x34] = 0xE0;
            p[0x35] = 0x4F;
            p[0x44] = 0xFF;
            p[0x45] = 0x4F;
            rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, 0x3B);
            rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, 0x78CA);
            p[5] = static_cast<unsigned char>(DivP2(shade << 7, 7));
            SH_CALL(MapView_LinkPrimAt)(x << 16, z << 16, 1, 0x48);
        }
    }
}

void EffectKind18Sub55_Inject() {
    BOF3_INJECT(EffectKind18Sub55_Run);
    BOF3_INJECT(EffectKind18Sub55_Wait);
    BOF3_INJECT(EffectKind18Sub55_Fade);
    BOF3_INJECT(EffectKind18Sub55_Draw);
}
