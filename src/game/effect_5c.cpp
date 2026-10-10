// Round thirteen group E5C (docs/effect_5c.md): the cut's 60 functions of
// analysis/round13_cut.tsv's group E5C and two starts no list holds (0x5015E0,
// a state 0x501520 jumps into, and 0x5016E0), 0x501500..0x503DDD, each read
// with capstone to its last instruction (2026-10-03). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; kind 0x18's handler jumps through
// EffectKind18_States by +1, and each entry here is a sub-kind: a dispatcher by
// +2 through its own table (none bounded by a compare) and the sub-states it
// names, or (0x16) one handler. What each sub-kind is, as far as the code says:
//
//   0x50 (80)   a textured wall (a vertical quad) at one of two cells by the
//               record's x byte, its texture picked by a row-9 flag, and a
//               flickering line above it; while another row-9 flag is clear it
//               sinks over nine frames, waits, and flashes a grey quad back
//   0x10 (16)   a textured panel at cell x 0x41 sliding along z by +0x30 when
//               the leader comes within half a cell of (0x42, 0x3E..0x3F) or
//               Cond_ByteFE is set, back when the leader leaves; the map bytes
//               of cells (0x41, 0x3E) and (0x41, 0x3F) set to 0x52 / 0xA1 by
//               row-9 flag 0xF; sounds 0x200 / 0x201
//   0x11 (17),  fifteen textured floor tiles over the cells of a table, the map
//   0x12 (18)   bytes of (0x45, 0x3C) and (0x46, 0x3C) set to 0x10 while they
//               stand; when a story flag is set they hold ten frames, clear
//               the cells, and sink and fade (sub-kind 0x11: flag 0x22, or 0x21
//               with Cond_ByteFA at most 7; 0x12: flag 0x20)
//   0x15 (21)   the camera angle and geometry offset set, then each frame a
//               320-wide band of textured strips and gradient quads committed
//               to draw layer 15's third list, and the map's corner heights of
//               columns 0x2D..0x30 round Field_Kind2Z's row rebuilt
//   0x16 (22)   a ribbon of sixteen shaded textured quads waving with
//               Frame_Counter at a point set on the first frame
//   0x17 (23)   a scripted sequence on the counter 0x903848: a point moved by
//               steps, E5D's draw-move and quad draws each frame, a second
//               record of the sub-kind spawned at its state 10, waits on two
//               enemies' bit 14 of +0x12 in game mode 5 step 5
//   0x56 (86)   eight fans of textured rays and a centre quad, faded in and out
//               over 0x78 frames once Cond_ByteFE is set
//   0x57 (87)   while Cond_ByteFE is set, a sub-kind 0x58 record spawned every
//               0x14..0x23 frames at one of eight points in turn
//   0x58 (88)   a column of eight textured squares at the record's point that
//               grows, holds and fades; sound 0x202
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies.
// Sprite_Current is read again wherever the original reads [0x937F88] again.
// No divergence: each is a faithful replacement. Where the original jumps
// through a state table past its end, indexes one of the image's small tables
// past its entries, divides by zero (sub-kind 0x58's column at 8) or writes
// the record Effect_FindFree did not find (sub-kind 0x17's spawn), ours
// aborts with a message (docs/effect_5c.md section 6).
#include "game/effect_5c.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5c_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"

namespace {

namespace at = effect_5c::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;
using StepFn = void(__cdecl*)(unsigned);
using QuadsFn = void(__cdecl*)(unsigned);
using MovesFn = void(__cdecl*)(void);

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
std::int32_t SW(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(I(v) >> n); }
// cdq; xor; sub: the absolute value of the 32 bits (0x80000000 stays itself).
U Abs(U v) {
    const U m = Sar(v, 31);
    return (v ^ m) - m;
}
// movsx eax, ax; cdq; sub eax, edx; sar eax, 1: a height word halved toward 0.
U HalfOf(long h) { return static_cast<U>(SW(static_cast<U>(h)) / 2); }

// Prim_VertexScratch 0x9037A0: four SVECTORs of 8 bytes (a0, a8, b0, b8).
unsigned char* VS(unsigned off) { return reinterpret_cast<unsigned char*>(Prim_VertexScratch) + off; }
const short* VX(unsigned off) { return reinterpret_cast<const short*>(VS(off)); }
float* F(unsigned char* p) { return reinterpret_cast<float*>(p); }
void Float(unsigned char* p, std::int32_t v) {
    const float f = static_cast<float>(v);   // fild / fstp: exact for these magnitudes
    std::memcpy(p, &f, 4);
}
// (x << 9) of a vertex word sign-extended and lifted by 0x4000: the 16.16 cell
// AreaMap_Elevation is asked for.
U CellOf(U word) { return static_cast<U>(SW(word) + 0x4000) << 9; }
U VertexCell(unsigned off) { return CellOf(Word(VS(off))); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through the
// dword after - the next table or data.
void Dispatch(const char* who, U table, unsigned entries) {
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
void SetCell(U offset, unsigned char v) { const_cast<unsigned char*>(AreaMap_Bytes)[offset] = v; }
void SetCorner(U index, U v) { SetUL(At(AddressOf(&AreaMap_Corners) + 4 * index), v); }

// --- sub-kind 0x50's small tables (the variant and the flag index) -----------------------
unsigned Sub50Variant(const char* who, unsigned v) {
    if (v >= at::kSub50Variants)
        bof3::Fatal("%s: variant %u, past the %u cells of 0x%X - the original reads the bytes after "
                    "(docs/effect_5c.md section 6)",
                    who, v, at::kSub50Variants, (unsigned)AddressOf(EffectKind18Sub50_Cells));
    return v;
}
unsigned char Sub50Byte(const char* who, U table, unsigned index) {
    if (index >= at::kSub50FlagCount)
        bof3::Fatal("%s: +0xB is %u, past the %u flag bytes of 0x%X - the original reads the bytes after "
                    "(docs/effect_5c.md section 6)",
                    who, index, at::kSub50FlagCount, (unsigned)table);
    return At(table + index)[0];
}

// Effect_FindFree's record (answer & 0xFF, as the originals widen it): a Fatal
// on an answer past the twenty (the callee answers 0xFF or 0..19; the original
// would write past the pool).
unsigned char* NewRecord(const char* who, unsigned index) {
    if (index >= 20)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past the pool "
                    "(docs/effect_5c.md section 6)",
                    who, index);
    return Effect_Objects + index * 0x80u;
}

// --- x87 as the original has it (the game's control word: round to nearest, 53
// bits); every value goes through the FPU as Capcom's does.

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
void Project4(unsigned char* prim, unsigned o0, unsigned o1, unsigned o2, unsigned o3) {
    long depth = 0;
    SH_CALL(Gte_RotTransPers4)(VX(0), VX(8), VX(0x10), VX(0x18), F(prim + o0), F(prim + o1), F(prim + o2), F(prim + o3),
                               &depth);
}
// MapView_LinkPrimAt at Sprite_Current's point (+0x34, +0x38; read afresh), row
// offset 1.
void LinkAtRecord(unsigned size) {
    unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, size);
}

// E5D's three (raw until E5D merges).
void Step(unsigned step) { SH_AT(StepFn, at::kDrawMoveStep)(step); }
void Quads(unsigned variant) { SH_AT(QuadsFn, at::kDrawQuads)(variant); }
void Moves() { SH_AT(MovesFn, at::kDrawMoves)(); }

}  // namespace

// ===========================================================================
// Sub-kind 0x50: EffectKind18_States[80] (0x6541AC), EffectKind18Sub50_States (5)
// ===========================================================================

// original 0x501500 (EffectKind18_States[80], hidden in E5B's 0x500EF0):
// jmp [EffectKind18Sub50_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub50_Run(void) {
    Dispatch("EffectKind18Sub50_Run", AddressOf(EffectKind18Sub50_States), EffectKind18Sub50_States_count);
}

// original 0x501520 (sub-state 0): the variant +0xB = the x byte +0x36; the
// point (+0x34, +0x38) the variant's cell (0x65E0AC, x and z bytes << 16);
// +0x3E = -(the ground there / 2) - the variant's height word (0x65E0B0);
// +0xB up one; row-9 flag 0x65E094[+0xB] set: +2 up and a tail jump to
// sub-state 1, clear: +2 = 3 and a tail jump to sub-state 3.
extern "C" void __cdecl EffectKind18Sub50_Start(void) {
    static const char kWho[] = "EffectKind18Sub50_Start";
    unsigned char* s = S();
    s[0xB] = s[0x36];
    unsigned v = Sub50Variant(kWho, s[0xB]);
    SetUL(s + 0x34, static_cast<U>(At(AddressOf(EffectKind18Sub50_Cells) + 2 * v)[0]) << 16);
    v = Sub50Variant(kWho, s[0xB]);
    SetUL(s + 0x38, static_cast<U>(At(AddressOf(EffectKind18Sub50_Cells) + 2 * v + 1)[0]) << 16);
    const long ground = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    s = S();
    v = Sub50Variant(kWho, s[0xB]);
    SetWord(s + 0x3E, (0u - HalfOf(ground)) - Word(At(AddressOf(EffectKind18Sub50_Heights) + 2 * v)));
    s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
    const unsigned char flag = Sub50Byte(kWho, AddressOf(EffectKind18Sub50_Flags), s[0xB]);
    const unsigned char set = SH_CALL(Flags_Test)(At(at::kFlagRow9), flag);
    s = S();
    if (set) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        SH_CALL(EffectKind18Sub50_Shine)();
        return;
    }
    s[2] = 3;
    SH_CALL(EffectKind18Sub50_WaitSet)();
}

// original 0x5015E0 (sub-state 1; also the tail of sub-state 0): row-9 flag
// 0x65E094[+0xB] clear: +9 = 9, +2 up. Then the wall, its texture 1 + +0xB +
// 4 * row-9 flag 0x65E034[+0xB], height 0x40, and a tail jump to the line.
extern "C" void __cdecl EffectKind18Sub50_Shine(void) {
    static const char kWho[] = "EffectKind18Sub50_Shine";
    unsigned char* s = S();
    if (!SH_CALL(Flags_Test)(At(at::kFlagRow9), Sub50Byte(kWho, AddressOf(EffectKind18Sub50_Flags), s[0xB]))) {
        s = S();
        s[9] = 9;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    s = S();
    const U light = SH_CALL(Flags_Test)(At(at::kFlagRow9), Sub50Byte(kWho, at::kSub50Lights, s[0xB])) & 0xFFu;
    s = S();
    SH_CALL(EffectKind18Sub50_DrawWall)(s[0xB] + light * 4u + 1u, 0x40);
    SH_CALL(EffectKind18Sub50_DrawLine)();
}

// original 0x501660 (sub-state 2): +9 down; at 0 +2 up. The bare wall
// (texture 0, height 0x40), then the lit one at height +9 * 8; a tail jump to
// the line.
extern "C" void __cdecl EffectKind18Sub50_Sink(void) {
    static const char kWho[] = "EffectKind18Sub50_Sink";
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub50_DrawWall)(0, 0x40);
    s = S();
    const U height = static_cast<U>(s[9]) << 3;
    const unsigned char flag = Sub50Byte(kWho, at::kSub50Lights, s[0xB]);
    const U light = SH_CALL(Flags_Test)(At(at::kFlagRow9), flag) & 0xFFu;
    s = S();
    SH_CALL(EffectKind18Sub50_DrawWall)(s[0xB] + light * 4u + 1u, height);
    SH_CALL(EffectKind18Sub50_DrawLine)();
}

// original 0x5016E0 (sub-state 3; also the tail of sub-state 0; a start no list
// of the cut holds): row-9 flag 0x65E094[+0xB] set: +9 = 9, +2 up. The bare
// wall.
extern "C" void __cdecl EffectKind18Sub50_WaitSet(void) {
    unsigned char* s = S();
    if (SH_CALL(Flags_Test)(At(at::kFlagRow9), Sub50Byte("EffectKind18Sub50_WaitSet", AddressOf(EffectKind18Sub50_Flags), s[0xB]))) {
        s = S();
        s[9] = 9;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectKind18Sub50_DrawWall)(0, 0x40);
}

// original 0x501730 (sub-state 4): +9 down; at 0 +2 = 1. A draw mode (page
// 0xB5) linked at the point (0xC), the lit wall, then a grey semi-transparent
// POLY_F4 (shade +9 * 0x1E) over the wall's vertices, linked (0x38); a call to
// the line.
extern "C" void __cdecl EffectKind18Sub50_Flash(void) {
    static const char kWho[] = "EffectKind18Sub50_Flash";
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] == 0) s[2] = 1;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    LinkAtRecord(0xC);
    s = S();
    const unsigned char flag = Sub50Byte(kWho, at::kSub50Lights, s[0xB]);
    const U light = SH_CALL(Flags_Test)(At(at::kFlagRow9), flag) & 0xFFu;
    s = S();
    SH_CALL(EffectKind18Sub50_DrawWall)(s[0xB] + light * 4u + 1u, 0x40);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const unsigned char shade = static_cast<unsigned char>(S()[9] * 0x1E);
    prim[6] = shade;
    prim[5] = shade;
    prim[4] = shade;
    Project4(prim, 8, 0x14, 0x20, 0x2C);
    SH_CALL(Gte_PrimDepths4_0C)(prim);
    LinkAtRecord(0x38);
    SH_CALL(EffectKind18Sub50_DrawLine)();
}

// original 0x501850: the wall - x the record's x cell (+0x36 << 7 - 0x3FC0),
// z from +0x3A << 7 - 0x4008 + height to - 0x3FF8 - height, each end's bottom
// and top 8 - height and + height off -(the ground / 2) - +0x3E - into
// Prim_VertexScratch; a textured POLY_FT4 (texture + 0x285000F3), linked at the
// point (0x48).
extern "C" void __cdecl EffectKind18Sub50_DrawWall(unsigned texture, unsigned height) {
    unsigned char* const s = S();
    const U b = height;
    const U x = (static_cast<U>(Word(s + 0x36)) << 7) - 0x3FC0u;
    SetWord(VS(0x18), x);
    SetWord(VS(0x10), x);
    SetWord(VS(8), x);
    SetWord(VS(0), x);
    const U z0 = (static_cast<U>(Word(s + 0x3A)) << 7) + b - 0x4008u;
    const U ex = CellOf(x);
    const U ez = CellOf(z0);
    SetWord(VS(0x12), z0);
    SetWord(VS(2), z0);
    const U z1 = (static_cast<U>(Word(s + 0x3A)) << 7) - b - 0x3FF8u;
    SetWord(VS(0x1A), z1);
    SetWord(VS(0xA), z1);
    long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex), static_cast<long>(ez));
    SetWord(VS(4), (0u - HalfOf(ground)) - Word(S() + 0x3E) - b + 8u);
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex), static_cast<long>(ez));
    const U ez2 = VertexCell(0xA);
    const U ex2 = VertexCell(8);
    SetWord(VS(0x14), (0u - HalfOf(ground)) - Word(S() + 0x3E) + b);
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex2), static_cast<long>(ez2));
    SetWord(VS(0xC), (0u - HalfOf(ground)) - Word(S() + 0x3E) - b + 8u);
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex2), static_cast<long>(ez2));
    SetWord(VS(0x1C), (0u - HalfOf(ground)) - Word(S() + 0x3E) + b);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    Project4(prim, 8, 0x18, 0x28, 0x38);
    SH_CALL(Gte_PrimDepths4_10)(prim);
    SH_CALL(Prim_SetTexture)(texture + at::kTexture50, prim, 1);
    LinkAtRecord(0x48);
}

// original 0x501A10: a draw mode (page 0x95, dtd) linked at the point (0xC); a
// semi-transparent LINE_F2 (shade 0 or 0x80 by Frame_Counter's bit 0) from
// vertex a0 to a8, each end 2 * (Frame_Counter % 48) - 0x3C - +0x3E - the
// ground / 2 high, projected with b0 (Gte_RotTransPers3, the third point into
// MapView_ScreenXY), linked (0x20); a draw mode (page 0x95) linked (0xC).
extern "C" void __cdecl EffectKind18Sub50_DrawLine(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    LinkAtRecord(0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const unsigned char shade = static_cast<unsigned char>((Frame_Counter & 1u) << 7);
    prim[6] = shade;
    prim[5] = shade;
    prim[4] = shade;
    long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(VertexCell(0)), static_cast<long>(VertexCell(2)));
    U rise = 2u * (Frame_Counter % 0x30u) - 0x3Cu;
    const U ex2 = VertexCell(8);
    const U ez2 = VertexCell(0xA);
    SetWord(VS(4), rise - Word(S() + 0x3E) - HalfOf(ground));
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex2), static_cast<long>(ez2));
    rise = 2u * (Frame_Counter % 0x30u) - 0x3Cu;
    SetWord(VS(0xC), rise - Word(S() + 0x3E) - HalfOf(ground));
    long depth = 0;
    SH_CALL(Gte_RotTransPers3)(VX(0), VX(8), VX(0x10), F(prim + 8), F(prim + 0x14), MapView_ScreenXY, &depth);
    float third = 0;
    SH_CALL(Gte_StoreDepthF3)(F(prim + 0x10), F(prim + 0x1C), &third);
    LinkAtRecord(0x20);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    LinkAtRecord(0xC);
}

// ===========================================================================
// Sub-kind 0x10: EffectKind18_States[16] (0x6540AC), EffectKind18Sub10_States (6)
// ===========================================================================

namespace {
// The leader (ObjTrio record 0's x, z, both read first) within `xr` of x
// 0x428000 and within `zr` of z 0x3E0000 or 0x3F0000 - the originals' order of
// tests (abs by cdq / xor / sub, signed compares).
bool LeaderNear(U xr, U zr) {
    const U x = UL(At(at::kLeaderX));
    const U z = UL(At(at::kLeaderZ));
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
    Dispatch("EffectKind18Sub10_Run", AddressOf(EffectKind18Sub10_States), EffectKind18Sub10_States_count);
}

// original 0x501BC0 (sub-state 0): +0x30 = 0. Row-9 flag 0xF clear: the two
// cells' map bytes 0x52, +2 = 5. Set: +2 up, and with the leader near +0x30 =
// 0xFF00, +2 = 3. The panel either way.
extern "C" void __cdecl EffectKind18Sub10_Start(void) {
    SetWord(S() + 0x30, 0);
    if (!SH_CALL(Flags_Test)(At(at::kFlagRow9), 0xF)) {
        Sub10Cells(0x52);
        S()[2] = 5;
        SH_CALL(EffectKind18Sub10_DrawPanel)();
        return;
    }
    unsigned char* const s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (LeaderNear(0x8000, 0x10000)) {
        SetWord(S() + 0x30, 0xFF00);
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
    SetWord(s + 0x30, Word(s + 0x30) + 0xFFE0u);
    if (SW(Word(s + 0x30)) <= -0x100) s[2] = static_cast<unsigned char>(s[2] + 1);
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
    SetWord(s + 0x30, Word(s + 0x30) + 0x20u);
    if (SW(Word(s + 0x30)) < 0) {
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
    if (SH_CALL(Flags_Test)(At(at::kFlagRow9), 0xF)) {
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
    SetWord(VS(0x18), 0xE0C0);
    SetWord(VS(0x10), 0xE0C0);
    SetWord(VS(8), 0xE0C0);
    SetWord(VS(0), 0xE0C0);
    const U z0 = Word(s + 0x30) - 0x2040u;
    const U ez = CellOf(z0);
    SetWord(VS(0x12), z0);
    SetWord(VS(2), z0);
    const U z1 = Word(s + 0x30) - 0x2140u;
    SetWord(VS(0x1A), z1);
    SetWord(VS(0xA), z1);
    long ground = SH_CALL(AreaMap_Elevation)(0x418000, static_cast<long>(ez));
    SetWord(VS(4), 0x40u - HalfOf(ground));
    ground = SH_CALL(AreaMap_Elevation)(0x418000, static_cast<long>(ez));
    const U ez2 = VertexCell(0xA);
    const U ex2 = VertexCell(8);
    SetWord(VS(0x14), 0x180u - HalfOf(ground));
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex2), static_cast<long>(ez2));
    SetWord(VS(0xC), 0x40u - HalfOf(ground));
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex2), static_cast<long>(ez2));
    SetWord(VS(0x1C), 0x180u - HalfOf(ground));
    Project4(prim, 8, 0x18, 0x28, 0x38);
    SH_CALL(Gte_PrimDepths4_10)(prim);
    SH_CALL(Prim_SetTexture)(0x25500127, prim, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

// ===========================================================================
// Sub-kind 0x56: EffectKind18_States[86] (0x6541C4), EffectKind18Sub56_States (2)
// ===========================================================================

// original 0x502020 (EffectKind18_States[86], hidden in 0x501E80):
// jmp [EffectKind18Sub56_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub56_Run(void) {
    Dispatch("EffectKind18Sub56_Run", AddressOf(EffectKind18Sub56_States), EffectKind18Sub56_States_count);
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
            SetWord(VS(8), right);
            SetWord(VS(0), left);
            SetWord(VS(0x18), right);
            const U near = z - edge + lean;
            SetWord(VS(2), near);
            const U y = (0xFFFFFFCEu - i * 8u - f) << 3;
            SetWord(VS(0xA), near);
            SetWord(VS(0x10), left);
            const U far = lean + z + edge;
            SetWord(VS(0x12), far);
            SetWord(VS(0x1A), far);
            SetWord(VS(4), y);
            SetWord(VS(0xC), y);
            SetWord(VS(0x14), y);
            SetWord(VS(0x1C), y);
            Project4(prim, 8, 0x18, 0x28, 0x38);
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
        SetWord(VS(0), 0xE0C0);
        SetWord(VS(2), z);
        SetWord(VS(4), 0xFE40);
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

// ===========================================================================
// Sub-kind 0x57: EffectKind18_States[87] (0x6541C8), EffectKind18Sub57_States (2)
// ===========================================================================

// original 0x502470 (EffectKind18_States[87], hidden in 0x5020C0):
// jmp [EffectKind18Sub57_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub57_Run(void) {
    Dispatch("EffectKind18Sub57_Run", AddressOf(EffectKind18Sub57_States), EffectKind18Sub57_States_count);
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
    rec[0] = 1;
    rec[5] = 0x18;
    rec[1] = 0x58;
    rec[2] = 0;
    unsigned k = cur[0xB];
    if (k >= at::kSub57PointCount)
        bof3::Fatal("%s: +0xB is %u, past the %u points of 0x%X - the original reads the table after "
                    "(docs/effect_5c.md section 6)",
                    kWho, k, at::kSub57PointCount, (unsigned)AddressOf(EffectKind18Sub57_Points));
    SetUL(rec + 0x34, static_cast<U>(Word(At(AddressOf(EffectKind18Sub57_Points) + 4 * k))) << 12);
    k = cur[0xB];
    SetUL(rec + 0x38, static_cast<U>(Word(At(AddressOf(EffectKind18Sub57_Points) + 4 * k + 2))) << 12);
    const int r = SH_CALL(Rand)();
    S()[9] = static_cast<unsigned char>((r & 0xF) + 0x14);
    s = S();
    s[0xB] = static_cast<unsigned char>((s[0xB] + 1) & 7);
}

// ===========================================================================
// Sub-kind 0x58: EffectKind18_States[88] (0x6541CC), EffectKind18Sub58_States (4)
// ===========================================================================

// original 0x502580 (EffectKind18_States[88], hidden in 0x5020C0):
// jmp [EffectKind18Sub58_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub58_Run(void) {
    Dispatch("EffectKind18Sub58_Run", AddressOf(EffectKind18Sub58_States), EffectKind18Sub58_States_count);
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
    const U b = (Frame_Counter & 1u) << 2;
    const U x = Sar(UL(s + 0x34), 9) - 0x4000u;
    const U z = Sar(UL(s + 0x38), 9) - 0x4000u;
    const U lift = static_cast<U>(I(b * n) / 16);
    const U divisor = 8u - n;
    const U base = b << 19;
    U c = 0;
    U y = 0x1F80000u;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        SetWord(VS(0), x);
        SetWord(VS(2), z);
        SetWord(VS(4), 0xFFFFFF80u - (b + c * 8u) * n);
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

// ===========================================================================
// Sub-kinds 0x11 and 0x12: EffectKind18_States[17] / [18] (0x6540B0 / 0x6540B4),
// EffectKind18Sub11_States / Sub12_States (4 each)
// ===========================================================================

namespace {
// Sub-kind 0x11's end: story flag 0x22, or 0x21 with Cond_ByteFA (s8) at most
// 7 - in the original's order; sub-kind 0x12's: story flag 0x20.
bool Sub11Done() {
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x22)) return true;
    if (!SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x21)) return false;
    return Cond_ByteFA <= 7;
}
bool Sub12Done() { return SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x20) != 0; }
// The map bytes of cells (0x45, 0x3C) and (0x46, 0x3C).
void TileCells(unsigned char v) {
    SetCell(MapWidth() * 60u + 0x45u, v);
    SetCell(MapWidth() * 60u + 0x46u, v);
}
// The fifteen tiles of `table` (x, z, row offset bytes): a horizontal textured
// POLY_FT4 a cell wide at y, z widened by the shade, texture shade << 16 |
// 0xB600B102, linked at the cell (0x48).
void Tiles(U table, U y, U shade) {
    const U texture = (shade << 16) | at::kTextureTiles;
    for (U e = table + 1; e < table + 1 + 3 * at::kTileCount; e += 3) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        const U x = static_cast<U>(At(e - 1)[0]) << 7;
        SetWord(VS(0x1C), y);
        SetWord(VS(0x14), y);
        SetWord(VS(0x10), x - 0x4040u);
        SetWord(VS(0), x - 0x4040u);
        const U z = static_cast<U>(At(e)[0]) << 7;
        SetWord(VS(0x18), x - 0x3FC0u);
        SetWord(VS(8), x - 0x3FC0u);
        SetWord(VS(0xC), y);
        SetWord(VS(4), y);
        SetWord(VS(0xA), z - shade - 0x4040u);
        SetWord(VS(2), z - shade - 0x4040u);
        SetWord(VS(0x1A), z + shade - 0x4040u);
        SetWord(VS(0x12), z + shade - 0x4040u);
        Project4(prim, 8, 0x18, 0x28, 0x38);
        SH_CALL(Gte_PrimDepths4_10)(prim);
        SH_CALL(Prim_SetTexture)(texture, prim, 1);
        SH_CALL(MapView_LinkPrimAt)(static_cast<U>(At(e - 1)[0]) << 16, static_cast<U>(At(e)[0]) << 16, At(e + 1)[0], 0x48);
    }
}
// Sub-state 0: done - Cond_ByteFE = 1 and a tail jump to Effect_Release; else
// the cells 0x10, the tiles at -0x100 shade 0x80, +2 up.
template <bool (*Done)(), void (*Draw)(unsigned, unsigned)> void TileStart() {
    if (Done()) {
        Cond_ByteFE = 1;
        SH_CALL(Effect_Release)();
        return;
    }
    TileCells(0x10);
    Draw(0xFFFFFF00u, 0x80);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}
// Sub-state 1: done - Cond_ByteFE = 1, +0x3C = -0x100, +0xB = 0x80, +9 = 0xA,
// +2 up, the tiles at -0x100 shade 0x80. Else nothing (not even drawn).
template <bool (*Done)(), void (*Draw)(unsigned, unsigned)> void TileWait() {
    if (!Done()) return;
    unsigned char* const s = S();
    Cond_ByteFE = 1;
    SetUL(s + 0x3C, 0xFFFFFF00u);
    s[0xB] = 0x80;
    s[9] = 0xA;
    s[2] = static_cast<unsigned char>(s[2] + 1);
    Draw(0xFFFFFF00u, 0x80);
}
// Sub-state 2: the tiles at +0x3C shade 0x80; +9 down; at 0 the cells 0, +2 up.
template <void (*Draw)(unsigned, unsigned)> void TileOpen() {
    Draw(UL(S() + 0x3C), 0x80);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) return;
    TileCells(0);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}
// Sub-state 3: the tiles at +0x3C shade +0xB & 0xF8; +0x3C up 1; +0xB down 2;
// above -0xC0 a tail jump to Effect_Release.
template <void (*Draw)(unsigned, unsigned)> void TileSink() {
    unsigned char* s = S();
    Draw(UL(s + 0x3C), s[0xB] & 0xF8u);
    s = S();
    SetUL(s + 0x3C, UL(s + 0x3C) + 1u);
    s[0xB] = static_cast<unsigned char>(s[0xB] - 2);
    if (I(UL(s + 0x3C)) > -0xC0) SH_CALL(Effect_Release)();
}
void Draw11(unsigned y, unsigned shade) { SH_CALL(EffectKind18Sub11_DrawTiles)(y, shade); }
void Draw12(unsigned y, unsigned shade) { SH_CALL(EffectKind18Sub12_DrawTiles)(y, shade); }
}  // namespace

// original 0x502810 (EffectKind18_States[17], hidden in 0x502670):
// jmp [EffectKind18Sub11_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub11_Run(void) {
    Dispatch("EffectKind18Sub11_Run", AddressOf(EffectKind18Sub11_States), EffectKind18Sub11_States_count);
}
// original 0x502830 (sub-state 0).
extern "C" void __cdecl EffectKind18Sub11_Start(void) { TileStart<&Sub11Done, &Draw11>(); }
// original 0x5028C0 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub11_Wait(void) { TileWait<&Sub11Done, &Draw11>(); }
// original 0x502940 (sub-state 2).
extern "C" void __cdecl EffectKind18Sub11_Open(void) { TileOpen<&Draw11>(); }
// original 0x5029B0 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub11_Sink(void) { TileSink<&Draw11>(); }
// original 0x502A00: the fifteen tiles of EffectKind18Sub11_Tiles 0x65E11C.
extern "C" void __cdecl EffectKind18Sub11_DrawTiles(unsigned y, unsigned shade) { Tiles(AddressOf(EffectKind18Sub11_Tiles), y, shade); }

// original 0x502B30 (EffectKind18_States[18], hidden in 0x502A00):
// jmp [EffectKind18Sub12_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub12_Run(void) {
    Dispatch("EffectKind18Sub12_Run", AddressOf(EffectKind18Sub12_States), EffectKind18Sub12_States_count);
}
// original 0x502B50 (sub-state 0).
extern "C" void __cdecl EffectKind18Sub12_Start(void) { TileStart<&Sub12Done, &Draw12>(); }
// original 0x502BC0 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub12_Wait(void) { TileWait<&Sub12Done, &Draw12>(); }
// original 0x502C20 (sub-state 2).
extern "C" void __cdecl EffectKind18Sub12_Open(void) { TileOpen<&Draw12>(); }
// original 0x502C90 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub12_Sink(void) { TileSink<&Draw12>(); }
// original 0x502CE0: the fifteen tiles of EffectKind18Sub12_Tiles 0x65E15C.
extern "C" void __cdecl EffectKind18Sub12_DrawTiles(unsigned y, unsigned shade) { Tiles(AddressOf(EffectKind18Sub12_Tiles), y, shade); }

// ===========================================================================
// Sub-kind 0x15: EffectKind18_States[21] (0x6540C0), EffectKind18Sub15_States (2)
// ===========================================================================

// original 0x502E10 (EffectKind18_States[21], hidden in 0x502CE0):
// jmp [EffectKind18Sub15_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub15_Run(void) {
    Dispatch("EffectKind18Sub15_Run", AddressOf(EffectKind18Sub15_States), EffectKind18Sub15_States_count);
}

// original 0x502E30 (sub-state 0): Camera_Angles' first word 0xFCC0;
// Gte_SetGeomOffset(0xA0, 0x90); +2 up.
extern "C" void __cdecl EffectKind18Sub15_Start(void) {
    SetWord(reinterpret_cast<unsigned char*>(Camera_Angles), 0xFCC0);
    SH_CALL(Gte_SetGeomOffset)(0xA0, 0x90);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

namespace {
// A texture-window RECT {x, 0, w, h} of 8 bytes at the cursor, the cursor past
// it, and a draw mode (page 0x95) with that window committed at slot 7 (0xC).
void WindowMode(U x, U w) {
    unsigned char* const rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetWord(rect, x);
    SetWord(rect + 2, 0);
    SetWord(rect + 6, w);
    SetWord(rect + 4, w);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, AddressOf(rect));
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
}
}  // namespace

// original 0x502E60 (sub-state 1): a 32 x 32 texture window at (0xE0, 0) and an
// unshaded textured quad 0..320 x 56..88 committed at slot 7 (0x48); a 256 x
// 256 window and three textured quads scrolled by Frame_Counter / 8 (64 high,
// edges 0x00 / 0xFF / 0x3F in u) committed to layer 15's list; a draw mode
// (page 0xB5, dtd) and four semi-transparent gradient quads (white above,
// black below) across 0..320 whose lower edges rise with Field_Kind2X; a draw
// mode (page 0xB5); then AreaMap_Corners of columns 0x2D..0x30 rebuilt round
// Field_Kind2Z's row: rows row + 3 and row - 3 flat (0x20 each corner), rows
// row + 4 .. row + 13 and row - 4 .. row - 17 a slope from 0x20 up.
//
// DIV-0041 (2026-10-07, the owner's bridge of area 41): under the wide picture
// the haze band and the gradients run from -53 to 373, the strips are clipped
// to those bounds instead of 0..320 with one more strip on the left so the
// scroll never leaves a gap, and a strip that starts past the right bound is
// skipped (the original's x0 > x1 quad, off its picture, was the growing
// sliver in the band). Widescreen_Fill() is 0 until the self-tests have run,
// so the fuzz and every narrow run get the original's packets bit for bit.
extern "C" void __cdecl EffectKind18Sub15_Draw(void) {
    const U wide = Widescreen_Fill();
    const U left_bits = std::bit_cast<U>(Widescreen_FillX());                           // 0.0f narrow
    const U right_bits = std::bit_cast<U>(320.0f + static_cast<float>(wide));           // 0x43A00000 narrow
    WindowMode(0xE0, 0x20);
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetShadeTex)(prim, 1);
    SetUL(prim + 0xC, 0x42600000u);   // 56.0f
    SetUL(prim + 0x1C, 0x42600000u);
    SetUL(prim + 0x2C, 0x42B00000u);   // 88.0f
    SetUL(prim + 0x3C, 0x42B00000u);
    SetUL(prim + 8, left_bits);
    SetUL(prim + 0x18, right_bits);   // 320.0f
    SetUL(prim + 0x28, left_bits);
    SetUL(prim + 0x38, right_bits);
    prim[0x14] = 0;
    prim[0x15] = 0;
    prim[0x24] = 0xFF;
    prim[0x25] = 0;
    prim[0x34] = 0;
    prim[0x35] = 0x40;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x40;
    SetWord(prim + 0x26, 0x95);
    SetWord(prim + 0x16, 0x7900);
    SH_CALL(Gfx_CommitPrim)(7, 0x48);
    WindowMode(0, 0x100);
    // The wide picture: strips of 256 at phase - 512, - 256, 0 and + 256,
    // each clipped to [-wide, 320 + wide] with the u range the clip leaves
    // (the original's rule: u runs from the clipped-off width on the left,
    // and to the visible width less one on the right).
    for (U col = wide ? 0u : 0x100u; wide && col < 0x400; col += 0x100) {
        const std::int32_t left = static_cast<std::int32_t>((Frame_Counter >> 3) & 0xFFu) + static_cast<std::int32_t>(col) - 0x200;
        const std::int32_t lo = -static_cast<std::int32_t>(wide), hi = 320 + static_cast<std::int32_t>(wide);
        const std::int32_t x0 = left > lo ? left : lo, x1 = left + 0x100 < hi ? left + 0x100 : hi;
        if (x1 <= x0) continue;
        prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 1);
        Float(prim + 8, x0);
        Float(prim + 0x28, x0);
        Float(prim + 0x18, x1);
        Float(prim + 0x38, x1);
        prim[0x14] = prim[0x34] = static_cast<unsigned char>(x0 - left);
        prim[0x24] = prim[0x44] = static_cast<unsigned char>(x1 - left - 1);
        SetUL(prim + 0x1C, 0);
        SetUL(prim + 0xC, 0);
        SetUL(prim + 0x3C, 0x42800000u);   // 64.0f
        SetUL(prim + 0x2C, 0x42800000u);
        prim[0x25] = 0;
        prim[0x15] = 0;
        prim[0x45] = 0x3F;
        prim[0x35] = 0x3F;
        SetWord(prim + 0x26, 0x99);
        SetWord(prim + 0x16, 0x7980);
        SH_CALL(EffectKind18Sub15_LinkLayer)(0x48);
    }
    for (U col = 0; !wide && col < 0x300; col += 0x100) {
        prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 1);
        const U left = ((Frame_Counter >> 3) & 0xFFu) + col - 0x100u;
        if (I(left) < 0) {
            SetUL(prim + 0x28, 0);
            SetUL(prim + 8, 0);
            const unsigned char u = static_cast<unsigned char>(0u - (left & 0xFFu));
            prim[0x34] = u;
            prim[0x14] = u;
            Float(prim + 0x38, I(left + 0x100u));
            Float(prim + 0x18, I(left + 0x100u));
            prim[0x44] = 0xFF;
            prim[0x24] = 0xFF;
        } else {
            Float(prim + 0x28, I(left));
            Float(prim + 8, I(left));
            const U right = left + 0x100u;
            if (I(right) > 0x140) {
                const unsigned char u = static_cast<unsigned char>(0x3Fu - (left & 0xFFu));
                SetUL(prim + 0x38, 0x43A00000u);
                SetUL(prim + 0x18, 0x43A00000u);
                prim[0x34] = 0;
                prim[0x14] = 0;
                prim[0x44] = u;
                prim[0x24] = u;
            } else {
                prim[0x34] = 0;
                prim[0x14] = 0;
                Float(prim + 0x38, I(right));
                Float(prim + 0x18, I(right));
                prim[0x44] = 0xFF;
                prim[0x24] = 0xFF;
            }
        }
        SetUL(prim + 0x1C, 0);
        SetUL(prim + 0xC, 0);
        SetUL(prim + 0x3C, 0x42800000u);   // 64.0f
        SetUL(prim + 0x2C, 0x42800000u);
        prim[0x25] = 0;
        prim[0x15] = 0;
        prim[0x45] = 0x3F;
        prim[0x35] = 0x3F;
        SetWord(prim + 0x26, 0x99);
        SetWord(prim + 0x16, 0x7980);
        SH_CALL(EffectKind18Sub15_LinkLayer)(0x48);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    SH_CALL(EffectKind18Sub15_LinkLayer)(0xC);
    const U shift = 0u - Sar(static_cast<U>(Field_Kind2X) - 0x2D0000u, 15);
    // the four quads at the cursor as it was here, 0x44 apart whatever the
    // commits did (the original's esi is never read again from the cursor)
    unsigned char* quad = Gfx_PacketNext;
    for (U k = 0; k < 4; ++k, quad += 0x44) {
        SH_CALL(Gpu_SetPolyG4)(quad);
        SH_CALL(Gpu_SetSemiTrans)(quad, 1);
        const U x0 = Word(At(AddressOf(EffectKind18Sub15_Columns) + 2 * k));
        const U x1 = Word(At(AddressOf(EffectKind18Sub15_Columns) + 2 * k + 2));
        Float(quad + 8, I(x0));
        SetUL(quad + 0xC, 0x42800000u);
        Float(quad + 0x18, I(x1));
        SetUL(quad + 0x1C, 0x42800000u);
        Float(quad + 0x28, I(x0));
        Float(quad + 0x2C, I(At(AddressOf(EffectKind18Sub15_Rows) + k)[0] + shift));
        Float(quad + 0x38, I(x1));
        Float(quad + 0x3C, I(At(AddressOf(EffectKind18Sub15_Rows) + k + 1)[0] + shift));
        quad[4] = 0xFF;
        quad[5] = 0xFF;
        quad[6] = 0xFF;
        quad[0x14] = 0xFF;
        quad[0x15] = 0xFF;
        quad[0x16] = 0xFF;
        quad[0x24] = 0;
        quad[0x25] = 0;
        quad[0x26] = 0;
        quad[0x34] = 0;
        quad[0x35] = 0;
        quad[0x36] = 0;
        SH_CALL(EffectKind18Sub15_LinkLayer)(0x44);
    }
    // The wide picture's bands: a gradient each side, flat at the outer
    // rows' height (the table's 90), from -wide to 0 and from 320 to 320 + wide.
    // They are written at the four quads' cursor, which runs on whether or not
    // a link was refused, so both are skipped - nothing written, nothing
    // linked - unless they fit under this buffer's pool limit (the test
    // EffectKind18Sub15_LinkLayer makes). A refused link above implies the
    // skip: the cursor is then at least 0x44 past Gfx_PacketNext.
    const U bands = 2u * 0x44u;
    const bool bands_fit = wide && (static_cast<U>(Gfx_BufferIndex) << 16) + at::kPoolLimit > AddressOf(quad) + bands;
    if (wide && !bands_fit && draw_order::Tagging())
        bof3::Log("draworder   packet pool full: the sky's wide bands (%u bytes) skipped (EffectKind18Sub15_Draw)", bands);
    for (U k = 0; bands_fit && k < 2; ++k, quad += 0x44) {
        SH_CALL(Gpu_SetPolyG4)(quad);
        SH_CALL(Gpu_SetSemiTrans)(quad, 1);
        const std::int32_t x0 = k == 0 ? -static_cast<std::int32_t>(wide) : 320;
        const std::int32_t x1 = k == 0 ? 0 : 320 + static_cast<std::int32_t>(wide);
        const std::int32_t row = At(AddressOf(EffectKind18Sub15_Rows) + (k == 0 ? 0u : 4u))[0] + static_cast<std::int32_t>(shift);
        Float(quad + 8, x0);
        SetUL(quad + 0xC, 0x42800000u);
        Float(quad + 0x18, x1);
        SetUL(quad + 0x1C, 0x42800000u);
        Float(quad + 0x28, x0);
        Float(quad + 0x2C, row);
        Float(quad + 0x38, x1);
        Float(quad + 0x3C, row);
        quad[4] = quad[5] = quad[6] = 0xFF;
        quad[0x14] = quad[0x15] = quad[0x16] = 0xFF;
        quad[0x24] = quad[0x25] = quad[0x26] = 0;
        quad[0x34] = quad[0x35] = quad[0x36] = 0;
        SH_CALL(EffectKind18Sub15_LinkLayer)(0x44);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    SH_CALL(EffectKind18Sub15_LinkLayer)(0xC);
    for (U i = 0; i < 0xA; ++i) {
        const U flat = static_cast<U>(SW(Word(At(at::kKind2ZCell)))) + 3u;
        const U lo = static_cast<U>(I(i) / 2) + 0x20u;
        const U hi = static_cast<U>(I(i + 1) / 2) + 0x20u;
        const U slope = lo | (lo << 8) | (hi << 16) | (hi << 24);
        const U row = static_cast<U>(SW(Word(At(at::kKind2ZCell)))) + i + 4u;
        for (U c = 0x2D; c < 0x31; ++c) {
            SetCorner(MapWidth() * flat + c, 0x20202020u);
            SetCorner(MapWidth() * row + c, slope);
        }
    }
    for (U j = 0; j < 0xE; ++j) {
        const U centre = static_cast<U>(SW(Word(At(at::kKind2ZCell))));
        const U flat = centre - 3u;
        const U lo = static_cast<U>(I(j + 1) / 2) + 0x20u;
        const U hi = static_cast<U>(I(j) / 2) + 0x20u;
        const U slope = lo | (lo << 8) | (hi << 16) | (hi << 24);
        const U row = centre - j - 4u;
        for (U c = 0x2D; c < 0x31; ++c) {
            SetCorner(MapWidth() * flat + c, 0x20202020u);
            SetCorner(MapWidth() * row + c, slope);
        }
    }
}

// original 0x5032C0: the primitive at the cursor (size & 0xFF bytes) appended
// to draw layer 15's third list for this buffer (0x802594 + 8 *
// Gfx_BufferIndex: Gpu_LinkPrim from its last pointer, which then becomes the
// primitive) and the cursor moved on - if the pool has room, as Gfx_CommitPrim
// tests it, and silently not at all if it has not. The buffer byte is not
// checked, as Gfx_CommitPrim's slot is not.
extern "C" void __cdecl EffectKind18Sub15_LinkLayer(unsigned size) {
    unsigned char* const next = Gfx_PacketNext;
    const unsigned buffer = Gfx_BufferIndex;
    const U bytes = size & 0xFFu;
    const U limit = (static_cast<U>(buffer) << 16) + at::kPoolLimit;
    if (limit <= AddressOf(next) + bytes) {
        if (draw_order::Tagging())
            bof3::Log("draworder   packet pool full: the sky's %u-byte primitive skipped (EffectKind18Sub15_LinkLayer)", bytes);
        return;
    }
    SH_CALL(Gpu_LinkPrim)(reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(UL(At(at::kLayer15Tails + 8 * buffer)))),
                          AddressOf(next));
    unsigned char* const now = Gfx_PacketNext;
    SetUL(At(at::kLayer15Tails + 8u * Gfx_BufferIndex), AddressOf(now));
    Gfx_PacketNext = now + bytes;
}

// ===========================================================================
// Sub-kind 0x16: EffectKind18_States[22] (0x6540C4), one handler
// ===========================================================================

namespace {
// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32 (the original's 0x20-byte local).
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");
// Math_Cos(((frame + k) << 9) & 0xFFF) * scale >> 11, + 0x80: a shade byte.
unsigned char Wave(U angle, U scale) {
    const int c = SH_CALL(Math_Cos)(static_cast<int>((angle << 9) & 0xFFFu));
    return static_cast<unsigned char>((I(static_cast<U>(c) * scale) >> 11) + 0x80);
}
}  // namespace

// original 0x503320 (EffectKind18_States[22], hidden in 0x5032C0; the code past
// its jmp over padding at 0x503360 is its own, nothing else reaches it): on +2
// = 0 the point (+0x34, +0x38, +0x3C) = (-0x3900, -0x3880, -0x7F0) and +2 up.
// Then each frame: the GTE matrix pushed; a draw mode (page 0x95, dtd)
// committed at slot 4; the point turned by the camera (Gte_RotTrans into the
// translation, a zero rotation times Camera_Matrix) and set; sixteen shaded
// POLY_GT4 quads a ribbon 16 wide each along x, waving in y with Math_Sin of
// Frame_Counter, shaded by Math_Cos, committed at slot 4 (0x54); a draw mode
// (page 0x95); the matrix popped.
extern "C" void __cdecl EffectKind18Sub16_Run(void) {
    unsigned char* s = S();
    if (s[2] == 0) {
        SetUL(s + 0x34, 0xFFFFC700u);
        SetUL(s + 0x38, 0xFFFFC780u);
        SetUL(s + 0x3C, 0xFFFFF810u);
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    s = S();
    const short angles[3] = {0, 0, 0};
    const short point[3] = {static_cast<short>(Word(s + 0x34)), static_cast<short>(Word(s + 0x38)),
                            static_cast<short>(Word(s + 0x3C))};
    Matrix m = {};
    SH_CALL(Gte_RotTrans)(point, m.t);
    SH_CALL(Gte_RotMatrix)(angles, m.m);
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
    for (U b = 0; b < 0x10; ++b) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyGT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        const U half = static_cast<U>(I(b) / 2);
        const unsigned char u0 = static_cast<unsigned char>(b * 0xFEu - half + 0x28u);
        SetWord(prim + 0x2A, 0x1B);
        const unsigned char u1 = static_cast<unsigned char>(0u - half - (b & 1u) - (b << 1) + 0x26u);
        SetWord(prim + 0x16, 0x78CF);
        prim[0x14] = u0;
        prim[0x15] = 0x70;
        prim[0x28] = u1;
        prim[0x29] = 0x70;
        prim[0x3C] = u0;
        prim[0x3D] = 0x90;
        prim[0x50] = u1;
        prim[0x51] = 0x90;
        const U before = b ? b - 1u : 0u;
        unsigned char shade = Wave(before + Frame_Counter, before);
        prim[6] = shade;
        prim[5] = shade;
        prim[4] = shade;
        shade = Wave(Frame_Counter + b, b);
        prim[0x2E] = shade;
        prim[0x2D] = shade;
        prim[0x2C] = shade;
        prim[0x1A] = shade;
        prim[0x19] = shade;
        prim[0x18] = shade;
        shade = Wave(Frame_Counter + b + 1u, b + 1u);
        prim[0x42] = shade;
        prim[0x41] = shade;
        prim[0x40] = shade;
        SetWord(VS(0x10), static_cast<U>(I(b << 8) / 16));
        SetWord(VS(0), static_cast<U>(I(b << 8) / 16));
        SetWord(VS(0x18), static_cast<U>(I((b + 1u) << 8) / 16));
        SetWord(VS(8), static_cast<U>(I((b + 1u) << 8) / 16));
        const U back = b ? b - 1u : 0u;
        int sine = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter - back) << 9) & 0xFFFu));
        SetWord(VS(2), static_cast<U>(I(static_cast<U>(sine) * back) >> 12));
        sine = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter - b) << 9) & 0xFFFu));
        const U mid = static_cast<U>(I(static_cast<U>(sine) * b) >> 12);
        const U next = Frame_Counter - b - 1u;
        SetWord(VS(0x12), mid);
        SetWord(VS(0xA), mid);
        sine = SH_CALL(Math_Sin)(static_cast<int>((next << 9) & 0xFFFu));
        SetWord(VS(0x1A), static_cast<U>(I(static_cast<U>(sine) * (b + 1u)) >> 12));
        SetWord(VS(0xC), 0xFF40);
        SetWord(VS(4), 0xFF40);
        SetWord(VS(0x1C), 0);
        SetWord(VS(0x14), 0);
        Project4(prim, 8, 0x1C, 0x30, 0x44);
        SH_CALL(Gte_PrimDepths4_14)(prim);
        SH_CALL(Gfx_CommitPrim)(4, 0x54);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    SH_CALL(Gte_PopMatrix)();
}

// ===========================================================================
// Sub-kind 0x17: EffectKind18_States[23] (0x6540C8), EffectKind18Sub17_States (16)
// ===========================================================================

namespace {
// The tail most of its sub-states share: every fifth frame E5D's draw-move
// step (Frame_Counter / 5) & 3; E5D's quads variants 0 and 1; a tail jump to
// E5D's four draw-moves.
void Sub17Tail() {
    const U f = Frame_Counter;
    if (f % 5u == 0) Step((f / 5u) & 3u);
    Quads(0);
    Quads(1);
    Moves();
}
// The enemy wait: in game mode 5 step 5 with bit 14 of the dword at `bits`, +2
// up. Then +9 at 0xF wraps to 0; on every fifth +9 E5D's step +9 / 5 + 7 (the
// record read again after it). Answers the record +9 is stepped on.
unsigned char* EnemyWait(U bits) {
    if (Game_Mode == 5 && Game_Step == 5 && (UL(At(bits)) & 0x4000u) != 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    unsigned char* s = S();
    if (s[9] == 0xF) {
        s[9] = 0;
        s = S();
    }
    const unsigned t = s[9];
    if (t % 5 == 0) {
        Step(t / 5 + 7);
        s = S();
    }
    return s;
}
// The settle: +9 at 0xF wraps to 0; +0x32 (s16) above 0 counts down, else at
// +9 / 5 = 0 +2 up; every fifth +9 E5D's step +9 / 5 + 7; +9 up.
void Settle() {
    unsigned char* s = S();
    if (s[9] == 0xF) {
        s[9] = 0;
        s = S();
    }
    const U w = Word(s + 0x32);
    if (SW(w) > 0) {
        SetWord(s + 0x32, w - 1u);
    } else if (s[9] / 5 == 0) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    s = S();
    const unsigned t = s[9];
    if (t % 5 == 0) {
        Step(t / 5 + 7);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
}
}  // namespace

// original 0x503660 (EffectKind18_States[23], hidden in 0x5032C0):
// jmp [EffectKind18Sub17_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub17_Run(void) {
    Dispatch("EffectKind18Sub17_Run", AddressOf(EffectKind18Sub17_States), EffectKind18Sub17_States_count);
}

// original 0x503680 (sub-state 0): the point (+0x34, +0x38) = (0x570000,
// 0x140000), +0x3E = 0xFF00, +0x32 = 0, +9 = 0, +0xB = 0; +2 up.
extern "C" void __cdecl EffectKind18Sub17_Start(void) {
    unsigned char* const s = S();
    SetUL(s + 0x34, 0x570000);
    SetUL(s + 0x38, 0x140000);
    SetWord(s + 0x3E, 0xFF00);
    SetWord(s + 0x32, 0);
    s[9] = 0;
    s[0xB] = 0;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x5036D0 (sub-state 1): at the counter 0x903848 = 0xC: +2 up,
// Cond_ByteFE = 0x23, and the record Effect_FindFree answers made kind 0x18,
// sub-kind 0x17, sub-state 0xA - unchecked: on none free (0xFF) the original
// writes 0x7F80 past the pool, ours aborts. Then the shared tail.
extern "C" void __cdecl EffectKind18Sub17_WaitCue(void) {
    if (At(at::kCounter)[0] == 0xC) {
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        Cond_ByteFE = 0x23;
        unsigned char* const rec = NewRecord("EffectKind18Sub17_WaitCue", SH_CALL(Effect_FindFree)() & 0xFFu);
        rec[0] = 1;
        rec[5] = 0x18;
        rec[1] = 0x17;
        rec[2] = 0xA;
    }
    Sub17Tail();
}

// original 0x503760 (sub-state 2): +0x32 (s16) below 0x80 up one; else +2 up
// and the counter 0x903848 = 0xD. The shared tail.
extern "C" void __cdecl EffectKind18Sub17_Rise(void) {
    unsigned char* const s = S();
    const U w = Word(s + 0x32);
    if (SW(w) < 0x80) {
        SetWord(s + 0x32, w + 1u);
    } else {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        At(at::kCounter)[0] = 0xD;
    }
    Sub17Tail();
}

// original 0x5037D0 (sub-state 3): the z cell +0x3A below 0x19: +0x38 up 0x1000;
// else +2 up. The shared tail.
extern "C" void __cdecl EffectKind18Sub17_MoveZ(void) {
    unsigned char* const s = S();
    if (SW(Word(s + 0x3A)) < 0x19) SetUL(s + 0x38, UL(s + 0x38) + 0x1000u);
    else s[2] = static_cast<unsigned char>(s[2] + 1);
    Sub17Tail();
}

// original 0x503830 (sub-state 4): +0x3A below 0x1C: +0x34 down 0x800, +0x38 up
// 0x800; else +2 up. The shared tail.
extern "C" void __cdecl EffectKind18Sub17_MoveXZ(void) {
    unsigned char* const s = S();
    if (SW(Word(s + 0x3A)) < 0x1C) {
        SetUL(s + 0x34, UL(s + 0x34) + 0xFFFFF800u);
        SetUL(s + 0x38, UL(s + 0x38) + 0x800u);
    } else {
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    Sub17Tail();
}

// original 0x5038A0 (sub-state 5): the x cell +0x36 at least 0x4F: +0x34 down
// 0x1000; else with (Frame_Counter / 5) & 3 = 3 +2 up. The shared tail.
extern "C" void __cdecl EffectKind18Sub17_MoveX(void) {
    unsigned char* const s = S();
    if (SW(Word(s + 0x36)) >= 0x4F) SetUL(s + 0x34, UL(s + 0x34) + 0xFFFFF000u);
    else if (((Frame_Counter / 5u) & 3u) == 3) s[2] = static_cast<unsigned char>(s[2] + 1);
    Sub17Tail();
}

// original 0x503920 (sub-state 6): every fifth +9 E5D's step +9 / 5 + 4; +9 up;
// E5D's quads variant 1; at +9 / 5 = 3: Cond_ByteFE = 0, +9 = 1, +2 up.
extern "C" void __cdecl EffectKind18Sub17_Count(void) {
    unsigned char* s = S();
    const unsigned t = s[9];
    if (t % 5 == 0) {
        Step(t / 5 + 4);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    Quads(1);
    s = S();
    if (s[9] / 5 != 3) return;
    Cond_ByteFE = 0;
    s[9] = 1;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x5039B0 (sub-state 7): the enemy wait on EnemyWorkingRecords record
// 0 (0x93B9F2); +9 up; E5D's quads variant 1.
extern "C" void __cdecl EffectKind18Sub17_WaitEnemy0(void) {
    unsigned char* const s = EnemyWait(at::kEnemy0Bits);
    s[9] = static_cast<unsigned char>(s[9] + 1);
    Quads(1);
}

// original 0x503A40 (sub-state 8): the settle; E5D's quads variant 1.
extern "C" void __cdecl EffectKind18Sub17_Settle(void) {
    Settle();
    Quads(1);
}

// original 0x503AE0 (sub-state 9): at +9 / 5 = 4 Effect_Release (and nothing
// more); else every fifth +9 E5D's step 7 - +9 / 5; +9 up; E5D's quads variant 1.
extern "C" void __cdecl EffectKind18Sub17_End(void) {
    unsigned char* s = S();
    const unsigned t = s[9];
    const unsigned q = t / 5;
    if (q == 4) {
        SH_CALL(Effect_Release)();
        return;
    }
    if (t % 5 == 0) {
        Step(7 - q);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    Quads(1);
}

// original 0x503B50 (sub-state 10, where sub-state 1's spawn starts): the point
// (+0x34, +0x38) = (0x430000, 0x1A0000), +0x3E = 0xFF00, +0x32 = 0x80, +9 = 0,
// +0xB = 1; MoveCmd_TestFB(0x55, 0x19) (al unread); +2 up.
extern "C" void __cdecl EffectKind18Sub17_Start10(void) {
    unsigned char* const s = S();
    SetUL(s + 0x34, 0x430000);
    SetUL(s + 0x38, 0x1A0000);
    SetWord(s + 0x3E, 0xFF00);
    SetWord(s + 0x32, 0x80);
    s[9] = 0;
    s[0xB] = 1;
    SH_CALL(MoveCmd_TestFB)(0x55, 0x19);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x503BB0 (sub-state 11): at the counter 0x903848 = 0xF with
// (Frame_Counter / 5) & 3 = 3: E5D's step 4, MoveCmd_TestFB(0x43, 0x1A), +2
// up. Else every fifth frame E5D's step (Frame_Counter / 5) & 3, and E5D's
// quads variant 2.
extern "C" void __cdecl EffectKind18Sub17_WaitCue11(void) {
    const unsigned char cue = At(at::kCounter)[0];
    const U f = Frame_Counter;
    if (cue == 0xF && ((f / 5u) & 3u) == 3) {
        Step(4);
        SH_CALL(MoveCmd_TestFB)(0x43, 0x1A);
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        return;
    }
    if (f % 5u == 0) Step((f / 5u) & 3u);
    Quads(2);
}

// original 0x503C20 (sub-state 12): every fifth +9 E5D's step +9 / 5 + 4; +9
// up; at +9 / 5 = 3: +2 up, +9 = 1.
extern "C" void __cdecl EffectKind18Sub17_Count12(void) {
    unsigned char* s = S();
    const unsigned t = s[9];
    if (t % 5 == 0) {
        Step(t / 5 + 4);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] / 5 != 3) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
    S()[9] = 1;
}

// original 0x503CA0 (sub-state 13): the enemy wait on EnemyWorkingRecords record
// 1 (0x93BB1A); +9 up.
extern "C" void __cdecl EffectKind18Sub17_WaitEnemy1(void) {
    unsigned char* const s = EnemyWait(at::kEnemy1Bits);
    s[9] = static_cast<unsigned char>(s[9] + 1);
}

// original 0x503D30 (sub-state 14): the settle; MoveCmd_TestFB(0x42, +0x32 / 6
// + 0x18) (al unread).
extern "C" void __cdecl EffectKind18Sub17_Settle14(void) {
    Settle();
    const std::int32_t lean = SW(Word(S() + 0x32)) / 6;
    SH_CALL(MoveCmd_TestFB)(0x42, static_cast<short>(lean + 0x18));
}

void Effect5C_Inject() {
    if (bof3::WantsShadow("effect_5c")) effect_5c::SelfTest();
    BOF3_INJECT(EffectKind18Sub50_Run);
    BOF3_INJECT(EffectKind18Sub50_Start);
    BOF3_INJECT(EffectKind18Sub50_Shine);
    BOF3_INJECT(EffectKind18Sub50_Sink);
    BOF3_INJECT(EffectKind18Sub50_WaitSet);
    BOF3_INJECT(EffectKind18Sub50_Flash);
    BOF3_INJECT(EffectKind18Sub50_DrawWall);
    BOF3_INJECT(EffectKind18Sub50_DrawLine);
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
    BOF3_INJECT(EffectKind18Sub11_Run);
    BOF3_INJECT(EffectKind18Sub11_Start);
    BOF3_INJECT(EffectKind18Sub11_Wait);
    BOF3_INJECT(EffectKind18Sub11_Open);
    BOF3_INJECT(EffectKind18Sub11_Sink);
    BOF3_INJECT(EffectKind18Sub11_DrawTiles);
    BOF3_INJECT(EffectKind18Sub12_Run);
    BOF3_INJECT(EffectKind18Sub12_Start);
    BOF3_INJECT(EffectKind18Sub12_Wait);
    BOF3_INJECT(EffectKind18Sub12_Open);
    BOF3_INJECT(EffectKind18Sub12_Sink);
    BOF3_INJECT(EffectKind18Sub12_DrawTiles);
    BOF3_INJECT(EffectKind18Sub15_Run);
    BOF3_INJECT(EffectKind18Sub15_Start);
    BOF3_INJECT(EffectKind18Sub15_Draw);
    BOF3_INJECT(EffectKind18Sub15_LinkLayer);
    BOF3_INJECT(EffectKind18Sub16_Run);
    BOF3_INJECT(EffectKind18Sub17_Run);
    BOF3_INJECT(EffectKind18Sub17_Start);
    BOF3_INJECT(EffectKind18Sub17_WaitCue);
    BOF3_INJECT(EffectKind18Sub17_Rise);
    BOF3_INJECT(EffectKind18Sub17_MoveZ);
    BOF3_INJECT(EffectKind18Sub17_MoveXZ);
    BOF3_INJECT(EffectKind18Sub17_MoveX);
    BOF3_INJECT(EffectKind18Sub17_Count);
    BOF3_INJECT(EffectKind18Sub17_WaitEnemy0);
    BOF3_INJECT(EffectKind18Sub17_Settle);
    BOF3_INJECT(EffectKind18Sub17_End);
    BOF3_INJECT(EffectKind18Sub17_Start10);
    BOF3_INJECT(EffectKind18Sub17_WaitCue11);
    BOF3_INJECT(EffectKind18Sub17_Count12);
    BOF3_INJECT(EffectKind18Sub17_WaitEnemy1);
    BOF3_INJECT(EffectKind18Sub17_Settle14);
}
