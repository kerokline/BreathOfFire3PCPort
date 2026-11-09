// Moved here by tools/move_funcs.py from effect_6b.cpp, effect_6c.cpp, rest_3g.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_44.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/draw_pool.h"
#include "game/effect/effect_kind_18_sub_44_callees.h"
#include "game/effect_6b.h"
#include "game/effect_6b_callees.h"
#include "game/effect_6c.h"
#include "game/effect_6c_callees.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_18_sub_44::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

const short* Vertex(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

U SignedByte(unsigned char b) { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(b))); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6b.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// The quad's four corners projected into the POLY_FT4 at p, its depths set.
// The original also pushes a tenth pointer (a flag local) the projection does
// not read.
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                               reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                               reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

// The curve byte at +9 << 4 (the original's (b << 12) / 256 with b unsigned).
U Curve(const char* who, unsigned c) {
    if (c >= at::kSub41CurveRoom)
        bof3::Fatal("%s: +9 is %u, past the %u bytes of 0x%X - the original reads on into EffectKind18Sub41_States "
                    "(docs/effect_6b.md section 7)",
                    who, c, at::kSub41CurveRoom, at::kSub41Curve.v);
    return static_cast<U>(static_cast<std::int32_t>(static_cast<U>(At(::bof3::Img(at::kSub41Curve.v))[c]) << 12) / 256);
}

bool FlagSet(unsigned index) {
    return (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), index) & 0xFFu) != 0;
}

using move_script::SetLong;

using move_script::Word;

U ULEffect6c(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect6c(U a) { return ULEffect6c(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetUL(U a, U v) { SetUL(At(a), v); }

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

std::int32_t S16(U a) { return S16(At(a)); }

std::int32_t S8(U a) { return static_cast<signed char>(*At(a)); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// cdq; and edx, 2^n - 1; add eax, edx; sar eax, n: a signed divide by 2^n,
// toward zero.
U DivP2(U v, unsigned n) { return Sar(v + (I(v) < 0 ? (1u << n) - 1u : 0u), n); }

// cdq; xor eax, edx; sub eax, edx.
U Abs(U v) { return I(v) < 0 ? 0u - v : v; }

// xor ecx, ecx; cmp eax, 0; setl cl; dec ecx; and ecx, eax: below zero, 0.
U FloorZero(U v) { return I(v) < 0 ? 0u : v; }

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

// fld dword / fstp dword: the bits kept, but a signalling NaN made quiet.
void CopyQuiet(unsigned char* to, const unsigned char* from) {
    U bits = ULEffect6c(from);
    if ((bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0) bits |= 0x00400000u;
    SetUL(to, bits);
}

// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000; the callers keep eax's low word.
U Ftol(double v) {
    if (!(v > -9.2233720368547758e18 && v < 9.2233720368547758e18)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

void SetV(unsigned offset, U v) { SetWord(Vertices() + offset, v); }

const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

float* Fp(unsigned char* p) { return reinterpret_cast<float*>(p); }

U VertexAddress(unsigned offset) { return AddressOf(Vertices()) + offset; }

// (angle * 10) >> 5 (lea; shl 1; sar 5): the sky's screen x of an angle.
U AngleX(U angle) { return Sar(angle * 10u, 5); }

// An (r, g, b) from the vertex words of row `row` (0x9037A0 + 8 * row, + 2,
// + 4): their low bytes.
void RowColour(unsigned char* to, unsigned row) {
    const unsigned char* const v = Vertices() + 8 * row;
    to[0] = v[0];
    to[1] = v[2];
    to[2] = v[4];
}

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

// A star's shade: (Rand() & 0x7F) * level / 128, toward zero.
unsigned char Twinkle(U level) {
    const U r = static_cast<U>(SH_CALL(Rand)());
    return static_cast<unsigned char>(DivP2((r & 0x7F) * level, 7));
}

U ULRest3g(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U ULRest3g(U a) { return ULRest3g(move_script::At(a)); }

void SetULRest3g(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetULRest3g(U a, U v) { SetULRest3g(At(a), v); }

std::int32_t S8Rest3g(unsigned char b) { return static_cast<signed char>(b); }

// sar on the 32 bits.
U SarRest3g(U v, unsigned n) { return static_cast<U>(I(v) >> n); }

float Fl(U a) {
    float f;
    std::memcpy(&f, At(a), sizeof f);
    return f;
}

// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 (its low dword 0); the callers keep eax or its low word.
U FtolRest3g(long double v) {
    if (!(v > -9.2233720368547758e18L && v < 9.2233720368547758e18L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// fld dword [from]; fstp dword [to]: the copy through the FPU, which quietens a
// signalling NaN where a mov would not.
void FpuCopy(void* to, const void* from) {
    float f;
    std::memcpy(&f, from, sizeof f);
    volatile long double through = f;
    const float back = static_cast<float>(through);
    std::memcpy(to, &back, sizeof back);
}

// The value of the float an `fcomp dword ptr [mem]` operand names: the two
// above, or DIV-0041's four as widescreen.cpp holds them. Values of ours, never
// a read of the .rdata place: our own exe does not carry .rdata that only our
// functions name, and the six floats read 0 there - the desert's floor culled
// whole, its cells kept only when y > 0 at the wide picture (issue 185).
float Bound(U operand_at) {
    if (operand_at == at::kWideYAt.v) return 120.0f;     // AreaMapBD_WideYBound
    if (operand_at == at::kNarrowYAt.v) return 121.0f;   // AreaMapBD_NarrowYBound
    return Widescreen_RangeValue(operand_at);
}

}  // namespace

// original 0x50FAE0 (sub-kind 0x40's draw, called by its last three states): a
// RECT {0x60, 0x10, 0x10, 0x10} put at the packet cursor (the cursor past it)
// as the texture window of a draw mode (page 0x1B) committed at slot 5 (0xC);
// three semi-transparent POLY_FT4s (tpage 0x7B, clut 0x78C6, colour 0x80 each),
// pane i from the four bytes of 0x65EC74 + 4i: x from (t0 << 5) + step to
// ((t0 + t2) << 5) + step, z likewise from t1 and t3, each times 4 - 0x4040;
// every corner's height 6 * step - 0x380; the third pane's corner 0 moved by 8
// * step in x and z; u, v 0..(t2 << 4) - 1, (t3 << 4) - 1; projected and
// committed at slot 5 (0x48). Then a RECT {0, 0, 0x100, 0x100} likewise
// (page 0x1B, slot 5).
extern "C" void __cdecl EffectKind18Sub40_Draw(int step) {
    const U f = static_cast<U>(step);
    {
        unsigned char* const rect = Gfx_PacketNext;
        Gfx_PacketNext = rect + 8;
        SetWord(rect, 0x60);
        SetWord(rect + 2, 0x10);
        SetWord(rect + 6, 0x10);
        SetWord(rect + 4, 0x10);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1B, AddressOf(rect));
        SH_CALL(Gfx_CommitPrim)(5, 0xC);
    }
    const U height = 6 * f - 0x380u;
    unsigned char* const v = Vertices();
    for (unsigned i = 0; i < at::kSub40PaneCount; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetWord(p + 0x26, 0x7B);
        SetWord(p + 0x16, 0x78C6);
        const unsigned char* const t = At(::bof3::Img(at::kSub40Panes.v) + 4 * i);
        const U x0 = ((static_cast<U>(t[0]) << 5) + f) * 4 - 0x4040u;
        SetWord(v + 0x10, x0);
        SetWord(v, x0);
        const U x1 = ((static_cast<U>(t[0] + t[2]) << 5) + f) * 4 - 0x4040u;
        SetWord(v + 0x18, x1);
        SetWord(v + 8, x1);
        const U z0 = ((static_cast<U>(t[1]) << 5) + f) * 4 - 0x4040u;
        SetWord(v + 0xA, z0);
        SetWord(v + 2, z0);
        const U z1 = ((static_cast<U>(t[1] + t[3]) << 5) + f) * 4 - 0x4040u;
        SetWord(v + 0x1A, z1);
        SetWord(v + 0x12, z1);
        SetWord(v + 0x1C, height);
        SetWord(v + 0x14, height);
        SetWord(v + 0xC, height);
        SetWord(v + 4, height);
        if (i == 2) {
            SetWord(v, UL(v + 0x10) + 8 * f);
            SetWord(v + 2, 8 * f + z0);
        }
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x24] = static_cast<unsigned char>((t[2] << 4) - 1);
        p[0x35] = static_cast<unsigned char>((t[3] << 4) - 1);
        p[0x44] = static_cast<unsigned char>((t[2] << 4) - 1);
        p[0x45] = static_cast<unsigned char>((t[3] << 4) - 1);
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
        Project(p);
        SH_CALL(Gfx_CommitPrim)(5, 0x48);
    }
    unsigned char* const rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetWord(rect, 0);
    SetWord(rect + 2, 0);
    SetWord(rect + 6, 0x100);
    SetWord(rect + 4, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1B, AddressOf(rect));
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x50FD30 (hidden in 0x50FAE0).
extern "C" void __cdecl EffectKind18Sub41_Run(void) {
    Dispatch("EffectKind18Sub41_Run", AddressOf(EffectKind18Sub41_States), EffectKind18Sub41_States_count);
}

// original 0x50FD50 (sub-state 0): Cond_ByteFE = 2, +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub41_Start(void) {
    unsigned char* const s = S();
    Cond_ByteFE = 2;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x50FD60 (sub-state 1): when Cond_ByteFE is 1, +9 = 0, +2 up and the
// draw (variant 0, angle 0); otherwise nothing drawn.
extern "C" void __cdecl EffectKind18Sub41_WaitOne(void) {
    if (Cond_ByteFE != 1) return;
    S()[9] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub41_Draw)(0, 0);
}

// original 0x50FD90 (sub-state 2): the draw (variant +9 > 0xB, angle the curve
// at +9); +9 up; past 0xF: sound 0x200 when story flag 0x4F is set, else 0x201,
// +0xA = 0, +2 up (each read afresh).
extern "C" void __cdecl EffectKind18Sub41_Turn(void) {
    const unsigned c = S()[9];
    const U angle = Curve("EffectKind18Sub41_Turn", c);
    SH_CALL(EffectKind18Sub41_Draw)(c > 0xB ? 1u : 0u, static_cast<int>(angle));
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] <= 0xF) return;
    const unsigned short sound = FlagSet(0x4F) ? at::kSoundOpen : at::kSoundShut;
    SH_CALL(Sound_PlayEffect)(sound);
    S()[0xA] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x50FE20 (sub-state 3): while +9 is not 0, the draw (variant 1, the
// curve at +9) and +9 up; at 0x17 story flag 0x50 set, MoveCmd_TestFB(0x13,
// 0x25) and +9 = 0. While story flag 0x4F is set: Capcom's 0x5100B0(0x65ECAC[+0xA]
// << 2, 0x65ECC8[+0xA >> 1]) when +0xA is below 0x19, and 0x5101C0(+0xA). +0xA
// up; past 0x30 a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind18Sub41_Hold(void) {
    const unsigned c = S()[9];
    if (c != 0) {
        const U angle = Curve("EffectKind18Sub41_Hold", c);
        SH_CALL(EffectKind18Sub41_Draw)(1, static_cast<int>(angle));
        S()[9] = static_cast<unsigned char>(S()[9] + 1);
    }
    if (S()[9] == 0x17) {
        SH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x50);
        SH_CALL(MoveCmd_TestFB)(0x13, 0x25);
        S()[9] = 0;
    }
    if (FlagSet(0x4F)) {
        const unsigned d = S()[0xA];
        if (d < 0x19) {
            const U first = static_cast<U>(At(::bof3::Img(at::kSub41MarkA.v))[d]) << 2;
            const U second = At(::bof3::Img(at::kSub41MarkB.v))[d >> 1];
            SH_AT(void(__cdecl*)(U, U), at::kWaveMark.v)(first, second);
        }
        SH_AT(void(__cdecl*)(U), at::kWaveStep.v)(S()[0xA]);
    }
    S()[0xA] = static_cast<unsigned char>(S()[0xA] + 1);
    if (S()[0xA] > 0x30) SH_CALL(Effect_Release)();
}

// original 0x50FF10 (sub-kind 0x41's draw, called by its states 1..3): a draw
// mode (page 0x95) committed at slot 5 (0xC); the GTE matrix pushed; a matrix
// turned (0, -angle, 0) about the point (-0x37C0, -0x2DC0, -0x480) rotated and
// translated (Gte_RotTrans into the matrix's translation, Gte_RotMatrix, the
// camera's Gte_MulMatrix0) and set; four POLY_FT4s, quad q = 0x65ED08[variant *
// 4 + i]: its four vertices from the twelve bytes at 0x65ECD8 + 12q (x, z,
// -y, each << 7), projected, the texture 0x65ED10[q] + ((s8 0x65ED20[q] *
// angle) & ~0x7FF) << 8, committed at slot 5 (0x48); the matrix popped.
extern "C" void __cdecl EffectKind18Sub41_Draw(unsigned variant, int angle) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    alignas(4) unsigned char angles[8] = {};
    alignas(4) unsigned char point[8] = {};
    alignas(4) unsigned char matrix[0x20] = {};
    SetWord(angles + 4, 0);
    SetWord(angles + 2, 0u - static_cast<U>(angle));
    SetWord(angles, 0);
    SetWord(point, 0xC840);
    SetWord(point + 2, 0xD240);
    SetWord(point + 4, 0xFB80);
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(Gte_RotTrans)(reinterpret_cast<const short*>(point), reinterpret_cast<long*>(matrix + 0x14));
    SH_CALL(Gte_RotMatrix)(reinterpret_cast<const short*>(angles), reinterpret_cast<short*>(matrix));
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<const short*>(matrix), reinterpret_cast<short*>(matrix));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    if (variant >= at::kSub41Variants)
        bof3::Fatal("EffectKind18Sub41_Draw: variant %u, past the %u rows of 0x%X - the original reads on into the "
                    "texture words (docs/effect_6b.md section 7)",
                    variant, at::kSub41Variants, at::kSub41Order.v);
    unsigned char* const v = Vertices();
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const unsigned q = At(::bof3::Img(at::kSub41Order.v) + 4 * variant)[i];
        if (q >= at::kSub41QuadCount)
            bof3::Fatal("EffectKind18Sub41_Draw: quad %u, past the %u of 0x%X (docs/effect_6b.md section 7)", q,
                        at::kSub41QuadCount, at::kSub41Quads.v);
        const unsigned char* const b = At(::bof3::Img(at::kSub41Quads.v) + 12 * q);
        for (unsigned j = 0; j < 4; ++j) {
            SetWord(v + 8 * j, static_cast<U>(b[3 * j]) << 7);
            SetWord(v + 8 * j + 2, static_cast<U>(b[3 * j + 1]) << 7);
            SetWord(v + 8 * j + 4, (0u - b[3 * j + 2]) << 7);
        }
        Project(p);
        const U turn = (SignedByte(At(::bof3::Img(at::kSub41Turns.v))[q]) * static_cast<U>(angle)) & 0xFFFFF800u;
        SH_CALL(Prim_SetTexture)((turn << 8) + UL(At(::bof3::Img(at::kSub41Textures.v) + 4 * q)), p, 1);
        SH_CALL(Gfx_CommitPrim)(5, 0x48);
    }
    SH_CALL(Gte_PopMatrix)();
}

// original 0x5100B0 (cdecl): a draw mode (page 0x95) committed at slot 5
// (0xC); two textured quads from the eight three-byte vertices at 0x65ED30 -
// x = b0 << 7 - 0x37C0, z = b1 << 7 - 0x2DC0, y = -0x3F8 - b2 * lift (each a
// word) - projected (Gte_RotTransPers4, Gte_PrimDepths4_10), the texture word
// (texture << 16) | 0xBB009120, committed at slot 5 (0x48).
extern "C" void __cdecl EffectKind18Sub41_DrawPanels(unsigned lift, unsigned texture) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    for (U from = ::bof3::Img(at::kPanelVertices.v); from < ::bof3::Img(at::kPanelVerticesEnd.v); from += 12) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        unsigned char* const v = Vertices();
        for (unsigned k = 0; k < 4; ++k) {
            const unsigned char* const b = At(from + 3 * k);
            rv::As<records::SVector>(v)[k].x = (static_cast<U>(b[0]) << 7) - 0x37C0u;
            rv::As<records::SVector>(v)[k].y = (static_cast<U>(b[1]) << 7) - 0x2DC0u;
            rv::As<records::SVector>(v)[k].z = 0xFFFFFC08u - static_cast<U>(b[2]) * lift;
        }
        long depth;
        const short* const vs = reinterpret_cast<short*>(Prim_VertexScratch);
        SH_CALL(Gte_RotTransPers4)(vs, vs + 4, vs + 8, vs + 12, reinterpret_cast<float*>(p + 8),
                                   reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                   reinterpret_cast<float*>(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Prim_SetTexture)((texture << 16) | 0xBB009120u, p, 1);
        SH_CALL(Gfx_CommitPrim)(5, 0x48);
    }
}

// original 0x5101C0 (cdecl, with +0xA): a draw mode (dtd, page 0xB5) committed
// at slot 5 (0xC); the vertex word z 0xFC08 (once). Three rings, from r = the
// argument and c = (0x28 - it) * 5, each next with r - 0x10 and c + 0x10: r
// below 0 taken as 0, c clamped to 0..0xFF; R = (r << 12) sar 10. Each: a
// semi-transparent LINE_F2 coloured (c / 2, c / 2, c) from (0xC840, R -
// 0x2CC0) to (0xC9C0, R - 0x2CC0); 16 chords of the circle of radius r about
// (-13888, -11456) - each end (sin * r sar 10 + x, cos * r sar 10 + y) through
// _ftol, coloured c / 2 - at angles 0..0x400 by 0x40; a line from (R - 0x3640,
// 0xD240) to (R - 0x3640, 0xD340); 16 chords about (-13888, -11712) at
// 0x400..0x800. Each line projected end by end (Gte_RotTransPers, the depth
// stored) and committed at slot 5 (0x20). The draw mode again (no dtd) to end.
extern "C" void __cdecl EffectKind18Sub41_DrawRings(int size) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    unsigned char* const v = Vertices();
    const short* const v0 = reinterpret_cast<short*>(Prim_VertexScratch);
    rv::As<records::SVector>(v)->z = 0xFC08;
    U radius_arg = static_cast<U>(size);
    U colour = (0x28u - static_cast<U>(size)) * 5u;
    long p_out;
    // A line's two ends projected into p + 8 / p + 0x14, each depth after it.
    auto end = [&](unsigned char* p, unsigned at_xy, unsigned at_depth) {
        SH_CALL(Gte_RotTransPers)(v0, reinterpret_cast<unsigned long*>(p + at_xy), &p_out);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + at_depth));
    };
    for (unsigned ring = 3; ring != 0; --ring) {
        const U r = I(radius_arg) < 0 ? 0u : radius_arg;
        U c = colour;
        if (I(c) > 0xFF) c = 0xFF;
        const U c8 = I(c) < 0 ? 0u : c;
        const unsigned char half = static_cast<unsigned char>(c8 >> 1);
        const U big = SarRest3g(r << 12, 10);
        // the straight line across
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetLineF2)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[6] = static_cast<unsigned char>(c8);
            p[4] = half;
            p[5] = half;
            rv::As<records::SVector>(v)->x = 0xC840;
            SetWord(v + 2, big - 0x2CC0u);
            end(p, 8, 0x10);
            rv::As<records::SVector>(v)->x = 0xC9C0;
            rv::As<records::SVector>(v)->y = big - 0x2CC0u;
            end(p, 0x14, 0x1C);
            SH_CALL(Gfx_CommitPrim)(5, 0x20);
        }
        // a circle's 16 chords about the centre MapView_ScreenXY holds
        auto chords = [&](U from, U to) {
            for (U angle = from; angle < to;) {
                unsigned char* const p = Gfx_PacketNext;
                SH_CALL(Gpu_SetLineF2)(p);
                SH_CALL(Gpu_SetSemiTrans)(p, 1);
                p[4] = half;
                p[5] = half;
                p[6] = half;
                for (unsigned k = 0; k < 2; ++k) {
                    if (k == 1) angle += 0x40;
                    const int s = SH_CALL(Math_Sin)(static_cast<int>(angle));
                    const U dx = SarRest3g(static_cast<U>(s) * r, 10);
                    SetWord(v, FtolRest3g(static_cast<long double>(I(dx)) + static_cast<long double>(Fl(AddressOf(MapView_ScreenXY)))));
                    const int co = SH_CALL(Math_Cos)(static_cast<int>(angle));
                    const U dy = SarRest3g(static_cast<U>(co) * r, 10);
                    SetWord(v + 2,
                            FtolRest3g(static_cast<long double>(I(dy)) + static_cast<long double>(Fl(AddressOf(MapView_ScreenXY) + 4))));
                    end(p, k == 0 ? 8 : 0x14, k == 0 ? 0x10 : 0x1C);
                }
                SH_CALL(Gfx_CommitPrim)(5, 0x20);
            }
        };
        SetULRest3g(AddressOf(MapView_ScreenXY), 0xC6590000u);       // -13888.0
        SetULRest3g(AddressOf(MapView_ScreenXY) + 4, 0xC6330000u);   // -11456.0
        chords(0, 0x400);
        // the straight line down
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetLineF2)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[6] = static_cast<unsigned char>(c8);
            p[4] = half;
            p[5] = half;
            SetWord(v, big - 0x3640u);
            rv::As<records::SVector>(v)->y = 0xD240;
            end(p, 8, 0x10);
            rv::As<records::SVector>(v)->x = big - 0x3640u;
            rv::As<records::SVector>(v)->y = 0xD340;
            end(p, 0x14, 0x1C);
            SH_CALL(Gfx_CommitPrim)(5, 0x20);
        }
        SetULRest3g(AddressOf(MapView_ScreenXY), 0xC6590000u);       // -13888.0
        SetULRest3g(AddressOf(MapView_ScreenXY) + 4, 0xC6370000u);   // -11712.0
        chords(0x400, 0x800);
        radius_arg -= 0x10;
        colour += 0x10;
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x510630 (AreaMap_Frame's whole work in area 0xBD): MapView_FocusX
// / Z = (0x800000 - Field_Kind2X / Z) sar 8; with any of the three angle words
// moved from Camera_AnglesDrawn, MapView_Redraw 3 and both dwords copied;
// Gte_RotMatrix(Camera_Angles, Camera_Matrix); the vector (((FocusX >> 1) -
// 0x4000) & 0x7FFF) - 0x4000, the same of FocusZ, MapView_Elevation >> 1 (each
// a word, read after the matrix) through Gte_ApplyMatrix, plus Camera_ShiftX,
// Camera_ShiftY and Camera_Distance + 0x1194, as the translation; the matrix
// set; with MapView_Redraw, AreaMapBD_BuildView and MapView_Redraw down.
extern "C" void __cdecl AreaMap_FrameAreaBD(void) {
    MapView_FocusX = static_cast<long>(SarRest3g(0x800000u - static_cast<U>(Field_Kind2X), 8));
    MapView_FocusZ = static_cast<long>(SarRest3g(0x800000u - static_cast<U>(Field_Kind2Z), 8));
    {
        unsigned char* const angles = reinterpret_cast<unsigned char*>(Camera_Angles);
        unsigned char* const drawn = reinterpret_cast<unsigned char*>(Camera_AnglesDrawn);
        const U first = ULRest3g(angles), second = ULRest3g(angles + 4);
        if (Word(angles) != Word(drawn) || Word(angles + 2) != Word(drawn + 2) || (second & 0xFFFFu) != Word(drawn + 4)) {
            MapView_Redraw = 3;
            SetULRest3g(drawn, first);
            SetULRest3g(drawn + 4, second);
        }
    }
    SH_CALL(Gte_RotMatrix)(Camera_Angles, Camera_Matrix);
    short vector[3];
    vector[0] = static_cast<short>((((static_cast<U>(MapView_FocusX) >> 1) - 0x4000u) & 0x7FFFu) - 0x4000u);
    vector[1] = static_cast<short>((((static_cast<U>(MapView_FocusZ) >> 1) - 0x4000u) & 0x7FFFu) - 0x4000u);
    vector[2] = static_cast<short>(static_cast<U>(MapView_Elevation) >> 1);
    long moved[3];
    SH_CALL(Gte_ApplyMatrix)(Camera_Matrix, vector, moved);
    unsigned char* const matrix = reinterpret_cast<unsigned char*>(Camera_Matrix);
    SetULRest3g(matrix + 0x14, static_cast<U>(static_cast<int>(Camera_ShiftX)) + static_cast<U>(moved[0]));
    SetULRest3g(matrix + 0x18, static_cast<U>(static_cast<int>(Camera_ShiftY)) + static_cast<U>(moved[1]));
    SetULRest3g(matrix + 0x1C, static_cast<U>(static_cast<int>(Camera_Distance)) + static_cast<U>(moved[2]) + 0x1194u);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(Camera_Matrix));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(Camera_Matrix));
    if (MapView_Redraw == 0) return;
    SH_CALL(AreaMapBD_BuildView)();
    MapView_Redraw = static_cast<unsigned char>(MapView_Redraw - 1);
}

// original 0x510780 (AreaMap_FrameAreaBD's, with MapView_Redraw): area 0xBD's
// cells drawn. DrawTable_Count 0; this buffer's lists 0 and 1 of the 0x38
// DrawLayers emptied; the camera's octant ((Cond_AngleFB + 0x100) sar 9) & 7
// picks a record of six s8 at 0x65ED48 (x0, dx per column, dx per row, z0, dz
// per column, dz per row); the first cell x0 - (byte (FocusX >> 8) - 0x80) +
// 0x100 and z0 likewise (words 0x903850 / 0x903852 and MapView_Origin); an odd
// octant walks 0x38 rows of 0x1C cells, an even one 0x2C of 0x28 (dwords
// 0x903854 / 0x903858); DrawItemPool_Top 0. Each cell, while the item index is
// below its bound (0x400): x = row * dxr / 2 + col * dxc + x0, z = (row + 1) *
// dzr / 2 + col * dzc + z0 (the halves toward 0); the block map 0x65ED78
// [((x sar 4) & 0xF) | (z & 0xF0)] gives the map cell (row nibble << 4 | z &
// 0xF) * 0x60 + (column nibble << 4 | x & 0xF); its first corner projected; kept
// when its screen y is above 120 and x inside (-200, 520), or y below 121 (or
// unordered) and x inside (-50, 370); a kept cell takes the next draw item
// (DrawItemPool_Top up), its texture (AreaMapBD_CellTexture), its first screen
// point (copied through the FPU), its three other corners projected, linked
// into list 0 of layer 0x37 - row, and its depths.
extern "C" void __cdecl AreaMapBD_BuildView(void) {
    map_rim::NoteOwnView(true);   // DIV-0041 (wide-ring): this view is Capcom's 28 x 56 (issue 185)
    DrawTable_Count = 0;
    {
        unsigned char* layer = At(AddressOf(DrawLayers) + Gfx_BufferIndex * 8u);
        for (unsigned n = 0; n < 0x38; ++n, layer += 0x30) {
            unsigned char* list = layer;
            for (unsigned k = 0; k < 2; ++k, list += 0x10) {
                SetULRest3g(list, 0);
                SetULRest3g(list + 4, AddressOf(list));
            }
        }
    }
    const U octant = SarRest3g(static_cast<U>(Cond_AngleFB) + 0x100u, 9) & 7u;
    const U fx = ((static_cast<U>(MapView_FocusX) >> 8) - 0x80u) & 0xFFu;
    const U fz = ((static_cast<U>(MapView_FocusZ) >> 8) - 0x80u) & 0xFFu;
    const unsigned char* const rec = At(::bof3::Img(at::kOctants.v) + octant * 6);
    const U even = (~octant) & 1u;   // not eax; and eax, 1
    const U x0 = static_cast<U>(S8Rest3g(rec[0])) - fx + 0x100u;
    const U z0 = static_cast<U>(S8Rest3g(rec[3])) - fz + 0x100u;
    const U rows = 0x38u - even * 12u, columns = even * 12u + 0x1Cu;
    at::kOriginX = x0;
    MapView_Origin[0] = static_cast<short>(x0);
    at::kOriginZ = z0;
    MapView_Origin[1] = static_cast<short>(z0);
    at::kRows = rows;
    at::kColumns = columns;
    DrawItemPool_Top = 0;
    U row_bound = rows;
    for (U row = 0; I(row) < I(row_bound); ++row) {
        U col_bound = row == 0 ? columns : static_cast<std::uint32_t>(at::kColumns);
        for (U col = 0; I(col) < I(col_bound); ++col, col_bound = static_cast<std::uint32_t>(at::kColumns)) {
            if (DrawItemPool_Top >= draw_pool::Count()) continue;   // at::kTopBoundAt: 0x400, DIV-0062's 0x800
            const U a = static_cast<U>(S8Rest3g(rec[2])) * row;
            const U x = static_cast<U>(I(a) / 2) + static_cast<U>(S8Rest3g(rec[1])) * col + static_cast<std::uint32_t>(static_cast<std::int16_t>(at::kOriginX));
            const U b = static_cast<U>(S8Rest3g(rec[5])) * (row + 1u);
            const U z = static_cast<U>(I(b) / 2) + static_cast<U>(S8Rest3g(rec[4])) * col + static_cast<std::uint32_t>(static_cast<std::int16_t>(at::kOriginZ));
            const unsigned char block = At(::bof3::Img(at::kBlocks.v) + ((SarRest3g(x, 4) & 0xFu) | (z & 0xF0u)))[0];
            const U map_row = (block & 0xF0u) | (z & 0xFu);
            const U map_col = ((block & 0xFu) << 4) | (x & 0xFu);
            const U cell = map_row * at::kMapWidth + map_col;
            unsigned char* const v = Vertices();
            rv::As<records::SVector>(v)->y = (z << 7) - 0x4040u;
            rv::As<records::SVector>(v)->x = (x << 7) - 0x4040u;
            unsigned char* corner = reinterpret_cast<unsigned char*>(&AreaMap_Corners) + cell * 4;
            MapView_CornerPtr = corner;
            rv::As<records::SVector>(v)->z = (0u - static_cast<U>(S8Rest3g(corner[0]))) << 4;
            SH_CALL(Gte_LoadVertex)(reinterpret_cast<const unsigned long*>(Prim_VertexScratch));
            SH_CALL(Gte_Rtps)();
            SH_CALL(Gte_StoreScreenXY)(reinterpret_cast<unsigned long*>(MapView_ScreenXY));
            // x87 fcomp against the operands' floats: an unordered compare sets
            // C0 and C3 both.
            const float sx = Fl(AddressOf(MapView_ScreenXY)), sy = Fl(AddressOf(MapView_ScreenXY) + 4);
            bool keep = sy > Bound(at::kWideYAt.v) && sx > Bound(at::kWideLoAt.v) && sx < Bound(at::kWideHiAt.v);
            if (!keep) {
                const bool y_ok = !(sy >= Bound(at::kNarrowYAt.v));
                keep = y_ok && sx > Bound(at::kNarrowLoAt.v) && !(sx >= Bound(at::kNarrowHiAt.v));
            }
            if (!keep) continue;

            const unsigned top = DrawItemPool_Top;
            unsigned char* const item = draw_pool::Items() + top * 0x90u;   // at::kItemsAt: DIV-0062's array once grown
            DrawItemPool_Top = static_cast<unsigned short>(top + 1);
            {
                const U offset = Word(AreaMap_Header + 2);
                unsigned char* const quad = item + Gfx_BufferIndex * 0x48u;
                const U tile = Word(AreaMap_Header + (cell + offset * 2u) * 2u);
                const U header = ULRest3g(AreaMap_Header);
                const U product = ((header >> 8) & 0xFFu) * (header & 0xFFu) + 1u;
                const U index = static_cast<U>(I(product) / 2) + offset + tile;
                SH_CALL(AreaMapBD_CellTexture)(static_cast<unsigned long>(ULRest3g(AreaMap_Header + index * 4u)), quad);
            }
            FpuCopy(item + Gfx_BufferIndex * 0x48u + 8, MapView_ScreenXY);
            FpuCopy(item + Gfx_BufferIndex * 0x48u + 0xC, MapView_ScreenXY + 1);
            {
                const U xy = ULRest3g(v);
                const U y = static_cast<std::uint16_t>(rv::As<records::SVector>(v)->y);
                corner = MapView_CornerPtr;
                const U x1 = xy + 0x80u;
                rv::As<records::SVector>(v)[1].x = x1;
                rv::As<records::SVector>(v)[1].y = y;
                rv::As<records::SVector>(v)[1].z = (0u - static_cast<U>(S8Rest3g(static_cast<unsigned char>(ULRest3g(corner) >> 8))) ) << 4;
                const U y1 = y + 0x80u;
                rv::As<records::SVector>(v)[2].x = static_cast<std::uint16_t>(rv::As<records::SVector>(v)->x);
                rv::As<records::SVector>(v)[2].y = y1;
                rv::As<records::SVector>(v)[3].x = x1;
                rv::As<records::SVector>(v)[2].z = (0u - static_cast<U>(S8Rest3g(static_cast<unsigned char>(ULRest3g(corner) >> 16))) ) << 4;
                rv::As<records::SVector>(v)[3].y = y1;
                rv::As<records::SVector>(v)[3].z = (0u - static_cast<U>(S8Rest3g(static_cast<unsigned char>(ULRest3g(corner) >> 24))) ) << 4;
            }
            SH_CALL(Gte_LoadVertices3)(reinterpret_cast<const unsigned long*>(v + 8));
            SH_CALL(Gte_Rtpt)();
            {
                const U last = ::bof3::Img(at::kLayerLast.v) + (Gfx_BufferIndex - row * 6u) * 8u;
                unsigned char* const quad = item + Gfx_BufferIndex * 0x48u;
                SH_CALL(Gpu_LinkPrim)(reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(ULRest3g(last))), AddressOf(quad));
            }
            SetULRest3g(::bof3::Img(at::kLayerLast.v) + (Gfx_BufferIndex - row * 6u) * 8u, AddressOf(item + Gfx_BufferIndex * 0x48u));
            {
                unsigned char* const quad = item + Gfx_BufferIndex * 0x48u;
                SH_CALL(Gte_StoreScreenXY3)(reinterpret_cast<unsigned long*>(quad + 0x18), reinterpret_cast<unsigned long*>(quad + 0x28),
                                            reinterpret_cast<unsigned long*>(quad + 0x38));
            }
            SH_CALL(Gte_PrimDepths4_10)(item + Gfx_BufferIndex * 0x48u);
        }
        row_bound = static_cast<std::uint32_t>(at::kRows);
    }
}

// original 0x510BB0 (cdecl, by AreaMapBD_BuildView): the quad not semi-
// transparent; its colour 0x80 x 3; its CLUT word ((texture sar 24) & 0xF) +
// 0x1E3 << 6, its page word 0x95; the 16 x 16 tile of the texture's low byte -
// u (texture & 0xF) << 4 to + 0xF, v texture & 0xF0 to + 0xF.
extern "C" void __cdecl AreaMapBD_CellTexture(unsigned long texture, unsigned char* quad) {
    SH_CALL(Gpu_SetSemiTrans)(quad, 0);
    quad[6] = 0x80;
    quad[5] = 0x80;
    quad[4] = 0x80;
    const U t = static_cast<U>(texture);
    SetWord(quad + 0x26, 0x95);
    SetWord(quad + 0x16, ((SarRest3g(t, 24) & 0xFu) + 0x1E3u) << 6);
    const unsigned char v0 = static_cast<unsigned char>(t & 0xF0u);
    const unsigned char u0 = static_cast<unsigned char>((t & 0xFu) << 4);
    quad[0x15] = v0;
    quad[0x25] = v0;
    quad[0x14] = u0;
    quad[0x24] = static_cast<unsigned char>(u0 + 0xF);
    quad[0x34] = u0;
    quad[0x35] = static_cast<unsigned char>(v0 + 0xF);
    quad[0x44] = static_cast<unsigned char>(u0 + 0xF);
    quad[0x45] = static_cast<unsigned char>(v0 + 0xF);
}

// original 0x510C20 (hidden in 0x510BB0's catalog extent, AreaMapBD_CellTexture, R3G's).
extern "C" void __cdecl EffectKind18Sub44_Run(void) {
    Dispatch("EffectKind18Sub44_Run", AddressOf(EffectKind18Sub44_States), EffectKind18Sub44_States_count);
}

// original 0x510C40 (sub-state 0): Sprite_SetAnimationBank(0x2A8), +0x24 = 0,
// Sprite_SetAnimation(0), +0x29 = 6, +0x48 = 0, +2 up (each read afresh).
extern "C" void __cdecl EffectKind18Sub44_Start(void) {
    SH_CALL(Sprite_SetAnimationBank)(0x2A8);
    S()[0x24] = 0;
    SH_CALL(Sprite_SetAnimation)(0);
    S()[0x29] = 6;
    S()[0x48] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x510C80 (sub-state 1): E6C's 0x510C90, then a tail jump to E6C's
// 0x510EB0 (both void, on Sprite_Current; raw until E6C merges).
extern "C" void __cdecl EffectKind18Sub44_Step(void) {
    scenario_harness::Phase(at::kE6CStep.v)();
    scenario_harness::Phase(at::kE6CTail.v)();
}

// original 0x510C90 (called by E6B's 0x510C80, sub-kind 0x44's state 1): the
// record kept on the leader's animation (Sprite_EnsureAnimation when +0x4B
// differs, then +0x4B = the leader's, read again); the leader in state 3 runs
// Sprite_ScriptTick. Cond_ByteFE 1: the colour cells +0x5D..+0x5F = 0x7F, 0x80,
// 0x80 and Cond_ByteFE 0; else 0, 0, 0. +0x32 = 0x3200; the vertex
// (((Field_Kind2X - 1) & 0xFFFFFF) + 1 >> 9) - 0x4000, the same of Field_Kind2Z,
// -(the leader's +0x3E / 2) projected into MapView_ScreenXY; +0x74 160.0f, +0x2E
// 0xA0; the leader's +0x14 set: +0x78 = the screen y (a copy), +0x30 and the
// leader's +0x30 = it truncated; else +0x78 = the leader's +0x30 and +0x30 =
// it. The record put on Sprite_DrawList while it has room (40). Cond_ByteFE 2:
// a red additive tile over the frame - DIV-0041's fill - and Cond_ByteFE 0.
extern "C" void __cdecl EffectKind18Sub44_Follow(void) {
    if (S()[0x4B] != ObjTrio[0].animation) {
        SH_CALL(Sprite_EnsureAnimation)(ObjTrio[0].animation);
        S()[0x4B] = ObjTrio[0].animation;
    }
    if (ObjTrio[0].step == 3) SH_CALL(Sprite_ScriptTick)();
    if (Cond_ByteFE == 1) {
        S()[0x5D] = 0x7F;
        S()[0x5E] = 0x80;
        S()[0x5F] = 0x80;
        Cond_ByteFE = 0;
    } else {
        S()[0x5D] = 0;
        S()[0x5E] = 0;
        S()[0x5F] = 0;
    }
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->draw_key, 0x3200);
    SetV(0, Sar(((static_cast<U>(Field_Kind2X) - 1u) & 0xFFFFFFu) + 1u, 9) - 0x4000u);
    SetV(4, 0u - DivP2(static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ObjTrio[0].height_whole))), 1));
    SetV(2, Sar(((static_cast<U>(Field_Kind2Z) - 1u) & 0xFFFFFFu) + 1u, 9) - 0x4000u);
    long p;
    SH_CALL(Gte_RotTransPers)(Vx(0), reinterpret_cast<unsigned long*>(MapView_ScreenXY), &p);
    rv::Current<records::EffectObject>()->screen_xf = 0x43200000u; // 160.0f
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0xA0);
    if (static_cast<std::uint32_t>(ObjTrio[0].step_y) != 0) {
        CopyQuiet(S() + 0x78, At(::bof3::Img(MapView_ScreenXY_addr) + 0x4));
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, Ftol(D(::bof3::Img(at::kScreenY.v))));
        ObjTrio[0].screen_y = Ftol(D(::bof3::Img(at::kScreenY.v)));
    } else {
        SetD(S() + 0x78, Fi(static_cast<U>(static_cast<std::int32_t>(ObjTrio[0].screen_y))));
        unsigned char* const s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, Ftol(static_cast<double>(F(s + 0x78))));
    }
    const unsigned count = Sprite_DrawListCount;
    if (count < 0x28) {
        SetUL(::bof3::Img(at::kDrawList.v) + 4 * count, AddressOf(S()));
        Sprite_DrawListCount = static_cast<unsigned char>(count + 1);
    }
    if (Cond_ByteFE == 2) {
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
        SH_CALL(Gfx_CommitPrim)(3, 0xC);
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SetUL(prim + 8, std::bit_cast<U>(Widescreen_FillX()));       // DIV-0041: (-53, 0) 426 wide under the wide picture
        SetUL(prim + 0xC, 0);
        SetUL(prim + 0x14, std::bit_cast<U>(Widescreen_FillWidth()));  // 0x43A00000, 320.0f narrow
        SetUL(prim + 0x18, 0x43700000u);                               // 240.0f
        prim[4] = 0xFF;
        prim[5] = 0;
        prim[6] = 0;
        SH_CALL(Gte_StoreDepthF)(Fp(prim + 0x10));
        SH_CALL(Gfx_CommitPrim)(3, 0x1C);
        Cond_ByteFE = 0;
    }
}

// original 0x510EB0 (E6B's 0x510C80 tail-jumps to it): sub-kind 0x44's sky,
// in screen space. A draw mode with a texture window (0x20, 0, 0x20, 0x20) and
// a textured band (0, 90)..(320, 122) at slot 7; a draw mode with the window
// (0, 0, 0x100, 0x100) at slot 7; a draw mode (page 0xD5) and a white tile (0,
// 50) 320 x 40 through EffectKind18Sub44_Commit. By area 189's frame word
// (0x90405C) the vertex words 0x9037A0.. are two colour rows and the CLUT rows
// 6.. are tinted (Gfx_ClutAdjust): below 0x1B0 rows (0, 0, 0) / (0x80, ..);
// below 0x1E0 a ramp (n = (w - 0x1B0) / 3); below 0x390 (0x80, ..) / (0, ..);
// else n = (w - 0x390) / 3 into the sixteen tints of kSkyTints. Two rows of
// three textured quads (x stepping 0x100 from the camera's angle, y 0..90)
// coloured by the rows; the stars (the word 0x9037A8 their level); two layers
// (pages 0xB5, 0xD5) of four Gouraud bands between kSkyXs, from y 90 down to
// kSkyYs, coloured by the rows; and the two glows: one by the camera's distance
// from (0x1780, 0x1380), one - flag 4 of 0x904000 set - by its distance from
// (0x1780, 0xE00), placed by Math_Ratan2. The bands are laid out from the
// cursor as it was after their draw mode (0x44 apart), as the original does.
//
// DIV-0041 (psp-widescreen-placements.md 2.2, the owner's answers in 6.1):
// with the fill columns c > 0 the sky reaches the wide frame's edges - the
// textured band gets a quad each side, x -c..0 and 320..320 + c, u continuing
// the middle's rate (255 texels over 320 columns) so the pattern carries on
// rather than stretching; the white tile runs from Widescreen_FillX() over
// Widescreen_FillWidth(); one more column of textured quads at x - 0x200 keeps
// the left band covered at every camera angle (the PSP moved its three by
// -32 instead, which shifts the pattern under the middle); and the haze bands
// get a flat quad each side at the outer rows' colour and depth, as sub-kind
// 0x15's sky does (effect_5c.cpp). With c = 0 - narrow, or any self-test -
// every packet is the original's.
extern "C" void __cdecl EffectKind18Sub44_Draw(void) {
    const U wide = Widescreen_Fill();
    {
        unsigned char* const window = Rect(0x20, 0, 0x20, 0x20);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, AddressOf(window));
        SH_CALL(Gfx_CommitPrim)(7, 0xC);
        // The band (x0, 90)..(x1, 122), u u0..u1, v 0..0x40.
        const auto band = [](U x0, U x1, unsigned char u0, unsigned char u1) {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 1);
            SetUL(p + 0xC, 0x42B40000u);   // 90.0f
            SetUL(p + 0x1C, 0x42B40000u);
            SetUL(p + 0x2C, 0x42F40000u);  // 122.0f
            SetUL(p + 0x3C, 0x42F40000u);
            SetUL(p + 8, x0);
            SetUL(p + 0x18, x1);
            SetUL(p + 0x28, x0);
            SetUL(p + 0x38, x1);
            p[0x14] = u0;
            p[0x15] = 0;
            p[0x24] = u1;
            p[0x25] = 0;
            p[0x34] = u0;
            p[0x35] = 0x40;
            p[0x44] = u1;
            p[0x45] = 0x40;
            SetWord(p + 0x26, 0x95);
            SetWord(p + 0x16, 0x7900);
            SH_CALL(Gfx_CommitPrim)(7, 0x48);
        };
        band(0, 0x43A00000u, 0, 0xFF);   // 0.0f, 320.0f
        if (wide != 0) {
            const auto texels = static_cast<unsigned char>((wide * 0xFFu + 160u) / 320u);   // 42 for 53
            band(std::bit_cast<U>(Widescreen_FillX()), 0, static_cast<unsigned char>(0xFFu - texels), 0xFF);
            band(0x43A00000u, std::bit_cast<U>(Widescreen_FillRight(320.0f)), 0, texels);
        }
    }
    {
        unsigned char* const window = Rect(0, 0, 0x100, 0x100);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, AddressOf(window));
        SH_CALL(Gfx_CommitPrim)(7, 0xC);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xD5, 0);
        SH_CALL(EffectKind18Sub44_Commit)(0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, std::bit_cast<U>(Widescreen_FillX()));       // DIV-0041: 0 unarmed
        SetUL(p + 0xC, 0x42480000u);   // 50.0f
        SetUL(p + 0x14, std::bit_cast<U>(Widescreen_FillWidth()));  // 320.0f unarmed
        SetUL(p + 0x18, 0x42200000u);  // 40.0f
        p[4] = p[5] = p[6] = 0xFF;
        SH_CALL(EffectKind18Sub44_Commit)(0x1C);
    }
    const U clock = Word(At(::bof3::Img(Cond_Flags_addr) + 0xcc));
    if (clock < 0x1B0) {
        SetV(0xC, 0x80);
        SetV(0xA, 0x80);
        SetV(8, 0x80);
        SetV(4, 0);
        SetV(2, 0);
        SetV(0, 0);
    } else if (clock < 0x1E0) {
        const U n = (clock - 0x1B0) / 3;
        SetV(4, n * 8);
        SetV(2, n * 8);
        SetV(0, n * 8);
        SetV(0xC, (0x10 - n) << 3);
        SetV(0xA, (0x10 - n) << 3);
        SetV(8, (0x10 - n) << 3);
        SH_CALL(Gfx_ClutAdjust)(0, 6, I(n * 4) / 10 - 6, I(n) / 8 - 2, 0);
    } else if (clock < 0x390) {
        SetV(4, 0x80);
        SetV(2, 0x80);
        SetV(0, 0x80);
        SetV(0xC, 0);
        SetV(0xA, 0);
        SetV(8, 0);
    } else {
        const U n = (clock - 0x390) / 3;
        if (n >= at::kSkyTintSteps)
            bof3::Fatal("EffectKind18Sub44_Draw: the frame word 0x%X is step %u, past the %u tints of 0x%X - the "
                        "original reads on into the twinkle offsets (docs/effect_6c.md section 7)",
                        (unsigned)clock, (unsigned)n, at::kSkyTintSteps, at::kSkyTints.v);
        const U past = FloorZero(n - 6);
        SetV(0, 0x80u - past * 12u);
        SetV(4, (0x10 - n) << 3);
        SetV(2, (0x10 - n) << 3);
        SetV(0xC, n * 8);
        SetV(0xA, n * 8);
        SetV(8, n * 8);
        SH_CALL(Gfx_ClutAdjust)(0, 6, S8(::bof3::Img(at::kSkyTints.v) + 3 * n), S8(::bof3::Img(at::kSkyTints.v) + 3 * n + 1),
                                S8(::bof3::Img(at::kSkyTints.v) + 3 * n + 2));
    }
    for (U row = 0; row < 2; ++row) {
        const U clut = (row + 0x1E6) << 6;
        const unsigned char v0 = static_cast<unsigned char>(row * 0x60), v1 = static_cast<unsigned char>(v0 + 0x5A);
        // DIV-0041: one more column at x - 0x200 when wide (the three reach
        // only x - 0x100 >= -1 at the largest angle).
        for (U column = wide != 0 ? 0u - 0x100u : 0u; I(column) < 0x300; column += 0x100) {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetUL(p + 0xC, 0);
            SetD(p + 8, Fi((AngleX(Cond_AngleFB) & 0xFF) + column - 0x100u));
            SetUL(p + 0x1C, 0);
            SetD(p + 0x18, Fi((AngleX(Cond_AngleFB) & 0xFF) + column));
            SetUL(p + 0x2C, 0x42B40000u);   // 90.0f
            SetD(p + 0x28, Fi((AngleX(Cond_AngleFB) & 0xFF) + column - 0x100u));
            SetUL(p + 0x3C, 0x42B40000u);
            SetD(p + 0x38, Fi((AngleX(Cond_AngleFB) & 0xFF) + column));
            RowColour(p + 4, row);
            SetWord(p + 0x16, clut);
            p[0x15] = v0;
            p[0x25] = v0;
            SetWord(p + 0x26, 0xB7);
            p[0x14] = 0;
            p[0x24] = 0xFF;
            p[0x34] = 0;
            p[0x35] = v1;
            p[0x44] = 0xFF;
            p[0x45] = v1;
            SH_CALL(EffectKind18Sub44_Commit)(0x48);
        }
    }
    const U stars = Word(Vertices() + 8);
    if (stars != 0) SH_CALL(EffectKind18Sub44_DrawStars)(static_cast<std::int16_t>(stars));
    for (U layer = 0x20, row = 0; I(layer) < 0x60; layer += 0x20, ++row) {
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, (layer & 0x60) | 0x95, 0);
        SH_CALL(EffectKind18Sub44_Commit)(0xC);
        unsigned char* const first = Gfx_PacketNext;
        for (U m = 0; m < 4; ++m) {
            unsigned char* const p = first + 0x44 * m;
            SH_CALL(Gpu_SetPolyG4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetD(p + 8, Fi(Word(At(::bof3::Img(at::kSkyXs.v) + 2 * m))));
            SetUL(p + 0xC, 0x42B40000u);   // 90.0f
            SetD(p + 0x18, Fi(Word(At(::bof3::Img(at::kSkyXs.v) + 2 * m + 2))));
            SetUL(p + 0x1C, 0x42B40000u);
            SetD(p + 0x28, Fi(Word(At(::bof3::Img(at::kSkyXs.v) + 2 * m))));
            SetD(p + 0x2C, Fi(*At(::bof3::Img(at::kSkyYs.v) + m)));
            SetD(p + 0x38, Fi(Word(At(::bof3::Img(at::kSkyXs.v) + 2 * m + 2))));
            SetD(p + 0x3C, Fi(*At(::bof3::Img(at::kSkyYs.v) + m + 1)));
            RowColour(p + 4, row);
            RowColour(p + 0x14, row);
            p[0x24] = p[0x25] = p[0x26] = 0;
            p[0x34] = p[0x35] = p[0x36] = 0;
            SH_CALL(EffectKind18Sub44_Commit)(0x44);
        }
        // DIV-0041: the bands' outer rows carried flat to the wide frame's
        // edges, each written at the cursor and linked through the commit's
        // own pool test.
        if (wide != 0) {
            for (U side = 0; side < 2; ++side) {
                unsigned char* const p = Gfx_PacketNext;
                const U edge = side == 0 ? 0u : 4u;
                const float x0 = side == 0 ? Widescreen_FillX() : 320.0f;
                const float x1 = side == 0 ? 0.0f : Widescreen_FillRight(320.0f);
                SH_CALL(Gpu_SetPolyG4)(p);
                SH_CALL(Gpu_SetSemiTrans)(p, 1);
                SetUL(p + 8, std::bit_cast<U>(x0));
                SetUL(p + 0xC, 0x42B40000u);   // 90.0f
                SetUL(p + 0x18, std::bit_cast<U>(x1));
                SetUL(p + 0x1C, 0x42B40000u);
                SetUL(p + 0x28, std::bit_cast<U>(x0));
                SetD(p + 0x2C, Fi(*At(::bof3::Img(at::kSkyYs.v) + edge)));
                SetUL(p + 0x38, std::bit_cast<U>(x1));
                SetD(p + 0x3C, Fi(*At(::bof3::Img(at::kSkyYs.v) + edge)));
                RowColour(p + 4, row);
                RowColour(p + 0x14, row);
                p[0x24] = p[0x25] = p[0x26] = 0;
                p[0x34] = p[0x35] = p[0x36] = 0;
                SH_CALL(EffectKind18Sub44_Commit)(0x44);
            }
        }
    }
    {
        const U near = Abs(static_cast<U>(S16(::bof3::Img(at::kKind2XHigh.v)) - 0x1780)) + Abs(static_cast<U>(S16(::bof3::Img(at::kKind2ZHigh.v)) - 0x1380));
        U level = 0xC0u - static_cast<U>(I(near) / 3);
        if (I(level) > 0x80) level = 0x80;
        level = FloorZero(level);
        if (level != 0)
            SH_CALL(EffectKind18Sub44_DrawGlow)(I(level), I(AngleX(((Cond_AngleFB - 0x800u) & 0xFFF) - 0x400u) - 0x28u));
    }
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x70), 4) == 0) return;
    const std::int32_t x = S16(::bof3::Img(at::kKind2XHigh.v)), z = S16(::bof3::Img(at::kKind2ZHigh.v));
    const U dx = static_cast<U>(x - 0x1780), dz = static_cast<U>(z - 0xE00);
    U level = 0x100u - DivP2(Abs(dz) + Abs(dx), 2);
    if (I(level) > 0x80) level = 0x80;
    level = FloorZero(level);
    if (level == 0) return;
    const U r = static_cast<U>(SH_CALL(Math_Ratan2)(static_cast<float>(I(dx)), static_cast<float>(-z)));
    SH_CALL(EffectKind18Sub44_DrawGlow)(I(level), I(AngleX(((r + Cond_AngleFB - 0x400u) & 0xFFF) - 0x200u) - 0x28u));
}

// original 0x5115A0 (called twice by EffectKind18Sub44_Draw): a glow at screen
// x: a textured quad (x .. x + 0x4E, 72 .. 102) shaded `level` (page 0xD7,
// CLUT 0x7A00), then two more over it (page 0xB7) shaded by the vertex words
// of rows 0 and 1 times level / 128 (their u stepped 0x50, CLUT 0x1E6 + row),
// each committed (0x48).
extern "C" void __cdecl EffectKind18Sub44_DrawGlow(int level_word, int x_word) {
    const U level = static_cast<U>(level_word), x = static_cast<U>(x_word);
    const float left = static_cast<float>(Fi(x)), right = static_cast<float>(Fi(x + 0x4Eu));
    auto corners = [&](unsigned char* p) {
        SetF(p + 8, left);
        SetUL(p + 0xC, 0x42900000u);   // 72.0f
        SetF(p + 0x18, right);
        SetUL(p + 0x1C, 0x42900000u);
        SetF(p + 0x28, left);
        SetUL(p + 0x2C, 0x42CC0000u);  // 102.0f
        SetF(p + 0x38, right);
        SetUL(p + 0x3C, 0x42CC0000u);
    };
    {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        corners(p);
        p[4] = p[5] = p[6] = static_cast<unsigned char>(level);
        SetWord(p + 0x26, 0xD7);
        SetWord(p + 0x16, 0x7A00);
        p[0x14] = 0;
        p[0x15] = 0xC0;
        p[0x24] = 0x4F;
        p[0x25] = 0xC0;
        p[0x34] = 0;
        p[0x35] = 0xDF;
        p[0x44] = 0x4F;
        p[0x45] = 0xDF;
        SH_CALL(EffectKind18Sub44_Commit)(0x48);
    }
    for (U row = 0; row < 2; ++row) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        corners(p);
        for (unsigned c = 0; c < 3; ++c)
            p[4 + c] = static_cast<unsigned char>(DivP2(static_cast<U>(S16(VertexAddress(8 * row + 2 * c))) * level, 7));
        SetWord(p + 0x26, 0xB7);
        p[0x15] = 0xC0;
        p[0x25] = 0xC0;
        SetWord(p + 0x16, (row + 0x1E6) << 6);
        const unsigned char u = static_cast<unsigned char>(row * 0x50);
        p[0x14] = u;
        p[0x24] = static_cast<unsigned char>(u + 0x4F);
        p[0x34] = u;
        p[0x35] = 0xDF;
        p[0x44] = static_cast<unsigned char>(u + 0x4F);
        p[0x45] = 0xDF;
        SH_CALL(EffectKind18Sub44_Commit)(0x48);
    }
}

// original 0x511740 (called by EffectKind18Sub44_Draw when the vertex word
// 0x9037A8 is not 0, that word its level): a draw mode (page 0xB5, dtd); up to
// six TILE_1 stars, each of the first three twinkled
// (EffectKind18Sub44_DrawTwinkle): one at the camera's angle + 0x200, y 45; one
// at + 0x400 and y Field_Kind2Z / 85 - 1; one at y |0x1400 - z|^2 / 98304 +
// (x past 0xF00) (x - 0xF00) / 25 + 8; three of a size from the camera's
// distance to (0x1480, 0x1380), placed by Math_Ratan2 of a clamped point - each
// of the last five only while its y (size) is inside [0, 90) ([.., 89]). The
// shades from Rand and the level. The y is stored into the cursor's prim
// before the test, drawn or not.
extern "C" void __cdecl EffectKind18Sub44_DrawStars(int level_word) {
    const U level = static_cast<U>(level_word);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    SH_CALL(EffectKind18Sub44_Commit)(0xC);
    const unsigned char shade = static_cast<unsigned char>(DivP2(level * 0xFFu, 7));
    {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 0xC, 0x42340000u);   // 45.0f
        SetD(p + 8, Fi(AngleX((Cond_AngleFB + 0x200u) & 0xFFF)));
        p[4] = shade;
        p[5] = Twinkle(level);
        p[6] = Twinkle(level);
        SH_CALL(EffectKind18Sub44_Commit)(0x14);
        SH_CALL(EffectKind18Sub44_DrawTwinkle)(p);
    }
    {
        unsigned char* const p = Gfx_PacketNext;
        const float y = static_cast<float>(Fi(static_cast<U>(S16(::bof3::Img(at::kKind2ZHigh.v)) / 85 - 1)));
        SetF(p + 0xC, y);
        if (!(y < F(at::kZero)) && y < F(at::kNinety)) {
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[4] = shade;
            SetD(p + 8, Fi(AngleX((Cond_AngleFB + 0x400u) & 0xFFF)));
            p[5] = Twinkle(level);
            p[6] = Twinkle(level);
            SH_CALL(EffectKind18Sub44_Commit)(0x14);
            SH_CALL(EffectKind18Sub44_DrawTwinkle)(p);
        }
    }
    {
        unsigned char* const p = Gfx_PacketNext;
        const U dz = Abs(0x1400u - static_cast<U>(S16(::bof3::Img(at::kKind2ZHigh.v))));
        const U x_word = Word(At(::bof3::Img(at::kKind2XHigh.v)));
        const U past = static_cast<std::int16_t>(x_word & 0xFF00u) >= 0xF00 ? 1u : 0u;
        const std::int32_t step = (static_cast<std::int16_t>(x_word) - 0xF00) / 25;
        const U y = static_cast<U>(I(dz * dz) / 98304) + past * static_cast<U>(step) + 8u;
        const float fy = static_cast<float>(Fi(y));
        SetF(p + 0xC, fy);
        if (!(fy < F(at::kZero)) && fy < F(at::kNinety)) {
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            const U x = Sar((static_cast<U>(static_cast<std::int16_t>(Cond_AngleFB & 0xFFFF)) - 0x200u) * 10u, 5) +
                        static_cast<U>((S16(::bof3::Img(at::kKind2ZHigh.v)) - 0x1480) / 17);
            SetD(p + 8, Fi(x));
            for (unsigned c = 4; c < 7; ++c) {
                const U r = static_cast<U>(SH_CALL(Rand)());
                p[c] = static_cast<unsigned char>(DivP2(((r - 0x80u) & 0xFF) * level, 7));
            }
            SH_CALL(EffectKind18Sub44_Commit)(0x14);
            SH_CALL(EffectKind18Sub44_DrawTwinkle)(p);
        }
    }
    const std::int32_t x = S16(::bof3::Img(at::kKind2XHigh.v)), z = S16(::bof3::Img(at::kKind2ZHigh.v));
    U ax = Abs(static_cast<U>(x - 0x1480)), az = Abs(static_cast<U>(z - 0x1380));
    if (I(ax) > 0x900) ax = 0x900;
    if (I(az) > 0x400) az = 0x400;
    U size = static_cast<U>(I(0xD00u - az - ax) / 43 + 0x14);
    if (I(size) < 0x28) size = 0x28;
    const std::int32_t cx = static_cast<std::int16_t>(Word(At(::bof3::Img(at::kKind2XHigh.v)))) < 0xB00 ? 0xB00 : (x > 0x1E00 ? 0x1E00 : x);
    const std::int32_t cz = static_cast<std::int16_t>(Word(At(::bof3::Img(at::kKind2ZHigh.v)))) < 0xF00 ? 0xF00 : (z > 0x1800 ? 0x1800 : z);
    for (U back = 0; I(back) < 0x1E; back += 0xA, size += 8) {
        unsigned char* const p = Gfx_PacketNext;
        const float fs = static_cast<float>(Fi(size));
        SetF(p + 0xC, fs);
        if (fs > F(at::kEightyNine)) continue;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        const U r = static_cast<U>(SH_CALL(Math_Ratan2)(static_cast<float>(cz - 0x1380), static_cast<float>(cx - 0x1480)));
        SetD(p + 8, Fi(AngleX((r + Cond_AngleFB + 0x633u) & 0xFFF) - back));
        p[4] = Twinkle(level);
        p[5] = Twinkle(level);
        p[6] = shade;
        SH_CALL(EffectKind18Sub44_Commit)(0x14);
    }
}

// original 0x511BB0 (called by every draw of sub-kind 0x44): Gfx_CommitPrim's
// test against another list - when (Gfx_BufferIndex << 16) + 0x7F1BAC is above
// the cursor + (size & 0xFF), Gpu_LinkPrim(the last pointer at 0x802B34 + 8 *
// Gfx_BufferIndex, cursor), that pointer = the cursor (read again), the cursor
// up the size; else nothing. The buffer byte is not checked, as
// Gfx_CommitPrim's slot is not.
extern "C" void __cdecl EffectKind18Sub44_Commit(unsigned size) {
    unsigned char* const next = Gfx_PacketNext;
    const unsigned buffer = Gfx_BufferIndex;
    const U bytes = size & 0xFFu;
    const U limit = (static_cast<U>(buffer) << 16) + ::bof3::Img(at::kPoolLimit);
    if (limit <= AddressOf(next) + bytes) return;
    SH_CALL(Gpu_LinkPrim)(reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(ULEffect6c(::bof3::Img(at::kLayerTails.v) + 8 * buffer))),
                          AddressOf(next));
    unsigned char* const now = Gfx_PacketNext;
    SetUL(::bof3::Img(at::kLayerTails.v) + 8u * Gfx_BufferIndex, AddressOf(now));
    Gfx_PacketNext = now + bytes;
}

// original 0x511C10 (called by Area189_LeaderStart and Area189_StepBegin): the
// cell (x + 0x8000) >> 16, (z + 0x8000) >> 16 (bytes); its 16 x 16 block's byte
// from kBlockMap (by the high nibbles), whose nibbles with the cell's low ones
// index AreaMap_Corners; the corner dword's four s8 heights c0..c3 interpolated
// by the point's fraction about the cell's centre ((x - 0x8000) & 0xFFFF): the
// lower triangle (fx + fz <= 0x10000) from c0, toward c1 along x and c2 along
// z; the upper from c3. Each product divided by 0x10000 toward zero. Answers
// the height << 5 in eax (the callers keep ax).
extern "C" long __cdecl AreaMap_CornerHeight(long x, long z) {
    const U ux = static_cast<U>(x), uz = static_cast<U>(z);
    const U xi = Sar(ux + 0x8000u, 16) & 0xFF, zi = Sar(uz + 0x8000u, 16) & 0xFF;
    const U block = At(::bof3::Img(at::kBlockMap.v))[((xi >> 4) & 0xF) | (zi & 0xF0)];
    const U index = ((block & 0xF0) | (zi & 0xF)) * 0x60 + (((block & 0xF) << 4) | (xi & 0xF));
    const U corners = ULEffect6c(::bof3::Img(at::kAreaCorners.v) + 4 * index);
    const std::int32_t c0 = static_cast<signed char>(corners & 0xFF);
    const std::int32_t c1 = static_cast<signed char>((corners >> 8) & 0xFF);
    const std::int32_t c2 = static_cast<signed char>((corners >> 16) & 0xFF);
    const std::int32_t c3 = static_cast<signed char>(corners >> 24);
    const U fz = (uz - 0x8000u) & 0xFFFF, fx = (ux - 0x8000u) & 0xFFFF;
    U h;
    if (I(fz + fx) <= 0x10000) {
        const U along_z = DivP2(static_cast<U>(c2 - c0) * fz, 16);
        const U along_x = DivP2(static_cast<U>(c1 - c0) * fx, 16);
        h = along_x + static_cast<U>(c0) + along_z;
    } else {
        const U from_x = DivP2(static_cast<U>(c3 - c1) * (0x10000u - fz), 16);
        const U from_z = DivP2(static_cast<U>(c3 - c2) * (0x10000u - fx), 16);
        h = static_cast<U>(c3) - from_z - from_x;
    }
    return static_cast<long>(h << 5);
}

// original 0x511D50 (called by EffectKind18Sub44_DrawStars after each star):
// four TILE_1s about the star `tile` at the s8 offsets kTwinkle, each its
// colour >> 2 (the star's prim read after each new tile is set up), committed
// (0x14). The original clears ax on return; no caller reads it.
extern "C" void __cdecl EffectKind18Sub44_DrawTwinkle(unsigned char* tile) {
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetD(p + 8, Fi(static_cast<U>(S8(::bof3::Img(at::kTwinkle.v) + 2 * i))) + static_cast<double>(F(tile + 8)));
        SetD(p + 0xC, Fi(static_cast<U>(S8(::bof3::Img(at::kTwinkle.v) + 2 * i + 1))) + static_cast<double>(F(tile + 0xC)));
        p[4] = static_cast<unsigned char>(tile[4] >> 2);
        p[5] = static_cast<unsigned char>(tile[5] >> 2);
        p[6] = static_cast<unsigned char>(tile[6] >> 2);
        SH_CALL(EffectKind18Sub44_Commit)(0x14);
    }
}

void EffectKind18Sub44_Inject() {
    BOF3_INJECT(EffectKind18Sub40_Draw);
    BOF3_INJECT(EffectKind18Sub41_Run);
    BOF3_INJECT(EffectKind18Sub41_Start);
    BOF3_INJECT(EffectKind18Sub41_WaitOne);
    BOF3_INJECT(EffectKind18Sub41_Turn);
    BOF3_INJECT(EffectKind18Sub41_Hold);
    BOF3_INJECT(EffectKind18Sub41_Draw);
    BOF3_INJECT(EffectKind18Sub41_DrawPanels);
    BOF3_INJECT(EffectKind18Sub41_DrawRings);
    BOF3_INJECT(AreaMap_FrameAreaBD);
    BOF3_INJECT(AreaMapBD_BuildView);
    BOF3_INJECT(AreaMapBD_CellTexture);
    BOF3_INJECT(EffectKind18Sub44_Run);
    BOF3_INJECT(EffectKind18Sub44_Start);
    BOF3_INJECT(EffectKind18Sub44_Step);
    BOF3_INJECT(EffectKind18Sub44_Follow);
    BOF3_INJECT(EffectKind18Sub44_Draw);
    BOF3_INJECT(EffectKind18Sub44_DrawGlow);
    BOF3_INJECT(EffectKind18Sub44_DrawStars);
    BOF3_INJECT(EffectKind18Sub44_Commit);
    BOF3_INJECT(AreaMap_CornerHeight);
    BOF3_INJECT(EffectKind18Sub44_DrawTwinkle);
}
