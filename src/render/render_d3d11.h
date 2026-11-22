// The render backend's GPU half: Direct3D 11 (docs/render-backend.md).
//
// Runs the frame that render_shim.cpp recorded - clears and TLVERTEX draws
// against the game's own surfaces - into a render target of the logical
// picture's size times an integer scale, then puts that target on the swap
// chain's back buffer, centred, and presents. Every entry into Direct3D runs on
// the main thread, on a render fiber with a 1 MB stack of its own, since the
// game calls the present from a 16 KB task stack (docs/SCAFFOLDING.md section
// 3, docs/render-backend.md section 4); a call from another thread ends the
// process.
#pragma once

#include <cstdint>

#include "render/render_shim.h"

namespace render {

struct Options {
    void* hwnd;            // the game's window
    U logical_w, logical_h;   // the view the game draws, in its own pixels: 320 x 240
    U scale;               // integer scale k: the target is (logical_w + 2 pad_x) k x logical_h k
    U pad_x;               // columns each side of the view, in the game's pixels: 32 at 16:10, 53 at 16:9 and 120 at 21:9 (DIV-0041), else 0
    bool point_filter;     // the present pass: nearest (true) or bilinear
    bool vsync;
    bool snap;             // DIV-0042: the present scales the target by a whole number (true) or fits it to the client
};

// Creates the device and swap chain on the window. Fatal on failure with the
// HRESULT in the log. Installs itself as the shim's present hook.
void InitD3d11(const Options& options);

// The present: runs `frame` and shows it. The shim calls it through the hook.
void PresentFrame(Frame& frame);

// Frees the GPU objects of released surfaces. Called by the present.
void SweepReleased();

// The target's size now in pixels - (logical_w + 2 pad_x) k by logical_h k -
// and its k (for the set-up to hand the game).
U TargetWidth();
U TargetHeight();
U TargetScale();

// DIV-0042: asks for the target to be remade at scale k (1..8) after the
// next present - between frames, so no recorded draw meets the wrong size.
// The hook is called on the render fiber right after the target changes,
// with the new k, for the set-up to give the game its new numbers.
void RequestScale(U k);
void SetRescaleHook(void (*hook)(U k));

// Writes the target - the last frame presented, at its own size, before the
// present's scaling and any look - to `path` as a 24-bit BMP. For a
// recipe's shot (BOF3X_SHOT_DIR) and F11: the frame as the game drew it,
// whatever covers the window. False before the device exists or when the
// file cannot be written; the reason is logged.
bool SaveFrame(const wchar_t* path);

#ifndef _WIN32
// The movies' picture on a system with no GDI (docs/replacing-mci.md
// section 9; the owner's call of 2026-11-19: the renderer comes up before
// the movies there). fmv_play.cpp's native::Draw calls it with each frame it
// draws. The first call makes the device and the swap chain on `window` (the
// renderer's window, game_window::RendererWindow); InitD3d11 later keeps
// them and makes the rest. Windows draws its movies with GDI and has no such
// call.
struct MoviePicture {
    void* window;              // the renderer's window
    const void* pixels;        // the decoder's frame: B, G, R, X bytes a pixel, rows top-down, width * 4 bytes each
    U width, height;
    int left, top, dest_w, dest_h;   // DIV-0035's rectangle in the client
    bool whole;                // DIV-0042: a whole multiple - each pixel repeated (point); else a fit, averaged (mips, linear)
    const char* shot;          // non-null: the picture presented is also written here as a 24-bit BMP, the whole client
};

// The client cleared to black, the frame drawn into the rectangle, the
// present. Fatal when the device cannot be made; false, with the reason
// logged, when a shot asked for cannot be written.
bool PresentMovie(const MoviePicture& picture);
#endif

}  // namespace render
