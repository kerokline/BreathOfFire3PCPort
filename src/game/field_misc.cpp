// Group M of the sixth takeover round (docs/field-misc.md): sixteen functions,
// each read to its last instruction with capstone against bof3/BOF3.exe and,
// where one is paired, against its PSX twin in SLPS_009.90 or the field
// overlay. Faithful: no divergence. The start-up fuzz is field_misc_fuzz.cpp.
//
//   AreaMap_BlockedNarrow  0x5183C0..0x5185FF (0x240) + table 0x518600 (7)  PSX 0x801A2EE0
//   AreaMap_TooSteepAt     0x5187A0..0x5187B2 (0x13)                         PSX 0x801A3244
//   MapCell_FlatOverlay    0x570AB0..0x570BBF (0x110)                        -
//   AreaMap_ApplyPatch     0x571110..0x5712DB (0x1CC)                        PSX 0x801581B8
//   Gfx_ClutAdjust         0x5718F0..0x571A2E (0x13F)                        PSX 0x80158CE8
//   MapView_SlopeAt        0x5725C0..0x5725E3 (0x24)                         PSX 0x80155A34
//   MoveCmd_TestFB         0x572650..0x572789 (0x13A)                        PSX 0x80155B38
//   MoveCmd_TestFC         0x572790..0x5728C9 (0x13A)                        PSX 0x80155CD4
//   MapView_ItemAt         0x572ED0..0x572F67 (0x98)                         PSX 0x801563AC
//   D3d_DrawPolyF4         0x5A0AB0..0x5A0C34 (0x185)  code 0x28             -
//   D3d_DrawPolyG4         0x5A1290..0x5A14BE (0x22F)  code 0x38             -
//   D3d_DrawLineF3         0x5A1A00..0x5A1B4B (0x14C)  code 0x48             -
//   Gpu_SetPolyF4          0x5A75B0..0x5A75C9 (0x1A)                         PSX 0x8017B31C
//   Gpu_SetPolyG4          0x5A7610..0x5A7629 (0x1A)                         PSX 0x8017B344
//   Gpu_SetLineF3          0x5A7670..0x5A7686 (0x17)                         PSX 0x8017B420
//   Gpu_SetDrawMove        0x5A7810..0x5A7838 (0x29)                         PSX 0x8017B554
#include "game/field_misc.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_misc_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_misc {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

const Callees kOriginals = {
    AreaMap_CellBlocked,
    AreaMap_TooSteep,
    AreaMap_TooSteepAt,
    Gpu_SetPolyF4,
    Gfx_CommitPrim,
    Area_TestCondition,
    MapView_ItemAt,
    Prim_SetTexture,
    MapView_CheckHeightScale,
    AreaMap_Slope,
    D3d_PrimColor,
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(kRetOnly)),
    D3d_SetBlend,
    D3d_SetShadeMode,
};
Callees g = kOriginals;

}  // namespace field_misc

namespace {

using namespace field_misc;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
std::int32_t Short(U v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
float Float(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
void PutFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// The map-view ring (docs/map-scroll.md): 0x38 rows of 0x1C cells, their heads
// MapView_Row / MapView_Column; MapView_Origin the map cell of the view's corner.
U OriginX() { return Word(At(at::MapView_OriginAt())); }
U OriginZ() { return Word(At(at::MapView_OriginAt() + 2)); }
std::int32_t Row() { return Short(Word(At(at::MapView_RowAt()))); }
std::int32_t Column() { return Short(Word(At(at::MapView_ColumnAt()))); }
unsigned char BufferIndex() { return At(at::Gfx_BufferIndexAt())[0]; }
U Header(U byte_offset) { return at::AreaMap_HeaderAt() + byte_offset; }

// A draw item's 0x48-byte half for this display buffer: DrawItems +
// (Gfx_BufferIndex + index * 2) * 0x48, all in 32 bits as the original's lea.
U ItemHalf(U index) { return at::DrawItemsAt() + (BufferIndex() + index * 2u) * 0x48u; }

// The diagonal ring lookup MoveCmd_TestFB / FC, MapView_ItemAt (and the
// cell handler 0x570A00, Capcom's) share: map cell (x', z') relative to the
// view's origin lies on ring row (x' + z' + MapView_Row + 1) mod 0x38 and
// column ((x' - z') / 2 + MapView_Column + 1) mod 0x1C - each wrapped by ONE
// subtraction, so a head outside 0..0x37 / 0..0x1B leaves the grid - when
// 0 <= x' + z' < 0x38 and 0 <= x' - z' < 0x38. `spill` is what the original
// leaves in eax on the way out: the sum, or the difference, or the column.
struct Ring {
    bool inside;
    U row, column;
    U spill;
};
Ring RingCell(std::int32_t dx, std::int32_t dz) {
    const std::int32_t s = static_cast<std::int32_t>(static_cast<U>(dx) + static_cast<U>(dz));
    if (s >= 0x38 || s < 0) return {false, 0, 0, static_cast<U>(dx)};
    std::int32_t row = static_cast<std::int32_t>(static_cast<U>(s) + static_cast<U>(Row()) + 1u);
    if (row >= 0x38) row -= 0x38;
    const std::int32_t d = static_cast<std::int32_t>(static_cast<U>(dx) - static_cast<U>(dz));
    if (d >= 0x38 || d < 0) return {false, 0, 0, static_cast<U>(d)};
    std::int32_t column = static_cast<std::int32_t>(static_cast<U>(d / 2) + static_cast<U>(Column()) + 1u);
    if (column >= 0x1C) column -= 0x1C;
    return {true, static_cast<U>(row), static_cast<U>(column), static_cast<U>(column)};
}

// --- the x87 sequences, as the draw handlers execute them (d3d_draw.cpp has the
// same; each rounds to the control word's precision exactly as Capcom's) ---
// fld a; fmul b; fstp
inline float X87Mul(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfmuls %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}
// fld a; fdiv b; fstp
inline float X87Div(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfdivs %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// D3d_Device, read afresh before every COM call as the original does.
void** Device() { return *reinterpret_cast<void** volatile*>(At(kDevice)); }
void* Method(void** device, U offset) { return (*reinterpret_cast<void***>(device))[offset / 4]; }
using Com3 = long(__stdcall*)(void*, U, U);
using Com6 = long(__stdcall*)(void*, U, U, U, U, U);
long SetTexture(U stage, U texture) {
    void** device = Device();
    return reinterpret_cast<Com3>(Method(device, 0x98))(device, stage, texture);
}
// DrawPrimitive(type, D3DFVF_TLVERTEX, D3d_Vertices, count, 0).
long DrawVertices(U type, U count) {
    void** device = Device();
    return reinterpret_cast<Com6>(Method(device, 0x70))(device, type, 0x1C4, kVertices, count, 0);
}
unsigned char* Vertex(U i) { return At(kVertices + i * 0x20); }
U DrawMode() { return Long(At(kDrawTpage)) & 0xFFFF; }
// sx, sy = the scales times the corner's float x, y; sz its z as it is (a mov);
// rhw = 0.1 / z. Diffuse as given; specular, tu and tv are not written.
void PutFlatCorner(unsigned char* out, const unsigned char* xyz, U diffuse) {
    PutFloat(out + 0x00, X87Mul(Float(At(kScaleX)), Float(xyz)));
    PutFloat(out + 0x04, X87Mul(Float(At(kScaleY)), Float(xyz + 4)));
    PutLong(out + 0x08, Long(xyz + 8));
    PutFloat(out + 0x0C, X87Div(Float(At(kRhwNumerator)), Float(xyz + 8)));
    PutLong(out + 0x10, diffuse);
}

// The two ops' shared walk: the map cell's run of 4-byte records; a record of
// kind 0x23 becomes kind 0x24 (stored at once) and names, by its low word plus
// AreaMap_PatchBase, a patch-list entry whose (entry & 0xF001) == `want` gets
// bit 0 set (want 0x8000) or cleared (0x8001). 1 when one did. `eax` is the
// original's whole register on the way out (see the naked forwarders below).
U TileTest(U x_arg, U z_arg, U want) {
    const std::int32_t dx = Short(x_arg - OriginX());   // 16-bit subtraction, then movsx
    const std::int32_t dz = Short(z_arg - OriginZ());
    const Ring ring = RingCell(dx, dz);
    if (!ring.inside) return ring.spill & 0xFFFFFF00u;   // xor al, al
    const U cell = Word(At(at::MapView_CellsAt() + (ring.column + ring.row * 28u) * 2u));
    if (cell == 0) return ring.column & 0xFFFF0000u;    // mov ax, [cell]; xor al, al
    const U index = cell + (Long(At(at::AreaMap_CellBaseAt())) & 0xFFFF);
    const U count = Long(At(kRunHeads + index * 4u)) >> 16;
    U walk = at::AreaMap_HeaderAt() + index * 4u;
    const U end = walk + count * 4u - 4u;
    if (walk == end) return index & 0xFFFFFF00u;          // mov al, bl with bl 0
    U found = 0;
    do {
        U record = Long(At(walk));
        if ((record & 0xFF000000u) == 0x23000000u) {
            record = (record & 0xFFFFFFu) | 0x24000000u;
            PutLong(At(walk), record);
            const U patch = at::AreaMap_HeaderAt() +
                            ((record & 0xFFFF) + (Long(At(at::AreaMap_PatchBaseAt())) & 0xFFFF)) * 4u;
            U entry = Long(At(patch));
            if ((entry & 0xF001) == want) {
                entry = want == 0x8000 ? entry | 1u : entry & ~1u;   // or al, 1 / and al, 0xFE
                found = 1;
                PutLong(At(patch), entry);
            }
        }
        walk += At(walk)[2] * 4u;   // the step, read after both stores
    } while (walk != end);
    return found;   // eax is the step byte, then al = bl
}

}  // namespace

// --- the map tests -----------------------------------------------------------

// original 0x5183C0 (PSX 0x801A2EE0): Field_MapBlockedAhead's test for a sprite
// of size 0. X, Y the cells (high words of x, y), fx, fy the fractions. By
// direction & 0xFF - 1 through the table 0x518600:
//   1: cell (X, Y); if fx: (X+1, Y), the slopes at (x & ~0xFFFF, y), (x + 0x8000, y)
//   7: cell (X, Y); if fy: (X, Y+1), the slopes at (x, y & ~0xFFFF), (x, y + 0x8000)
//   5: row Y+1 if fy, else Y: cell (X, row); if fx: (X+1, row) and direction 1's slopes
//   3: column X+1 if fx, else X: cell (col, Y); if fy: (col, Y+1) and direction 7's slopes
// then AreaMap_TooSteep(x, y) in each - its slopes through AreaMap_TooSteepAt;
// any other direction: 1, nothing called. The answers are summed as a byte
// (AreaMap_BlockedWide's shape, docs/field-blocked.md 1.1) and its non-zero-ness
// returned in al. The original passes the cells as dwords whose upper halves
// are the neighbouring argument's bytes (it reads its arguments unaligned, and
// X + 1 is added to the whole dword); AreaMap_CellBlocked reads 16 bits. Its one
// caller, Field_MapBlockedAhead (ours), reads al.
extern "C" unsigned char __cdecl AreaMap_BlockedNarrow(long x_arg, long y_arg, unsigned direction) {
    const U x = static_cast<U>(x_arg), y = static_cast<U>(y_arg);
    const auto X = static_cast<short>(x >> 16), Y = static_cast<short>(y >> 16);
    const auto X1 = static_cast<short>((x >> 16) + 1), Y1 = static_cast<short>((y >> 16) + 1);
    const bool fx = (x & 0xFFFF) != 0, fy = (y & 0xFFFF) != 0;
    const auto xl = static_cast<long>(x), yl = static_cast<long>(y);
    const auto x_cell = static_cast<long>(x & 0xFFFF0000u), y_cell = static_cast<long>(y & 0xFFFF0000u);
    const auto x_half = static_cast<long>(x + 0x8000u), y_half = static_cast<long>(y + 0x8000u);
    unsigned char sum;
    switch (direction & 0xFF) {
    case 1:
        sum = g.cell_blocked(X, Y);
        if (fx) {
            sum += g.cell_blocked(X1, Y);
            sum += g.too_steep_at(x_cell, yl);
            sum += g.too_steep_at(x_half, yl);
        }
        break;
    case 7:
        sum = g.cell_blocked(X, Y);
        if (fy) {
            sum += g.cell_blocked(X, Y1);
            sum += g.too_steep_at(xl, y_cell);
            sum += g.too_steep_at(xl, y_half);
        }
        break;
    case 5: {
        const short row = fy ? Y1 : Y;
        sum = g.cell_blocked(X, row);
        if (fx) {
            sum += g.cell_blocked(X1, row);
            sum += g.too_steep_at(x_cell, yl);
            sum += g.too_steep_at(x_half, yl);
        }
        break;
    }
    case 3: {
        const short column = fx ? X1 : X;
        sum = g.cell_blocked(column, Y);
        if (fy) {
            sum += g.cell_blocked(column, Y1);
            sum += g.too_steep_at(xl, y_cell);
            sum += g.too_steep_at(xl, y_half);
        }
        break;
    }
    default:
        return 1;
    }
    sum += g.too_steep(xl, yl);
    return sum != 0;
}

// original 0x5187A0 (PSX 0x801A3244, a wrapper there too): AreaMap_TooSteep(x,
// y), its eax passed through. Called only by AreaMap_BlockedNarrow (ten sites,
// E8 scan), which adds al.
extern "C" unsigned char __cdecl AreaMap_TooSteepAt(long x, long y) { return g.too_steep(x, y); }

// original 0x5725C0 (PSX 0x80155A34): the slope at (x, y) along `direction`
// with the height scale the leader's state asks for: MapView_CheckHeightScale,
// AreaMap_Slope(x, y, direction) - the three dwords passed on whole - then
// MapView_HeightScale = 0; returns AreaMap_Slope's eax untouched (the PSX
// sign-extends its low half; the PC's 102 callers get the whole register).
// AreaMap_Slope's scratch flag 0x903850 is left as it set it.
extern "C" long __cdecl MapView_SlopeAt(long x, long y, unsigned long direction) {
    g.check_height_scale();
    const long slope = g.slope(x, y, direction);
    At(at::MapView_HeightScaleAt())[0] = 0;
    return slope;
}

// original 0x572ED0 (PSX 0x801563AC): the draw item of map cell (x, y) - the
// low 12 bits of the word at +2 of its MapView_CellItems slot - or 0 when the
// cell is outside the view's ring. The arguments are whole dwords, less
// MapView_Origin's words sign-extended, in 32 bits (MoveCmd_TestFB's subtract
// in 16). Returns the whole eax: the masked word, or 0.
extern "C" unsigned long __cdecl MapView_ItemAt(long x, long y) {
    const Ring ring = RingCell(static_cast<std::int32_t>(static_cast<U>(x) - static_cast<U>(Short(OriginX()))),
                               static_cast<std::int32_t>(static_cast<U>(y) - static_cast<U>(Short(OriginZ()))));
    if (!ring.inside) return 0;
    return Word(At(at::MapView_CellItemsAt() + 2u + (ring.column + ring.row * 28u) * 4u)) & 0xFFF;
}

// originals 0x572650 (PSX 0x80155B38) and 0x572790 (PSX 0x80155CD4): the
// movement script's ops FB and FC (MoveScript_GroupF, docs/move-cmds.md) and
// about sixty other callers: at map cell (x, z) - the words less
// MapView_Origin's in 16 bits - walk the cell's run (the dword before it holds
// the count, in dwords, in its high half; each record steps by its byte +2
// dwords, read after the stores, until the step lands exactly on the last
// dword, which is not visited: a step of 0, or one past the end, runs on as in
// the original). Every record of kind 0x23 is made kind 0x24 - both ops do
// it - and the patch-list entry its low word names (plus AreaMap_PatchBase)
// is switched on (FB: from 0x8000 to 0x8001 under the mask 0xF001) or off
// (FC: 0x8001 to 0x8000). al: 1 when an entry was switched. The PSX is the
// same, its count a u16 read of the same half.
//
// Ours returns the original's whole eax, not only al: sixty-eight call sites,
// several of which return it on or partly overwrite it (an E8 scan,
// docs/field-misc.md section 2). The declared signature (symbols.toml, and
// the callers' pointer tables) is unsigned char, so each is a naked jump into
// a body that returns the register.
extern "C" unsigned long __cdecl FieldMisc_TestFB(unsigned long x, unsigned long z) {
    return TileTest(static_cast<U>(x), static_cast<U>(z), 0x8000);
}
extern "C" unsigned long __cdecl FieldMisc_TestFC(unsigned long x, unsigned long z) {
    return TileTest(static_cast<U>(x), static_cast<U>(z), 0x8001);
}
extern "C" __attribute__((naked)) unsigned char __cdecl MoveCmd_TestFB(short, short) {
    asm("jmp _FieldMisc_TestFB");
}
extern "C" __attribute__((naked)) unsigned char __cdecl MoveCmd_TestFC(short, short) {
    asm("jmp _FieldMisc_TestFC");
}

// --- the area patch list -------------------------------------------------------

// original 0x571110 (PSX 0x801581B8): one entry of the area's patch list
// (AreaMap_SetupEntries' second walk, and the cell handlers 0x571090 /
// 0x5710D0, and two callers in 0x413xxx / 0x41Cxxx). The entry's low word is an
// Area_TestCondition code; its result, al sign-extended, is `c` (0 or 1 from
// that function). The high word, read after the call, is the entry's length n
// in dwords. Records follow, each headed by a dword: kind (byte 3), bits 20-23
// a face, bits 16-19 a texture offset, x (byte 1), y (byte 0). While the
// dwords consumed are fewer than n (signed):
//   kind 0 (3 dwords): dword (c ? +8 : +4) into the area's texture run at
//       ((height * width + 1) / 2 + offset + tile + bits 16-19), `tile` the
//       word of map (x, y) - AreaMap_BakePatches' address (docs/map-scroll.md)
//       - then kind 1's work;
//   kind 1 (2 dwords - its +8 is the next record's head): the face's draw item
//       of MapView_ItemAt(x, y), when there is one - face 0 the item, 1 the
//       word at its +0x8E, 2 and up the word at +0x7E - its half for this
//       buffer given the dword (c ? +8 : +4) through Prim_SetTexture(tex,
//       half, 1), the head and dword re-read after the lookup;
//   kind 2 (3 dwords): dword (c ? +8 : +4) to AreaMap_Corners[width * y + x],
//       MapView_Redraw 2;
//   any other (2 dwords): the low byte of dword +4 >> ((c << 4) & 31) - its
//       low half for c 0, its high half for c 1 - to AreaMap_Bytes[width * y + x].
// The width and height are the header's bytes 0 and 1, its word +2 the offset,
// all re-read per record. The original also stores c over its own argument
// slot; every caller pops it unread (field-misc.md section 2), so ours does not.
extern "C" void __cdecl AreaMap_ApplyPatch(const unsigned char* entry) {
    const std::int32_t c = static_cast<signed char>(g.test_condition(Word(entry)));
    const auto n = static_cast<std::int32_t>(Long(entry) >> 16);
    const unsigned char* cursor = entry + 4;
    std::int32_t used = 0;
    if (n <= 0) return;
    do {
        const U head = Long(cursor);
        const unsigned char* const record = cursor;
        const U kind = head >> 24, x = (head >> 8) & 0xFF, y = head & 0xFF;
        switch (kind) {
        case 0: {
            const U header = Long(At(Header(0)));
            const U offset = Word(At(kHeaderOffset));
            const U width = header & 0xFF;
            const U tile = Word(At(Header(0) + (x + width * y + offset * 2u) * 2u));
            const auto area = static_cast<std::int32_t>(((header >> 8) & 0xFF) * width + 1u);
            const U run = ((head >> 16) & 0xF) + static_cast<U>(area / 2) + offset + tile;
            used += 1;
            const U value = Long(record + static_cast<U>(c) * 4u + 4u);
            cursor += 4;
            PutLong(At(Header(0) + run * 4u), value);
        }
            [[fallthrough]];
        case 1: {
            const U face = (Long(record) >> 20) & 0xF;
            const U item = static_cast<U>(g.item_at(static_cast<long>(x), static_cast<long>(y)));
            if (item != 0) {
                U index;
                if (face == 0) index = item;
                else if (face == 1) index = Word(At(kItemFaceA + item * 0x90u));
                else index = Word(At(kItemFaceB + item * 0x90u));
                unsigned char* const half = At(ItemHalf(index));
                g.set_texture(Long(record + static_cast<U>(c) * 4u + 4u), half, 1);
            }
            used += 2;
            cursor += 8;
            break;
        }
        case 2: {
            const U width = Long(At(Header(0))) & 0xFF;
            used += 3;
            const U value = Long(cursor + static_cast<U>(c) * 4u + 4u);
            cursor += 12;
            PutLong(At(at::AreaMap_CornersAt() + (width * y + x) * 4u), value);
            At(at::MapView_RedrawAt())[0] = 2;
            break;
        }
        default: {
            const U value = Long(cursor + 4) >> ((static_cast<U>(c) << 4) & 31);
            const U width = Long(At(Header(0))) & 0xFF;
            const U bytes = Long(At(at::AreaMap_BytesAt()));
            At(width * y + bytes + x)[0] = static_cast<unsigned char>(value);
            used += 2;
            cursor += 8;
            break;
        }
        }
    } while (used < n);
}

// --- the cell overlay and the palette tint -----------------------------------

// original 0x570AB0 (no PSX twin paired): the cell handler 0x570A00's draw, for
// map-cell records of kinds 0x10 and up with `faces` = kind - 0x10 and `item`
// the cell's draw item (MapView_ItemAt's lookup, inline there). For each of
// faces' bits 1, 2 and 4 that is set: a POLY_F4 at Gfx_PacketNext (read once
// for the face) made by Gpu_SetPolyF4, its four corners' x and y copied from
// the face's POLY_FT4 half for this buffer - bit 1 the item itself, bit 2 the
// item at its +0x8E, bit 4 the item at its +0x7E - its colour the 15-bit word
// of the area header at +0x1A + (faces >> 3) * 2 (arithmetic shift), each
// channel's five bits << 3, and Gfx_CommitPrim(Draw_OtSlot, 0x38). The
// original's eax on the way out is whatever the last call or its caller left;
// its one caller returns it to DrawLayer_Open, which does not read it.
extern "C" void __cdecl MapCell_FlatOverlay(int faces, unsigned item) {
    for (U bit = 1; bit < 8; bit <<= 1) {
        if (!(static_cast<U>(faces) & bit)) continue;
        unsigned char* const packet = At(Long(At(at::Gfx_PacketNextAt())));
        g.set_poly_f4(packet);
        U index;
        if (bit == 1) index = item;
        else if (bit == 2) index = Word(At(kItemFaceA + item * 0x90u));
        else index = Word(At(kItemFaceB + item * 0x90u));
        const unsigned char* const src = At(ItemHalf(index));
        for (U corner = 0; corner < 4; ++corner) {
            PutLong(packet + 8 + corner * 0xC, Long(src + 8 + corner * 0x10));
            PutLong(packet + 0xC + corner * 0xC, Long(src + 0xC + corner * 0x10));
        }
        const unsigned char* const colour = At(kOverlayColours + static_cast<U>(faces >> 3) * 2u);
        packet[4] = static_cast<unsigned char>((colour[0] & 0x1F) << 3);
        packet[5] = static_cast<unsigned char>(((Word(colour) >> 5) & 0x1F) << 3);
        packet[6] = static_cast<unsigned char>(((Word(colour) >> 10) & 0x1F) << 3);
        g.commit_prim(At(at::Draw_OtSlotAt())[0], 0x38);
    }
}

// original 0x5718F0 (PSX 0x80158CE8): the area palette's tint. For CLUT rows
// 3..14 whose bit of `rows` is set (bit 0 row 3; `rows` halved per row, signed),
// each 16-colour CLUT of the row - on row 3 only those whose bit of `columns`
// is set (`columns` halved per CLUT of every row processed, but tested on row 3
// alone, which comes first) - copied from Gfx_ClutStripSource to Gfx_ClutStrip
// with red += `red`, green += `green`, blue += `blue`, each only when non-zero
// and then clamped to 1..0x1F; bit 15 kept. Then Gfx_ClutStripDirty = 1.
// Returns the original's eax: `rows` after its twelve halvings. The halvings
// write the caller's argument slots; the five callers pop them unread.
extern "C" long __cdecl Gfx_ClutAdjust(int columns, int rows, int red, int green, int blue) {
    const auto channel = [](U v, int add) -> U {
        if (v == 0) return 0;
        const auto s = static_cast<std::int32_t>(v + static_cast<U>(add));
        if (s <= 0) return 1;
        if (s > 0x1F) return 0x1F;
        return static_cast<U>(s);
    };
    for (U row = 3; row < 15; ++row) {
        if (rows % 2 != 0) {
            for (U clut = 0; clut < 16; ++clut) {
                if (row != 3 || columns % 2 != 0) {
                    const U from = at::Gfx_ClutStripSourceAt() + (row * 16 + clut) * 32;
                    for (U k = 0; k < 16; ++k) {
                        const U w = Word(At(from + k * 2));
                        const U r = channel(w & 0x1F, red), gr = channel((w >> 5) & 0x1F, green),
                                b = channel((w >> 10) & 0x1F, blue);
                        PutWord(At(from + 0x4000 + k * 2), (((b << 5) | gr) << 5) | (w & 0x8000) | r);
                    }
                }
                columns /= 2;
            }
        }
        rows /= 2;
    }
    At(at::Gfx_ClutStripDirtyAt())[0] = 1;
    return rows;
}

// --- the Direct3D handlers -------------------------------------------------------
// Each is reached from one site of Gfx_DrawOTag's Direct3D table (d3d-draw.md
// section 2) and returns what DrawPrimitive returned (the caller does not read
// it). Untextured: SetTexture(0, NULL); the blend mode Gfx_DrawTpage's low word,
// read for each colour and again for the blend; the colour helper gets a null
// specular pointer, so the vertices' specular, tu and tv stay as the last draw
// left them. Not kept: F4 and F3 let the colour helper write the diffuse into
// their own `prim` argument slot, which the caller pops unread; G4 into locals.

// original 0x5A0AB0, code 0x28 (Gpu_SetPolyF4): a flat quad, four corners of
// 0xC bytes from +8 (float x, y, z), one colour from +4..+6.
extern "C" long __cdecl D3d_DrawPolyF4(const unsigned char* prim) {
    unsigned long diffuse;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], DrawMode(), &diffuse, nullptr);
    for (U i = 0; i < 4; ++i) PutFlatCorner(Vertex(i), prim + 8 + i * 0xC, static_cast<U>(diffuse));
    SetTexture(0, 0);
    g.ret_only(0);
    g.ret_only(0);
    g.set_blend(prim[7], DrawMode());
    g.set_shade(1);   // flat
    return DrawVertices(5, 4);   // TRIANGLESTRIP
}

// original 0x5A1290, code 0x38 (Gpu_SetPolyG4): a Gouraud quad, four corners of
// 0x10 bytes from +4: r, g, b, (code or pad), float x, y, z. The four colours
// first, each with the code byte and the mode read again; then the corners.
extern "C" long __cdecl D3d_DrawPolyG4(const unsigned char* prim) {
    unsigned long diffuse[4];
    for (U i = 0; i < 4; ++i) {
        const unsigned char* rgb = prim + 4 + i * 0x10;
        g.prim_color(rgb[0], rgb[1], rgb[2], prim[7], DrawMode(), &diffuse[i], nullptr);
    }
    for (U i = 0; i < 4; ++i) PutFlatCorner(Vertex(i), prim + 8 + i * 0x10, static_cast<U>(diffuse[i]));
    SetTexture(0, 0);
    g.ret_only(0);
    g.ret_only(0);
    g.set_blend(prim[7], DrawMode());
    g.set_shade(2);   // Gouraud
    return DrawVertices(5, 4);
}

// original 0x5A1A00, code 0x48 (Gpu_SetLineF3): a flat two-segment polyline,
// three corners of 0xC bytes from +8. The two bare-ret calls get 0 and 1, as
// the other lines' do.
extern "C" long __cdecl D3d_DrawLineF3(const unsigned char* prim) {
    unsigned long diffuse;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], DrawMode(), &diffuse, nullptr);
    for (U i = 0; i < 3; ++i) PutFlatCorner(Vertex(i), prim + 8 + i * 0xC, static_cast<U>(diffuse));
    SetTexture(0, 0);
    g.ret_only(0);
    g.ret_only(1);
    g.set_blend(prim[7], DrawMode());
    g.set_shade(1);
    return DrawVertices(3, 3);   // LINESTRIP
}

// --- the libgpu setters ------------------------------------------------------------
// The port's primitives hold floats: each setter writes the GPU code byte +7
// and the float 0.01 (0x3C23D70A) into every corner's z, where the PSX's wrote
// the length byte +3 instead. Each returns its argument in eax, as the
// originals leave it (mov eax, [esp + 4] first); the callers are many and
// unread here, so ours keeps the register.

// original 0x5A75B0 (PSX SetPolyF4 0x8017B31C): code 0x28, z at +0x10 +0x1C +0x28 +0x34.
extern "C" unsigned char* __cdecl Gpu_SetPolyF4(unsigned char* prim) {
    prim[7] = 0x28;
    for (U z = 0x10; z <= 0x34; z += 0xC) PutLong(prim + z, 0x3C23D70A);
    return prim;
}
// original 0x5A7610 (PSX SetPolyG4 0x8017B344): code 0x38, z at +0x10 +0x20 +0x30 +0x40.
extern "C" unsigned char* __cdecl Gpu_SetPolyG4(unsigned char* prim) {
    prim[7] = 0x38;
    for (U z = 0x10; z <= 0x40; z += 0x10) PutLong(prim + z, 0x3C23D70A);
    return prim;
}
// original 0x5A7670 (PSX SetLineF3 0x8017B420): code 0x48, z at +0x10 +0x1C
// +0x28. The PSX also writes the polyline's terminator 0x55555555 at +0x14;
// the PC's primitive has its third corner there and no terminator.
extern "C" unsigned char* __cdecl Gpu_SetLineF3(unsigned char* prim) {
    prim[7] = 0x48;
    for (U z = 0x10; z <= 0x28; z += 0xC) PutLong(prim + z, 0x3C23D70A);
    return prim;
}
// original 0x5A7810 (PSX SetDrawMove 0x8017B554): the port's move-image
// primitive - +4 the code dword 0xEC000000 (Gfx_DrawOTag hands it to
// Gfx_MoveImage(prim + 8, +0x10, +0x14)), +8 and +0xC the rect's eight bytes,
// +0x10 x and +0x14 y as whole dwords. In this order, the rect read after the
// code is stored (they may overlap). The PSX packs x and y into one word and
// makes a zero-sized rect a no-op; the PC does neither.
extern "C" unsigned char* __cdecl Gpu_SetDrawMove(unsigned char* prim, const unsigned char* rect, unsigned long x,
                                                  unsigned long y) {
    PutLong(prim + 4, 0xEC000000u);
    PutLong(prim + 8, Long(rect));
    const U second = Long(rect + 4);
    PutLong(prim + 0xC, second);
    PutLong(prim + 0x10, static_cast<U>(x));
    PutLong(prim + 0x14, static_cast<U>(y));
    return prim;
}

void FieldMisc_Inject() {
    if (bof3::WantsShadow("field_misc")) field_misc::SelfTest();
    BOF3_INJECT(AreaMap_BlockedNarrow);
    BOF3_INJECT(AreaMap_TooSteepAt);
    BOF3_INJECT(MapCell_FlatOverlay);
    BOF3_INJECT(AreaMap_ApplyPatch);
    BOF3_INJECT(Gfx_ClutAdjust);
    BOF3_INJECT(MapView_SlopeAt);
    BOF3_INJECT(MoveCmd_TestFB);
    BOF3_INJECT(MoveCmd_TestFC);
    BOF3_INJECT(MapView_ItemAt);
    BOF3_INJECT(D3d_DrawPolyF4);
    BOF3_INJECT(D3d_DrawPolyG4);
    BOF3_INJECT(D3d_DrawLineF3);
    BOF3_INJECT(Gpu_SetPolyF4);
    BOF3_INJECT(Gpu_SetPolyG4);
    BOF3_INJECT(Gpu_SetLineF3);
    BOF3_INJECT(Gpu_SetDrawMove);
}
