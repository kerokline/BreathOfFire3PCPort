// Group 1 of the world-map wave (docs/world-map.md section 7): seven functions,
// each read to its last instruction with capstone against bof3/BOF3.exe and,
// for the map's five, against the PlayStation's world-map overlay - the JP
// disc's WORLD00/AREA016.EMI section at 0x801F2C00, read with capstone MIPS
// through the sibling's tools - and for the two helpers against the boot exe.
// Faithful: no divergence (the dial's opacity is a port change, kept: D42).
// The start-up fuzz is world_map_fuzz.cpp.
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

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/world_map_callees.h"
#include "hook/detour.h"
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

// The button table search the frame runs for each legend: the first entry
// (of `count`) whose mask word has a bit of the button word's low 16 bits;
// its sprite index byte, or -1 for none.
int KeyIndex(U button_word, U count) {
    for (U k = 0; k < count; ++k) {
        const unsigned char* entry = At(kButtonTable + k * 4);
        if ((Word(entry) & button_word & 0xFFFF) != 0) return entry[2];
    }
    return -1;
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
    g.set_draw_mode(PacketNext(), 0, 0, 0x9C, 0);
    g.commit_prim(1, 0xC);
    g.draw_sprite(x, y, 0);
    const unsigned char cell =
        g.byte_at(static_cast<short>(Word(At(kLeaderCellX))), static_cast<short>(Word(At(kLeaderCellZ))));
    const bool withheld1 =
        cell == 0xA0 || cell == 0xA1 || cell == 0xAE || (Word(At(at::Field_ScriptFlags2At())) & 0x1000) != 0;
    if (!withheld1) g.draw_sprite(x + 0x30, y, 1);
    {
        const int key = KeyIndex(Long(At(kButtonMap0)), 6);
        if (key >= 0) g.draw_sprite(x + 0x38, y + 8, static_cast<unsigned>(withheld1 ? key : ((key + 1) & 0xFF)));
    }
    const bool withheld2 = cell == 0xA1 || cell == 0xA0;
    if (!withheld2) g.draw_sprite(x + 0x30, y + 0x10, 2);
    {
        const int key = KeyIndex(Long(At(kButtonMap6)), 8);
        if (key >= 0) g.draw_sprite(x + 0x38, y + 0x10, static_cast<unsigned>(withheld2 ? key : ((key + 1) & 0xFF)));
    }
    const unsigned char event =
        g.cell_event(static_cast<short>(Word(At(kLeaderCellX))), static_cast<short>(Word(At(kLeaderCellZ))));
    bool third = event != 0;
    if (!third) third = (Word(At(at::Field_ScriptFlags2At())) & 0x1000) != 0;
    if (!third) third = (At(kPartySet)[0] & 0x7F) == 0xC;
    if (!third) third = (Word(At(at::Field_ScriptFlagsAt())) & 0x4000) != 0;
    if (third) g.draw_sprite(x + 0x30, y + 0x18, 3);
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
    const U bound = (static_cast<U>(At(at::Gfx_BufferIndexAt())[0]) << 16) + kPoolBound;
    if (bound <= prim + bytes) return;
    const U slot_index = static_cast<U>(row) * 6u;
    unsigned char* const slot = At(kRowTails + (slot_index + At(at::Gfx_BufferIndexAt())[0]) * 8u);
    g.link_prim(reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(Long(slot))), prim);
    const U next = Long(At(at::Gfx_PacketNextAt()));
    PutLong(At(kRowTails + (slot_index + At(at::Gfx_BufferIndexAt())[0]) * 8u), next);
    PutLong(At(at::Gfx_PacketNextAt()), next + bytes);
}

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
