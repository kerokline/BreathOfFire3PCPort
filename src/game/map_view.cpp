#include "game/map_view.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// --- BOF3X_SHADOW=map_view: a differential fuzz, once at start-up --------------
// No calls and every jump internal, so a byte-copy runs in place against the
// same globals. The fuzz owns the first kOwned bytes of the area map and the
// view origin for its duration and puts them back: random header with the
// offset word held small enough that every cell it can name lies inside
// kOwned, cells a third zero, origins and arguments that land outside the map
// about as often as inside, and a quarter of the rounds aimed at the edges
// (x or y of 0, 1, size - 2, size - 1), where the four comparisons live.

constexpr unsigned kOwned = 0x30000;
constexpr unsigned kMaxOffset = 0x3FF;   // (255 + 255 * 255 + 0x3FF * 2) * 2 + 2 < kOwned
static_assert((255u + 255u * 255u + kMaxOffset * 2u) * 2u + 2u <= kOwned);

std::uint32_t g_rng = 0xC2B2AE35u;
std::uint32_t Rng() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}
int RngIn(int lo, int hi) { return lo + static_cast<int>(Rng() % static_cast<unsigned>(hi - lo + 1)); }

using CellToMapFn = void (__cdecl*)(int, int, unsigned char*);

void SelfTest(CellToMapFn theirs) {
    constexpr unsigned kRounds = 20000;
    static unsigned char saved[kOwned];
    std::memcpy(saved, AreaMap_Header, kOwned);
    const short saved_origin[2] = {MapView_Origin[0], MapView_Origin[1]};

    unsigned bad = 0, inside = 0, empty = 0, outside = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        if (round % 200 == 0) {
            auto* words = reinterpret_cast<unsigned short*>(AreaMap_Header);
            for (unsigned i = 0; i < kOwned / 2; ++i)
                words[i] = Rng() % 3 == 0 ? 0 : static_cast<unsigned short>(Rng());
            AreaMap_Header[0] = static_cast<unsigned char>(Rng() % 8 == 0 ? RngIn(0, 3) : RngIn(4, 255));
            AreaMap_Header[1] = static_cast<unsigned char>(Rng() % 8 == 0 ? RngIn(0, 3) : RngIn(4, 255));
            words[1] = static_cast<unsigned short>(RngIn(0, kMaxOffset));
        }
        const int width = AreaMap_Header[0], height = AreaMap_Header[1];
        const int row = RngIn(-40, 120), col = RngIn(-40, 60);
        int x = RngIn(-20, width + 20), y = RngIn(-20, height + 20);
        if (Rng() % 4 == 0) {
            const int ex[] = {0, 1, width - 2, width - 1}, ey[] = {0, 1, height - 2, height - 1};
            if (Rng() % 2) x = ex[Rng() % 4]; else y = ey[Rng() % 4];
        }
        // Choose the origin that puts this (row, col) on the wanted (x, y).
        MapView_Origin[0] = static_cast<short>(x - col - (row + 1) / 2);
        MapView_Origin[1] = static_cast<short>(y - row / 2 + col);

        unsigned char their_out[4] = {0xA5, 0xA5, 0xA5, 0xA5}, ours[4] = {0xA5, 0xA5, 0xA5, 0xA5};
        theirs(row, col, their_out);
        MapView_CellToMap(row, col, ours);
        if (ours[0] == 0 && ours[1] == 0)
            ++((x > 0 && x < width - 1 && y > 0 && y < height - 1) ? empty : outside);
        else
            ++inside;
        if (std::memcmp(their_out, ours, sizeof ours) != 0 && ++bad <= 8)
            bof3::Log("shadow      MapView_CellToMap self-test MISMATCH round %u: row %d col %d, map %dx%d, "
                      "aimed at (%d,%d): {%u,%u} vs ours {%u,%u}", round, row, col, width, height, x, y,
                      their_out[0], their_out[1], ours[0], ours[1]);
    }
    std::memcpy(AreaMap_Header, saved, kOwned);
    MapView_Origin[0] = saved_origin[0];
    MapView_Origin[1] = saved_origin[1];
    bof3::Log("shadow      MapView_CellToMap self-test: %u rounds (%u on a cell, %u on an empty cell, %u outside "
              "the map), %u MISMATCHES; 4 output bytes compared", kRounds, inside, empty, outside, bad);
    if (bad) bof3::Fatal("MapView_CellToMap differs from the original in %u of %u self-test rounds", bad, kRounds);
}

// The same for AreaMap_ByteAt: a random plane with AreaMap_Bytes pointed at
// its middle, so that negative coordinates are in bounds; random width byte;
// x and y over all 16 bits, with noise above bit 15 of both arguments, which
// the original sign-extends away.
using ByteAtFn = unsigned char (__cdecl*)(unsigned, unsigned);

void SelfTestByteAt(ByteAtFn theirs) {
    constexpr unsigned kRounds = 8000;
    constexpr int kHalf = 0x820000;   // |x + y * width| <= 0x8000 + 0x8000 * 255 < kHalf
    // 17 MB, so from the heap and given back, not a static the DLL carries.
    auto* const plane = static_cast<unsigned char*>(std::malloc(2 * kHalf));
    if (!plane) { bof3::Log("shadow      AreaMap_ByteAt self-test SKIPPED: no memory"); return; }
    for (int i = 0; i < 2 * kHalf; ++i) plane[i] = static_cast<unsigned char>(Rng());
    const unsigned char* const saved = AreaMap_Bytes;
    const unsigned char saved_width = AreaMap_Header[0];
    AreaMap_Bytes = plane + kHalf;

    unsigned bad = 0, negative = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        AreaMap_Header[0] = static_cast<unsigned char>(Rng());
        const unsigned x = Rng(), y = Rng();
        if ((x | y) & 0x8000u) ++negative;
        const unsigned char their_result = theirs(x, y);
        const unsigned char our_result = reinterpret_cast<ByteAtFn>(reinterpret_cast<void*>(&AreaMap_ByteAt))(x, y);
        if (their_result != our_result && ++bad <= 8)
            bof3::Log("shadow      AreaMap_ByteAt self-test MISMATCH round %u: x 0x%08X y 0x%08X width %u: %u vs ours %u",
                      round, x, y, AreaMap_Header[0], their_result, our_result);
    }
    AreaMap_Bytes = saved;
    AreaMap_Header[0] = saved_width;
    std::free(plane);
    bof3::Log("shadow      AreaMap_ByteAt self-test: %u rounds (%u with a negative coordinate), %u MISMATCHES",
              kRounds, negative, bad);
    if (bad) bof3::Fatal("AreaMap_ByteAt differs from the original in %u of %u self-test rounds", bad, kRounds);
}

// The same for MapView_SetElevation: random elevation, offset and argument -
// the argument a clean s16 half the time and any 32 bits the other half - with
// the header word that switches the offset off set one round in three. The
// header bytes +0x1E, +0x1F are outside what AreaMap_Header declares; the
// fuzz saves and restores them with the rest.
using SetElevationFn = void (__cdecl*)(int);

void SelfTestSetElevation(SetElevationFn theirs) {
    constexpr unsigned kRounds = 8000;
    unsigned char saved_fixed[2];
    std::memcpy(saved_fixed, AreaMap_Header + 0x1E, 2);
    const long saved_elevation = MapView_Elevation;
    const unsigned short saved_offset = MapView_ElevationOffset;
    const unsigned char saved_redraw = MapView_Redraw;

    unsigned bad = 0, fixed_rounds = 0, wide = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        const bool fixed = Rng() % 3 == 0;
        const std::uint16_t fixed_word = fixed ? static_cast<std::uint16_t>(Rng() | 1u) : 0;
        std::memcpy(AreaMap_Header + 0x1E, &fixed_word, 2);
        const long elevation_in = static_cast<long>(Rng() % 2 ? static_cast<short>(Rng()) : static_cast<std::int32_t>(Rng()));
        const unsigned short offset_in = static_cast<unsigned short>(Rng());
        const int value = Rng() % 2 ? static_cast<short>(Rng()) : static_cast<int>(Rng());
        fixed_rounds += fixed;
        if (value != static_cast<short>(value)) ++wide;

        long elevation[2];
        unsigned short offset[2];
        unsigned char redraw[2];
        for (int pass = 0; pass < 2; ++pass) {
            MapView_Elevation = elevation_in;
            MapView_ElevationOffset = offset_in;
            MapView_Redraw = static_cast<unsigned char>(0xA5);
            if (pass) MapView_SetElevation(value); else theirs(value);
            elevation[pass] = MapView_Elevation;
            offset[pass] = MapView_ElevationOffset;
            redraw[pass] = MapView_Redraw;
        }
        if ((elevation[0] != elevation[1] || offset[0] != offset[1] || redraw[0] != redraw[1]) && ++bad <= 8)
            bof3::Log("shadow      MapView_SetElevation self-test MISMATCH round %u: value 0x%08X: elevation %ld vs %ld, "
                      "offset 0x%04X vs 0x%04X", round, static_cast<unsigned>(value), elevation[0], elevation[1],
                      offset[0], offset[1]);
    }
    std::memcpy(AreaMap_Header + 0x1E, saved_fixed, 2);
    MapView_Elevation = saved_elevation;
    MapView_ElevationOffset = saved_offset;
    MapView_Redraw = saved_redraw;
    bof3::Log("shadow      MapView_SetElevation self-test: %u rounds (%u with the offset switched off, %u with an "
              "argument wider than 16 bits), %u MISMATCHES", kRounds, fixed_rounds, wide, bad);
    if (bad) bof3::Fatal("MapView_SetElevation differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

// original 0x56F910. Which map cell the view's cell (row, col) shows, as two
// bytes to out - or 0, 0 for none. The view is isometric: going down a row
// moves half a cell in x and in y, going along a column one cell in x and
// back one in y.
//
// As the original has it: the outermost ring of the map never shows (0 < x <
// width - 1), a cell whose word is 0 does not show, and "none" and the map
// cell (0, 0) are the same answer - which costs nothing, (0, 0) being in the
// ring. Only the low bytes of x and y are kept; with a width and height of one
// byte each, nothing is lost.
extern "C" void __cdecl MapView_CellToMap(int row, int col, unsigned char* out) {
    const int x = MapView_Origin[0] + col + (row + 1) / 2;
    const int y = MapView_Origin[1] + row / 2 - col;
    const int width = AreaMap_Header[0], height = AreaMap_Header[1];
    const unsigned offset = static_cast<unsigned>(AreaMap_Header[2] | (AreaMap_Header[3] << 8));
    if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
        const auto* cells = reinterpret_cast<const unsigned short*>(AreaMap_Header);
        if (cells[static_cast<unsigned>(x + width * y) + offset * 2] != 0) {
            out[0] = static_cast<unsigned char>(x);
            out[1] = static_cast<unsigned char>(y);
            return;
        }
    }
    out[0] = 0;
    out[1] = 0;
}

// original 0x536700. The area map's byte for a cell. No bounds check; the
// arguments are 16 bits, signed.
extern "C" unsigned char __cdecl AreaMap_ByteAt(short x, short y) {
    return AreaMap_Bytes[x + y * static_cast<int>(AreaMap_Header[0])];
}

// original 0x5725F0. Sets the view's elevation and asks for a redraw; unless
// the area's header word +0x1E is set, the offset moves by two for every unit
// the elevation changed.
//
// The original takes the difference from all 32 bits of the argument and the
// stored elevation from its low 16; since the offset is itself 16 bits the
// first cannot show, and a build that differed there passed the fuzz.
extern "C" void __cdecl MapView_SetElevation(int value) {
    std::uint16_t fixed;
    std::memcpy(&fixed, AreaMap_Header + 0x1E, sizeof fixed);
    if (fixed == 0) {
        const std::uint32_t moved = (static_cast<std::uint32_t>(MapView_Elevation) - static_cast<std::uint32_t>(value)) << 1;
        MapView_ElevationOffset = static_cast<unsigned short>(MapView_ElevationOffset + moved);
    }
    MapView_Elevation = static_cast<short>(value);
    MapView_Redraw = 2;
}

void MapView_Inject() {
    // 0x56F910..0x56F9A0: no calls, every jump internal. 0x536700..0x536722:
    // no calls, no jumps. 0x5725F0..0x57261F: no calls, one jump, internal
    // (disasm 2026-09-20).
    if (bof3::WantsShadow("map_view")) {
        SelfTest(reinterpret_cast<CellToMapFn>(
            bof3::CloneOriginal("MapView_CellToMap", bof3::addr::MapView_CellToMap, 0x91)));
        SelfTestByteAt(reinterpret_cast<ByteAtFn>(
            bof3::CloneOriginal("AreaMap_ByteAt", bof3::addr::AreaMap_ByteAt, 0x23)));
        SelfTestSetElevation(reinterpret_cast<SetElevationFn>(
            bof3::CloneOriginal("MapView_SetElevation", bof3::addr::MapView_SetElevation, 0x30)));
    }
    BOF3_INJECT(MapView_CellToMap);
    BOF3_INJECT(AreaMap_ByteAt);
    BOF3_INJECT(MapView_SetElevation);
}
