// Widescreen (DIV-0041, docs/widescreen.md). See widescreen.h.
//
// The backend does the picture: the render target is (320 + 2 x 53) k wide,
// the scene shader adds 53k to every x (render_d3d11.cpp), so the game keeps
// drawing in its own 0..320 space with the projection centre at (160, 120)
// and anything it already draws past its edges - 3D geometry, scrolling
// layers - lands in the side bands. What this file holds is the game side:
// the culls that were tuned to a 320-wide view and now pop at the new edges.
//
// The survey build (widescreen.md section 3e) widens the two the PSP widened:
//   - the terrain cull in MapView_Build (ours): [-50, 370] to [-150, 470] -
//     the PSP went from [-50, 370] to [-96, 416] for 32 columns, 14 beyond
//     the columns it added; 53 + 14 = 67 here popped at the corners of a
//     rotating map, so kTerrainMargin is 100;
//   - the area-map frame pass AreaMap_FrameAreaBD 0x510780 (Capcom's): the
//     wide range [-200, 520] to [-252, 572] and the narrow [-50, 370] to
//     [-102, 422] - the PSP widened both by 31 for 32 columns; 52 for 53.
// The function reads them through `fcomp dword ptr [0x5C42xx]`: the .rdata
// floats are shared with MapView_Build's own compares (0x5C4230 / 0x5C4234
// with 0x56EE14 / 0x56EDFA, both ours now), so the floats stay and the four
// operand addresses are re-aimed at copies in this dll, as sprt_draw.cpp does
// with its far-edge table. Disassembly 2026-09-23 (widescreen.md section 3b).
#include "game/widescreen.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "hook/detour.h"
#include "hook/log.h"

using U = std::uint32_t;

float Widescreen_TerrainLo = -50.0f, Widescreen_TerrainHi = 370.0f;
unsigned g_live;

unsigned Widescreen_Live() { return g_live; }

namespace {

constexpr U kColumns = 53;   // 426 = 240 x 16 / 9 rounded down to even, less 320, halved
// How far past the original's [-50, 370] the terrain cull keeps cells. The
// PSP's 14 beyond its columns (53 + 14 = 67) still let the map's far corners
// pop about 20 px in at the top left and right while the attract sequence
// rotates the map (the owner, 2026-09-23); 100 is the next try.
constexpr U kTerrainMargin = 100;

float g_wide_lo = -200.0f - (kColumns - 1), g_wide_hi = 520.0f + (kColumns - 1);
float g_narrow_lo = -50.0f - (kColumns - 1), g_narrow_hi = 370.0f + (kColumns - 1);

struct Site {
    U at;          // the 4-byte operand of an fcomp dword ptr [mem] in 0x510780
    U rdata;       // the .rdata float it names in the original
    float* ours;   // the copy it names when the view is wide
};
const Site kSites[] = {
    {0x51097E, 0x5C4240, &g_wide_lo},     // fcomp [-200.0], wide x range, low
    {0x510991, 0x5C423C, &g_wide_hi},     // fcomp [520.0], wide x range, high
    {0x5109BB, 0x5C4234, &g_narrow_lo},   // fcomp [-50.0], narrow x range, low
    {0x5109D2, 0x5C4230, &g_narrow_hi},   // fcomp [370.0], narrow x range, high
};

// The menu boxes that slide off the screen's edge - the field menu's time
// and money boxes when a sub-menu opens, the shop's panels, and their way
// back in - are window-task states in 0x596000..0x59D000, each stepping
// the window's x by 0x20 a frame until a bound that lies just past the 320
// view's edge (then holding, or freeing the window through 0x59E310). In
// the wide view those bounds left the boxes hanging in the bands (the
// owner's screenshots, 2026-09-23); each moves outward by the columns
// added, so the box clears the new edge as it cleared the old. The bound is
// an immediate: `mov ecx, imm32` (9 sites) or `cmp cx / ax, imm16` (5).
// Scan and disassembly 2026-09-23 (widescreen.md section 5).
struct Slide {
    U at;        // the immediate's first byte
    U size;      // 2 or 4
    int bound;   // the original's; negative slides off the left, positive off the right
};
const Slide kSlides[] = {
    {0X596926, 4, -170},
    {0X599CC6, 4, -300},
    {0X599DF6, 4, -100},
    {0X59A136, 4, -180},
    {0X59A2B6, 4, -110},
    {0X59A586, 4, -200},
    {0X59B446, 4, -150},
    {0X59C136, 4, -120},
    {0X59A5E6, 4, 320},
    {0X598A08, 2, 322},
    {0X598A1C, 2, -190},
    {0X599061, 2, -165},
    {0X599451, 2, 347},
    {0X599551, 2, 323},
};

}  // namespace

unsigned Widescreen_Columns() {
    static int columns = -1;
    if (columns >= 0) return static_cast<unsigned>(columns);
    char text[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_WIDE", text, sizeof text);
    if (n == 0 || std::strcmp(text, "0") == 0) columns = 0;
    else if (n < sizeof text && std::strcmp(text, "1") == 0) columns = kColumns;
    else bof3::Fatal("BOF3X_WIDE=%s: 1 or 0", n < sizeof text ? text : "...");
    return static_cast<unsigned>(columns);
}

void Widescreen_Inject() {
    if (Widescreen_Columns() == 0) return;
    for (const Site& s : kSites) {
        std::uint8_t expected[4], replacement[4];
        const U ours = static_cast<U>(reinterpret_cast<std::uintptr_t>(s.ours));
        std::memcpy(expected, &s.rdata, 4);
        std::memcpy(replacement, &ours, 4);
        bof3::PatchBytes("Widescreen", s.at, expected, replacement, 4);
    }
    for (const Slide& s : kSlides) {
        std::uint8_t expected[4], replacement[4];
        const int wide = s.bound < 0 ? s.bound - static_cast<int>(kColumns) : s.bound + static_cast<int>(kColumns);
        if (s.size == 4) {
            std::memcpy(expected, &s.bound, 4);
            std::memcpy(replacement, &wide, 4);
        } else {
            const std::int16_t e = static_cast<std::int16_t>(s.bound), w = static_cast<std::int16_t>(wide);
            std::memcpy(expected, &e, 2);
            std::memcpy(replacement, &w, 2);
        }
        bof3::PatchBytes("Widescreen", s.at, expected, replacement, s.size);
    }
    g_live = kColumns;
    Widescreen_TerrainLo = -50.0f - kTerrainMargin;
    Widescreen_TerrainHi = 370.0f + kTerrainMargin;
    bof3::Log("DIV-0041    widescreen: %u columns a side; terrain cull [%.0f, %.0f]; area-map frame ranges [%.0f, %.0f] "
              "and [%.0f, %.0f] (BOF3X_WIDE)",
              kColumns, Widescreen_TerrainLo, Widescreen_TerrainHi, g_wide_lo, g_wide_hi, g_narrow_lo, g_narrow_hi);
}
