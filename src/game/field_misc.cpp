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

#include "game/d3d_lines.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/field_misc_callees.h"
#include "game/map_rim.h"
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
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(::bof3::Code(kRetOnly.v))),
    D3d_SetBlend,
    D3d_SetShadeMode,
};
Callees g = kOriginals;

}  // namespace field_misc

namespace {

using namespace field_misc;

unsigned char* AtFieldMisc(field_misc::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
field_misc::U WordFieldMisc(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
std::int32_t Short(field_misc::U v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
field_misc::U LongFieldMisc(const unsigned char* p) {
    field_misc::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutLongFieldMisc(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

// The map-view ring (docs/map-scroll.md): 0x38 rows of 0x1C cells, their heads
// MapView_Row / MapView_Column; MapView_Origin the map cell of the view's corner.
field_misc::U OriginX() { return WordFieldMisc(AtFieldMisc(field_misc::at::MapView_OriginAt())); }
field_misc::U OriginZ() { return WordFieldMisc(AtFieldMisc(field_misc::at::MapView_OriginAt() + 2)); }
std::int32_t Row() { return Short(WordFieldMisc(AtFieldMisc(field_misc::at::MapView_RowAt()))); }
std::int32_t Column() { return Short(WordFieldMisc(AtFieldMisc(field_misc::at::MapView_ColumnAt()))); }

// The diagonal ring lookup MoveCmd_TestFB / FC, MapView_ItemAt (and the
// cell handler 0x570A00, Capcom's) share: map cell (x', z') relative to the
// view's origin lies on ring row (x' + z' + MapView_Row + 1) mod 0x38 and
// column ((x' - z') / 2 + MapView_Column + 1) mod 0x1C - each wrapped by ONE
// subtraction, so a head outside 0..0x37 / 0..0x1B leaves the grid - when
// 0 <= x' + z' < 0x38 and 0 <= x' - z' < 0x38. `spill` is what the original
// leaves in eax on the way out: the sum, or the difference, or the column.
struct Ring {
    bool inside;
    field_misc::U row, column;
    field_misc::U spill;
};
Ring RingCell(std::int32_t dx, std::int32_t dz) {
    const std::int32_t s = static_cast<std::int32_t>(static_cast<field_misc::U>(dx) + static_cast<field_misc::U>(dz));
    if (s >= 0x38 || s < 0) return {false, 0, 0, static_cast<field_misc::U>(dx)};
    std::int32_t row = static_cast<std::int32_t>(static_cast<field_misc::U>(s) + static_cast<field_misc::U>(Row()) + 1u);
    if (row >= 0x38) row -= 0x38;
    const std::int32_t d = static_cast<std::int32_t>(static_cast<field_misc::U>(dx) - static_cast<field_misc::U>(dz));
    if (d >= 0x38 || d < 0) return {false, 0, 0, static_cast<field_misc::U>(d)};
    std::int32_t column = static_cast<std::int32_t>(static_cast<field_misc::U>(d / 2) + static_cast<field_misc::U>(Column()) + 1u);
    if (column >= 0x1C) column -= 0x1C;
    return {true, static_cast<field_misc::U>(row), static_cast<field_misc::U>(column), static_cast<field_misc::U>(column)};
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
    const U cell = WordFieldMisc(AtFieldMisc(at::MapView_CellsAt() + (ring.column + ring.row * 28u) * 2u));
    if (cell == 0) return ring.column & 0xFFFF0000u;    // mov ax, [cell]; xor al, al
    const U index = cell + (LongFieldMisc(AtFieldMisc(at::AreaMap_CellBaseAt())) & 0xFFFF);
    const U count = LongFieldMisc(AtFieldMisc(::bof3::Img(kRunHeads) + index * 4u)) >> 16;
    U walk = at::AreaMap_HeaderAt() + index * 4u;
    const U end = walk + count * 4u - 4u;
    if (walk == end) return index & 0xFFFFFF00u;          // mov al, bl with bl 0
    U found = 0;
    do {
        U record = LongFieldMisc(AtFieldMisc(walk));
        if ((record & 0xFF000000u) == 0x23000000u) {
            record = (record & 0xFFFFFFu) | 0x24000000u;
            PutLongFieldMisc(AtFieldMisc(walk), record);
            const U patch = at::AreaMap_HeaderAt() +
                            ((record & 0xFFFF) + (LongFieldMisc(AtFieldMisc(at::AreaMap_PatchBaseAt())) & 0xFFFF)) * 4u;
            U entry = LongFieldMisc(AtFieldMisc(patch));
            if ((entry & 0xF001) == want) {
                entry = want == 0x8000 ? entry | 1u : entry & ~1u;   // or al, 1 / and al, 0xFE
                found = 1;
                PutLongFieldMisc(AtFieldMisc(patch), entry);
            }
        }
        walk += AtFieldMisc(walk)[2] * 4u;   // the step, read after both stores
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
    const field_misc::U x = static_cast<field_misc::U>(x_arg), y = static_cast<field_misc::U>(y_arg);
    const auto X = static_cast<short>(x >> 16), Y = static_cast<short>(y >> 16);
    const auto X1 = static_cast<short>((x >> 16) + 1), Y1 = static_cast<short>((y >> 16) + 1);
    const bool fx = (x & 0xFFFF) != 0, fy = (y & 0xFFFF) != 0;
    const auto xl = static_cast<long>(x), yl = static_cast<long>(y);
    const auto x_cell = static_cast<long>(x & 0xFFFF0000u), y_cell = static_cast<long>(y & 0xFFFF0000u);
    const auto x_half = static_cast<long>(x + 0x8000u), y_half = static_cast<long>(y + 0x8000u);
    unsigned char sum;
    switch (direction & 0xFF) {
    case 1:
        sum = field_misc::g.cell_blocked(X, Y);
        if (fx) {
            sum += field_misc::g.cell_blocked(X1, Y);
            sum += field_misc::g.too_steep_at(x_cell, yl);
            sum += field_misc::g.too_steep_at(x_half, yl);
        }
        break;
    case 7:
        sum = field_misc::g.cell_blocked(X, Y);
        if (fy) {
            sum += field_misc::g.cell_blocked(X, Y1);
            sum += field_misc::g.too_steep_at(xl, y_cell);
            sum += field_misc::g.too_steep_at(xl, y_half);
        }
        break;
    case 5: {
        const short row = fy ? Y1 : Y;
        sum = field_misc::g.cell_blocked(X, row);
        if (fx) {
            sum += field_misc::g.cell_blocked(X1, row);
            sum += field_misc::g.too_steep_at(x_cell, yl);
            sum += field_misc::g.too_steep_at(x_half, yl);
        }
        break;
    }
    case 3: {
        const short column = fx ? X1 : X;
        sum = field_misc::g.cell_blocked(column, Y);
        if (fy) {
            sum += field_misc::g.cell_blocked(column, Y1);
            sum += field_misc::g.too_steep_at(xl, y_cell);
            sum += field_misc::g.too_steep_at(xl, y_half);
        }
        break;
    }
    default:
        return 1;
    }
    sum += field_misc::g.too_steep(xl, yl);
    return sum != 0;
}

// original 0x5187A0 (PSX 0x801A3244, a wrapper there too): AreaMap_TooSteep(x,
// y), its eax passed through. Called only by AreaMap_BlockedNarrow (ten sites,
// E8 scan), which adds al.
extern "C" unsigned char __cdecl AreaMap_TooSteepAt(long x, long y) { return field_misc::g.too_steep(x, y); }

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
// a body that returns the register. The jump is the same on both widths: a
// tail jump forwards the arguments where they lie (the stack on i686, ecx and
// edx on x86-64) and the body answers in eax; the symbol's spelling is
// common/asm_name.h's.
extern "C" unsigned long __cdecl FieldMisc_TestFB(unsigned long x, unsigned long z) {
    return TileTest(static_cast<U>(x), static_cast<U>(z), 0x8000);
}
extern "C" unsigned long __cdecl FieldMisc_TestFC(unsigned long x, unsigned long z) {
    return TileTest(static_cast<U>(x), static_cast<U>(z), 0x8001);
}

// --- the area patch list -------------------------------------------------------

// --- the cell overlay and the palette tint -----------------------------------

// --- the Direct3D handlers -------------------------------------------------------
// Each is reached from one site of Gfx_DrawOTag's Direct3D table (d3d-draw.md
// section 2) and returns what DrawPrimitive returned (the caller does not read
// it). Untextured: SetTexture(0, NULL); the blend mode Gfx_DrawTpage's low word,
// read for each colour and again for the blend; the colour helper gets a null
// specular pointer, so the vertices' specular, tu and tv stay as the last draw
// left them. Not kept: F4 and F3 let the colour helper write the diffuse into
// their own `prim` argument slot, which the caller pops unread; G4 into locals.

// --- the libgpu setters ------------------------------------------------------------
// The port's primitives hold floats: each setter writes the GPU code byte +7
// and the float 0.01 (0x3C23D70A) into every corner's z, where the PSX's wrote
// the length byte +3 instead. Each returns its argument in eax, as the
// originals leave it (mov eax, [esp + 4] first); the callers are many and
// unread here, so ours keeps the register.

void FieldMisc_Inject() {
    if (bof3::WantsShadow("field_misc")) field_misc::SelfTest();
    BOF3_INJECT(AreaMap_BlockedNarrow);
    BOF3_INJECT(AreaMap_TooSteepAt);
}
