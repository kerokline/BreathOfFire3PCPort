// The render backend's GPU half: Direct3D 11 (docs/render-backend.md).
//
// Runs the frame that render_shim.cpp recorded - clears and TLVERTEX draws
// against the game's own surfaces - into a render target of the logical
// picture's size times an integer scale, then puts that target on the swap
// chain's back buffer, centred, and presents. Every call in here happens under
// the present, on the main thread; a call from a game task's 16 KB stack ends
// the process (docs/SCAFFOLDING.md section 3).
#pragma once

#include <cstdint>

#include "render/render_shim.h"

namespace render {

struct Options {
    void* hwnd;            // the game's window
    U logical_w, logical_h;   // the view the game draws, in its own pixels: 320 x 240
    U scale;               // integer scale k: the target is (logical_w + 2 pad_x) k x logical_h k
    U pad_x;               // columns each side of the view, in the game's pixels: 53 wide (DIV-0041), else 0
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

// The logical size the target has now (for the set-up to hand the game).
U TargetWidth();
U TargetHeight();
U TargetScale();

// DIV-0042: asks for the target to be remade at scale k (1..8) after the
// next present - between frames, so no recorded draw meets the wrong size.
// The hook is called on the render fiber right after the target changes,
// with the new k, for the set-up to give the game its new numbers.
void RequestScale(U k);
void SetRescaleHook(void (*hook)(U k));

}  // namespace render
