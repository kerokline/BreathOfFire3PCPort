// Area 4's walls, as every build after the Japanese one has them. See
// area4_walls.h and docs/region-diff.md sections 8.1, 8.2 and 10.
//
// AREA004.DAT's kind-0 chunk of tag 0xC8000 (the PSX section for 0x80104000)
// is the area block at AreaMap_Header 0x8CB580: byte 0 the map's width (90),
// byte 1 its depth (88), and at 4 x its dword +0x14 the cell bytes,
// AreaMap_Bytes, one per cell at width * z + x - the byte AreaMap_ByteAt
// answers and AreaMap_CellBlocked 0x518620 tests (a high nibble of 1..5, A, B
// or F blocks a step; field_blocked.cpp). The chunk of tag 0xC0800 (PSX
// 0x8002A000) is the battle placement map at 0x8C3D80, a nibble per cell at
// the same index, the high nibble of byte i / 2 for an even i
// (AreaMap_CellNibble 0x592890, inventory_ops.cpp); 0 places no one.
//
// The values are the later discs': each of the 72 cells is 0x00 on the JP disc
// and 0x10 on the US, French, German and PSP discs - 0x10 being the value the
// JP map's own wall stubs on the same lines already carry (x 28 at z 7, 8,
// 31, 34, 66..68; x 25 at z 7, 8; the row z 68 over x 7..21). The 8 placement
// nibbles go to 0 as on the Western PSX discs (the PSP kept JP's map there);
// on the JP data a blocking cell has nibble 0 in 980,813 of 981,024 cells over
// all 200 areas, so 0 is the map's own rule. tools/region_read.py `fix`
// applies this table to the JP disc's sections and compares with the US
// disc's: the cell bytes and the placement map come out identical.
//
// Not taken: the 30 cells the later discs re-texture (x 26 at z 10, 11 and
// 45..63; x 52 at z 59..67). That change inserts, replaces and drops texture
// records whose contents are Capcom's - not expressible by coordinate without
// shipping their bytes (docs/region-diff.md section 10).
#include "game/area4_walls.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/log.h"

namespace area4_walls {

unsigned char g_on = 0;

namespace {

constexpr std::uint32_t kMapTag = 0xC8000;      // the area block, AreaMap_Header
constexpr std::uint32_t kPlaceTag = 0xC0800;    // the placement nibble map, 0x8C3D80
constexpr unsigned kWidth = 90, kDepth = 88;
constexpr std::uint8_t kOpen = 0x00, kWall = 0x10;

struct Run {
    std::uint8_t x0, z0, x1, z1;   // inclusive
};
// The 72 cells, as runs.
constexpr Run kWalls[] = {
    {28, 9, 28, 30},    // the raised strip's east edge (x 26..27 are the strip), north of the doorway at z 32..33
    {28, 35, 28, 65},   // the same edge, south of the doorway
    {25, 9, 25, 11},    // the strip's west side at its north end, below JP's stubs at z 7, 8
    {7, 71, 22, 71},    // the row three below the wall row z 68: the bottom edge of the corridor z 69..70
};

struct Nibble {
    std::uint8_t x, z, jp;   // jp: the value on the JP disc, checked before writing 0
};
// The 8 placement cells the Western PSX discs clear: the new walls whose
// nibble was not already 0.
constexpr Nibble kPlacement[] = {
    {28, 10, 6}, {28, 11, 2}, {28, 64, 1},
    {14, 71, 2}, {15, 71, 1}, {18, 71, 2}, {19, 71, 2}, {20, 71, 1},
};

std::uint8_t* Arena() { return reinterpret_cast<std::uint8_t*>(bof3::addr::MessagePools); }

std::uint8_t* Cells() {
    std::uint8_t* map = Arena() + kMapTag;
    std::uint32_t plane;
    std::memcpy(&plane, map + 0x14, 4);
    return map + 4 * plane;
}

std::uint8_t NibbleAt(const std::uint8_t* m, unsigned i) { return (i & 1) ? (m[i >> 1] & 0xF) : (m[i >> 1] >> 4); }
void SetNibble(std::uint8_t* m, unsigned i, std::uint8_t v) {
    m[i >> 1] = (i & 1) ? static_cast<std::uint8_t>((m[i >> 1] & 0xF0) | v)
                        : static_cast<std::uint8_t>((m[i >> 1] & 0x0F) | (v << 4));
}

// How many of the 72 cells hold `v`.
unsigned CountCells(const std::uint8_t* cells, std::uint8_t v) {
    unsigned n = 0;
    for (const Run& r : kWalls)
        for (unsigned z = r.z0; z <= r.z1; ++z)
            for (unsigned x = r.x0; x <= r.x1; ++x) n += cells[kWidth * z + x] == v;
    return n;
}

bool g_logged = false, g_warned = false;

}  // namespace

void Apply(const char* name) {
    if (!g_on || _stricmp(name, "AREA004.DAT") != 0) return;
    const std::uint8_t* map = Arena() + kMapTag;
    std::uint8_t* cells = Cells();
    std::uint8_t* place = Arena() + kPlaceTag;
    unsigned jp_nibbles = 0, zero_nibbles = 0;
    for (const Nibble& c : kPlacement) {
        const std::uint8_t v = NibbleAt(place, kWidth * c.z + c.x);
        jp_nibbles += v == c.jp;
        zero_nibbles += v == 0;
    }
    const unsigned open = CountCells(cells, kOpen), walled = CountCells(cells, kWall);
    if (map[0] == kWidth && map[1] == kDepth && walled == 72 && zero_nibbles == 8) {
        if (!g_logged) bof3::Log("area4_walls: AREA004 already has the later discs' walls; left as loaded");
        g_logged = true;
        return;
    }
    if (map[0] != kWidth || map[1] != kDepth || open != 72 || jp_nibbles != 8) {
        if (!g_warned)
            bof3::Log("area4_walls: AREA004 is not the map this fix knows (%u x %u, %u of 72 cells open, %u of 8 "
                      "placement cells as JP); left as loaded", map[0], map[1], open, jp_nibbles);
        g_warned = true;
        return;
    }
    for (const Run& r : kWalls)
        for (unsigned z = r.z0; z <= r.z1; ++z)
            for (unsigned x = r.x0; x <= r.x1; ++x) cells[kWidth * z + x] = kWall;
    for (const Nibble& c : kPlacement) SetNibble(place, kWidth * c.z + c.x, 0);
    if (!g_logged)
        bof3::Log("area4_walls: AREA004's 72 open edge cells walled (0x10) and 8 placement cells cleared, as the "
                  "later discs have them (BOF3X_AREA4_WALLS=0 for the shipped map)");
    g_logged = true;
}

void Arm() {
    char text[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_AREA4_WALLS", text, sizeof text);
    if (n > 1 || (n == 1 && text[0] != '0' && text[0] != '1')) bof3::Fatal("BOF3X_AREA4_WALLS must be 0 or 1");
    if (n == 1 && text[0] == '0') {
        bof3::Log("area4_walls off (BOF3X_AREA4_WALLS=0): AREA004's map as shipped");
        return;
    }
    g_on = 1;
    bof3::Log("area4_walls Dauna Mine's minecart area gets the later discs' walls (BOF3X_AREA4_WALLS=0 for the shipped map)");
}

}  // namespace area4_walls
