// Internal to psx_rest.cpp and psx_rest_fuzz.cpp: the addresses the PSX
// library layer's last seventeen touch (docs/psx-rest.md), the COM methods and
// Win32 imports they call, and every relative call they make - through
// pointers, so that the start-up fuzz can stand recorders in for the callees,
// for Capcom's copies and for ours alike.
#pragma once

#include <cstdint>
#include <cstring>

namespace psx_rest {

using U = std::uint32_t;

// --- Data, all Capcom's addresses (docs/psx-rest.md section 1) ---------------
// kClutRows: now Gfx_ClutRows (symbols.toml).
constexpr U kClutRowsEnd = 0x6C3A40;  // Gfx_ClutRows: 512 pairs (counter, row buffer)
// kTexCache: now Gfx_TexCache (symbols.toml).
constexpr U kTexCacheEnd = 0x6C9F40;  // Gfx_TexCache: 32 pages of 32 entries of 0x18
constexpr U kTexPageBytes = 0x300, kTexEntryBytes = 0x18;
// kFontCache: now Font_TexCache (symbols.toml).
constexpr U kFontCacheEnd = 0x7CA950;  // Font_TexCache: 128 entries of 0x14
constexpr U kFontEntryBytes = 0x14;
// kAfterDraw: now D3d_CaptureReady (symbols.toml). D3d_AfterDraw's block, 7 dwords (display-setup.md section 7)
// kAfterDrawSurface: now D3d_CaptureSurface (symbols.toml).
// kAfterDrawTexSurface: now D3d_CaptureTexSurface (symbols.toml).
// kAfterDrawTexture: now D3d_CaptureTexture (symbols.toml).
// kOrphans: now PsxRest_Orphans (symbols.toml). six COM pointers only the teardown names (section 2)
constexpr ::bof3::image::Va kBackdropBlock{0x6BE9F8};                        // 7 dwords; Dd_CellBackdrop 0x6BEA00 is its third
// kBackdropMagic: now PsxRest_BackdropMagic (symbols.toml). set to 0x12345678 after the block is zeroed
// kBackdrop: now Dd_CellBackdrop (symbols.toml). Dd_CellBackdrop
constexpr U kBackdropPairs = 0x6BEA04;                        // two pairs (+0, +8), released, not zeroed by name
constexpr ::bof3::image::Va kRenderFlags{0x6C3A4C};                          // Gfx_RenderFlags: bit 0x100 (a dword test) fullscreen
// The teardown's ten slots, in the order it releases them.
// kBackMaterial: now D3d_BackMaterial (symbols.toml).
// kViewport: now D3d_Viewport (symbols.toml).
// kDevice: now D3d_Device (symbols.toml).
// kDirect3D: now PsxRest_Direct3D (symbols.toml).
// kClipper: now PsxRest_Clipper (symbols.toml).
// kZBuffer: now PsxRest_ZBuffer (symbols.toml).
// kStage: now Dd_StageSurface (symbols.toml).
// kBackBuffer: now DDraw_BackBuffer (symbols.toml).
// kPrimary: now DDraw_Primary (symbols.toml).
constexpr ::bof3::image::Va kDirectDraw{0x7CC334};
// kSndDevice: now Snd_Device (symbols.toml).
// kSndPrimary: now Snd_Primary (symbols.toml). Snd_Device, Snd_Primary
constexpr ::bof3::image::Va kErrorText{0x66B6A8};                            // Display_ErrorBox's table: codes below 100
constexpr ::bof3::image::Va kErrorTextHigh{0x66B568};                        // ... and 100 on, indexed by the code itself
constexpr ::bof3::image::Va kErrorCaption{0x66BC1C};                         // the box's caption, a string in the image
constexpr U kCellSlots = 0x80;                                // D3d_CellTexCache's 128 entries

// The float the setters store per vertex, 0.01 (docs/psx-library-layer.md
// section 1), and the texture-window primitive's code dword.
constexpr U kPointZeroOne = 0x3C23D70Au;
constexpr U kTexWindowCode = 0xF0000000u;
constexpr U kMagic = 0x12345678u;

// --- COM, as the original calls it: through the vtable each time ------------
// IUnknown::Release +0x08 (every object); IDirectDraw4 RestoreDisplayMode
// +0x4C and SetCooperativeLevel +0x50; IDirectDrawSurface4 GetDC +0x44 and
// ReleaseDC +0x68.
constexpr unsigned kRelease = 0x08, kRestoreDisplayMode = 0x4C, kSetCooperativeLevel = 0x50;
constexpr unsigned kGetDC = 0x44, kReleaseDC = 0x68;
constexpr unsigned kDdsclNormal = 8;                          // DDSCL_NORMAL

using ComCall = long (__stdcall*)(void*);
using ComCoop = long (__stdcall*)(void*, void* hwnd, unsigned long flags);
using ComGetDC = long (__stdcall*)(void*, void** hdc);
using ComReleaseDC = long (__stdcall*)(void*, void* hdc);

template <class Fn>
inline Fn Method(void* object, unsigned offset) {
    void* const* vtable;
    std::memcpy(&vtable, object, sizeof vtable);
    return reinterpret_cast<Fn>(vtable[offset / 4]);
}

// The Win32 import slots (the IAT, by name from the import directory
// 2026-10-05).
// kImportSetTextColor: now PsxRest_ImportSetTextColor (symbols.toml). GDI32 SetTextColor
// kImportSetBkMode: now PsxRest_ImportSetBkMode (symbols.toml). GDI32 SetBkMode
// kImportTextOutA: now PsxRest_ImportTextOutA (symbols.toml). GDI32 TextOutA
// kImportMessageBoxA: now PsxRest_ImportMessageBoxA (symbols.toml). USER32 MessageBoxA
constexpr U kWhite = 0xFFFFFF, kTransparent = 1, kIconHand = 0x10;

struct Callees {
    // The teardown's seven, this module's own
    void (__cdecl* free_clut_rows)();
    void (__cdecl* release_tex_cache)();
    void (__cdecl* release_font_cache)();
    void (__cdecl* release_orphans)();
    void (__cdecl* release_backdrop)();
    void (__cdecl* free_cell_textures)();
    void (__cdecl* release_after_draw)();
    // Others': the C runtime's (Capcom's), tex_cells.cpp's, battle_items.cpp's
    // and sound.cpp's (ours)
    void (__cdecl* free)(void*);                          // Crt_free
    void (__cdecl* free_cell_texture)(int slot);          // D3d_FreeCellTexture
    void (__cdecl* stream_stop)();                        // SndStream_Stop
    void (__cdecl* music_release)();                      // Music_Release
    // Win32, through the game's own import slots
    unsigned long (__stdcall* set_text_color)(void* hdc, unsigned long color);
    int (__stdcall* set_bk_mode)(void* hdc, int mode);
    int (__stdcall* text_out)(void* hdc, int x, int y, const char* text, int n);
    int (__stdcall* message_box)(void* hwnd, const char* text, const char* caption, unsigned type);
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=psx_rest: the start-up fuzz, psx_rest_fuzz.cpp. Clones every
// original before PsxRest_Inject patches it.
void SelfTest();

}  // namespace psx_rest
