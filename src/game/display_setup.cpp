// The display set-up, replaced: Direct3D 11 behind DirectX 6's objects
// (docs/render-backend.md section 4, docs/display-setup.md for what the
// original establishes). One function of Capcom's is taken over:
//
//   Display_Setup   0x5A5160..0x5A5BB4 (0xA55 bytes)   int (HWND, int *fullscreen, int *device, int *mode)
//
// The original enumerates DirectDraw drivers and modes, creates an
// IDirectDraw4, an IDirect3D3 and a HAL IDirect3DDevice3 on a 640 x 480 x 16
// mode or a 640 x 480 window, the primary and back surfaces, a 320 x 256
// staging surface, a viewport and a material, and sets ten texture-stage and
// seven render states. Every global it writes is listed in
// display-setup.md section 3; this function writes the same ones, with the
// backend's objects (src/render/render_shim.cpp) in the seven object slots.
//
// DIVERGENCE DIV-0031 (docs/DIVERGENCE.md): no exclusive display mode is ever
// set and the "Software Render" device (Cfg_RenderMode 0) no longer exists -
// every device index gets the hardware path. The picture is drawn by
// Direct3D 11 into a target of the logical size times an integer scale and
// presented centred on the window. BOF3X_ORIGINAL=Display_Setup runs
// Capcom's set-up and therefore Capcom's DirectDraw, untouched.
#include "game/display_setup.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/sprt_draw.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "render/render_d3d11.h"
#include "render/render_shim.h"

namespace {

using U = std::uint32_t;

// kScreenRight: now Gfx_ScreenRect[2] (symbols.toml).
// kScreenBottom: now Gfx_ScreenRect[3] (symbols.toml).
// kModeBpp: now Display_ModeBpp (symbols.toml).
// kScaleX: now D3d_ScaleX (symbols.toml).
// kScaleY: now D3d_ScaleY (symbols.toml).
constexpr U kMaxScale = 8;   // BOF3X_SCALE's range since the backend (render-backend.md)
// kPrimary: now DDraw_Primary (symbols.toml).
// kBackBuffer: now DDraw_BackBuffer (symbols.toml).
// kStage: now Dd_StageSurface (symbols.toml).
// kDevice: now D3d_Device (symbols.toml).
// kViewport: now D3d_Viewport (symbols.toml).
// kMaterial: now D3d_BackMaterial (symbols.toml).
// kMaterialHandle: now D3d_BackMaterialHandle (symbols.toml).
// kBackPitch: now Display_BackPitch (symbols.toml).

}  // namespace
namespace display_setup { bool g_backend_up; }

namespace display_setup { U g_scale; }   // the target's k (DIV-0036, DIV-0042)
namespace {
// The largest k in 1..kMaxScale whose (view width) k x 240k fits cw x ch; 1 if none.
U FitScale(U view_w, U cw, U ch) {
    U k = kMaxScale;
    while (k > 1 && (view_w * k > cw || 240 * k > ch)) --k;
    return k;
}

}  // namespace

// The default k follows the display's scale (the owner, 2026-10-11, with
// DPI awareness declared in bof3x.exe's manifest - DIV-0036 amended): twice
// the scale, rounded down, never below 2 - so 100 % and 125 % open at k = 2,
// 150 % and 175 % at k = 3, 200 % at k = 4, and the window is the same
// apparent size on every desktop. An unaware process (the hosted build,
// BOF3.exe has no manifest) is told 96 dpi and keeps k = 2.
unsigned DefaultWindowedScale() {
    unsigned dpi = 96;
    if (HMODULE user32 = GetModuleHandleA("user32.dll")) {
        using Fn = UINT(WINAPI*)();
        if (auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(user32, "GetDpiForSystem"))))
            dpi = fn();   // Windows 10 1607 and later; absent, the system dc below
    }
    if (dpi == 96) {
        if (HDC dc = GetDC(nullptr)) {
            dpi = static_cast<unsigned>(GetDeviceCaps(dc, LOGPIXELSX));
            ReleaseDC(nullptr, dc);
        }
    }
    unsigned k = (2 * dpi) / 96;
    if (k < 2) k = 2;
    if (k > kMaxScale) k = kMaxScale;
    if (k != 2) bof3::Log("DIV-0036    windowed default k = %u from %u dpi (%u %%)", k, dpi, dpi * 100 / 96);
    return k;
}

unsigned DisplaySetup_WindowedScale() {
    char text[32];
    if (GetEnvironmentVariableA("BOF3X_SCALE", text, sizeof text) == 0) return DefaultWindowedScale();
    U k = 0;
    for (const char* p = text; *p >= '0' && *p <= '9'; ++p) k = k * 10 + static_cast<U>(*p - '0');
    if (k < 1 || k > kMaxScale) bof3::Fatal("BOF3X_SCALE=%s: an integer 1..%u", text, kMaxScale);
    return k;
}

unsigned DisplaySetup_TargetScale() { return display_setup::g_backend_up ? display_setup::g_scale : 0; }

unsigned DisplaySetup_ViewWidth() { return 320 + 2 * Widescreen_Columns(); }

bool DisplaySetup_Snap() {
    static int snap = -1;
    if (snap < 0) {
        char text[8];
        snap = !(GetEnvironmentVariableA("BOF3X_SNAP", text, sizeof text) > 0 && text[0] == '0');
    }
    return snap != 0;
}

unsigned DisplaySetup_ScaleForClient(unsigned cw, unsigned ch) {
    const U view_w = DisplaySetup_ViewWidth();
    if (DisplaySetup_Snap()) return FitScale(view_w, cw, ch);
    U k = (ch + 239) / 240;   // the smallest k whose 240k covers the height: shrunk to fit, never enlarged
    if (k < 1) k = 1;
    if (k > kMaxScale) k = kMaxScale;
    return k;
}

namespace {
// The present's k rule at each picture width (DIV-0036 / DIV-0042, the 21:9
// picture of docs/ultrawide-frame.md): FitScale takes the view's width, so the
// three widths share it. Checked at start-up on the monitors the docs name:
// the owner's 3440 x 1440 lands on k = 6 at every width (1920 / 2556 / 3360
// wide, the full 1440 rows), a 2560 x 1080 21:9 panel on 4 (2240 x 960 at
// 560), a 1920 x 1080 16:9 one on 4 (1704 x 960 at 426; 560 x 3 = 1680 at
// 21:9). Fatal on a difference: the rule changed under the widths.
void CheckFitScale() {
    struct Case {
        U view_w, cw, ch, k;
    };
    static const Case kCases[] = {
        {320, 3440, 1440, 6}, {426, 3440, 1440, 6}, {560, 3440, 1440, 6},
        {560, 2560, 1080, 4}, {426, 1920, 1080, 4}, {560, 1920, 1080, 3},
        {560, 559, 239, 1},   // a client smaller than one picture: 1, never 0
    };
    for (const Case& c : kCases)
        if (FitScale(c.view_w, c.cw, c.ch) != c.k)
            bof3::Fatal("display_setup: FitScale(%u, %u, %u) is %u, not %u", c.view_w, c.cw, c.ch,
                        FitScale(c.view_w, c.cw, c.ch), c.k);
}
}  // namespace

void DisplaySetup_Inject() {
    CheckFitScale();
}
