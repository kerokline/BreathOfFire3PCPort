#include "game/map_view.h"

#include <cstdint>
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

void MapView_Inject() {
    // 0x56F910..0x56F9A0: no calls, every jump internal (disasm 2026-09-20).
    if (bof3::WantsShadow("map_view"))
        SelfTest(reinterpret_cast<CellToMapFn>(
            bof3::CloneOriginal("MapView_CellToMap", bof3::addr::MapView_CellToMap, 0x91)));
    BOF3_INJECT(MapView_CellToMap);
}
