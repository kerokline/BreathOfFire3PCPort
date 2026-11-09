// Moved here by tools/move_funcs.py from effect_6c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_59.h"

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

// A rect of four words allocated at the cursor (the cursor up 8), as the sky
// and sub-kind 0x66 do before a draw mode that names it.
unsigned char* Rect(U x, U y, U w, U h) {
    unsigned char* const r = Gfx_PacketNext;
    Gfx_PacketNext = r + 8;
    SetWord(r, x);
    SetWord(r + 2, y);
    SetWord(r + 6, h);
    SetWord(r + 4, w);
    return r;
}

// +0xA up `by`, then & 0x3F (two reads of the record, as the original).
void Scroll59(unsigned by) {
    S()[0xA] = static_cast<unsigned char>(S()[0xA] + by);
    S()[0xA] &= 0x3F;
}

}  // namespace

// original 0x512B20 (EffectKind18_States[0x59], hidden in 0x512960; the
// catalog's Table EffectKind18_States row, in no group's list): jmp
// [EffectKind18Sub59_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub59_Run(void) {
    Dispatch("EffectKind18Sub59_Run", E6C_TABLE(EffectKind18Sub59_States));
}

// original 0x512B40 (sub-state 0): +9 0, +0xA 0, +2 up; the ring drawn dark;
// the texture move.
extern "C" void __cdecl EffectKind18Sub59_Start(void) {
    S()[9] = 0;
    S()[0xA] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub59_DrawRing)(0);
    SH_CALL(EffectKind18Sub59_Move)();
}

// original 0x512B70 (sub-state 1): once Cond_ByteFE is 1, +2 up; the ring
// drawn dark (no move).
extern "C" void __cdecl EffectKind18Sub59_Wait(void) {
    if (Cond_ByteFE == 1) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub59_DrawRing)(0);
}

// original 0x512B90 (sub-state 2): once Cond_ByteFE is 2, +2 up; +9 up to 0x20;
// the ring drawn with the glow 0x20 - +9; +0xA up 1; the move.
extern "C" void __cdecl EffectKind18Sub59_Rise(void) {
    if (Cond_ByteFE == 2) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    if (S()[9] < 0x20) S()[9] = static_cast<unsigned char>(S()[9] + 1);
    SH_CALL(EffectKind18Sub59_DrawRing)(I(0x20u - S()[9]));
    Scroll59(1);
    SH_CALL(EffectKind18Sub59_Move)();
}

// original 0x512BF0 (sub-state 3): once Cond_ByteFE is 3, +9 0 and +2 up; the
// ring dark; +0xA up 2; the move.
extern "C" void __cdecl EffectKind18Sub59_Hold(void) {
    if (Cond_ByteFE == 3) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub59_DrawRing)(0);
    Scroll59(2);
    SH_CALL(EffectKind18Sub59_Move)();
}

// original 0x512C30 (sub-state 4): +9 up; the fade of kShades59 (.data, never
// restored): the first of its first nine bytes that is not 0 down 2, and while
// each so stepped is at most 0x60 the next down 2 too (four at most); the ring
// with the glow min(+9, 0x60); +0xA up 4; the move; once the ninth byte
// (0x65EF14) is 0, +9 0x60 and +2 up.
extern "C" void __cdecl EffectKind18Sub59_Fade(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* const shades = At(::bof3::Img(at::kShades59.v));
    for (unsigned e = 0; e < 9; ++e) {
        if (shades[e] == 0) continue;
        for (unsigned n = 0; n < 4; ++n) {
            shades[e + n] = static_cast<unsigned char>(shades[e + n] - 2);
            if (n == 3 || shades[e + n] > 0x60) break;
        }
        break;
    }
    const U glow = S()[9] > 0x60 ? 0x60u : S()[9];
    SH_CALL(EffectKind18Sub59_DrawRing)(I(glow));
    Scroll59(4);
    SH_CALL(EffectKind18Sub59_Move)();
    if (*At(::bof3::Img(at::kShades59.v) + 8) != 0) return;
    S()[9] = 0x60;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x512CF0 (sub-state 5): +9 down 4; at 0 the record released; the
// ring with the glow +9 (read again).
extern "C" void __cdecl EffectKind18Sub59_Close(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 4);
    if (S()[9] == 0) SH_CALL(Effect_Release)();
    SH_CALL(EffectKind18Sub59_DrawRing)(I(static_cast<U>(S()[9])));
}

// original 0x512D30 (called by sub-kind 0x59's six states): the record's point
// ((+0x34 >> 9) - 0x4000, (+0x38 >> 9) - 0x4000) into MapView_ScreenXY as
// floats; sixteen columns i (angle 0x1000 i, /16 for Math_Sin / Math_Cos) of
// eight rings k: two shaded quads (POLY_GT4, pages 0xD5 and 0xB5) between the
// radii kRadii59[7 - k] and [8 - k] (the products >> 8), lifted by kHeights59,
// shaded by the twelve .data bytes kShades59 (the fade's), projected once and
// the second given the first's screen points; each linked at the record (dy
// kRows59[i], 0x54). Then two glows a column: a point on the outer ring
// projected into 0x903838, a square of half side (|3 - (i / 2 + Frame_Counter
// & 7)| + 0x10) * 4500 / (Camera_Distance + 0x1194) (+ 8 the second), shaded
// (glow, glow, glow / 2), linked (dy kRows59[i] + 1, 0x48).
extern "C" void __cdecl EffectKind18Sub59_DrawRing(int glow_word) {
    const U glow = static_cast<U>(glow_word);
    {
        const unsigned char* const s = S();
        SetD(AddressOf(MapView_ScreenXY), Fi(Sar(ULEffect6c(s + 0x34), 9) - 0x4000u));
        SetD(::bof3::Img(at::kScreenY.v), Fi(Sar(ULEffect6c(s + 0x38), 9) - 0x4000u));
    }
    for (U i = 0, angle = 0; I(angle) < 0x10000; ++i, angle += 0x1000) {
        const U b0 = DivP2(angle, 4), b1 = DivP2(angle + 0x1000u, 4);
        const U row = *At(::bof3::Img(at::kRows59.v) + i);
        for (std::int32_t k = 0; k + 7 >= 0; --k) {
            const U ki = static_cast<U>(k);
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyGT4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            struct Corner { unsigned at; U b; U radius; U height; };
            const Corner corners[] = {
                {0, b0, ::bof3::Img(at::kRadii59.v) + 7 + ki, ::bof3::Img(at::kHeights59.v) + 7 + ki},
                {8, b1, ::bof3::Img(at::kRadii59.v) + 7 + ki, ::bof3::Img(at::kHeights59.v) + 7 + ki},
                {0x10, b0, ::bof3::Img(at::kRadii59.v) + 8 + ki, ::bof3::Img(at::kHeights59.v) + 8 + ki},
                {0x18, b1, ::bof3::Img(at::kRadii59.v) + 8 + ki, ::bof3::Img(at::kHeights59.v) + 8 + ki},
            };
            for (const Corner& c : corners) {
                const U sine = static_cast<U>(SH_CALL(Math_Sin)(I(c.b)));
                SetV(c.at, Ftol(Fi(Sar(sine, 8) * *At(c.radius)) + D(AddressOf(MapView_ScreenXY))));
                const U cosine = static_cast<U>(SH_CALL(Math_Cos)(I(c.b)));
                SetV(c.at + 2, Ftol(Fi(Sar(cosine, 8) * *At(c.radius)) + D(::bof3::Img(at::kScreenY.v))));
                SetV(c.at + 4, 0u - (static_cast<U>(*At(c.height)) << 5));
            }
            long depth;
            SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x1C), Fp(p + 0x30), Fp(p + 0x44), &depth);
            SH_CALL(Gte_PrimDepths4_14)(p);
            auto dress = [&](unsigned char* q, U page, U clut) {
                q[0x14] = 0xB0;
                q[0x15] = 0x20;
                q[0x28] = 0xCF;
                q[0x29] = 0x20;
                q[0x3C] = 0xB0;
                q[0x3D] = 0x3F;
                q[0x50] = 0xCF;
                q[0x51] = 0x3F;
                SetWord(q + 0x2A, page);
                SetWord(q + 0x16, clut);
                const unsigned char near = *At(::bof3::Img(at::kShades59.v) + 7 + ki), far = *At(::bof3::Img(at::kShades59.v) + 8 + ki);
                q[4] = q[5] = q[6] = near;
                q[0x18] = q[0x19] = q[0x1A] = near;
                q[0x2C] = q[0x2D] = q[0x2E] = far;
                q[0x40] = q[0x41] = q[0x42] = far;
            };
            dress(p, 0xD5, 0x7940);
            {
                unsigned char* const s = S();
                SH_CALL(MapView_LinkPrimAt)(ULEffect6c(s + 0x34), ULEffect6c(s + 0x38), I(row), 0x54);
            }
            unsigned char* const q = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyGT4)(q);
            SH_CALL(Gpu_SetShadeTex)(q, 0);
            SH_CALL(Gpu_SetSemiTrans)(q, 1);
            // the first's four screen points, dword by dword in the original's
            // order (the disturbance can leave the cursor less than a prim
            // past p, and a forward copy then reads what it has written), the
            // last dword read before two uv bytes are written and stored after
            static const unsigned kPoints[] = {8, 0xC, 0x10, 0x1C, 0x20, 0x24, 0x30, 0x34, 0x38, 0x44, 0x48};
            for (const unsigned at : kPoints) SetUL(q + at, ULEffect6c(p + at));
            {
                const U last = ULEffect6c(p + 0x4C);
                q[0x14] = 0xB0;
                q[0x15] = 0x20;
                SetUL(q + 0x4C, last);
            }
            dress(q, 0xB5, 0x7900);
            unsigned char* const s = S();
            SH_CALL(MapView_LinkPrimAt)(ULEffect6c(s + 0x34), ULEffect6c(s + 0x38), I(row), 0x54);
        }
        const U half = DivP2(i, 1);
        const unsigned char bright = static_cast<unsigned char>(DivP2(glow << 7, 7));
        const unsigned char blue = static_cast<unsigned char>(DivP2(glow << 6, 7));
        U e = b0;
        for (U back = 0; I(back) < 0x10; back += 8, e += 0x80) {
            const U sine = static_cast<U>(SH_CALL(Math_Sin)(I(e)));
            SetV(0x10, Ftol(Fi(Sar(sine, 8) * *At(::bof3::Img(at::kRadii59.v) + 8)) + D(AddressOf(MapView_ScreenXY))));
            const U cosine = static_cast<U>(SH_CALL(Math_Cos)(I(e)));
            SetV(0x12, Ftol(Fi(Sar(cosine, 8) * *At(::bof3::Img(at::kRadii59.v) + 8)) + D(::bof3::Img(at::kScreenY.v))));
            SetV(0x14, 0u - (static_cast<U>(*At(::bof3::Img(at::kHeights59.v) + 8)) << 5));
            long a;
            SH_CALL(Gte_RotTransPers)(Vx(2), reinterpret_cast<unsigned long*>(At(::bof3::Img(Effect6c_GlowXY_addr))), &a);
            const std::int32_t divisor = static_cast<std::int16_t>(Camera_Distance) + 0x1194;
            if (divisor == 0)
                bof3::Fatal("EffectKind18Sub59_DrawRing: Camera_Distance is -0x1194, the glow's divisor 0 - the original "
                            "divides by zero (docs/effect_6c.md section 7)");
            const U size = static_cast<U>(I((Abs(3u - ((half + Frame_Counter) & 7)) + 0x10) * 4500u) / divisor) + back;
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gte_PrimDepthFlat4_10)(p);
            const double h = Fi(size), side = Fi(size * 2);
            SetD(p + 8, D(::bof3::Img(at::kGlowXY.v)) - h);
            SetD(p + 0xC, D(::bof3::Img(at::kGlowY.v)) - h);
            SetD(p + 0x18, (D(::bof3::Img(at::kGlowXY.v)) - h) + side);
            SetD(p + 0x1C, D(::bof3::Img(at::kGlowY.v)) - h);
            SetD(p + 0x28, D(::bof3::Img(at::kGlowXY.v)) - h);
            SetD(p + 0x2C, (D(::bof3::Img(at::kGlowY.v)) - h) + side);
            SetD(p + 0x38, (D(::bof3::Img(at::kGlowXY.v)) - h) + side);
            SetD(p + 0x3C, (D(::bof3::Img(at::kGlowY.v)) - h) + side);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[4] = bright;
            p[5] = bright;
            p[0x14] = 0xE0;
            p[0x15] = 0x30;
            p[0x24] = 0xFF;
            p[0x25] = 0x30;
            p[0x34] = 0xE0;
            p[0x35] = 0x4F;
            p[0x44] = 0xFF;
            p[0x45] = 0x4F;
            SetWord(p + 0x26, 0x3B);
            SetWord(p + 0x16, 0x78CA);
            p[6] = blue;
            unsigned char* const s = S();
            SH_CALL(MapView_LinkPrimAt)(ULEffect6c(s + 0x34), ULEffect6c(s + 0x38), I((row + 1) & 0xFF), 0x48);
        }
    }
}

// original 0x513410 (sub-kind 0x59's draws end with it, a tail jump from
// three states and a call from the fourth): a texture-page move (DR_MOVE) of
// the rect (0x1A8, 0x140 - (+0xA >> 1), 0x10, 0x20) to (0x198, 0x120),
// committed at slot 2 (0x18). The rect is a local the original builds on its
// stack.
extern "C" void __cdecl EffectKind18Sub59_Move(void) {
    unsigned char rect[8];
    SetWord(rect, 0x1A8);
    SetWord(rect + 2, 0x140u - (S()[0xA] >> 1));
    SetWord(rect + 4, 0x10);
    SetWord(rect + 6, 0x20);
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, 0x198, 0x120);
    SH_CALL(Gfx_CommitPrim)(2, 0x18);
}

// original 0x513470 (hidden in 0x513410): nothing while Cond_ByteFE is 0.
// Else two layers, each a draw mode with a texture window (0, 0, 0x40, 0x40;
// page 0x97, dtd) at slot 4, a 2 x 2 grid of 256-pixel textured quads (page
// 0x7B, CLUT 0x78C0) at (-d - 0x80, d - 0x80), and a draw mode with the window
// (0, 0, 0x100, 0x100): layer 0 offset d = -(+9 & 0x3F), shaded (0x80 - |0x80
// - +9|) / 2; layer 1 d = -((+9 & 0x1F) * 2), shaded the same unhalved past
// 0x40 of 0x80, else ((Math_Cos((+9 - 0x40) << 5) >> 7) + 0x60) / 2. +9 up 4;
// at 0 the record released.
extern "C" void __cdecl EffectKind18Sub66_Run(void) {
    if (Cond_ByteFE == 0) return;
    const U n = S()[9];
    const U distance = Abs(0x80u - n);
    const U edge = 0x80u - distance;
    U shades[2];
    shades[0] = Sar(edge, 1);
    if (I(distance) > 0x40) {
        shades[1] = edge;
    } else {
        const U cosine = static_cast<U>(SH_CALL(Math_Cos)(I((n - 0x40u) << 5)));
        shades[1] = Sar(Sar(cosine, 7) + 0x60u, 1);
    }
    const U m = S()[9];
    const U offsets[2] = {0u - (m & 0x3F), 0u - ((m & 0x1F) << 1)};
    for (unsigned layer = 0; layer < 2; ++layer) {
        const U d = offsets[layer];
        const unsigned char shade = static_cast<unsigned char>(shades[layer]);
        unsigned char* const window = Rect(0, 0, 0x40, 0x40);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x97, AddressOf(window));
        SH_CALL(Gfx_CommitPrim)(4, 0xC);
        const U xs[3] = {0u - d - 0x80u, 0u - d + 0x80u, 0u - d + 0x180u};
        const U ys[3] = {d - 0x80u, d + 0x80u, d + 0x180u};
        for (unsigned tile = 0; tile < 4; ++tile) {
            const unsigned cx = tile & 1, cy = tile >> 1;
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetD(p + 8, Fi(xs[cx]));
            SetD(p + 0xC, Fi(ys[cy]));
            SetD(p + 0x18, Fi(xs[cx + 1]));
            SetD(p + 0x1C, Fi(ys[cy]));
            SetD(p + 0x28, Fi(xs[cx]));
            SetD(p + 0x2C, Fi(ys[cy + 1]));
            SetD(p + 0x38, Fi(xs[cx + 1]));
            SetD(p + 0x3C, Fi(ys[cy + 1]));
            p[0x14] = 0;
            p[0x15] = 0;
            p[0x24] = 0xFF;
            p[0x25] = 0;
            p[0x34] = 0;
            p[0x35] = 0xFF;
            p[0x44] = 0xFF;
            p[0x45] = 0xFF;
            p[4] = p[5] = p[6] = shade;
            rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, 0x7B);
            rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, 0x78C0);
            SH_CALL(Gfx_CommitPrim)(4, 0x48);
        }
        unsigned char* const whole = Rect(0, 0, 0x100, 0x100);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x97, AddressOf(whole));
        SH_CALL(Gfx_CommitPrim)(4, 0xC);
    }
    S()[9] = static_cast<unsigned char>(S()[9] + 4);
    if (S()[9] == 0) SH_CALL(Effect_Release)();
}

void EffectKind18Sub59_Inject() {
    BOF3_INJECT(EffectKind18Sub59_Run);
    BOF3_INJECT(EffectKind18Sub59_Start);
    BOF3_INJECT(EffectKind18Sub59_Wait);
    BOF3_INJECT(EffectKind18Sub59_Rise);
    BOF3_INJECT(EffectKind18Sub59_Hold);
    BOF3_INJECT(EffectKind18Sub59_Fade);
    BOF3_INJECT(EffectKind18Sub59_Close);
    BOF3_INJECT(EffectKind18Sub59_DrawRing);
    BOF3_INJECT(EffectKind18Sub59_Move);
    BOF3_INJECT(EffectKind18Sub66_Run);
}
