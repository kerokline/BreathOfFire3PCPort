// The narrow areas pillarboxed (DIV-0041, side group PB of the wide-ring
// round; docs/narrow-pillarbox.md). See pillarbox.h.
#include "game/pillarbox.h"

#include <windows.h>

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "render/render_shim.h"

namespace pillarbox {
namespace {

using U = std::uint32_t;

// GameMode_Start 0x495840 sets Game_AreaNumber to 0xFFFF so that the area
// always counts as changed (docs/mode-flow.md section 3): no area yet.
constexpr unsigned kNoArea = 0xFFFF;
// Game_Mode 5 runs Battle_Frame - the battle (docs/mode-rest.md section 1.1):
// its picture is the arena's, not the area's.
constexpr unsigned kBattleMode = 5;
// Task_Records 0x66C7D0: four 0x20-byte records, +0 the u16 state, 0 free
// (symbols.toml). Record 0 is the field task's slot - and the title menu's
// (TitleTask_Run 0x588E70), which leaves Game_AreaNumber as the demo left it.
constexpr U kTaskSlot0State = bof3::addr::Task_Records;

bool g_on;            // Inject ran and BOF3X_PILLARBOX is not 0
// Game_AreaNumber is believed only once it has changed since task slot 0 was
// last seen free: the title menu runs in that slot after the demo with the
// demo's last area still in the word (section 3 of the doc).
bool g_trusted;
unsigned g_epoch_area;

// The widest row of the area last measured, keyed by the area and the map
// block's header as it was then.
struct Measured {
    unsigned area = kNoArea;
    std::uint8_t w = 0, h = 0;
    std::uint16_t off = 0;
    unsigned widest = 0;
};
Measured g_measured;

unsigned g_logged_visible = ~0u;   // the visible columns last logged, for the change lines

std::uint16_t U16(const unsigned char* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

// How much of the map block may be read: to the end of the image, which holds
// LoadDatFile's arena (AreaMap_Header is that arena + 0xC8000).
U HeaderBytes() { return bof3::image::kBase + bof3::image::kSizeOfImage - static_cast<U>(AreaMap_Header_addr); }

unsigned WidestNow(unsigned area) {
    const unsigned char* h = AreaMap_Header;
    if (g_measured.area != area || g_measured.w != h[0] || g_measured.h != h[1] || g_measured.off != U16(h + 2)) {
        g_measured.area = area;
        g_measured.w = h[0];
        g_measured.h = h[1];
        g_measured.off = U16(h + 2);
        g_measured.widest = WidestRow(h, HeaderBytes());
        const unsigned c = Widescreen_Live();
        bof3::Log("pillarbox   area %u: map %u x %u, widest row %u ring columns (%u px): framed at %u columns a side of %u",
                  area, g_measured.w, g_measured.h, g_measured.widest, g_measured.widest * kRingColumnPx,
                  Framed(g_measured.widest, c), c);
    }
    return g_measured.widest;
}

void UpdateTrust() {
    const unsigned area = Game_AreaNumber;
    if (*reinterpret_cast<const volatile std::uint16_t*>(static_cast<std::uintptr_t>(kTaskSlot0State)) == 0) {
        g_trusted = false;
        g_epoch_area = area;
    } else if (!g_trusted && area != g_epoch_area) {
        g_trusted = true;
    }
}

// --- the self-test (BOF3X_SHADOW=pillarbox) ---------------------------------

unsigned g_bad;
void Check(bool ok, const char* what, unsigned a, unsigned b) {
    if (ok) return;
    if (++g_bad <= 20) bof3::Log("shadow      pillarbox self-test MISMATCH: %s (%u, expected %u)", what, a, b);
}

// A map block of w x h with the offset `off`, tiles set by `set`.
template <class F> unsigned Block(unsigned w, unsigned h, unsigned off, F set, unsigned char* buf, U size, U limit) {
    for (U i = 0; i < size; ++i) buf[i] = 0;
    buf[0] = static_cast<unsigned char>(w);
    buf[1] = static_cast<unsigned char>(h);
    buf[2] = static_cast<unsigned char>(off);
    buf[3] = static_cast<unsigned char>(off >> 8);
    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x)
            if (set(x, y)) {
                const U i = (x + w * y + off * 2) * 2;
                if (i + 2 > size) bof3::Fatal("pillarbox self-test: a test block past its buffer");
                buf[i] = 0x34;
                buf[i + 1] = 0x12;
            }
    return WidestRow(buf, limit ? limit : size);
}

void SelfTest() {
    static unsigned char buf[2 * (4 + 2 * 3 + 255 * 255) + 64];
    const U size = sizeof buf;
    g_bad = 0;
    unsigned rounds = 0;
    // The widest row.
    auto expect = [&](unsigned got, unsigned want, const char* what) {
        Check(got == want, what, got, want);
        ++rounds;
    };
    expect(Block(10, 10, 0, [](unsigned, unsigned) { return false; }, buf, size, 0), 0, "an empty block");
    expect(Block(10, 10, 0, [](unsigned x, unsigned y) { return x == 4 && y == 5; }, buf, size, 0), 1, "one cell");
    // A corridor ten wide and 241 long (areas 31 and 186's shape): interior x 1..8.
    // (Interior cells only: with offset 0 the words of cells (0, 0) and (1, 0) are the header's own bytes.)
    expect(Block(10, 241, 0, [](unsigned x, unsigned y) { return x >= 1 && x <= 8 && y >= 1 && y <= 239; }, buf, size, 0), 8,
           "a corridor 10 wide");
    // One diagonal x + y = 20, x 3..17: fifteen columns, with a gap inside it still fifteen.
    expect(Block(80, 55, 0, [](unsigned x, unsigned y) { return x + y == 20 && x >= 3 && x <= 17; }, buf, size, 0), 15,
           "one diagonal of 15");
    expect(Block(80, 55, 0, [](unsigned x, unsigned y) { return x + y == 20 && x >= 3 && x <= 17 && x != 9; }, buf, size, 0),
           15, "one diagonal of 15 with a gap");
    // A line along map x is one cell on every diagonal.
    expect(Block(80, 55, 0, [](unsigned x, unsigned y) { return y == 7 && x >= 1 && x <= 60; }, buf, size, 0), 1,
           "a line along x");
    // The edge cells are never in the ring (MapView_CellToMap's 1..w-2 x 1..h-2).
    expect(Block(30, 30, 3, [](unsigned x, unsigned y) { return x == 0 || y == 0 || x == 29 || y == 29; }, buf, size, 0), 0,
           "only edge cells");
    // The offset moves the tile words: a block written with offset 7 read as such.
    expect(Block(40, 40, 7, [](unsigned x, unsigned y) { return x + y == 30 && x >= 5 && x <= 16; }, buf, size, 0), 12,
           "a diagonal of 12 behind offset 7");
    // The readable bytes end the block: cells past them are empty.
    {
        const unsigned w = 40;
        const U limit = (5 + w * 10) * 2;   // rows 0..9 readable (offset 0): the diagonal from y 9 down, x 16..22
        expect(Block(w, 40, 0, [](unsigned x, unsigned y) { return x + y == 25 && x >= 2 && x <= 22; }, buf, size, limit), 7,
               "a diagonal cut by the readable bytes");
    }
    // The rule, at the three pictures.
    struct Row {
        unsigned widest, at0, at53, at120;
    };
    const Row rule[] = {{0, 0, 0, 0},    {5, 0, 0, 0},     {8, 0, 0, 0},     {10, 0, 0, 0},   {13, 0, 0, 0},
                        {14, 0, 53, 53}, {15, 0, 53, 53},  {16, 0, 53, 53},  {17, 0, 53, 53}, {18, 0, 53, 120},
                        {30, 0, 53, 120}, {255, 0, 53, 120}};
    for (const Row& r : rule) {
        expect(Framed(r.widest, 0), r.at0, "framed, narrow");
        expect(Framed(r.widest, 53), r.at53, "framed at 16:9");
        expect(Framed(r.widest, 120), r.at120, "framed at 21:9");
    }
    // Negative control: "the narrowest picture that holds the area" frames area
    // 53's 15 columns at 560, which the brief's class list refuses.
    unsigned refused = 0;
    {
        auto holds = [](unsigned widest, unsigned c) {
            const unsigned px = widest * kRingColumnPx;
            const unsigned f = px <= 320u ? 0u : px <= 426u ? 53u : 120u;
            return f < c ? f : c;
        };
        for (const Row& r : rule)
            if (holds(r.widest, 120) != r.at120) ++refused;
    }
    if (refused == 0) bof3::Fatal("pillarbox self-test: the control (the narrowest picture that holds) was not refused");
    if (g_bad) bof3::Fatal("pillarbox self-test: %u mismatches in %u rounds", g_bad, rounds);
    bof3::Log("shadow      pillarbox self-test: %u rounds, 0 mismatches; the control refused on %u rows", rounds, refused);
}

}  // namespace

unsigned WidestRow(const unsigned char* header, std::uint32_t bytes) {
    if (bytes < 4) return 0;
    const unsigned w = header[0], h = header[1], off = U16(header + 2);
    if (w < 3 || h < 3) return 0;
    // Per diagonal d = x + y (2 .. w + h - 4), the lowest and highest x with a tile.
    static unsigned short lo[512], hi[512];
    for (unsigned d = 0; d < 512; ++d) {
        lo[d] = 0xFFFF;
        hi[d] = 0;
    }
    for (unsigned y = 1; y + 1 < h; ++y)
        for (unsigned x = 1; x + 1 < w; ++x) {
            const U i = (x + w * y + off * 2u) * 2u;
            if (i + 2 > bytes || U16(header + i) == 0) continue;
            const unsigned d = x + y;
            if (x < lo[d]) lo[d] = static_cast<unsigned short>(x);
            if (x > hi[d]) hi[d] = static_cast<unsigned short>(x);
        }
    unsigned widest = 0;
    for (unsigned d = 0; d < 512; ++d)
        if (lo[d] != 0xFFFF && hi[d] - lo[d] + 1u > widest) widest = hi[d] - lo[d] + 1u;
    return widest;
}

unsigned VisibleColumns() {
    const unsigned c = Widescreen_Live();
    if (!g_on || c == 0 || !Widescreen_FillsArmed() || !g_trusted) return c;
    const unsigned area = Game_AreaNumber;
    if (area == kNoArea || Game_Mode == kBattleMode) return c;
    return Framed(WidestNow(area), c);
}

unsigned Anchor(unsigned fill) {
    if (fill == 0) return 0;
    const unsigned v = VisibleColumns();
    return v < fill ? v : fill;
}

void Cover() {
    if (!g_on) return;
    UpdateTrust();
    const unsigned c = Widescreen_Live();
    if (c == 0 || !Widescreen_FillsArmed()) return;
    const unsigned f = VisibleColumns();
    if (f != g_logged_visible) {
        bof3::Log("pillarbox   frame %u, area %u, mode %u: %u columns a side visible of %u%s", static_cast<unsigned>(Frame_Counter),
                  static_cast<unsigned>(Game_AreaNumber), static_cast<unsigned>(Game_Mode), f, c, f < c ? " (bars on)" : "");
        g_logged_visible = f;
    }
    if (f >= c) return;
    // The draws' own coordinates: game x and y times D3d_ScaleX / Y, x from
    // the 320-wide view's left edge (the scene shader adds the band).
    const float kx = D3d_ScaleX, ky = D3d_ScaleY;
    const float band = static_cast<float>(c), keep = static_cast<float>(f), bottom = 240.0f * ky;
    render::AppendFill(-band * kx, 0.0f, -keep * kx, bottom, 0xFF000000u);
    render::AppendFill((320.0f + keep) * kx, 0.0f, (320.0f + band) * kx, bottom, 0xFF000000u);
}

void Inject() {
    if (bof3::WantsShadow("pillarbox")) SelfTest();
    char text[8];
    const DWORD n = GetEnvironmentVariableA("BOF3X_PILLARBOX", text, sizeof text);
    if (n != 0 && !(n == 1 && (text[0] == '0' || text[0] == '1')))
        bof3::Fatal("BOF3X_PILLARBOX=%s: 0 or 1", n < sizeof text ? text : "...");
    g_on = !(n == 1 && text[0] == '0');
    g_trusted = false;
    g_epoch_area = Game_AreaNumber;
    bof3::Log("pillarbox   DIV-0041 (PB): the narrow areas framed %s; %u columns a side",
              g_on ? "on" : "off (BOF3X_PILLARBOX=0)", Widescreen_Live());
}

}  // namespace pillarbox
