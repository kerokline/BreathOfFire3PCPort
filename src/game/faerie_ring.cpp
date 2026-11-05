// DIVERGENCE DIV-0204, the drawn ring (docs/DIVERGENCE.md, docs/desert-faerie.md).
//
// The world map's faerie entrances are painted into their cell's tile: the
// ring and the ground under it are one 16 x 16 texture of the map's own page
// (measured 2026-11-05: Rhapala's (0x2D, 0x2D) texture 0x3E on grass, Lost
// Shore's (0x36, 0x1A) texture 0x5F on dirt). The desert map's page has no
// ring, and its sand tile is shared by hundreds of cells, so the ring is made
// here and given to the one cell:
//
//   - the ring: the texels and palette of the texture that Lost Shore's 0xAE
//     cell names (the player's AREA151.DAT: kind 1 tag 0x0A081000, kind 0
//     tags 0xC8000 and 0x8600);
//   - its ground, taken out: every texel whose colour is within 12 (RGB at
//     8 bits a channel) of a colour of the textures of the tile west of that
//     cell, its dirt - the cut-off the owner chose from four (2026-11-05);
//   - the desert's sand put in its place: the texel at the same place of the
//     texture the desert cell's own tile names, from the loaded page;
//   - the result's colours, one entry each, written at the end of palette
//     row kRingRow (below), and its texels into the first page-0 grid cell
//     that no texture word names and the loaded page leaves zero.
//
// So nothing of Capcom's is in this source: each choice is a rule over the
// player's data. The page and palette writes go through Gfx_LoadImage and
// both CLUT strips (as loaded and live, Gfx_ClutStripDirty set), so a fade
// that rebuilds the live strip from the loaded one keeps the ring. The
// square and the row were watched on the live game for three minutes of play
// on the desert map (2026-11-05): never written by anything, and no palette
// cycle there (the map's header entries hold one texture cycle).

#include "game/faerie_ring.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <set>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/dat_cache.h"
#include "hook/log.h"

namespace {

constexpr unsigned kDesertArea = 152;
constexpr const char* kRingFile = "AREA151.DAT";
constexpr std::uint32_t kBlockTag = 0xC8000, kPaletteTag = 0x8600, kPageTag = 0x0A081000;
constexpr int kTolerance = 12;
// The palette row for the ring's colours, at the end of the row: 14, VRAM row
// 497 (Gfx_ClutStrip row 17). Not a row of the map's own twelve - their
// spare rows feed the compass and the place plates (the first build wrote
// row 11 and broke both, the owner's capture 2026-11-05) - but a sprite row
// whose entries 160..255 stayed zero through live play on the desert map;
// no code this project names writes that row. RunFree checks it each load.
constexpr unsigned kRingRow = 14;
constexpr std::size_t kRunMax = 96;   // entries 160..255

// The page chunk (kind 1, tag 0x0A081000): VRAM x 320..831, y 256..511, 16
// tiles of 32 x 32 halfwords a row, 8 rows, each tile 2,048 bytes in turn. The byte of an
// 8-bit texel at page byte column X (page n at 256 n), row Y.
std::size_t PageByte(unsigned X, unsigned Y) {
    return static_cast<std::size_t>(((Y / 32) * 16 + X / 64) * 2048 + (Y % 32) * 64 + X % 64);
}

// A texture word in the 16 x 16 grid form, 8-bit: (word & 0xF00) == 0 and bit 31 clear.
bool GridWord(std::uint32_t w) { return w != 0 && (w & 0xF00) == 0 && !(w & 0x80000000u); }
unsigned PageOf(std::uint32_t w) { return (w >> 28) & 3; }
unsigned PaletteOf(std::uint32_t w) { return (w >> 24) & 0xF; }
unsigned CellOf(std::uint32_t w) { return w & 0xFF; }

std::uint16_t U16(const std::uint8_t* p) { std::uint16_t v; std::memcpy(&v, p, 2); return v; }
std::uint32_t U32(const std::uint8_t* p) { std::uint32_t v; std::memcpy(&v, p, 4); return v; }

// The map band's arithmetic (MapView_CellTextures, map_layers.cpp): the tile
// word of cell (x, z) and the texture dwords from tile t on.
struct Band {
    const std::uint8_t* b = nullptr;
    std::size_t size = 0;
    unsigned w = 0, h = 0, offset = 0, bytes = 0;
    bool Init(const std::uint8_t* block, std::size_t n) {
        if (n < 0x30) return false;
        b = block; size = n; w = b[0]; h = b[1]; offset = U16(b + 2); bytes = U16(b + 0x14) * 4u;
        return w != 0 && h != 0 && bytes + w * h <= n;
    }
    unsigned Byte(unsigned x, unsigned z) const { return b[bytes + x + w * z]; }
    unsigned Tile(unsigned x, unsigned z) const {
        const std::size_t at = (x + w * z + offset * 2u) * 2u;
        return at + 2 <= size ? U16(b + at) : 0;
    }
    std::uint32_t Texture(unsigned t) const {
        const std::size_t at = ((w * h + 1) / 2 + offset + t) * 4u;
        return at + 4 <= size ? U32(b + at) : 0;
    }
};

struct RingFile {
    std::vector<std::uint8_t> block, palettes, page;
};

void TakeChunk(void* ctx, const dat_cache::Chunk& c, std::uint8_t* payload) {
    auto* f = static_cast<RingFile*>(ctx);
    if (c.size <= 0) return;
    const auto n = static_cast<std::size_t>(c.size);
    if (c.kind == 0 && c.tag == kBlockTag) f->block.assign(payload, payload + n);
    if (c.kind == 0 && c.tag == kPaletteTag) f->palettes.assign(payload, payload + n);
    if (c.kind == 1 && c.tag == kPageTag) f->page.assign(payload, payload + n);
}

// DAT\AREA151.DAT as LoadDatFile finds it: the cache's when it holds it
// whole (DIV-0089), else the install's file. Its own chunks only: the
// optional layers are not walked (the ring is the shipped one).
bool ReadRingFile(RingFile* f, const char** why) {
    if (dat_cache::WalkShipped(kRingFile, TakeChunk, f)) return true;
    const HANDLE h = CreateFileA("DAT\\AREA151.DAT", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { *why = "DAT\\AREA151.DAT cannot be opened"; return false; }
    const DWORD size = GetFileSize(h, nullptr);
    std::vector<std::uint8_t> data(size);
    DWORD got = 0;
    const BOOL ok = size != INVALID_FILE_SIZE && ReadFile(h, data.data(), size, &got, nullptr) && got == size;
    CloseHandle(h);
    if (!ok) { *why = "DAT\\AREA151.DAT cannot be read"; return false; }
    for (std::size_t at = 0; at + sizeof(dat_cache::Chunk) <= data.size();) {
        dat_cache::Chunk c;
        std::memcpy(&c, data.data() + at, sizeof c);
        if (c.size < 0 || at + sizeof c + static_cast<std::size_t>(c.size) > data.size()) {
            *why = "DAT\\AREA151.DAT: a chunk runs past the file";
            return false;
        }
        TakeChunk(f, c, data.data() + at + sizeof c);
        at += sizeof c + static_cast<std::size_t>(c.size);
    }
    return true;
}

int Distance2(std::uint16_t a, std::uint16_t b) {
    const int dr = static_cast<int>((a & 31) << 3) - static_cast<int>((b & 31) << 3);
    const int dg = static_cast<int>(((a >> 5) & 31) << 3) - static_cast<int>(((b >> 5) & 31) << 3);
    const int db = static_cast<int>(((a >> 10) & 31) << 3) - static_cast<int>(((b >> 10) & 31) << 3);
    return dr * dr + dg * dg + db * db;
}

// The composed ring: once a session (the files do not change under a game).
struct Ring {
    bool tried = false, ready = false;
    std::uint8_t texels[256] = {};      // 16 rows of 16 bytes
    std::uint16_t colours[256] = {};
    unsigned count = 0, start = 0;      // the colours, at entries start .. 255 of kRingRow
    unsigned cell = 0;                  // page-0 grid cell
    bool laid = false;                  // written on this load of the map
    std::uint32_t word = 0;             // the cell's texture word
    unsigned x = 0, z = 0;
};
Ring g_ring;

// Lost Shore's ring with its dirt swapped for this map's sand (the composite
// into g_ring.texels / palette), or false with the reason.
bool Compose(const Band& desert, unsigned x, unsigned z, std::uint32_t* sand_word, const char** why) {
    RingFile f;
    if (!ReadRingFile(&f, why)) return false;
    Band lost;
    if (f.block.empty() || !lost.Init(f.block.data(), f.block.size())) { *why = "AREA151.DAT: no map band"; return false; }
    // The page: 512 x 256 halfwords, 128 tiles of 2,048 bytes (PageByte's reach).
    if (f.palettes.size() < 12 * 512u || f.page.size() < 128u * 2048u) {
        *why = f.palettes.size() < 12 * 512u ? "AREA151.DAT: no palettes" : "AREA151.DAT: no page";
        return false;
    }
    // Lost Shore has two 0xAE cells, (0x36, 0x1A) and (0x26, 0x36), on one
    // tile (measured 2026-11-05). Every one must name the same texture; the
    // first in row order gives the ground (its west neighbour).
    unsigned rx = 0, rz = 0, found = 0;
    std::uint32_t ring = 0;
    for (unsigned cz = 0; cz < lost.h; ++cz)
        for (unsigned cx = 0; cx < lost.w; ++cx) {
            if (lost.Byte(cx, cz) != 0xAE) continue;
            const std::uint32_t w = lost.Texture(lost.Tile(cx, cz));
            if (found++ == 0) { rx = cx; rz = cz; ring = w; }
            else if (w != ring) { *why = "AREA151.DAT: its 0xAE cells name different textures"; return false; }
        }
    if (found == 0) { *why = "AREA151.DAT: no 0xAE cell"; return false; }
    if (rx == 0) { *why = "AREA151.DAT: the ring has no west neighbour"; return false; }
    if (!GridWord(ring) || PaletteOf(ring) >= 12) { *why = "AREA151.DAT: the ring's texture is not an 8-bit grid cell"; return false; }
    const std::uint8_t* const ring_pal = f.palettes.data() + PaletteOf(ring) * 512u;
    auto ring_texel = [&](const std::vector<std::uint8_t>& page, std::uint32_t w, unsigned px, unsigned py) {
        return page[PageByte(PageOf(w) * 256 + (CellOf(w) & 0xF) * 16 + px, (CellOf(w) & 0xF0) + py)];
    };
    // The dirt: every colour of the west neighbour's tile's three textures in the ring's palette.
    std::vector<std::uint16_t> dirt;
    {
        const unsigned t = lost.Tile(rx - 1, rz);
        std::set<std::uint16_t> seen;
        for (unsigned k = 0; t != 0 && k < 3; ++k) {
            const std::uint32_t w = lost.Texture(t + k);
            if (!GridWord(w) || PaletteOf(w) != PaletteOf(ring)) continue;
            for (unsigned py = 0; py < 16; ++py)
                for (unsigned px = 0; px < 16; ++px) seen.insert(U16(ring_pal + ring_texel(f.page, w, px, py) * 2u));
        }
        dirt.assign(seen.begin(), seen.end());
    }
    if (dirt.empty()) { *why = "AREA151.DAT: the ring's west neighbour has no texture in its palette"; return false; }
    // The sand: the texture this map's tile at (x, z) names, from the loaded page and palettes.
    const std::uint32_t sand = desert.Texture(desert.Tile(x, z));
    if (!GridWord(sand) || PaletteOf(sand) >= 12) { *why = "the desert cell's texture is not an 8-bit grid cell"; return false; }
    *sand_word = sand;
    const auto* const shadow = reinterpret_cast<const std::uint8_t*>(Gfx_VramShadow);
    const std::uint16_t* const sand_pal = Gfx_ClutStripSource + (PaletteOf(sand) + 3) * 256u;
    auto sand_colour = [&](unsigned px, unsigned py) {
        const unsigned X = (320 + 128 * PageOf(sand)) * 2 + (CellOf(sand) & 0xF) * 16 + px;
        const unsigned Y = 256 + (CellOf(sand) & 0xF0) + py;
        return sand_pal[shadow[Y * 2048u + X]];
    };
    std::vector<std::uint16_t> colours;
    unsigned kept = 0;
    for (unsigned py = 0; py < 16; ++py)
        for (unsigned px = 0; px < 16; ++px) {
            std::uint16_t c = U16(ring_pal + ring_texel(f.page, ring, px, py) * 2u);
            bool ground = c == 0;
            for (const auto d : dirt)
                if (!ground && Distance2(c, d) <= kTolerance * kTolerance) ground = true;
            if (ground) c = sand_colour(px, py); else ++kept;
            unsigned i = 0;
            while (i < colours.size() && colours[i] != c) ++i;
            if (i == colours.size()) {
                if (colours.size() == kRunMax) { *why = "the ring needs more colours than the row's free run"; return false; }
                colours.push_back(c);
            }
            g_ring.texels[py * 16 + px] = static_cast<std::uint8_t>(i);
        }
    // The colours at the end of the row: entries 256 - n .. 255.
    g_ring.count = static_cast<unsigned>(colours.size());
    g_ring.start = 256 - g_ring.count;
    for (std::size_t i = 0; i < colours.size(); ++i) g_ring.colours[i] = colours[i];
    for (auto& t : g_ring.texels) t = static_cast<std::uint8_t>(t + g_ring.start);
    bof3::Log("DIV-0204    the ring composed from %s's (0x%x, 0x%x): %u ring texels, %u colours", kRingFile, rx, rz,
              kept, g_ring.count);
    return true;
}

// The first page-0 grid cell no texture word of the map names and the loaded
// page leaves zero.
bool PickRoom(const Band& desert, const char** why) {
    unsigned top = 0;
    for (unsigned cz = 0; cz < desert.h; ++cz)
        for (unsigned cx = 0; cx < desert.w; ++cx) top = desert.Tile(cx, cz) > top ? desert.Tile(cx, cz) : top;
    std::set<unsigned> named_cells;
    for (unsigned t = 1; t <= top + 2; ++t) {
        const std::uint32_t w = desert.Texture(t);
        if (w == 0) continue;
        if (GridWord(w) && PageOf(w) == 0) named_cells.insert(CellOf(w));
    }
    const auto* const shadow = reinterpret_cast<const std::uint8_t*>(Gfx_VramShadow);
    bool cell_found = false;
    for (unsigned c = 1; c < 256 && !cell_found; ++c) {
        if (named_cells.count(c)) continue;
        bool zero = true;
        for (unsigned py = 0; py < 16 && zero; ++py)
            for (unsigned px = 0; px < 16 && zero; ++px)
                zero = shadow[(256 + (c & 0xF0) + py) * 2048u + 640 + (c & 0xF) * 16 + px] == 0;
        if (zero) { g_ring.cell = c; cell_found = true; }
    }
    if (!cell_found) { *why = "no free grid cell on page 0"; return false; }
    return true;
}

// The row's run, in both strips: each entry zero or already the ring's.
bool RunFree() {
    for (const std::uint16_t* strip : {Gfx_ClutStripSource, Gfx_ClutStrip})
        for (unsigned i = 0; i < g_ring.count; ++i) {
            const std::uint16_t e = strip[(kRingRow + 3) * 256u + g_ring.start + i];
            if (e != 0 && e != g_ring.colours[i]) return false;
        }
    return true;
}

void Write() {
    const std::int16_t rect[4] = {static_cast<std::int16_t>(320 + (g_ring.cell & 0xF) * 8),
                                  static_cast<std::int16_t>(256 + (g_ring.cell & 0xF0)), 8, 16};
    Gfx_LoadImage(rect, g_ring.texels);
    const std::size_t at = (kRingRow + 3) * 256u + g_ring.start, bytes = g_ring.count * sizeof(std::uint16_t);
    std::memcpy(Gfx_ClutStripSource + at, g_ring.colours, bytes);
    std::memcpy(Gfx_ClutStrip + at, g_ring.colours, bytes);
    Gfx_ClutStripDirty = 1;
}

}  // namespace

void FaerieRing_Lay(unsigned x, unsigned z) {
    if (Game_AreaNumber != kDesertArea) return;
    Band desert;
    // The band as loaded at AreaMap_Header (kind 0, tag 0xC8000); 64 KiB
    // bounds every read (the world maps' bands are under 0xC500 bytes).
    if (!desert.Init(AreaMap_Header, 0x10000)) {
        bof3::Log("DIV-0204    the ring: the desert map's band is not loaded; not drawn");
        return;
    }
    if (!g_ring.tried) {
        g_ring.tried = true;
        const char* why = "";
        std::uint32_t sand = 0;
        if (!Compose(desert, x, z, &sand, &why) || !PickRoom(desert, &why)) {
            bof3::Log("DIV-0204    the ring is not drawn: %s (the entrance works without it)", why);
            return;
        }
        g_ring.word = (sand & ~0x3F0000FFu) | (kRingRow << 24) | g_ring.cell;
        g_ring.x = x;
        g_ring.z = z;
        g_ring.ready = true;
    }
    g_ring.laid = false;
    if (!g_ring.ready) return;
    if (!RunFree()) {
        bof3::Log("DIV-0204    the ring is not drawn on this load: palette %u's entries %u..255 are in use", kRingRow,
                  g_ring.start);
        return;
    }
    Write();
    g_ring.laid = true;
    bof3::Log("DIV-0204    the ring laid: page-0 cell 0x%02x, palette %u entries %u..255, texture word %08X",
              g_ring.cell, kRingRow, g_ring.start, g_ring.word);
}

void FaerieRing_Leave() {
    if (!g_ring.laid) return;
    g_ring.laid = false;
    unsigned cleared = 0;
    for (std::uint16_t* strip : {Gfx_ClutStripSource, Gfx_ClutStrip})
        for (unsigned i = 0; i < g_ring.count; ++i) {
            std::uint16_t& e = strip[(kRingRow + 3) * 256u + g_ring.start + i];
            if (e != 0 && e == g_ring.colours[i]) { e = 0; ++cleared; }
        }
    Gfx_ClutStripDirty = 1;
    bof3::Log("DIV-0204    the ring's palette entries cleared on leaving the desert map (%u of %u)", cleared,
              2 * g_ring.count);
}

bool FaerieRing_CellWord(unsigned x, unsigned z, std::uint32_t* word) {
    if (!g_ring.laid || x != g_ring.x || z != g_ring.z || Game_AreaNumber != kDesertArea) return false;
    *word = g_ring.word;
    return true;
}
