// Internal to d3d_rest.cpp and d3d_rest_fuzz.cpp: every call the renderer's
// live remainder makes that is not a COM method, through pointers, so that the
// start-up fuzz can stand recording functions in for them - for Capcom's copies
// and for ours alike. The COM calls go through D3d_Device and through the
// DirectDraw surfaces in the after-draw block and DDraw_BackBuffer, which the
// fuzz replaces with fakes. docs/d3d-rest.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace d3d_rest {

using U = std::uint32_t;

// A bare `ret` (d3d_draw_callees.h kRetOnly): every handler calls it twice.
constexpr U kRetOnly = bof3::addr::BareRet;

// Data the handlers read, all Capcom's addresses (docs/d3d-draw.md section 2).
constexpr U kScaleX = 0x7C9F4C;        // D3d_ScaleX, float
constexpr U kScaleY = 0x7C9F48;        // D3d_ScaleY, float
constexpr U kVertices = 0x7CA958;      // D3d_Vertices, 4 x D3DTLVERTEX
constexpr U kTexCoords = 0x7CA9E0;     // D3d_TexCoords, 256 floats
constexpr U kDrawTpage = 0x7DED14;     // Gfx_DrawTpage, u16 (read as a dword, masked)
constexpr U kDevice = 0x7CC350;        // D3d_Device, an IDirect3DDevice3 *
constexpr U kRhwNumerator = 0x5C4610;  // float 0.1: rhw = 0.1 / z
constexpr U kRenderFlags = 0x6C3A4C;   // Gfx_RenderFlags, byte: bit 0 the software surfaces

// D3d_FlattenFT3 / D3d_PageTexel4 (docs/d3d-rest.md section 3).
constexpr U kVram = 0x6C9F44;          // Gfx_VramShadow (a macro of symbols.gen.h): 1024 x 512 cells of 16 bits
constexpr U kPixelFormat = 0x7DED60;   // Gfx_PixelFormat: byte +3 bytes per texel; +4 / +8 / +0xC red / green /
                                       // blue shifts; +0x10 / +0x14 / +0x18 their masks
constexpr U kModulate = 0x5C4614;      // float 1/128: the PSX's texture modulation, 0x80 = 1.0

// D3d_SetAlphaModulate.
constexpr U kAlphaOpCache = 0x66B720;  // D3d_AlphaOpCache, dword: 1 when ALPHAOP is MODULATE

// D3d_AfterDraw: the capture block (Display_Setup zeroes 0x7CADE8..0x7CAE03).
constexpr U kCaptureReady = 0x7CADE8;  // u16: set to 1 after each capture
constexpr U kCaptureW = 0x7CADEC;      // u16: the texture's used width (0x140, or the texture's width below it)
constexpr U kCaptureH = 0x7CADEE;      // u16: its used height
constexpr U kCaptureTexW = 0x7CADF0;   // dword: the texture's width (D3d_FitTextureSize's out_w)
constexpr U kCaptureTexH = 0x7CADF2;   // dword: its height (out_h - overlapping the dword above)
constexpr U kCaptureScale = 0x7CADF4;  // float: used width / 320
constexpr U kCaptureSurface = 0x7CADF8;    // IDirectDrawSurface4 *: the 320-wide plain surface
constexpr U kCaptureTexSurface = 0x7CADFC; // IDirectDrawSurface4 *: the texture's surface
constexpr U kCaptureTexture = 0x7CAE00;    // IDirect3DTexture2 *
constexpr U kBackBuffer = 0x7CC33C;    // DDraw_BackBuffer
constexpr U kScreenBpp = 0x7DEDE3;     // byte +3 of the third pixel-format record (0x7DEDE0): the screen's bytes per pixel
constexpr U kFixedOne = 0x5C4608;      // float 65536.0
constexpr U kPerColumn = 0x5C460C;     // float 0.003125 = 1 / 320

struct Callees {
    void (__cdecl* prim_color)(unsigned r, unsigned g, unsigned b, unsigned code, unsigned mode,
                               unsigned long* diffuse, unsigned long* specular);   // D3d_PrimColor
    long (__cdecl* bind_texture)(unsigned tpage, unsigned clut);                    // D3d_BindTexture
    void (__cdecl* ret_only)(unsigned);                                             // 0x437CC0
    void (__cdecl* set_blend)(unsigned code, unsigned mode);                        // D3d_SetBlend
    void (__cdecl* set_shade)(unsigned mode);                                       // D3d_SetShadeMode
    void* (__cdecl* clut_pixels)(unsigned clut_id);                                 // Gfx_ClutPixels
    void (__cdecl* init_desc)(void* desc);                                          // Dd_InitSurfaceDesc
    int (__cdecl* create_plain)(unsigned w, unsigned h, void** surface, unsigned format);   // Dd_CreatePlainSurface
    void (__cdecl* fit_size)(unsigned w, unsigned h, unsigned* out_w, unsigned* out_h);   // D3d_FitTextureSize
    int (__cdecl* create_texture)(unsigned w, unsigned h, void** surface, void** texture,
                                  unsigned format);                                 // Dd_CreateTextureSurface
};

extern const Callees kOriginals;
extern Callees g;

void SelfTest();   // d3d_rest_fuzz.cpp

}  // namespace d3d_rest
