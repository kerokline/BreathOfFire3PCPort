// Moved here by tools/move_funcs.py from effect_5c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_10.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"

namespace {

namespace at = effect_5c::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

std::int32_t SWEffect5c(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(I(v) >> n); }

// cdq; xor; sub: the absolute value of the 32 bits (0x80000000 stays itself).
U Abs(U v) {
    const U m = Sar(v, 31);
    return (v ^ m) - m;
}

// movsx eax, ax; cdq; sub eax, edx; sar eax, 1: a height word halved toward 0.
U HalfOf(long h) { return static_cast<U>(SWEffect5c(static_cast<U>(h)) / 2); }

// Prim_VertexScratch 0x9037A0: four SVECTORs of 8 bytes (a0, a8, b0, b8).
unsigned char* VSEffect5c(unsigned off) { return reinterpret_cast<unsigned char*>(Prim_VertexScratch) + off; }

const short* VX(unsigned off) { return reinterpret_cast<const short*>(VSEffect5c(off)); }

float* F(unsigned char* p) { return reinterpret_cast<float*>(p); }

// (x << 9) of a vertex word sign-extended and lifted by 0x4000: the 16.16 cell
// AreaMap_Elevation is asked for.
U CellOf(U word) { return static_cast<U>(SWEffect5c(word) + 0x4000) << 9; }

U VertexCell(unsigned off) { return CellOf(Word(VSEffect5c(off))); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through the
// dword after - the next table or data.
void DispatchEffect5c(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5c.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// The map: AreaMap_Header's first byte the row length, AreaMap_Bytes a byte a
// cell, AreaMap_Corners a dword a cell - each read again for every store, as
// the originals read them.
unsigned MapWidth() { return AreaMap_Header[0]; }

void SetCell(U offset, unsigned char v) { const_cast<unsigned char*>(AreaMap_Bytes.Native())[offset] = v; }

// Effect_FindFree's record (answer & 0xFF, as the originals widen it): a Fatal
// on an answer past the twenty (the callee answers 0xFF or 0..19; the original
// would write past the pool).
unsigned char* NewRecord(const char* who, unsigned index) {
    if (index >= 20)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past the pool "
                    "(docs/effect_5c.md section 6)",
                    who, index);
    return rv::Bytes(Effect_Objects) + index * 0x80u;
}

// Sub-kind 0x58's square round the projected point xy, half-size w:
// fild w; fld x; fsub st1; fst [+0x28]; fstp [+8]; fld st0; fadd x; fst
// [+0x38]; fstp [+0x18]; fld y; fsub st1; fst [+0x1C]; fstp [+0xC]; fadd y;
// fst [+0x3C]; fstp [+0x2C].
void Square(const float* xy, std::int32_t w, unsigned char* p) {
    __asm__ volatile(
        "fildl %[w]\n\t"
        "flds (%[xy])\n\t"
        "fsub %%st(1), %%st\n\t"
        "fsts 0x28(%[p])\n\t"
        "fstps 0x8(%[p])\n\t"
        "fld %%st(0)\n\t"
        "fadds (%[xy])\n\t"
        "fsts 0x38(%[p])\n\t"
        "fstps 0x18(%[p])\n\t"
        "flds 4(%[xy])\n\t"
        "fsub %%st(1), %%st\n\t"
        "fsts 0x1c(%[p])\n\t"
        "fstps 0xc(%[p])\n\t"
        "fadds 4(%[xy])\n\t"
        "fsts 0x3c(%[p])\n\t"
        "fstps 0x2c(%[p])\n\t"
        :
        : [w] "m"(w), [xy] "r"(xy), [p] "r"(p)
        : "st", "st(1)", "memory");
}

// Sub-kind 0x56's centre quad: corners (x - h, y - h) .. (x - h + f, y - h +
// f), each through the FPU as the original computes it (fild h; fild f; fld x;
// fsub st2; fadd st1; fstp ...).
void CentreQuad(const float* xy, std::int32_t h, std::int32_t f, unsigned char* p) {
    __asm__ volatile(
        "fildl %[h]\n\t"
        "flds (%[xy])\n\t"
        "fsub %%st(1), %%st\n\t"
        "fstps 0x8(%[p])\n\t"
        "flds 4(%[xy])\n\t"
        "fsub %%st(1), %%st\n\t"
        "fstps 0xc(%[p])\n\t"
        "fildl %[f]\n\t"
        "flds (%[xy])\n\t"
        "fsub %%st(2), %%st\n\t"
        "fadd %%st(1), %%st\n\t"
        "fstps 0x18(%[p])\n\t"
        "flds 4(%[xy])\n\t"
        "fsub %%st(2), %%st\n\t"
        "fstps 0x1c(%[p])\n\t"
        "flds (%[xy])\n\t"
        "fsub %%st(2), %%st\n\t"
        "fstps 0x28(%[p])\n\t"
        "flds 4(%[xy])\n\t"
        "fsub %%st(2), %%st\n\t"
        "fadd %%st(1), %%st\n\t"
        "fstps 0x2c(%[p])\n\t"
        "flds (%[xy])\n\t"
        "fsub %%st(2), %%st\n\t"
        "fadd %%st(1), %%st\n\t"
        "fstps 0x38(%[p])\n\t"
        "flds 4(%[xy])\n\t"
        "fsub %%st(2), %%st\n\t"
        "fadd %%st(1), %%st\n\t"
        "fstps 0x3c(%[p])\n\t"
        "fstp %%st(0)\n\t"
        "fstp %%st(0)\n\t"
        :
        : [h] "m"(h), [f] "m"(f), [xy] "r"(xy), [p] "r"(p)
        : "st", "st(1)", "st(2)", "memory");
}

// The four vertices in Prim_VertexScratch projected into a quad's four screen
// points at the given offsets (the depth out a local nothing reads).
void Project4Effect5c(unsigned char* prim, unsigned o0, unsigned o1, unsigned o2, unsigned o3) {
    long depth = 0;
    SH_CALL(Gte_RotTransPers4)(VX(0), VX(8), VX(0x10), VX(0x18), F(prim + o0), F(prim + o1), F(prim + o2), F(prim + o3),
                               &depth);
}

// MapView_LinkPrimAt at Sprite_Current's point (+0x34, +0x38; read afresh), row
// offset 1.
void LinkAtRecord(unsigned size) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(so->x), static_cast<std::uint32_t>(so->z), 1, size);
}

// The leader (ObjTrio record 0's x, z, both read first) within `xr` of x
// 0x428000 and within `zr` of z 0x3E0000 or 0x3F0000 - the originals' order of
// tests (abs by cdq / xor / sub, signed compares).
bool LeaderNear(U xr, U zr) {
    const U x = static_cast<std::uint32_t>(ObjTrio[0].x);
    const U z = static_cast<std::uint32_t>(ObjTrio[0].z);
    const std::int32_t dx = I(Abs(x - 0x428000u));
    if (dx <= I(xr) && I(Abs(z - 0x3E0000u)) <= I(zr)) return true;
    if (dx > I(xr)) return false;
    return I(Abs(z - 0x3F0000u)) <= I(zr);
}

// The map bytes of cells (0x41, 0x3E) and (0x41, 0x3F).
void Sub10Cells(unsigned char v) {
    SetCell(MapWidth() * 62u + 0x41u, v);
    SetCell(MapWidth() * 63u + 0x41u, v);
}

void Sub10Sound(unsigned short id) {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(id);
}

}  // namespace

// original 0x501BA0 (EffectKind18_States[16], hidden in 0x501A10):
// jmp [EffectKind18Sub10_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub10_Run(void) {
    DispatchEffect5c("EffectKind18Sub10_Run", AddressOf(EffectKind18Sub10_States), EffectKind18Sub10_States_count);
}

// original 0x501BC0 (sub-state 0): +0x30 = 0. Row-9 flag 0xF clear: the two
// cells' map bytes 0x52, +2 = 5. Set: +2 up, and with the leader near +0x30 =
// 0xFF00, +2 = 3. The panel either way.
extern "C" void __cdecl EffectKind18Sub10_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0);
    if (!SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x48), 0xF)) {
        Sub10Cells(0x52);
        S()[2] = 5;
        SH_CALL(EffectKind18Sub10_DrawPanel)();
        return;
    }
    unsigned char* const s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (LeaderNear(0x8000, 0x10000)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0xFF00);
        S()[2] = 3;
    }
    SH_CALL(EffectKind18Sub10_DrawPanel)();
}

// original 0x501CA0 (sub-state 1; sub-state 4 calls it): Cond_ByteFE set, or
// the leader near: sound 0x200 (no message open) and +2 = 2 - each test on its
// own. The panel.
extern "C" void __cdecl EffectKind18Sub10_Wait(void) {
    if (Cond_ByteFE != 0) {
        Sub10Sound(0x200);
        S()[2] = 2;
    }
    if (LeaderNear(0x8000, 0x10000)) {
        Sub10Sound(0x200);
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub10_DrawPanel)();
}

// original 0x501D40 (sub-state 2): +0x30 down 0x20; at -0x100 or below (s16) +2
// up. A tail jump to the panel.
extern "C" void __cdecl EffectKind18Sub10_SlideOut(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, Word(s + 0x30) + 0xFFE0u);
    if (SWEffect5c(rv::As<records::EffectWork>(s)->work_30) <= -0x100) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub10_DrawPanel)();
}

// original 0x501D60 (sub-state 3): Cond_ByteFE clear and the leader not within
// 0x18000 of the panel's cells: +2 = 4. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub10_Hold(void) {
    if (Cond_ByteFE != 0) return;
    if (LeaderNear(0x18000, 0x18000)) return;
    S()[2] = 4;
}

// original 0x501DD0 (sub-state 4): +0x30 up 0x20; still below 0 (s16): a tail
// jump to the panel. Else sub-state 1's test (it draws); unless that set +2 to
// 2, +2 = 1, sound 0x201 (no message open), and a tail jump to the panel.
extern "C" void __cdecl EffectKind18Sub10_SlideIn(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + 0x20u);
    if (SWEffect5c(rv::As<records::EffectWork>(s)->work_30) < 0) {
        SH_CALL(EffectKind18Sub10_DrawPanel)();
        return;
    }
    SH_CALL(EffectKind18Sub10_Wait)();
    s = S();
    if (s[2] == 2) return;
    s[2] = 1;
    Sub10Sound(0x201);
    SH_CALL(EffectKind18Sub10_DrawPanel)();
}

// original 0x501E20 (sub-state 5): row-9 flag 0xF set: the two cells' map bytes
// 0xA1, +2 = 1. A tail jump to the panel.
extern "C" void __cdecl EffectKind18Sub10_WaitFlag(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x48), 0xF)) {
        Sub10Cells(0xA1);
        S()[2] = 1;
    }
    SH_CALL(EffectKind18Sub10_DrawPanel)();
}

// original 0x501E80: with Draw_PassFlags bit 2 - a draw mode (page 0x95)
// committed at slot 6 (0xC); a textured POLY_FT4 at x 0xE0C0, z +0x30 - 0x2040
// to - 0x2140, its bottoms 0x40 and tops 0x180 off -(the ground / 2) (x
// 0x418000), texture 0x25500127, committed at slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub10_DrawPanel(void) {
    if ((Draw_PassFlags & 4) == 0) return;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    unsigned char* const s = S();
    SetWord(VSEffect5c(0x18), 0xE0C0);
    SetWord(VSEffect5c(0x10), 0xE0C0);
    SetWord(VSEffect5c(8), 0xE0C0);
    SetWord(VSEffect5c(0), 0xE0C0);
    const U z0 = Word(s + 0x30) - 0x2040u;
    const U ez = CellOf(z0);
    SetWord(VSEffect5c(0x12), z0);
    SetWord(VSEffect5c(2), z0);
    const U z1 = Word(s + 0x30) - 0x2140u;
    SetWord(VSEffect5c(0x1A), z1);
    SetWord(VSEffect5c(0xA), z1);
    long ground = SH_CALL(AreaMap_Elevation)(0x418000, static_cast<long>(ez));
    SetWord(VSEffect5c(4), 0x40u - HalfOf(ground));
    ground = SH_CALL(AreaMap_Elevation)(0x418000, static_cast<long>(ez));
    const U ez2 = VertexCell(0xA);
    const U ex2 = VertexCell(8);
    SetWord(VSEffect5c(0x14), 0x180u - HalfOf(ground));
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex2), static_cast<long>(ez2));
    SetWord(VSEffect5c(0xC), 0x40u - HalfOf(ground));
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex2), static_cast<long>(ez2));
    SetWord(VSEffect5c(0x1C), 0x180u - HalfOf(ground));
    Project4Effect5c(prim, 8, 0x18, 0x28, 0x38);
    SH_CALL(Gte_PrimDepths4_10)(prim);
    SH_CALL(Prim_SetTexture)(0x25500127, prim, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

// original 0x502020 (EffectKind18_States[86], hidden in 0x501E80):
// jmp [EffectKind18Sub56_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub56_Run(void) {
    DispatchEffect5c("EffectKind18Sub56_Run", AddressOf(EffectKind18Sub56_States), EffectKind18Sub56_States_count);
}

// original 0x502040 (sub-state 0): Cond_ByteFE set: +2 up.
extern "C" void __cdecl EffectKind18Sub56_Wait(void) {
    if (Cond_ByteFE != 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x502060 (sub-state 1): the level 0x80, or 4 * +9 below 0x20, or 4
// * (0x78 - +9) above 0x58; at 0x78 or more Effect_Release (and on). The rays at
// that level; +9 up (Sprite_Current read again).
extern "C" void __cdecl EffectKind18Sub56_Glow(void) {
    const unsigned t = S()[9];
    U level = 0x80;
    if (t < 0x20) level = t * 4u;
    if (t > 0x58) level = (0x78u - t) * 4u;
    if (t >= 0x78) SH_CALL(Effect_Release)();
    SH_CALL(EffectKind18Sub56_DrawRays)(level);
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
}

// original 0x5020C0: three fans (z -0x2120, -0x20C0, -0x2060; j 0..2) of eight
// semi-transparent textured POLY_FT4 rays each, their x spread by Math_Sin of
// an angle 16 (Frame_Counter & 7) + 0x80 i, their red / blue and green from the
// level (126 L - 16 L i - 2 f L) / 128 and (63 L - 8 L i - f L) / 128,
// committed at slot 4 (0x48); then a centre quad of half-size 16 + |3 -
// ((Frame_Counter / 3 + j) & 7)| round the point (0xE0C0, z, 0xFE40) projected
// into MapView_ScreenXY, shaded 96 L / 128 and 48 L / 128, linked at (0x3F0000,
// 0x3D0000) (0x48).
extern "C" void __cdecl EffectKind18Sub56_DrawRays(int level) {
    const U L = static_cast<U>(level);
    U z = 0xFFFFDEE0u;
    U j = 0;
    do {
        const U f = Frame_Counter & 7u;
        U angle = f << 4;
        const U c = f * L;
        const U down8 = (0u - L) << 3;
        U edge = 2u * f + 0x18u;
        U green = (L << 6) - L;
        const U down16 = (0u - L) << 4;
        U red = ((L << 6) - L) << 1;
        for (U i = 0; i < 8; ++i) {
            unsigned char* const prim = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(prim);
            SH_CALL(Gpu_SetShadeTex)(prim, 0);
            SH_CALL(Gpu_SetSemiTrans)(prim, 1);
            const U spread = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)) >> 5);
            const U lean = (j - 1u) * spread;
            const U left = spread - edge - 0x1F40u;
            const U right = edge + spread - 0x1F40u;
            SetWord(VSEffect5c(8), right);
            SetWord(VSEffect5c(0), left);
            SetWord(VSEffect5c(0x18), right);
            const U near = z - edge + lean;
            SetWord(VSEffect5c(2), near);
            const U y = (0xFFFFFFCEu - i * 8u - f) << 3;
            SetWord(VSEffect5c(0xA), near);
            SetWord(VSEffect5c(0x10), left);
            const U far = lean + z + edge;
            SetWord(VSEffect5c(0x12), far);
            SetWord(VSEffect5c(0x1A), far);
            SetWord(VSEffect5c(4), y);
            SetWord(VSEffect5c(0xC), y);
            SetWord(VSEffect5c(0x14), y);
            SetWord(VSEffect5c(0x1C), y);
            Project4Effect5c(prim, 8, 0x18, 0x28, 0x38);
            SH_CALL(Gte_PrimDepths4_10)(prim);
            prim[0x14] = 0xE0;
            prim[0x15] = 0x30;
            prim[0x24] = 0xFF;
            prim[0x25] = 0x30;
            prim[0x34] = 0xE0;
            prim[0x35] = 0x4F;
            prim[0x44] = 0xFF;
            prim[0x45] = 0x4F;
            SetWord(prim + 0x26, 0x3B);
            SetWord(prim + 0x16, 0x78CA);
            prim[4] = static_cast<unsigned char>(I(red - 2u * c) / 128);
            prim[5] = static_cast<unsigned char>(I(green - c) / 128);
            prim[6] = static_cast<unsigned char>(I(red - 2u * c) / 128);
            SH_CALL(Gfx_CommitPrim)(4, 0x48);
            red += down16;
            edge += 0x10;
            angle += 0x80;
            green += down8;
        }
        SetWord(VSEffect5c(0), 0xE0C0);
        SetWord(VSEffect5c(2), z);
        SetWord(VSEffect5c(4), 0xFE40);
        long depth = 0;
        SH_CALL(Gte_RotTransPers)(VX(0), reinterpret_cast<unsigned long*>(MapView_ScreenXY), &depth);
        const U turn = 3u - (((Frame_Counter / 3u) + j) & 7u);
        const U half = Abs(turn) + 0x10u;
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        CentreQuad(MapView_ScreenXY, I(half), I(half * 2u), prim);
        SH_CALL(Gte_PrimDepthFlat4_10)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        prim[0x14] = 0xE0;
        prim[0x15] = 0x30;
        prim[0x24] = 0xFF;
        prim[0x25] = 0x30;
        prim[0x34] = 0xE0;
        prim[0x35] = 0x4F;
        prim[0x44] = 0xFF;
        prim[0x45] = 0x4F;
        SetWord(prim + 0x26, 0x3B);
        prim[5] = static_cast<unsigned char>(I(L * 48u) / 128);
        SetWord(prim + 0x16, 0x78CA);
        const unsigned char outer = static_cast<unsigned char>(I(L * 96u) / 128);
        prim[4] = outer;
        prim[6] = outer;
        SH_CALL(MapView_LinkPrimAt)(0x3F0000, 0x3D0000, 1, 0x48);
        z += 0x60;
        ++j;
    } while (I(z) < I(0xFFFFE000u));
}

// original 0x502470 (EffectKind18_States[87], hidden in 0x5020C0):
// jmp [EffectKind18Sub57_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub57_Run(void) {
    DispatchEffect5c("EffectKind18Sub57_Run", AddressOf(EffectKind18Sub57_States), EffectKind18Sub57_States_count);
}

// original 0x502490 (sub-state 0): Cond_ByteFE set: +9 = 1, +0xB = 0, +2 up.
extern "C" void __cdecl EffectKind18Sub57_Wait(void) {
    if (Cond_ByteFE == 0) return;
    unsigned char* const s = S();
    s[9] = 1;
    s[0xB] = 0;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x5024C0 (sub-state 1): Cond_ByteFE clear: a tail jump to
// Effect_Release. Else +9 down; at 0, a free record (none: nothing) made kind
// 0x18, sub-kind 0x58 (+1), sub-state 0, at point +0xB of EffectKind18Sub57_Points
// (x, z words << 12); +9 = 0x14 + (Rand & 0xF); +0xB = (+0xB + 1) & 7.
extern "C" void __cdecl EffectKind18Sub57_Spawn(void) {
    static const char kWho[] = "EffectKind18Sub57_Spawn";
    if (Cond_ByteFE == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] != 0) return;
    const unsigned n = SH_CALL(Effect_FindFree)() & 0xFFu;
    if (n == 0xFF) return;
    unsigned char* const cur = S();
    unsigned char* const rec = NewRecord(kWho, n);
    auto* const reco = rv::As<records::EffectObject>(rec);
    rec[0] = 1;
    rec[5] = 0x18;
    rec[1] = 0x58;
    rec[2] = 0;
    unsigned k = cur[0xB];
    if (k >= at::kSub57PointCount)
        bof3::Fatal("%s: +0xB is %u, past the %u points of 0x%X - the original reads the table after "
                    "(docs/effect_5c.md section 6)",
                    kWho, k, at::kSub57PointCount, (unsigned)AddressOf(EffectKind18Sub57_Points));
    reco->x = static_cast<U>(Word(At(AddressOf(EffectKind18Sub57_Points) + 4 * k))) << 12;
    k = cur[0xB];
    reco->z = static_cast<U>(Word(At(AddressOf(EffectKind18Sub57_Points) + 4 * k + 2))) << 12;
    const int r = SH_CALL(Rand)();
    S()[9] = static_cast<unsigned char>((r & 0xF) + 0x14);
    s = S();
    s[0xB] = static_cast<unsigned char>((s[0xB] + 1) & 7);
}

// original 0x502580 (EffectKind18_States[88], hidden in 0x5020C0):
// jmp [EffectKind18Sub58_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub58_Run(void) {
    DispatchEffect5c("EffectKind18Sub58_Run", AddressOf(EffectKind18Sub58_States), EffectKind18Sub58_States_count);
}

// original 0x5025A0 (sub-state 0): +9 = 0; sound 0x202; +2 up.
extern "C" void __cdecl EffectKind18Sub58_Start(void) {
    S()[9] = 0;
    SH_CALL(Sound_PlayEffect)(0x202);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x5025C0 (sub-state 1): the column at +9; +9 up; past 7: +9 = 0, +2
// up.
extern "C" void __cdecl EffectKind18Sub58_Grow(void) {
    SH_CALL(EffectKind18Sub58_DrawColumn)(S()[9]);
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (s[9] > 7) {
        s[9] = 0;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
}

// original 0x502600 (sub-state 2): the column at 7; +9 up; past 0x1E: +9 =
// 0x1F, +2 up.
extern "C" void __cdecl EffectKind18Sub58_Hold(void) {
    SH_CALL(EffectKind18Sub58_DrawColumn)(7);
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (s[9] > 0x1E) {
        s[9] = 0x1F;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
}

// original 0x502630 (sub-state 3): the column at +9 / 4; +9 down; below 8 a
// tail jump to Effect_Release.
extern "C" void __cdecl EffectKind18Sub58_Fade(void) {
    SH_CALL(EffectKind18Sub58_DrawColumn)(static_cast<unsigned>(S()[9]) >> 2);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] < 8) SH_CALL(Effect_Release)();
}

// original 0x502670: eight textured squares up the record's point (x, z =
// +0x34 / +0x38 >> 9 - 0x4000), the y of square c -0x80 - (b + 8 c) n (b 4 on
// odd frames), each projected into MapView_ScreenXY (Gte_RotTransPers),
// half-size (m n) / 2 + (b n) / 16 + 4 (m = c for the four upper, 4 below),
// texture 0xBA009124 | ((y' - (b << 19)) / 4 / (8 - n)) & 0xF80000 for y' =
// 0x1F80000 down 0x400000 a square; the green byte halved; linked at the point
// (0x48). n = 8 divides by zero in the original; ours aborts.
extern "C" void __cdecl EffectKind18Sub58_DrawColumn(unsigned n) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    const U b = (Frame_Counter & 1u) << 2;
    const U x = Sar(static_cast<std::uint32_t>(so->x), 9) - 0x4000u;
    const U z = Sar(static_cast<std::uint32_t>(so->z), 9) - 0x4000u;
    const U lift = static_cast<U>(I(b * n) / 16);
    const U divisor = 8u - n;
    const U base = b << 19;
    U c = 0;
    U y = 0x1F80000u;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        SetWord(VSEffect5c(0), x);
        SetWord(VSEffect5c(2), z);
        SetWord(VSEffect5c(4), 0xFFFFFF80u - (b + c * 8u) * n);
        long depth = 0;
        SH_CALL(Gte_RotTransPers)(VX(0), reinterpret_cast<unsigned long*>(MapView_ScreenXY), &depth);
        SH_CALL(Gte_PrimDepthFlat4_10)(prim);
        const U m = I(y) < 0x1380000 ? 4u : c;
        const U half = static_cast<U>(I(m * n) / 2) + lift + 4u;
        if (divisor == 0)
            bof3::Fatal("EffectKind18Sub58_DrawColumn: called with %u - the original divides by 8 - it, zero "
                        "(docs/effect_5c.md section 6)",
                        n);
        const U band = static_cast<U>(I(static_cast<U>(I(y - base) / 4)) / I(divisor));
        Square(MapView_ScreenXY, I(half), prim);
        SH_CALL(Prim_SetTexture)((band & 0xF80000u) | 0xBA009124u, prim, 1);
        prim[5] = static_cast<unsigned char>(prim[5] >> 1);
        LinkAtRecord(0x48);
        y -= 0x400000u;
        ++c;
    } while (I(y) > I(0xFFF80000u));
}

void EffectKind18Sub10_Inject() {
    BOF3_INJECT(EffectKind18Sub10_Run);
    BOF3_INJECT(EffectKind18Sub10_Start);
    BOF3_INJECT(EffectKind18Sub10_Wait);
    BOF3_INJECT(EffectKind18Sub10_SlideOut);
    BOF3_INJECT(EffectKind18Sub10_Hold);
    BOF3_INJECT(EffectKind18Sub10_SlideIn);
    BOF3_INJECT(EffectKind18Sub10_WaitFlag);
    BOF3_INJECT(EffectKind18Sub10_DrawPanel);
    BOF3_INJECT(EffectKind18Sub56_Run);
    BOF3_INJECT(EffectKind18Sub56_Wait);
    BOF3_INJECT(EffectKind18Sub56_Glow);
    BOF3_INJECT(EffectKind18Sub56_DrawRays);
    BOF3_INJECT(EffectKind18Sub57_Run);
    BOF3_INJECT(EffectKind18Sub57_Wait);
    BOF3_INJECT(EffectKind18Sub57_Spawn);
    BOF3_INJECT(EffectKind18Sub58_Run);
    BOF3_INJECT(EffectKind18Sub58_Start);
    BOF3_INJECT(EffectKind18Sub58_Grow);
    BOF3_INJECT(EffectKind18Sub58_Hold);
    BOF3_INJECT(EffectKind18Sub58_Fade);
    BOF3_INJECT(EffectKind18Sub58_DrawColumn);
}
