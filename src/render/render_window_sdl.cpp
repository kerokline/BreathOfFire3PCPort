// render_window.h off Windows (docs/linux-round.md, R9; wave B, B1): the
// renderer's window is the game's SDL window. game_window_other.cpp hands the
// renderer the SDL_Window* itself (native::RendererWindow), and DXVK-native's
// SDL3 window layer takes an SDL_Window* where Direct3D takes an HWND: its
// fromHwnd is a reinterpret_cast of the one to the other
// (src/wsi/sdl3/wsi_window_sdl3.cpp at the pinned commit, b1a1c99a). So the
// swap chain's window is the SDL window unchanged, and the client size is
// SDL's size in pixels, which is what DXVK's swap chain makes its back buffer
// from (the same file's getWindowSize: SDL_GetWindowSizeInPixels).
//
// On Windows render_d3d11.cpp defines both from the HWND; this file compiles
// to nothing there.
#ifndef _WIN32

#include <SDL3/SDL.h>

#include "hook/log.h"
#include "render/render_window.h"

namespace render {

void* SwapChainWindow(void* window) { return window; }

// A window already destroyed: SDL3 checks every SDL_Window* it is handed
// against its live objects (SDL_ObjectValid), so a gone window fails here
// with "Invalid window" instead of reading freed memory - the counterpart of
// GetClientRect failing on a destroyed HWND.
bool WindowClientSize(void* window, U* w, U* h) {
    *w = 0;
    *h = 0;
    int pw = 0, ph = 0;
    if (!SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(window), &pw, &ph)) {
        bof3::Log("render: SDL_GetWindowSizeInPixels failed (%s) - window gone, nothing shown", SDL_GetError());
        return false;
    }
    *w = static_cast<U>(pw < 0 ? 0 : pw);
    *h = static_cast<U>(ph < 0 ? 0 : ph);
    return true;
}

}  // namespace render

#endif  // !_WIN32
