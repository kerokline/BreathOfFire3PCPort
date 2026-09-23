// Internal to display_env.cpp and display_env_fuzz.cpp: the addresses the
// display environments, the present and the sound bank load touch, the COM
// methods they call through vtables, and every relative call they make -
// through pointers, so that the start-up fuzz can stand recording functions in
// for them, for Capcom's copies and for ours alike. docs/display-env.md.
#pragma once

#include <cstdint>
#include <cstring>

namespace display_env {

using U = std::uint32_t;

// --- Data, all Capcom's addresses (docs/display-env.md section 1) -----------
constexpr U kDispEnv = 0x7DECE0;        // Gpu_DispEnv: the last DISPENV put, 0x14 bytes
constexpr U kDrawEnv = 0x7DED00;        // Gpu_DrawEnv: the last DRAWENV put, 0x5C bytes
constexpr U kDrawEnvBackground = 0x7DED18;   // Gpu_DrawEnv +0x18, the DRAWENV's isbg byte
constexpr U kRenderFlags = 0x6C3A4C;    // Gfx_RenderFlags: bit 0 software, bit 0x200 (a dword test) windowed
constexpr U kPrimary = 0x7CC338;        // DDraw_Primary, an IDirectDrawSurface *
constexpr U kBackBuffer = 0x7CC33C;     // DDraw_BackBuffer
constexpr U kViewport = 0x7CC354;       // D3d_Viewport, an IDirect3DViewport *
constexpr U kBackMaterial = 0x7CC358;   // D3d_BackMaterial, an IDirect3DMaterial *
constexpr U kBackHandle = 0x6C3A40;     // D3d_BackMaterialHandle, what SetBackground takes
constexpr U kScreenRect = 0x66B708;     // Gfx_ScreenRect, {0, 0, 640, 480} in the image
constexpr U kWindowRect = 0x6BE1D0;     // Gfx_WindowRect, the client area on the desktop (0x5A5130 sets it)
constexpr U kInverse255At = 0x5C4620;   // float 1 / 255, 0x3B808081

// The sound banks (docs/sound.md section 1): bank n's record is
// Sound_Banks + (n - 1) * 0x384 - cues from +0, 64 voice entries of 8 bytes
// (sample, buffer) from +0x180, the sample data pointer at +0x380 - and the 23
// channel dwords Sound_Channels 0x6BC8C8..0x6BC924.
constexpr U kBanks = 0x6BC928;
constexpr U kBankBytes = 0x384;
constexpr U kBankHeader = 0x380;
constexpr U kVoices = 0x180;
constexpr U kChannels = 0x6BC8C8, kChannelsEnd = 0x6BC924;

// --- COM, as the original calls it: through the vtable each time -----------
// IDirectDrawSurface: Blt +0x14, Flip +0x2C, Restore +0x6C.
// IDirect3DViewport: SetBackground +0x20, Clear +0x30.
// IDirect3DMaterial: SetMaterial +0x0C.
constexpr unsigned kBlt = 0x14, kFlip = 0x2C, kRestore = 0x6C;
constexpr unsigned kSetBackground = 0x20, kClear = 0x30;
constexpr unsigned kSetMaterial = 0x0C;
constexpr long kSurfaceLost = static_cast<long>(0x887601C2u);   // DDERR_SURFACELOST
constexpr unsigned kBltWait = 0x1000000, kBltColorFill = 0x400;  // DDBLT_WAIT, DDBLT_COLORFILL
constexpr unsigned kFlipWait = 1;                                 // DDFLIP_WAIT
constexpr unsigned kClearTarget = 1;                              // D3DCLEAR_TARGET
constexpr unsigned kBltFxBytes = 0x64;                            // sizeof(DDBLTFX)
constexpr unsigned kMaterialBytes = 0x50;                         // sizeof(D3DMATERIAL)

using ComCall = long (__stdcall*)(void*);
using ComBlt = long (__stdcall*)(void*, const void* dst_rect, void* src, const void* src_rect, unsigned long flags,
                                 void* fx);
using ComFlip = long (__stdcall*)(void*, void* target, unsigned long flags);
using ComClear = long (__stdcall*)(void*, unsigned long count, const void* rects, unsigned long flags);
using ComValue = long (__stdcall*)(void*, unsigned long);
using ComPointer = long (__stdcall*)(void*, void*);

template <class Fn>
inline Fn Method(void* object, unsigned offset) {
    void* const* vtable;
    std::memcpy(&vtable, object, sizeof vtable);
    return reinterpret_cast<Fn>(vtable[offset / 4]);
}

struct Callees {
    // This module's own
    void (__cdecl* present)();                                     // Gfx_Present
    void (__cdecl* clear_present)(unsigned flags);                 // Gfx_ClearPresent
    void (__cdecl* set_back_color)(unsigned r, unsigned g, unsigned b);   // D3d_SetBackColor
    // The C runtime (Capcom's) and group Q's sound buffers (ours, sound.cpp)
    void* (__cdecl* malloc)(unsigned);                             // Crt_malloc
    void (__cdecl* free)(void*);                                   // Crt_free
    void (__cdecl* release)(void*);                                // SndBuf_Release
    void* (__cdecl* from_wave)(const unsigned char*);              // SndBuf_FromWave
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=display_env: the start-up fuzz, display_env_fuzz.cpp. Clones
// every original before DisplayEnv_Inject patches it.
void SelfTest();

}  // namespace display_env
