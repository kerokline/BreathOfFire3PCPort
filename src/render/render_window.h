// The renderer's window: the two things render_d3d11.cpp needs from it
// (docs/linux-round.md, "The 38 files, staged as one round", R9).
//
// `window` is Options::hwnd, as the set-up handed it to InitD3d11. The
// renderer does not make, move or size a window; it asks only these two.
//
//   Windows: `window` is the game's HWND. Both are defined in render_d3d11.cpp
//            from it, as they always were: the HWND itself, and GetClientRect.
//   Linux:   DXVK-native (section 6, call 1) takes an SDL_Window* where
//            Direct3D takes an HWND - its SDL3 window layer, picked by
//            DXVK_WSI_DRIVER=SDL3. The renderer does not define these two
//            there: the SDL window code (R6's window, stage LC's link) must,
//            for whatever it puts in Options::hwnd. Nothing links on Linux
//            yet, so an unresolved symbol at the first link is the reminder,
//            not a silent fallback.
#pragma once

#include "render/render_shim.h"

namespace render {

// What the swap chain's OutputWindow and DXGI's MakeWindowAssociation take
// for `window`. Windows: `window` unchanged (the HWND). Linux: the
// SDL_Window* of the game's window.
void* SwapChainWindow(void* window);

// The window's client area in pixels, into *w and *h. False when there is no
// window to ask - it is already destroyed (WM_DESTROY arrives through the
// loop, and the frame in flight still presents after it) - with the reason
// logged; *w and *h are then 0. A minimised window answers true with a size
// that may be 0. Linux: SDL_GetWindowSizeInPixels on the SDL_Window* - the
// swap chain's back buffer is in pixels, not in SDL's window coordinates.
bool WindowClientSize(void* window, U* w, U* h);

}  // namespace render
