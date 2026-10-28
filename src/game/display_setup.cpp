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

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
[[gnu::always_inline]] inline void PutLong(::bof3::image::Va address, U v) { PutLong(::bof3::Img(address), v); }
void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}
void PutFloat(U address, float v) { std::memcpy(At(address), &v, sizeof v); }
void PutPtr(U address, const void* p) { PutLong(address, static_cast<U>(reinterpret_cast<std::uintptr_t>(p))); }
[[gnu::always_inline]] inline void PutPtr(::bof3::image::Va address, const void* p) { PutPtr(::bof3::Img(address), p); }
void Zero(U address, U bytes) { std::memset(At(address), 0, bytes); }

// --- the globals (display-setup.md section 3) -------------------------------------
// kDesktopH: now PsxRest_TexCacheEnd (symbols.toml).
// kDesktopW: now Display_DesktopWidth (symbols.toml).
// kDesktopBpp: now Display_DesktopBpp (symbols.toml).
// kTexCache: now Gfx_TexCache (symbols.toml).
constexpr U kTexCacheBytes = 0x6000;
// kClutRows: now Gfx_ClutRows (symbols.toml).
constexpr U kClutRowsBytes = 0x1000;
constexpr U kFontCache = 0x7C9F50, kFontCacheBytes = 0xA00, kFontEntry = 0x14;
constexpr U kBackdropBlock = 0x6BE9F8, kBackdropBytes = 0x1C;
// kAfterDrawBlock: now D3d_CaptureReady (symbols.toml).
constexpr U kAfterDrawBytes = 0x1C;
// kCellCache: now D3d_CellTexCache (symbols.toml).
constexpr U kCellCacheBytes = 0x1400;
// kPixelFormat: now Gfx_PixelFormat (symbols.toml).
constexpr U kPixelFormatRecord = 0x40;
// kShadeCache: now D3d_ShadeModeCache (symbols.toml).
constexpr ::bof3::image::Va kRenderFlags{0x6C3A4C};
// kUnread6C3A48: now Display_ZeroedUnread (symbols.toml).
// kTexCoords: now D3d_TexCoords (symbols.toml).
// kDeviceCountLess1: now Display_DeviceCountLess1 (symbols.toml).
constexpr U kDeviceRecords = 0x6C3A50, kDeviceRecord = 0x13C;
constexpr U kModeLists = 0x6BE1E8, kModeListsBytes = 0x810;
// kDirectDraw: now Dd_DirectDraw (symbols.toml).
// kClipper: now PsxRest_Clipper (symbols.toml).
// kZBuffer: now PsxRest_ZBuffer (symbols.toml).
constexpr ::bof3::image::Va kDirect3D{0x7CC34C};
// kScreenRight: now Gfx_ScreenRect[2] (symbols.toml).
// kScreenBottom: now Gfx_ScreenRect[3] (symbols.toml).
// kModeBpp: now Display_ModeBpp (symbols.toml).
// kScaleX: now D3d_ScaleX (symbols.toml).
// kScaleY: now D3d_ScaleY (symbols.toml).
constexpr U kMaxScale = 8;   // BOF3X_SCALE's range since the backend (render-backend.md)
constexpr U kDeviceDesc = 0x7CC238, kDeviceDescBytes = 0xFC;
// kPrimary: now DDraw_Primary (symbols.toml).
// kBackBuffer: now DDraw_BackBuffer (symbols.toml).
// kStage: now Dd_StageSurface (symbols.toml).
// kDevice: now D3d_Device (symbols.toml).
// kViewport: now D3d_Viewport (symbols.toml).
// kMaterial: now D3d_BackMaterial (symbols.toml).
// kMaterialHandle: now D3d_BackMaterialHandle (symbols.toml).
// kBackPitch: now Display_BackPitch (symbols.toml).

// D3DDEVICEDESC fields D3d_FitTextureSize reads (tex-cells.md section 3).
constexpr U kDescTextureCaps = 0x84, kDescMinW = 0xAC, kDescMinH = 0xB0, kDescMaxW = 0xB4, kDescMaxH = 0xB8;

// What the HAL device on this machine reported through the original set-up,
// sampled read-only from a running game on 2026-09-23 (render-backend.md
// section 4): the texture caps and size limits, and the three pixel-format
// records. Reproduced so that the texture builders make the same textures.
struct Format {
    U bpp, rmask, gmask, bmask, amask;
};
// Sampled: 0x7DED60 = record 0 rank 2, 1-5-5-5 with DDPF_ALPHAPIXELS; record 1
// rank 3, 5-5-5 without; record 2 rank 1, X-8-8-8 32-bit; Gfx_RenderFlags
// 0x202 (bit 0x20 clear: an alpha format exists); D3DDEVICEDESC +0x84 0xCCD,
// min 1 x 1, max 0x4000 x 0x4000; D3d_ScaleX/Y 2.0; 0x7CADE0 16; the back
// buffer's pitch 0xA00 (32-bit); desktop 32 bpp.
constexpr Format kFormatAlpha = {16, 0x7C00, 0x03E0, 0x001F, 0x8000};   // record 0: textures with alpha
constexpr U kRankAlpha = 2;
constexpr Format kFormatOpaque = {16, 0x7C00, 0x03E0, 0x001F, 0};       // record 1: textures without
constexpr U kRankOpaque = 3;
constexpr Format kFormatScreen = {32, 0xFF0000, 0x00FF00, 0x0000FF, 0}; // record 2: the screen
constexpr U kRankScreen = 1;
constexpr bool kNoAlphaFormat = false;                                  // Gfx_RenderFlags bit 0x20
constexpr U kTextureCaps = 0xCCD;                                       // D3DPTEXTURECAPS_*
constexpr U kMinTexture = 1, kMaxTexture = 0x4000;
constexpr U kModeBits = 16;

U TopBit(U mask) {
    U top = 0;
    for (U i = 0; i < 32; ++i)
        if (mask & (1u << i)) top = i;
    return top;
}

// A Gfx_PixelFormat record as 0x5A62C0 fills it (display-setup.md section 5.6):
// +0 rank, +1 BGR order, +2 bpp, +3 bytes per pixel, +4/+8/+0xC the shift of
// each mask's top bit down from bit 23, +0x10.. the masks, +0x1C alpha,
// +0x20 the DDPIXELFORMAT.
void FillFormat(U record, const Format& f, U rank) {
    Zero(record, kPixelFormatRecord);
    At(record)[0] = static_cast<unsigned char>(rank);
    At(record)[1] = (f.rmask == 0x1F || f.rmask == 0xFF) ? 1 : 0;
    At(record)[2] = static_cast<unsigned char>(f.bpp);
    At(record)[3] = static_cast<unsigned char>(f.bpp / 8);
    PutLong(record + 0x04, 23 - TopBit(f.rmask));
    PutLong(record + 0x08, 23 - TopBit(f.gmask));
    PutLong(record + 0x0C, 23 - TopBit(f.bmask));
    PutLong(record + 0x10, f.rmask);
    PutLong(record + 0x14, f.gmask);
    PutLong(record + 0x18, f.bmask);
    PutLong(record + 0x1C, f.amask);
    PutLong(record + 0x20, 0x20);                        // dwSize
    PutLong(record + 0x24, 0x40 | (f.amask ? 1 : 0));   // DDPF_RGB | DDPF_ALPHAPIXELS
    PutLong(record + 0x2C, f.bpp);
    PutLong(record + 0x30, f.rmask);
    PutLong(record + 0x34, f.gmask);
    PutLong(record + 0x38, f.bmask);
    PutLong(record + 0x3C, f.amask);
}

bool g_backend_up;

U g_scale;   // the target's k (DIV-0036, DIV-0042)

// The largest k in 1..kMaxScale whose (view width) k x 240k fits cw x ch; 1 if none.
U FitScale(U view_w, U cw, U ch) {
    U k = kMaxScale;
    while (k > 1 && (view_w * k > cw || 240 * k > ch)) --k;
    return k;
}

// DIV-0042: the game's numbers for a target at k, written at set-up and again
// from the render fiber whenever the target is remade (render::RequestScale):
// D3d_ScaleX/Y, DIV-0010's far-edge table, Gfx_ScreenRect's size, and the
// primary and back surfaces' size as GetSurfaceDesc reports it.
void Rescale(U k) {
    g_scale = k;
    const U width = render::TargetWidth(), height = render::TargetHeight();
    Gfx_ScreenRect[2] = width;
    Gfx_ScreenRect[3] = height;
    PutFloat(D3d_ScaleX_addr, static_cast<float>(k));
    PutFloat(D3d_ScaleY_addr, static_cast<float>(k));
    SprtDraw_SetScale(k);
    auto* primary = *reinterpret_cast<render::Surface**>(At(::bof3::Img(DDraw_Primary_addr)));
    auto* back = *reinterpret_cast<render::Surface**>(At(::bof3::Img(DDraw_BackBuffer_addr)));
    if (primary && back) {
        if (!render::ResizeSurface(primary, width, height) || !render::ResizeSurface(back, width, height))
            bof3::Fatal("Display_Setup: out of memory resizing the primary and back surfaces to %u x %u", width, height);
        Display_BackPitch = back->pitch;
    }
}

// DIVERGENCE DIV-0036: the render target is 320k x 240k, k chosen once at
// set-up (display-overhaul.md 4b; the owner's rule, 2026-09-23): a borderless
// window takes the largest k whose target fits its client - the monitor - and
// a window takes the launcher's size, BOF3X_SCALE, 2 when unset. A resize or
// F8 later changes only the present's integer scale of that target.
render::Options ReadOptions(HWND hwnd, bool borderless) {
    render::Options o = {};
    o.hwnd = hwnd;
    char text[32];
    // DIV-0042: the client decides, in both modes - the window opened at the
    // remembered size (win_main.cpp) or the BOF3X_SCALE fallback, the
    // borderless window at its monitor's.
    RECT client = {};
    GetClientRect(hwnd, &client);
    const U k = DisplaySetup_ScaleForClient(static_cast<U>(client.right - client.left), static_cast<U>(client.bottom - client.top));
    bof3::Log("DIV-0036    scale %u for the %s client %ld x %ld%s", k, borderless ? "borderless" : "windowed",
              client.right - client.left, client.bottom - client.top, DisplaySetup_Snap() ? "" : " (DIV-0042: fitted, not snapped)");
    o.logical_w = 320;
    o.logical_h = 240;
    o.scale = k;
    o.pad_x = Widescreen_Columns();   // DIV-0041: the target is (320 + 2 c) k wide (c 53 or 120), the view centred in it
    g_scale = k;
    o.point_filter = false;
    if (GetEnvironmentVariableA("BOF3X_FILTER", text, sizeof text) > 0) {
        if (std::strcmp(text, "point") == 0) o.point_filter = true;
        else if (std::strcmp(text, "linear") != 0) bof3::Fatal("BOF3X_FILTER=%s: point or linear", text);
    }
    o.vsync = GetEnvironmentVariableA("BOF3X_VSYNC", nullptr, 0) > 0;
    o.snap = DisplaySetup_Snap();
    return o;
}

}  // namespace

extern "C" int __cdecl Display_Setup(void* hwnd_, int* fullscreen, int* device, int* /*mode*/) {
    HWND hwnd = static_cast<HWND>(hwnd_);

    // 2.1 the desktop
    HDC dc = GetDC(nullptr);
    Display_DesktopWidth = static_cast<U>(GetDeviceCaps(dc, HORZRES));
    PsxRest_TexCacheEnd = static_cast<U>(GetDeviceCaps(dc, VERTRES));
    Display_DesktopBpp = static_cast<U>(GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES));
    ReleaseDC(nullptr, dc);

    // 2.2 the caches, reset - every one the original zeroes
    Zero(Gfx_TexCache_addr, kTexCacheBytes);
    Zero(Gfx_ClutRows_addr, kClutRowsBytes);
    Zero(Font_TexCache_addr, kFontCacheBytes);
    for (U i = 0; i < kFontCacheBytes / kFontEntry; ++i) PutWord(kFontCache + i * kFontEntry, 0xFFFF);
    Zero(kBackdropBlock, kBackdropBytes);
    Zero(D3d_CaptureReady_addr, kAfterDrawBytes);
    Zero(D3d_CellTexCache_addr, kCellCacheBytes);
    Zero(Gfx_PixelFormat_addr, 3 * kPixelFormatRecord);
    PutLong(kRenderFlags, 0);
    Display_ZeroedUnread = 0;
    D3d_ShadeModeCache = 0;

    // 2.3 the texture coordinates: i / 256 + 0.002 in double, stored as float
    for (U i = 0; i < 256; ++i) PutFloat(D3d_TexCoords_addr + i * 4, static_cast<float>(i * 0.00390625 + 0.0020000000949949026));

    // 2.4 the device records: 0 the original's synthetic software renderer (kept
    // so that F7's name overlay reads it), 1 this backend. No enumeration.
    if (static_cast<std::int32_t>(Display_DeviceCountLess1) < 1) {
        Zero(kDeviceRecords, 2 * kDeviceRecord);
        Zero(kModeLists, kModeListsBytes);
        PutLong(kDeviceRecords + 0, 1);
        PutLong(kDeviceRecords + 4, 1);
        std::memcpy(At(kDeviceRecords + 0x18), "Software Render", 16);
        PutLong(kDeviceRecords + kDeviceRecord + 0, 1);
        PutLong(kDeviceRecords + kDeviceRecord + 4, 1);
        std::memcpy(At(kDeviceRecords + kDeviceRecord + 0x18), "Direct3D 11 (bof3x)", 20);
        Display_DeviceCountLess1 = 1;
    }
    // 2.5 which device: the original wraps an index past the last record to 0
    // and takes record 0 as the software renderer. Here every index is the
    // hardware path (DIV-0031); the wrap is kept for F7's cycling.
    if (*device < 0 || *device > 1) *device = 0;
    PutLong(kRenderFlags, 2);   // bit 1: the device allows windowed

    // 2.6 the mode and the scale: the logical picture is the target's size
    const render::Options options = ReadOptions(hwnd, *fullscreen != 0);
    if (!g_backend_up) {
        render::InitShim(256 * 1024, 32 * 1024, 8 * 1024 * 1024);
        render::InitD3d11(options);
        render::SetRescaleHook(&Rescale);
        g_backend_up = true;
    }
    const U width = render::TargetWidth(), height = render::TargetHeight();
    Gfx_ScreenRect[2] = width;
    Gfx_ScreenRect[3] = height;
    Display_ModeBpp = kModeBits;
    // The game's scale is k, the view's - not the target's width over 320,
    // which a wide target (DIV-0041) would make 1.33k: the game keeps
    // drawing a 320-wide view and the backend shifts it into the target.
    PutFloat(D3d_ScaleX_addr, static_cast<float>(g_scale));
    PutFloat(D3d_ScaleY_addr, static_cast<float>(g_scale));
    SprtDraw_SetScale(g_scale);   // DIV-0010's far edge follows the scale (display-overhaul.md 4b)
    Zero(D3d_DeviceDesc_addr, kDeviceDescBytes);
    PutLong(kDeviceDesc + kDescTextureCaps, kTextureCaps);
    PutLong(kDeviceDesc + kDescMinW, kMinTexture);
    PutLong(kDeviceDesc + kDescMinH, kMinTexture);
    PutLong(kDeviceDesc + kDescMaxW, kMaxTexture);
    PutLong(kDeviceDesc + kDescMaxH, kMaxTexture);

    // 2.7 the windowed path, always: DirectDraw, primary, back buffer, no
    // clipper, Gfx_RenderFlags bit 0x200. The window rect stays 0x5A5130's.
    Dd_DirectDraw = render::DirectDrawObject();
    PutPtr(kDirect3D, render::DirectDrawObject());   // released by the teardown, read by nothing else
    render::Surface* primary = render::MakeSurface(width, height, 32, 0x200, 0, true, false);
    render::Surface* back = render::MakeSurface(width, height, 32, 0x2040, 0, false, true);
    if (!primary || !back) bof3::Fatal("Display_Setup: out of memory for the primary and back surfaces");
    DDraw_Primary = primary;
    DDraw_BackBuffer = back;
    PsxRest_Clipper = 0;
    PsxRest_ZBuffer = 0;
    *reinterpret_cast<U*>(At(::bof3::Img(Gfx_RenderFlags_addr))) |= 0x200;
    (void)fullscreen;   // never forced: no display mode is set (DIV-0031)

    // 2.9 the device
    D3d_Device = render::DeviceObject();

    // 2.10 the texture formats, the staging surface, the back buffer's pitch
    FillFormat(Gfx_PixelFormat_addr + 0 * kPixelFormatRecord, kFormatAlpha, kRankAlpha);
    FillFormat(Gfx_PixelFormat_addr + 1 * kPixelFormatRecord, kFormatOpaque, kRankOpaque);
    FillFormat(Gfx_PixelFormat_addr + 2 * kPixelFormatRecord, kFormatScreen, kRankScreen);
    if (kNoAlphaFormat) *reinterpret_cast<U*>(At(::bof3::Img(Gfx_RenderFlags_addr))) |= 0x20;
    if (!Dd_CreatePlainSurface(0x140, 0x100, reinterpret_cast<void**>(At(::bof3::Img(Dd_StageSurface_addr))), 0))
        bof3::Fatal("Display_Setup: the staging surface");
    Display_BackPitch = back->pitch;

    // 2.11 viewport, material, states - through the backend's objects, so that
    // the device holds the same state the original's did
    D3d_Viewport = render::ViewportObject();
    D3d_BackMaterial = render::MaterialObject();
    D3d_BackMaterialHandle = 1;
    render::SetClearColor(0xFF000000u);
    render::PipeState& s = render::CurrentState();
    s.texture = nullptr;
    s.alpha_modulate = true;                 // ALPHAOP MODULATE
    s.point_filter = options.point_filter;   // MIN/MAGFILTER: LINEAR, or POINT under BOF3X_FILTER (DIV-0012)
    s.alpha_test = true;                     // ALPHATESTENABLE 1
    s.alpha_ref = 8;                         // ALPHAREF 8
    s.alpha_func = 5;                        // ALPHAFUNC GREATER
    s.specular = false;                      // SPECULARENABLE 0
    s.color_key = false;                     // COLORKEYENABLE is never set by the game: alpha does it
    s.blend_enable = false;
    s.src_blend = 2;
    s.dst_blend = 1;
    s.flat = false;

    bof3::LogFlush();
    bof3::Log("Display_Setup: %u x %u, scale %u, %s filter, device %d", width, height, g_scale,
              options.point_filter ? "point" : "linear", *device);
    return 0;
}

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

unsigned DisplaySetup_TargetScale() { return g_backend_up ? g_scale : 0; }

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
    BOF3_INJECT(Display_Setup);
}
