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
#include "game/record_views.h"
#include "game/records.h"
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

// --- x87 as the original has it (the game's control word: round to nearest, 53
// bits); every value goes through the FPU as Capcom's does.

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

}  // namespace

// ===========================================================================
// Sub-kind 0x50: EffectKind18_States[80] (0x6541AC), EffectKind18Sub50_States (5)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x10: EffectKind18_States[16] (0x6540AC), EffectKind18Sub10_States (6)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x56: EffectKind18_States[86] (0x6541C4), EffectKind18Sub56_States (2)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x57: EffectKind18_States[87] (0x6541C8), EffectKind18Sub57_States (2)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x58: EffectKind18_States[88] (0x6541CC), EffectKind18Sub58_States (4)
// ===========================================================================

// ===========================================================================
// Sub-kinds 0x11 and 0x12: EffectKind18_States[17] / [18] (0x6540B0 / 0x6540B4),
// EffectKind18Sub11_States / Sub12_States (4 each)
// ===========================================================================

namespace {
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
        SetWord(VSEffect5c(0x1C), y);
        SetWord(VSEffect5c(0x14), y);
        SetWord(VSEffect5c(0x10), x - 0x4040u);
        SetWord(VSEffect5c(0), x - 0x4040u);
        const U z = static_cast<U>(At(e)[0]) << 7;
        SetWord(VSEffect5c(0x18), x - 0x3FC0u);
        SetWord(VSEffect5c(8), x - 0x3FC0u);
        SetWord(VSEffect5c(0xC), y);
        SetWord(VSEffect5c(4), y);
        SetWord(VSEffect5c(0xA), z - shade - 0x4040u);
        SetWord(VSEffect5c(2), z - shade - 0x4040u);
        SetWord(VSEffect5c(0x1A), z + shade - 0x4040u);
        SetWord(VSEffect5c(0x12), z + shade - 0x4040u);
        Project4Effect5c(prim, 8, 0x18, 0x28, 0x38);
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
    auto* const so = rv::As<records::EffectObject>(s);
    Cond_ByteFE = 1;
    so->height = 0xFFFFFF00u;
    s[0xB] = 0x80;
    s[9] = 0xA;
    s[2] = static_cast<unsigned char>(s[2] + 1);
    Draw(0xFFFFFF00u, 0x80);
}
// Sub-state 2: the tiles at +0x3C shade 0x80; +9 down; at 0 the cells 0, +2 up.
template <void (*Draw)(unsigned, unsigned)> void TileOpen() {
    Draw(static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height), 0x80);
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
    Draw(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height), s[0xB] & 0xF8u);
    s = S();
    rv::As<records::EffectObject>(s)->height = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height) + 1u;
    s[0xB] = static_cast<unsigned char>(s[0xB] - 2);
    if (rv::As<records::EffectObject>(s)->height > -0xC0) SH_CALL(Effect_Release)();
}
}  // namespace

// ===========================================================================
// Sub-kind 0x15: EffectKind18_States[21] (0x6540C0), EffectKind18Sub15_States (2)
// ===========================================================================

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
}  // namespace

// ===========================================================================
// Sub-kind 0x17: EffectKind18_States[23] (0x6540C8), EffectKind18Sub17_States (16)
// ===========================================================================

void Effect5C_Inject() {
    if (bof3::WantsShadow("effect_5c")) effect_5c::SelfTest();
}
