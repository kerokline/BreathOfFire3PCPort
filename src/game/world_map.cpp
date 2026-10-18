// Group 1 of the world-map wave (docs/world-map.md section 7): seven functions,
// each read to its last instruction with capstone against bof3/BOF3.exe and,
// for the map's five, against the PlayStation's world-map overlay - the JP
// disc's WORLD00/AREA016.EMI section at 0x801F2C00, read with capstone MIPS
// through the sibling's tools - and for the two helpers against the boot exe.
// Faithful (the dial's opacity is a port change, kept: D42), but for the
// legend's keys: the button round's DIV-0135, world_map_legend below
// and world_map.h (docs/world-map-hud.md section 9). The start-up fuzz is
// world_map_fuzz.cpp.
//
//   WorldMap_FrameStep    0x404160..0x404220 (0xC1: the dispatcher and states 1..3)  PSX 0x801F4178's family
//   WorldMap_DrawFrame    0x404390..0x404554 (0x1C5)                                PSX 0x801F3B00
//   WorldMap_DrawSprite   0x404560..0x40461B (0xBC)                                 PSX 0x801F39D8
//   WorldMap_DrawHud      0x404620..0x404677 (0x58)                                 PSX 0x801F40C4
//   WorldMap_DrawNeedle   0x408530..0x4086C5 (0x196)                                PSX 0x801F3ECC
//   MapView_ItemHalfAt    0x572F70..0x572F9E (0x2F)                                 PSX 0x80156468
//   MapView_LinkPrimAt    0x572FA0..0x57304E (0xAF)                                 PSX 0x801564C4
//
// The world map's code is compiled into the exe once per world-map area:
// WorldMap_DrawSprite has eleven byte-identical copies (0x4024B0, 0x408470,
// 0x40C1F0, 0x4105A0, 0x411720, 0x414F00, 0x419540, 0x41B3B0, 0x4241E0,
// 0x425280), each with its own tables, and the frame and the state machine
// ten; this module takes the copy the owner's route runs, the Yraall region's.
#include "game/world_map.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include "bof3/symbols.gen.h"
#include "game/labels.h"
#include "game/world_map_callees.h"
#include "hook/detour.h"
#include "input/sigma.h"
#include "hook/draw_order.h"
#include "hook/log.h"

namespace world_map {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
template <typename T> T Raw(U address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

const Callees kOriginals = {
    Raw<void (__cdecl*)()>(kState0),
    WorldMap_DrawFrame,
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    WorldMap_DrawSprite,
    AreaMap_ByteAt,
    Raw<unsigned char (__cdecl*)(short, short)>(kCellEvent),
    WorldMap_DrawNeedle,
    Gpu_SetSprt,
    Gpu_SetSemiTrans,
    Text_DrawAt,
    Gte_PushMatrix,
    Gte_RotMatrix,
    Gte_SetRotMatrix,
    Gte_SetTransMatrix,
    Gpu_SetPolyG4,
    As<long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*, long*,
                       long*)>(&Gte_RotTransPers4),
    Gte_PrimDepths4_10B,
    Gte_PopMatrix,
    MapView_ItemAt,
    Gpu_LinkPrim,
};
Callees g = kOriginals;

}  // namespace world_map

namespace {

using namespace world_map;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
std::int32_t Short(const unsigned char* p) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(Word(p))); }
U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
void PutFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }
unsigned char* Object() { return At(Long(At(at::Sprite_CurrentAt()))); }
unsigned char* PacketNext() { return At(Long(At(at::Gfx_PacketNextAt()))); }

// The needle's corner arithmetic, as the original's x87 executes it: the
// integer offset loaded with fild, the corner's float loaded over it, the
// subtraction at the control word's precision, one store to a float.
//   fild d; fld a; fsub st(1); fstp a
inline float X87SubInt(float a, std::int32_t d) {
    float r;
    __asm__ volatile(
        "fildl %2\n\t"
        "flds %1\n\t"
        "fsub %%st(1), %%st\n\t"
        "fstps %0\n\t"
        "fstp %%st(0)"
        : "=m"(r)
        : "m"(a), "m"(d)
        : "st", "st(1)");
    return r;
}
float Float(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

}  // namespace

// --- the map task's frame state machine ------------------------------------------

// original 0x404160 (the PSX 0x801F4178 family: a jalr through a table of the
// object's state byte): the map task's frame, called from 0x404150 before the
// HUD's state machine 0x404230. By byte +2 of Sprite_Current through the table
// 0x5EF5F8 (four entries; the byte is loaded whole and the original indexes
// past four into the HUD's table and the sprite table - never reached, and
// ours refuses it):
//   0: the shared block 0x411310 (Capcom's; eleven state machines share it):
//      unless the mode byte 0x9045FA is 2 or Field_ScriptFlags bit 8 is set,
//      y (the word at +0x2E) = -0x30 and the state 1;
//   1: y += 0x10; at 0x10 and above the state becomes 2; then as state 2;
//   2: the mode byte 2 makes the state 3; WorldMap_DrawFrame(0x10, y);
//   3: y -= 0x10; at -0x30 and below the state is 0; unless the mode byte is 2
//      the state is 1 (over the 0); WorldMap_DrawFrame(0x10, y).
// The y the original pushes carries the object pointer's (state 3) or a stale
// register's (state 2) high half above the word; every reader of the frame's
// y takes its low word (docs/world-map.md section 7.1), so ours passes the
// word sign-extended.
extern "C" __attribute__((disable_tail_calls)) void __cdecl WorldMap_FrameStep() {
    unsigned char* object = Object();
    switch (object[2]) {
    case 0:
        g.state0();
        return;
    case 1:
        PutWord(object + 0x2E, Word(object + 0x2E) + 0x10);
        object = Object();
        if (Short(object + 0x2E) >= 0x10) object[2] = static_cast<unsigned char>(object[2] + 1);
        [[fallthrough]];
    case 2:
        if (At(kMapMode)[0] == 2) {
            object = Object();
            object[2] = 3;
        }
        object = Object();
        g.draw_frame(0x10, Short(object + 0x2E));
        return;
    case 3:
        PutWord(object + 0x2E, Word(object + 0x2E) - 0x10);
        object = Object();
        if (Short(object + 0x2E) <= -0x30) {
            object[2] = 0;
            object = Object();
        }
        if (At(kMapMode)[0] != 2) {
            object[2] = 1;
            object = Object();
        }
        g.draw_frame(0x10, Short(object + 0x2E));
        return;
    default:
        bof3::Fatal("WorldMap_FrameStep: the map task's state byte is %u, past its four-entry table", object[2]);
    }
}

// --- the frame ----------------------------------------------------------------------

// original 0x404390 (PSX 0x801F3B00, branch for branch): the map's frame at
// (x, y), nothing unless Draw_PassFlags & 0x1B. A draw-mode primitive
// (Gpu_SetDrawMode(prim, 0, 0, 0x9C, 0): tpage 0x9C, 8-bit at (768, 256),
// blend mode 0; the PSX picks 0x22C on GetGraphType 1 or 2) committed to slot
// 1; the dial at (x, y); the cell under the leader (AreaMap_ByteAt of the
// high words of Field_Kind2X, Field_Kind2Z). Then three legend rows:
//   1 at (x + 0x30, y), unless the cell is 0xA0, 0xA1 or 0xAE or
//     Field_ScriptFlags2 bit 12: its key - the first of the six button-table
//     entries whose mask has a bit of the button map's word 0 - drawn at
//     (x + 0x38, y + 8) as the entry's sprite index + 1 (the lit glyph), or,
//     when the label is withheld, as the index itself (the dim one);
//   2 at (x + 0x30, y + 0x10) unless the cell is 0xA0 or 0xA1: its key from
//     word 6 over EIGHT entries (the seventh and eighth are the state table's
//     words 0x5EF688 / 0x5EF68C - kept), the same lit / dim rule at
//     (x + 0x38, y + 0x10);
//   3 at (x + 0x30, y + 0x18) when 0x531920(cell x, cell z) says so, or flag
//     bit 12, or the party set (0x90412C & 0x7F) is 0xC, or Field_ScriptFlags
//     bit 14;
// and the needle at (x + 0x18, y + 0x18). The flags are read after the calls
// that precede them; the index the original pushes for a lit key carries the
// button word's upper bytes, which the sprite draw masks off.
extern "C" __attribute__((disable_tail_calls)) void __cdecl WorldMap_DrawFrame(int x, int y) {
    if (!(At(at::Draw_PassFlagsAt())[0] & 0x1B)) return;
    world_map_legend::Begin();   // DIV-0135: which page; the port's key names painted over
    g.set_draw_mode(PacketNext(), 0, 0, 0x9C, 0);
    g.commit_prim(1, 0xC);
    g.draw_sprite(x, y, 0);
    const unsigned char cell =
        g.byte_at(static_cast<short>(Word(At(kLeaderCellX))), static_cast<short>(Word(At(kLeaderCellZ))));
    const bool withheld1 =
        cell == 0xA0 || cell == 0xA1 || cell == 0xAE || (Word(At(at::Field_ScriptFlags2At())) & 0x1000) != 0;
    if (!withheld1) g.draw_sprite(x + 0x30, y, 1);
    world_map_legend::Key(g.draw_sprite, kButtonTable, 6, Long(At(kButtonMap0)), x + 0x38, y + 8, withheld1);
    const bool withheld2 = cell == 0xA1 || cell == 0xA0;
    if (!withheld2) g.draw_sprite(x + 0x30, y + 0x10, 2);
    world_map_legend::Key(g.draw_sprite, kButtonTable, 8, Long(At(kButtonMap6)), x + 0x38, y + 0x10, withheld2);
    const unsigned char event =
        g.cell_event(static_cast<short>(Word(At(kLeaderCellX))), static_cast<short>(Word(At(kLeaderCellZ))));
    bool third = event != 0;
    if (!third) third = (Word(At(at::Field_ScriptFlags2At())) & 0x1000) != 0;
    if (!third) third = (At(kPartySet)[0] & 0x7F) == 0xC;
    if (!third) third = (Word(At(at::Field_ScriptFlagsAt())) & 0x4000) != 0;
    if (third) g.draw_sprite(x + 0x30, y + 0x18, 3);
    world_map_legend::Start(x + 0x30, y + 0x18, !third);
    g.draw_needle(x + 0x18, y + 0x18);
}

// original 0x404560 (PSX 0x801F39D8): one sprite of the dial page at (x, y):
// a draw-mode primitive (tpage 0x9C, as the frame's) committed to slot 1,
// then at Gfx_PacketNext - read again after that commit - a SPRT
// (Gpu_SetSprt), semi-transparent when the index's low byte is not 0 - THE
// PORT'S CHANGE: the PSX passes 1 for every sprite, so its dial is
// translucent and the port's opaque (docs/world-map.md section 7.2, D42);
// kept - colour 0x80 x 3, x and y as floats of the arguments' low words,
// CLUT 0x7B80, and (w, h, u, v) from the table 0x5EF618 by index & 0xFF;
// Gfx_CommitPrim(1, 0x1C). Neither the link nor the code byte beyond the
// setters' is written.
extern "C" __attribute__((disable_tail_calls)) void __cdecl WorldMap_DrawSprite(int x, int y, unsigned index) {
    g.set_draw_mode(PacketNext(), 0, 0, 0x9C, 0);
    g.commit_prim(1, 0xC);
    unsigned char* const prim = PacketNext();
    g.set_sprt(prim);
    g.set_semi_trans(prim, (index & 0xFF) != 0 ? 1u : 0u);
    PutFloat(prim + 8, static_cast<float>(static_cast<short>(x)));
    PutFloat(prim + 0xC, static_cast<float>(static_cast<short>(y)));
    prim[4] = prim[5] = prim[6] = 0x80;
    PutWord(prim + 0x16, 0x7B80);
    const unsigned char* const entry = At(kSpriteTable + (index & 0xFF) * 4u);
    PutWord(prim + 0x18, entry[0]);
    PutWord(prim + 0x1A, entry[1]);
    prim[0x14] = entry[2];
    prim[0x15] = entry[3];
    g.commit_prim(1, 0x1C);
}

// original 0x404620 (PSX 0x801F40C4): the HUD at (x, y), nothing unless
// Draw_PassFlags & 0x1B: the region box (sprite 4, 128 x 24) at (x, y), its
// cap (sprite 5, 8 x 24) at (x + 0x80, y), then Text_DrawAt(x + 4, y + 4, 0,
// 0xFF, text) with the region's name at 0x803580 + the low word of the dword
// 0x803588 - the area's 0x80010000 section's word +8, read after the two
// sprites. The return (Text_DrawAt's) is not read by the three callers.
extern "C" __attribute__((disable_tail_calls)) void __cdecl WorldMap_DrawHud(int x, int y) {
    if (!(At(at::Draw_PassFlagsAt())[0] & 0x1B)) return;
    g.draw_sprite(x, y, 4);
    g.draw_sprite(x + 0x80, y, 5);
    const unsigned char* text = At(kAreaText + (Long(At(kAreaTextOffset)) & 0xFFFF));
    g.text_draw_at(x + 4, y + 4, 0, 0xFF, text);
}

// original 0x408530 (PSX 0x801F3ECC): the compass needle in the dial at (x, y)
// - the arguments' low words. Gte_PushMatrix; a local MATRIX from Camera_Angles
// by Gte_RotMatrix, its translation zeroed, Gte_SetRotMatrix and
// Gte_SetTransMatrix of it; the four corners (-10, 0, 0), (0, -4, 0), (0, 4,
// 0), (10, 0, 0) into Prim_VertexScratch (their pads untouched); a POLY_G4 at
// Gfx_PacketNext by Gpu_SetPolyG4; Gte_RotTransPers4 over the four into the
// primitive's corners with two locals for the depth and the flag (ten
// arguments, as libgte's); Gte_PrimDepths4_10B of the primitive (depths 1/4096,
// 0, 0, 0 - D41, DIV-0044 in the backend); each corner moved by (x - 0x9E,
// y - 0x76) on the x87; colours red, purple, purple, blue; Gfx_CommitPrim(1,
// 0x44); Gte_PopMatrix.
extern "C" __attribute__((disable_tail_calls)) void __cdecl WorldMap_DrawNeedle(int x, int y) {
    g.push_matrix();
    alignas(4) unsigned char matrix[0x20];
    g.rot_matrix(reinterpret_cast<const short*>(At(at::Camera_AnglesAt())), reinterpret_cast<short*>(matrix));
    PutLong(matrix + 0x14, 0);
    PutLong(matrix + 0x18, 0);
    PutLong(matrix + 0x1C, 0);
    g.set_rot_matrix(reinterpret_cast<const unsigned long*>(matrix));
    g.set_trans_matrix(reinterpret_cast<const unsigned long*>(matrix));
    unsigned char* const prim = PacketNext();
    unsigned char* const scratch = At(at::Prim_VertexScratchAt());
    PutWord(scratch + 0x00, 0xFFF6);   // (-10, 0, 0)
    PutWord(scratch + 0x02, 0);
    PutWord(scratch + 0x04, 0);
    PutWord(scratch + 0x08, 0);        // (0, -4, 0)
    PutWord(scratch + 0x0A, 0xFFFC);
    PutWord(scratch + 0x0C, 0);
    PutWord(scratch + 0x10, 0);        // (0, 4, 0)
    PutWord(scratch + 0x12, 4);
    PutWord(scratch + 0x14, 0);
    PutWord(scratch + 0x18, 0xA);      // (10, 0, 0)
    PutWord(scratch + 0x1A, 0);
    PutWord(scratch + 0x1C, 0);
    g.set_poly_g4(prim);
    long depth, flag;
    g.rot_trans_pers4(reinterpret_cast<const short*>(scratch), reinterpret_cast<const short*>(scratch + 8),
                      reinterpret_cast<const short*>(scratch + 0x10), reinterpret_cast<const short*>(scratch + 0x18),
                      reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
                      reinterpret_cast<float*>(prim + 0x28), reinterpret_cast<float*>(prim + 0x38), &depth, &flag);
    g.prim_depths(prim);
    const std::int32_t dx = 0x9E - static_cast<short>(x);
    const std::int32_t dy = 0x76 - static_cast<short>(y);
    for (U corner = 0; corner < 4; ++corner) {
        unsigned char* const xy = prim + 8 + corner * 0x10;
        PutFloat(xy, X87SubInt(Float(xy), dx));
        PutFloat(xy + 4, X87SubInt(Float(xy + 4), dy));
    }
    prim[0x04] = 0xFF; prim[0x05] = 0;    prim[0x06] = 0;      // red
    prim[0x14] = 0x80; prim[0x15] = 0;    prim[0x16] = 0x80;   // purple
    prim[0x24] = 0x80; prim[0x25] = 0;    prim[0x26] = 0x80;   // purple
    prim[0x34] = 0;    prim[0x35] = 0;    prim[0x36] = 0xFF;   // blue
    g.commit_prim(1, 0x44);
    g.pop_matrix();
}

// --- the map-view helpers ----------------------------------------------------------

// original 0x572F70 (PSX 0x80156468): the 0x48-byte half for this display
// buffer of map cell (x, y)'s draw item - DrawItems + (Gfx_BufferIndex + item
// * 2) * 0x48, the buffer index read after MapView_ItemAt - or 0 when the
// cell has none. The whole of MapView_ItemAt's eax is the item.
extern "C" __attribute__((disable_tail_calls)) unsigned char* __cdecl MapView_ItemHalfAt(long x, long y) {
    const U item = static_cast<U>(g.item_at(x, y));
    if (item == 0) return nullptr;
    return At(at::DrawItemsAt() + (At(at::Gfx_BufferIndexAt())[0] + item * 2u) * 0x48u);
}

// original 0x572FA0 (PSX 0x801564C4): links the primitive at Gfx_PacketNext
// into the map-view row of map (x, z) - the arguments' high words less
// MapView_Origin's, plus one each for a non-zero low word, plus `dy` (its low
// byte, signed) plus 2 - when that row is 0..0x37 and the pool has room:
// (Gfx_BufferIndex << 16) + 0x7F1BAC above Gfx_PacketNext + (size & 0xFF),
// unsigned. Then Gpu_LinkPrim(tail, prim) with the row's tail for this
// buffer - the dword at 0x8022C4 + (Gfx_BufferIndex + row * 6) * 8 - and,
// with the buffer index and Gfx_PacketNext READ AGAIN after that call, the
// tail slot = Gfx_PacketNext and Gfx_PacketNext += size. No result; the
// original's eax is the tail slot's new value or, on the exits, the row or
// the bound - its 371 callers (E8 scan) are draw code that reloads eax.
extern "C" __attribute__((disable_tail_calls)) void __cdecl MapView_LinkPrimAt(unsigned long x, unsigned long z,
                                                                           int dy, unsigned size) {
    std::int32_t row = static_cast<std::int16_t>(z >> 16);
    row -= (z & 0xFFFF) == 0 ? 1 : 0;
    row -= (x & 0xFFFF) == 0 ? 1 : 0;
    row -= Short(At(at::MapView_OriginAt() + 2));
    row += static_cast<std::int16_t>(x >> 16);
    row -= Short(At(at::MapView_OriginAt()));
    row += static_cast<signed char>(dy) + 2;
    if (row < 0 || row >= 0x38) return;
    const U bytes = size & 0xFF;
    const U prim = Long(At(at::Gfx_PacketNextAt()));
    // Tooling only (BOF3X_DRAWORDER): the walk's log names a frame node by its
    // row alone; this says who linked it.
    if (draw_order::Tagging())
        bof3::Log("draworder   linked %08X (%u bytes) into row %d from %08X", prim, bytes, row,
                  static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))));
    const U bound = (static_cast<U>(At(at::Gfx_BufferIndexAt())[0]) << 16) + kPoolBound;
    if (bound <= prim + bytes) {
        if (draw_order::Tagging())
            bof3::Log("draworder   packet pool full: a %u-byte primitive for row %d skipped (MapView_LinkPrimAt) from %08X", bytes, row,
                      static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))));
        return;
    }
    const U slot_index = static_cast<U>(row) * 6u;
    unsigned char* const slot = At(kRowTails + (slot_index + At(at::Gfx_BufferIndexAt())[0]) * 8u);
    g.link_prim(reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(Long(slot))), prim);
    const U next = Long(At(at::Gfx_PacketNextAt()));
    PutLong(At(kRowTails + (slot_index + At(at::Gfx_BufferIndexAt())[0]) * 8u), next);
    PutLong(At(at::Gfx_PacketNextAt()), next + bytes);
}

// --- the legend's keys: DIV-0135 (world_map.h) -------------------------------

namespace {

// DIV-0135: 1 after every module's self-test (PatchBytes "WorldMapLegend").
unsigned char g_legend_on = 0;
// This frame's page is the port's own (the glyph sheet blank, the key names
// painted over): the stand-ins. Set by Begin, read by Key and Start.
bool g_standin = false;
// The FNVs Begin has met on a page with no glyph sheet that is not the port's
// legend, logged once each.
U g_unknown_logged[8];
unsigned g_unknown_count = 0;

// The dial page through tpage 0x9C: 8-bit texels from VRAM (768, 256), two to
// a halfword of Gfx_VramShadow (1024 x 512 halfwords, rows 0x800 bytes).
constexpr int kPageX = 768, kPageY = 256;
// The 8 x 8 key-glyph sheet: sprites 6..21 of the sprite table, u 64..95, v
// 80..111 (blank on the port's page, docs/world-map-hud.md 1.2).
constexpr int kSheetU0 = 64, kSheetU1 = 96, kSheetV0 = 80, kSheetV1 = 112;
// The port's key names (SPACE, X, ENTER): dim in the frame (sprite 0) and lit
// in labels 1..3 (sprites 1..3, u 8..63 from v 80), the frame's box the
// labels' moved by (+40, -80) - where the frame's x + 0x30 puts them over it.
// u0, u1, v0, v1, ends exclusive; tools/importer.py DIAL_PORT_BOXES.
struct Box { int u0, u1, v0, v1; };
constexpr Box kPortBoxes[2] = {{51, 75, 8, 33}, {11, 35, 88, 113}};
// The panel behind them: one index with a diagonal band of three at its top
// left (importer.py DIAL_PORT_PANEL).
bool IsPanel(unsigned char t) { return t == 0x0C || t == 0x52 || t == 0x54 || t == 0x59; }
// FNV-1a 32 of the two boxes, the frame's first, row by row: the port's page
// (all 16 world-map areas' pages, measured 2026-10-17) and the same after the
// repaint - the numbers importer.py DIAL_PORT_FNV / DIAL_REPAINTED_FNV checks,
// so the engine's repaint and the importer's are one rule.
constexpr U kPortFnv = 0x498B8D15, kRepaintedFnv = 0x0D297D59;
// The rectangle the repaint reads and writes: u 10..75 (halfwords 773..805),
// v 8..112 - both boxes and the texel right of each.
constexpr int kRectU0 = 10, kRectU1 = 76, kRectV0 = 8, kRectV1 = 113;
constexpr int kRectW = (kRectU1 - kRectU0) / 2, kRectH = kRectV1 - kRectV0;
static_assert(kRectU0 % 2 == 0 && kRectU1 % 2 == 0, "the rectangle is whole halfwords");

// The port's stand-ins (Config_DrawControllerCell's original 0x461C00, the
// Config screen's glyph column): 0x66A2FC's eight one-byte strings b c d X f g
// o p, the first six circle, cross, triangle, square, L1, R1 - the port's
// circled numerals 1..6 and an X - in colours 2, 1, 6, 5, 0, 0
// (docs/controls.md 2; DIV-0051's Original behaviour).
constexpr U kStandInStrings = 0x66A2FC;
struct StandIn { std::uint16_t bit; unsigned string; unsigned colour; };
constexpr StandIn kStandIns[6] = {
    {0x20, 0, 2}, {0x40, 1, 1}, {0x10, 2, 6}, {0x80, 3, 5}, {0x04, 4, 0}, {0x08, 5, 0},
};
// START on the port's page: the small font's S T A R T (one-byte codes, glyph
// b - 0x26), in colour 0, each a 5-wide quad on an advance of 4 - five 8-wide
// cells would run over the label's dots at u 35 - from label u 12 (the
// label's x + 4; the label starts at u 8), where a disc's START starts, to u
// 32, on the label's own y (v 104, the row ENTER was on). Measured off the
// first capture (2026-10-17, an advance of 5 from u 10 touched the dots).
// The small font's (u, v) pair for colour 0: Text_DrawSmall's 0x65F5A8[0].
constexpr U kSmallUV = 0x65F5A8;
constexpr int kStartWidth = 5, kStartAdvance = 4, kStartDx = 4;

const unsigned char* Vram() { return reinterpret_cast<const unsigned char*>(Gfx_VramShadow); }
unsigned char Texel(int u, int v) { return Vram()[(kPageY + v) * 0x800 + kPageX * 2 + u]; }

U Fnv(U h, unsigned char b) { return (h ^ b) * 0x01000193u; }

// The repaint, on texels at buf[(v - v0) * stride + (u - u0)]: row by row
// from each box's second, an ink texel takes the one up and to the right when
// that is panel, else the one above. The box's top row holds no ink (checked
// by the hashes), so the one above is panel by the time it is read.
void Repaint(unsigned char* buf, int stride, int u0, int v0) {
    auto at = [&](int u, int v) -> unsigned char& { return buf[(v - v0) * stride + (u - u0)]; };
    for (const Box& b : kPortBoxes)
        for (int v = b.v0 + 1; v < b.v1; ++v)
            for (int u = b.u0; u < b.u1; ++u) {
                if (IsPanel(at(u, v))) continue;
                const unsigned char up_right = at(u + 1, v - 1);
                at(u, v) = IsPanel(up_right) ? up_right : at(u, v - 1);
            }
}

U BoxesFnv(const unsigned char* buf, int stride, int u0, int v0) {
    U h = 0x811C9DC5u;
    for (const Box& b : kPortBoxes)
        for (int v = b.v0; v < b.v1; ++v)
            for (int u = b.u0; u < b.u1; ++u) h = Fnv(h, buf[(v - v0) * stride + (u - u0)]);
    return h;
}

// The sheet as the port left it: every texel 0, but for the rows of the dim
// CLUTs (below), which this module writes there itself.
bool SheetBlank() {
    for (int v = kSheetV0; v < kSheetV1; ++v) {
        if (v >= 104 && v < 109) continue;
        for (int u = kSheetU0; u < kSheetU1; ++u)
            if (Texel(u, v) != 0) return false;
    }
    return true;
}

// The stand-in for one of the six buttons, or nullptr.
const StandIn* StandInFor(U bit) {
    for (const StandIn& s : kStandIns)
        if (s.bit == bit) return &s;
    return nullptr;
}

const unsigned char* StandInText(const StandIn& s) {
    return At(Long(At(kStandInStrings + s.string * 4)));
}

// The withheld (dim) form of a stand-in. A disc draws a withheld row's key
// from a second cell of its key sheet (the sprite index, not index + 1;
// 1.2): the same shape drawn in a second palette ramp - the lit cell's ink
// indices 0xE2..0xEF / 0x84..0x87 are 0x42..0x4F / 0x4B..0x83 there (u 64
// and 80 against u 72 and 88, every pair, the JP disc's page) - which the
// page's CLUT 0x7B80 draws lighter, +56 a channel in the mode (the 189 ink
// texel pairs of the eight cell pairs through the CLUT read from VRAM during
// the route, 2026-10-17: +56 174 times, +64 154, +48 74 of 567 channels;
// the main inks, white 0xE2, red 0xE9, blue 0xEE, +56..+64). The frame's dim
// START is in the same ramp. So the stand-in's dim form is the same glyph
// through a second CLUT: its text CLUT (0x7800 | colour, VRAM row 480, x
// colour * 16) with every entry but the transparent 0 lighter by 7 of 31 a
// channel (+56), clamped, its STP bit kept. A shade cannot do it: the
// renderer's glyphs only darken by their shade, the excess going to a
// specular this renderer leaves off (docs/d3d-draw.md).
//
// The five dim CLUTs (colours 0, 1, 2, 5, 6: L1 / R1 / START, cross, circle,
// square, triangle) are kept in the port's own blank key sheet, which no
// draw reads on that page (the sheet's cells are drawn by the original's path
// only, which the port's page leaves for the stand-ins): rows v 104..108 at
// u 64..95 - halfwords 800..815, a CLUT's sixteen at a multiple of 16 - as
// CLUT words (y << 6) | (800 >> 4). Begin writes them (Gfx_LoadImage) and
// keeps them current with the text CLUTs.
constexpr unsigned kDimColours[5] = {0, 1, 2, 5, 6};
constexpr int kDimClutV0 = 104;
constexpr int kDimClutX = kPageX + kSheetU0 / 2;   // 800
static_assert(kDimClutX % 16 == 0, "a CLUT starts at a multiple of 16 halfwords");
constexpr int kDimStep = 7;                         // of 31: +56 of 255

std::uint16_t DimEntry(std::uint16_t e) {
    if (e == 0) return 0;
    auto up = [](unsigned c) { return c + kDimStep > 31 ? 31u : c + kDimStep; };
    return static_cast<std::uint16_t>((e & 0x8000) | up(e & 31) | up((e >> 5) & 31) << 5 | up((e >> 10) & 31) << 10);
}

int DimRow(unsigned colour) {
    for (int k = 0; k < 5; ++k)
        if (kDimColours[k] == colour) return k;
    bof3::Fatal("WorldMapLegend: no dim CLUT for text colour %u", colour);
}

unsigned DimClut(unsigned colour) {
    return static_cast<unsigned>(((kPageY + kDimClutV0 + DimRow(colour)) << 6) | (kDimClutX >> 4));
}

// The five dim CLUTs as they should be now, from the text CLUTs in VRAM.
void DimCluts(std::uint16_t out[5][16]) {
    for (int k = 0; k < 5; ++k)
        for (int i = 0; i < 16; ++i) out[k][i] = DimEntry(Gfx_VramShadow[480 * 1024 + kDimColours[k] * 16 + i]);
}

// One stand-in glyph through a dim CLUT: Text_DrawSmall's draw of one
// character (the small font's (u, v) pair for colours under 16, the glyph
// b - 0x26 or an overlay's 8 x 8 one) with that CLUT for 0x7800 | colour.
void EmitDim(int x, int y, int w, const unsigned char* text, unsigned colour) {
    const unsigned char* const uv = At(kSmallUV + (colour >> 4) * 2);
    unsigned glyph = (text[0] - 0x26u) & 0xFFFF;
    if (const unsigned small = Labels_SmallGlyph(text)) glyph = small;
    PutWord(PacketNext() + 0x16, glyph);
    Text_EmitGlyph(x, y, w, static_cast<int>(8 - uv[1]), static_cast<int>(uv[0]), static_cast<int>(uv[1]),
                   static_cast<int>(DimClut(colour)));
}

// The original's search: the first of `count` entries whose mask word has a
// bit of the button word's low 16 bits; the entry, or nullptr.
const unsigned char* KeyEntry(U buttons, U count, U word) {
    for (U k = 0; k < count; ++k) {
        const unsigned char* const entry = At(buttons + k * 4);
        if ((Word(entry) & word & 0xFFFF) != 0) return entry;
    }
    return nullptr;
}

void LegendSelfTest();

}  // namespace

namespace {

// The dim CLUTs written into the sheet when they differ from what the text
// CLUTs make now (the first frame on the page, and after a palette change);
// a sheet row holding anything else there is a Fatal: not the port's blank.
void KeepDimCluts() {
    std::uint16_t want[5][16];
    DimCluts(want);
    const std::uint16_t* const have = Gfx_VramShadow + (kPageY + kDimClutV0) * 1024 + kDimClutX;
    bool same = true, blank = true;
    for (int k = 0; k < 5; ++k)
        for (int i = 0; i < 16; ++i) {
            same = same && have[k * 1024 + i] == want[k][i];
            blank = blank && have[k * 1024 + i] == 0;
        }
    if (same) return;
    static bool written = false;
    if (!blank && !written)
        bof3::Fatal("WorldMapLegend: the port's key sheet rows v 104..108 hold texels this module did not write "
                    "(BOF3X_ORIGINAL=WorldMapLegend draws the original's)");
    const short rect[4] = {static_cast<short>(kDimClutX), static_cast<short>(kPageY + kDimClutV0), 16, 5};
    Gfx_LoadImage(rect, want);
    // The renderer reads a CLUT from its row's converted copy (Gfx_ClutPixels),
    // which a row gets once, cells 0..255 only, at its first use, and which
    // Gfx_LoadImage does not refresh (gfx_clut.cpp). So each row's first-use
    // conversion is made as Gfx_ClutPixels would make it, if not yet made,
    // and then our sixteen cells are converted (bumping the row's generation,
    // which the glyph cache checks).
    struct ClutRow { std::uint32_t generation; void* pixels; };
    auto* const rows = reinterpret_cast<ClutRow*>(Gfx_ClutRows);
    for (int k = 0; k < 5; ++k) {
        const int y = kPageY + kDimClutV0 + k;
        if (rows[y].pixels == nullptr) Gfx_ClutPixels(static_cast<unsigned>(y << 6));
        Gfx_ConvertRow(kDimClutX, y, 16);
    }
    written = true;
}

}  // namespace

namespace world_map_legend {

void Begin() {
    g_standin = false;
    if (g_legend_on == 0 || !SheetBlank()) return;
    const unsigned char* const page = Vram() + kPageY * 0x800 + kPageX * 2;
    const U fnv = BoxesFnv(page, 0x800, 0, 0);
    if (fnv == kRepaintedFnv) {
        KeepDimCluts();
        g_standin = true;
        return;
    }
    if (fnv != kPortFnv) {
        for (unsigned i = 0; i < g_unknown_count; ++i)
            if (g_unknown_logged[i] == fnv) return;
        if (g_unknown_count < sizeof g_unknown_logged / sizeof g_unknown_logged[0]) {
            g_unknown_logged[g_unknown_count++] = fnv;
            bof3::Log("WorldMapLegend: the dial page in VRAM has no glyph sheet and is not the port's legend (FNV %08X): "
                      "drawn as the original", fnv);
        }
        return;
    }
    unsigned char rect_texels[kRectH][kRectW * 2];
    for (int v = kRectV0; v < kRectV1; ++v)
        for (int u = kRectU0; u < kRectU1; ++u) rect_texels[v - kRectV0][u - kRectU0] = Texel(u, v);
    Repaint(&rect_texels[0][0], kRectW * 2, kRectU0, kRectV0);
    const short rect[4] = {static_cast<short>(kPageX + kRectU0 / 2), static_cast<short>(kPageY + kRectV0),
                           static_cast<short>(kRectW), static_cast<short>(kRectH)};
    Gfx_LoadImage(rect, rect_texels);
    const U after = BoxesFnv(page, 0x800, 0, 0);
    if (after != kRepaintedFnv)
        bof3::Fatal("WorldMapLegend: the port's legend repainted in VRAM is FNV %08X, not the importer's %08X "
                    "(BOF3X_ORIGINAL=WorldMapLegend draws the original's)", after, kRepaintedFnv);
    bof3::Log("WorldMapLegend: the port's dial page has no glyph sheet - its key names painted over, the port's "
              "stand-ins drawn");
    KeepDimCluts();
    g_standin = true;
}

void Key(SpriteFn sprite, std::uint32_t buttons, std::uint32_t count, std::uint32_t word, int x, int y, bool withheld) {
    const unsigned char* const entry = KeyEntry(buttons, count, word);
    if (entry == nullptr) return;
    if (!g_standin) {
        const unsigned key = entry[2];
        sprite(x, y, withheld ? key : ((key + 1) & 0xFF));
        return;
    }
    const StandIn* const s = StandInFor(Word(entry));
    if (s == nullptr)
        bof3::Fatal("WorldMapLegend: the legend's button word %08X picks the table entry %08X past the six buttons "
                    "(mask %04X): no stand-in for it (BOF3X_ORIGINAL=WorldMapLegend draws the original's)",
                    word, static_cast<U>(reinterpret_cast<std::uintptr_t>(entry)), Word(entry));
    if (withheld)
        EmitDim(x, y, 8, StandInText(*s), s->colour);
    else
        Text_DrawSmall(x, y, s->colour, 1, StandInText(*s));
}

void Start(int x, int y, bool withheld) {
    if (!g_standin) return;
    const unsigned char* const uv = At(kSmallUV);
    const unsigned u = uv[0], v = uv[1];
    static const char kStart[] = "START";
    for (int i = 0; kStart[i] != 0; ++i) {
        const auto glyph = static_cast<std::uint16_t>(static_cast<unsigned char>(kStart[i]) - 0x26u);
        PutWord(PacketNext() + 0x16, glyph);
        Text_EmitGlyph(x + kStartDx + i * kStartAdvance, y, kStartWidth, static_cast<int>(8 - v),
                       static_cast<int>(u), static_cast<int>(v),
                       static_cast<int>(withheld ? DimClut(0) : 0x7800));
    }
}

void Arm() {
    if (bof3::WantsShadow("world_map_legend")) LegendSelfTest();
    static const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("WorldMapLegend", static_cast<U>(reinterpret_cast<std::uintptr_t>(&g_legend_on)), &was, &is, 1);
    bof3::Log("WorldMapLegend the world map's legend: the port's key names painted over and its stand-ins drawn on "
              "the port's own page: %s",
              g_legend_on ? "on" : "OFF (BOF3X_ORIGINAL=WorldMapLegend)");
}

}  // namespace world_map_legend

namespace {

// BOF3X_SHADOW=world_map_legend. Pure: the button table (area 33's,
// 0x5EF670), the stand-in strings and a synthetic page; no VRAM, no draw.
//  1. sigma agrees with the words: for every Config the rebind can make (720
//     assignments of the six lines to the six buttons, Japan's words) the
//     entry row 1's search finds over six is Sigma(circle)'s button and row
//     2's over eight Sigma(cross)'s - the keys the legend draws are the
//     buttons the Speak and Action lines are on; at the identity rows 1 and 2
//     draw sprites 11 and 13 (circle and cross, lit), as the original; a
//     swap of Speak and Action trades them; a swap of View and Change moves
//     neither.
//  2. Every one of the six entries has a stand-in, a one-byte string.
//  3. The repaint, on a synthetic page: the panel's band and random ink in
//     both boxes - after it no ink is left in either box, nothing outside them
//     changed, and every panel texel inside them kept; run twice it changes
//     nothing more. FNV-1a's own vectors.
void LegendSelfTest() {
    namespace sg = bof3x::input::sigma;
    unsigned mismatches = 0, tables = 0;
    auto fail = [&](const char* what, unsigned a, unsigned b) {
        if (mismatches++ < 16) bof3::Log("world_map_legend self-test: %s (%04X, %04X)", what, a, b);
    };
    // 1. Every assignment of the six lines (sg::kLineWord order) to the six bits.
    const std::uint16_t bits[6] = {0x20, 0x80, 0x40, 0x10, 0x08, 0x04};   // Japan's: Speak, Move, Action, Menu, View, Change
    int perm[6] = {0, 1, 2, 3, 4, 5};
    do {
        std::uint16_t words[9] = {0, 0, 0, 0, 0, 0x100, 0, 0, 0};
        for (int line = 0; line < 6; ++line) words[sg::kLineWord[line]] = bits[perm[line]];
        words[0] = static_cast<std::uint16_t>(words[0] | sg::kL2R2);
        words[7] = words[0];
        words[8] = words[6];
        std::string why;
        const sg::Table t = sg::Build(words, why);
        if (!t.valid) {
            fail("a permutation refused", words[0], words[6]);
            continue;
        }
        ++tables;
        const unsigned char* const row1 = KeyEntry(kButtonTable, 6, words[0]);
        const unsigned char* const row2 = KeyEntry(kButtonTable, 8, words[6]);
        if (row1 == nullptr || Word(row1) != sg::Apply(t, 0x20)) fail("row 1 is not Sigma(circle)", words[0], sg::Apply(t, 0x20));
        if (row2 == nullptr || Word(row2) != sg::Apply(t, 0x40)) fail("row 2 is not Sigma(cross)", words[6], sg::Apply(t, 0x40));
        bool identity = true;
        for (int i = 0; i < 6; ++i) identity = identity && perm[i] == i;
        if (identity && row1 && row2 && (row1[2] + 1 != 11 || row2[2] + 1 != 13))
            fail("the identity's lit sprites are not 11 and 13", row1[2] + 1u, row2[2] + 1u);
        const bool speak_action = perm[0] == 2 && perm[2] == 0 && perm[1] == 1 && perm[3] == 3 && perm[4] == 4 && perm[5] == 5;
        if (speak_action && row1 && row2 && (row1[2] + 1 != 13 || row2[2] + 1 != 11))
            fail("Speak <-> Action does not trade the keys", row1[2] + 1u, row2[2] + 1u);
        const bool view_change = perm[0] == 0 && perm[1] == 1 && perm[2] == 2 && perm[3] == 3 && perm[4] == 5 && perm[5] == 4;
        if (view_change && row1 && row2 && (row1[2] + 1 != 11 || row2[2] + 1 != 13))
            fail("View <-> Change moves a key", row1[2] + 1u, row2[2] + 1u);
    } while (std::next_permutation(perm, perm + 6));
    // 2. The stand-ins.
    for (U k = 0; k < 6; ++k) {
        const unsigned char* const entry = At(kButtonTable + k * 4);
        const StandIn* const s = StandInFor(Word(entry));
        if (s == nullptr) {
            fail("a button entry has no stand-in", k, Word(entry));
            continue;
        }
        const unsigned char* const text = StandInText(*s);
        if (text[0] <= 0x26 || text[0] >= 0x80 || text[1] != 0) fail("a stand-in is not one one-byte glyph", k, text[0]);
    }
    // 2b. The dim rule: a dim CLUT entry is its text entry lighter by 7 of 31
    // a channel (+56), clamped, the transparent 0 kept 0 and the STP bit kept;
    // every stand-in colour has a dim CLUT, each at its own row of the sheet's
    // halfwords 800..815, a CLUT word the renderer reads back as that place.
    for (unsigned e = 0; e < 0x10000; e += 0x0107) {
        const auto d = DimEntry(static_cast<std::uint16_t>(e));
        bool ok = e == 0 ? d == 0 : (d & 0x8000) == (e & 0x8000);
        for (unsigned sh = 0; e != 0 && sh < 15; sh += 5) {
            const unsigned c = (e >> sh) & 31, dc = (d >> sh) & 31;
            ok = ok && dc == (c + 7 > 31 ? 31 : c + 7);
        }
        if (!ok) fail("a dim CLUT entry", e, d);
    }
    if (DimEntry(0x8000) != 0x8000 + 7 + (7 << 5) + (7 << 10)) fail("STP black's dim entry", 0x8000, DimEntry(0x8000));
    for (const StandIn& st : kStandIns) {
        const unsigned clut = DimClut(st.colour);
        if ((clut & 0x3F) * 16 != 800 || (clut >> 6) != static_cast<unsigned>(kPageY + kDimClutV0 + DimRow(st.colour)))
            fail("a dim CLUT word", st.colour, clut);
    }
    // 3. The repaint on a synthetic page.
    if (Fnv(0x811C9DC5u, 'a') != 0xE40C292Cu) fail("FNV-1a of \"a\"", 0, 0);
    unsigned char page[kRectH][kRectW * 2], before[kRectH][kRectW * 2];
    U seed = 0x2468ACE1u;
    auto next = [&] { seed = seed * 1103515245u + 12345u; return (seed >> 16) & 0x7FFF; };
    for (int round = 0; round < 64; ++round) {
        for (int v = kRectV0; v < kRectV1; ++v)
            for (int u = kRectU0; u < kRectU1; ++u) {
                const int band = u + v;                       // the band runs down to the left
                unsigned char t = 0x0C;
                if (band < 70) t = 0x52;
                else if (band < 76) t = 0x59;
                else if (band < 80) t = 0x54;
                if (u == kRectU0) t = 0x0F;                   // a border column outside both boxes
                page[v - kRectV0][u - kRectU0] = t;
            }
        for (const Box& b : kPortBoxes)
            for (int v = b.v0 + 1; v < b.v1; ++v)
                for (int u = b.u0; u < b.u1; ++u)
                    if (next() % 3 == 0) page[v - kRectV0][u - kRectU0] = static_cast<unsigned char>(0xD0 + next() % 32);
        std::memcpy(before, page, sizeof page);
        Repaint(&page[0][0], kRectW * 2, kRectU0, kRectV0);
        for (int v = kRectV0; v < kRectV1; ++v)
            for (int u = kRectU0; u < kRectU1; ++u) {
                bool in = false;
                for (const Box& b : kPortBoxes) in = in || (u >= b.u0 && u < b.u1 && v >= b.v0 && v < b.v1);
                const unsigned char was = before[v - kRectV0][u - kRectU0], now = page[v - kRectV0][u - kRectU0];
                if (!in && was != now) fail("the repaint changed a texel outside the boxes", static_cast<unsigned>(u), static_cast<unsigned>(v));
                if (in && !IsPanel(now)) fail("ink left in a box", static_cast<unsigned>(u), static_cast<unsigned>(v));
                if (in && IsPanel(was) && was != now) fail("a panel texel in a box changed", static_cast<unsigned>(u), static_cast<unsigned>(v));
            }
        std::memcpy(before, page, sizeof page);
        Repaint(&page[0][0], kRectW * 2, kRectU0, kRectV0);
        if (std::memcmp(before, page, sizeof page) != 0) fail("the repaint is not idempotent", static_cast<unsigned>(round), 0);
    }
    bof3::Log("world_map_legend self-test: %u Configs (sigma against the words), 6 stand-ins, the dim rule (CLUT "
              "entries +7 of 31 over every 0x107th entry, the five dim CLUT words), 64 synthetic repaints; %u MISMATCHES", tables, mismatches);
    if (mismatches != 0 || tables != 720)
        bof3::Fatal("world_map_legend self-test: %u mismatches over %u Configs", mismatches, tables);
}

}  // namespace

void WorldMap_Inject() {
    if (bof3::WantsShadow("world_map")) world_map::SelfTest();
    BOF3_INJECT(WorldMap_FrameStep);
    BOF3_INJECT(WorldMap_DrawFrame);
    BOF3_INJECT(WorldMap_DrawSprite);
    BOF3_INJECT(WorldMap_DrawHud);
    BOF3_INJECT(WorldMap_DrawNeedle);
    BOF3_INJECT(MapView_ItemHalfAt);
    BOF3_INJECT(MapView_LinkPrimAt);
}
