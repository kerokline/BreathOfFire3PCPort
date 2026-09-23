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
#include "hook/detour.h"
#include "hook/log.h"
#include "render/render_d3d11.h"
#include "render/render_shim.h"

namespace {

using U = std::uint32_t;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}
void PutFloat(U address, float v) { std::memcpy(At(address), &v, sizeof v); }
void PutPtr(U address, const void* p) { PutLong(address, static_cast<U>(reinterpret_cast<std::uintptr_t>(p))); }
void Zero(U address, U bytes) { std::memset(At(address), 0, bytes); }

// --- the globals (display-setup.md section 3) -------------------------------------
constexpr U kDesktopW = 0x7C9F44, kDesktopH = 0x6C9F40, kDesktopBpp = 0x6BE1E0;
constexpr U kTexCache = 0x6C3F40, kTexCacheBytes = 0x6000;
constexpr U kClutRows = 0x6C2A40, kClutRowsBytes = 0x1000;
constexpr U kFontCache = 0x7C9F50, kFontCacheBytes = 0xA00, kFontEntry = 0x14;
constexpr U kBackdropBlock = 0x6BE9F8, kBackdropBytes = 0x1C;
constexpr U kAfterDrawBlock = 0x7CADE8, kAfterDrawBytes = 0x1C;
constexpr U kCellCache = 0x7CAE38, kCellCacheBytes = 0x1400;
constexpr U kPixelFormat = 0x7DED60, kPixelFormatRecord = 0x40;
constexpr U kRenderFlags = 0x6C3A4C, kUnread6C3A48 = 0x6C3A48, kShadeCache = 0x6C3A44;
constexpr U kTexCoords = 0x7CA9E0;
constexpr U kDeviceCountLess1 = 0x66B718, kDeviceRecords = 0x6C3A50, kDeviceRecord = 0x13C;
constexpr U kModeLists = 0x6BE1E8, kModeListsBytes = 0x810;
constexpr U kDirectDraw = 0x7CC334, kDirect3D = 0x7CC34C, kClipper = 0x7CC348, kZBuffer = 0x7CC340;
constexpr U kScreenRight = 0x66B710, kScreenBottom = 0x66B714, kModeBpp = 0x7CADE0;
constexpr U kScaleX = 0x7C9F4C, kScaleY = 0x7C9F48;
constexpr U kDeviceDesc = 0x7CC238, kDeviceDescBytes = 0xFC;
constexpr U kPrimary = 0x7CC338, kBackBuffer = 0x7CC33C, kStage = 0x7CC344;
constexpr U kDevice = 0x7CC350, kViewport = 0x7CC354, kMaterial = 0x7CC358, kMaterialHandle = 0x6C3A40;
constexpr U kBackPitch = 0x7DED5C;

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

render::Options ReadOptions(HWND hwnd) {
    render::Options o = {};
    o.hwnd = hwnd;
    o.scale = 1;
    char text[32];
    U k = 2;
    if (GetEnvironmentVariableA("BOF3X_SCALE", text, sizeof text) > 0) {
        k = 0;
        for (const char* p = text; *p >= '0' && *p <= '9'; ++p) k = k * 10 + static_cast<U>(*p - '0');
        if (k < 1 || k > 8) bof3::Fatal("BOF3X_SCALE=%s: an integer 1..8", text);
    }
    o.logical_w = 320 * k;
    o.logical_h = 240 * k;
    o.point_filter = false;
    if (GetEnvironmentVariableA("BOF3X_FILTER", text, sizeof text) > 0) {
        if (std::strcmp(text, "point") == 0) o.point_filter = true;
        else if (std::strcmp(text, "linear") != 0) bof3::Fatal("BOF3X_FILTER=%s: point or linear", text);
    }
    o.vsync = GetEnvironmentVariableA("BOF3X_VSYNC", nullptr, 0) > 0;
    return o;
}

}  // namespace

extern "C" int __cdecl Display_Setup(void* hwnd_, int* fullscreen, int* device, int* /*mode*/) {
    HWND hwnd = static_cast<HWND>(hwnd_);

    // 2.1 the desktop
    HDC dc = GetDC(nullptr);
    PutLong(kDesktopW, static_cast<U>(GetDeviceCaps(dc, HORZRES)));
    PutLong(kDesktopH, static_cast<U>(GetDeviceCaps(dc, VERTRES)));
    PutLong(kDesktopBpp, static_cast<U>(GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES)));
    ReleaseDC(nullptr, dc);

    // 2.2 the caches, reset - every one the original zeroes
    Zero(kTexCache, kTexCacheBytes);
    Zero(kClutRows, kClutRowsBytes);
    Zero(kFontCache, kFontCacheBytes);
    for (U i = 0; i < kFontCacheBytes / kFontEntry; ++i) PutWord(kFontCache + i * kFontEntry, 0xFFFF);
    Zero(kBackdropBlock, kBackdropBytes);
    Zero(kAfterDrawBlock, kAfterDrawBytes);
    Zero(kCellCache, kCellCacheBytes);
    Zero(kPixelFormat, 3 * kPixelFormatRecord);
    PutLong(kRenderFlags, 0);
    PutLong(kUnread6C3A48, 0);
    PutLong(kShadeCache, 0);

    // 2.3 the texture coordinates: i / 256 + 0.002 in double, stored as float
    for (U i = 0; i < 256; ++i) PutFloat(kTexCoords + i * 4, static_cast<float>(i * 0.00390625 + 0.0020000000949949026));

    // 2.4 the device records: 0 the original's synthetic software renderer (kept
    // so that F7's name overlay reads it), 1 this backend. No enumeration.
    if (static_cast<int>(*reinterpret_cast<U*>(At(kDeviceCountLess1))) < 1) {
        Zero(kDeviceRecords, 2 * kDeviceRecord);
        Zero(kModeLists, kModeListsBytes);
        PutLong(kDeviceRecords + 0, 1);
        PutLong(kDeviceRecords + 4, 1);
        std::memcpy(At(kDeviceRecords + 0x18), "Software Render", 16);
        PutLong(kDeviceRecords + kDeviceRecord + 0, 1);
        PutLong(kDeviceRecords + kDeviceRecord + 4, 1);
        std::memcpy(At(kDeviceRecords + kDeviceRecord + 0x18), "Direct3D 11 (bof3x)", 20);
        PutLong(kDeviceCountLess1, 1);
    }
    // 2.5 which device: the original wraps an index past the last record to 0
    // and takes record 0 as the software renderer. Here every index is the
    // hardware path (DIV-0031); the wrap is kept for F7's cycling.
    if (*device < 0 || *device > 1) *device = 0;
    PutLong(kRenderFlags, 2);   // bit 1: the device allows windowed

    // 2.6 the mode and the scale: the logical picture is the target's size
    const render::Options options = ReadOptions(hwnd);
    if (!g_backend_up) {
        render::InitShim(256 * 1024, 32 * 1024, 8 * 1024 * 1024);
        render::InitD3d11(options);
        g_backend_up = true;
    }
    const U width = render::TargetWidth(), height = render::TargetHeight();
    PutLong(kScreenRight, width);
    PutLong(kScreenBottom, height);
    PutLong(kModeBpp, kModeBits);
    PutFloat(kScaleX, static_cast<float>(width / 320.0));
    PutFloat(kScaleY, static_cast<float>(height / 240.0));
    Zero(kDeviceDesc, kDeviceDescBytes);
    PutLong(kDeviceDesc + kDescTextureCaps, kTextureCaps);
    PutLong(kDeviceDesc + kDescMinW, kMinTexture);
    PutLong(kDeviceDesc + kDescMinH, kMinTexture);
    PutLong(kDeviceDesc + kDescMaxW, kMaxTexture);
    PutLong(kDeviceDesc + kDescMaxH, kMaxTexture);

    // 2.7 the windowed path, always: DirectDraw, primary, back buffer, no
    // clipper, Gfx_RenderFlags bit 0x200. The window rect stays 0x5A5130's.
    PutPtr(kDirectDraw, render::DirectDrawObject());
    PutPtr(kDirect3D, render::DirectDrawObject());   // released by the teardown, read by nothing else
    render::Surface* primary = render::MakeSurface(width, height, 32, 0x200, 0, true, false);
    render::Surface* back = render::MakeSurface(width, height, 32, 0x2040, 0, false, true);
    if (!primary || !back) bof3::Fatal("Display_Setup: out of memory for the primary and back surfaces");
    PutPtr(kPrimary, primary);
    PutPtr(kBackBuffer, back);
    PutLong(kClipper, 0);
    PutLong(kZBuffer, 0);
    *reinterpret_cast<U*>(At(kRenderFlags)) |= 0x200;
    (void)fullscreen;   // never forced: no display mode is set (DIV-0031)

    // 2.9 the device
    PutPtr(kDevice, render::DeviceObject());

    // 2.10 the texture formats, the staging surface, the back buffer's pitch
    FillFormat(kPixelFormat + 0 * kPixelFormatRecord, kFormatAlpha, kRankAlpha);
    FillFormat(kPixelFormat + 1 * kPixelFormatRecord, kFormatOpaque, kRankOpaque);
    FillFormat(kPixelFormat + 2 * kPixelFormatRecord, kFormatScreen, kRankScreen);
    if (kNoAlphaFormat) *reinterpret_cast<U*>(At(kRenderFlags)) |= 0x20;
    if (!Dd_CreatePlainSurface(0x140, 0x100, reinterpret_cast<void**>(At(kStage)), 0))
        bof3::Fatal("Display_Setup: the staging surface");
    PutLong(kBackPitch, back->pitch);

    // 2.11 viewport, material, states - through the backend's objects, so that
    // the device holds the same state the original's did
    PutPtr(kViewport, render::ViewportObject());
    PutPtr(kMaterial, render::MaterialObject());
    PutLong(kMaterialHandle, 1);
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
    bof3::Log("Display_Setup: %u x %u, scale %.1f, %s filter, device %d", width, height, static_cast<double>(width / 320.0),
              options.point_filter ? "point" : "linear", *device);
    return 0;
}

void DisplaySetup_Inject() { BOF3_INJECT(Display_Setup); }
